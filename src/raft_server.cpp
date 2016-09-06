#include <raft_server.hpp>

#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <simple_serialize.hpp>
#include <functional>

namespace raft {

void Server::on(RPC::TimeoutRequest) {
  std::cout << time(NULL) << " " << state.id << " is a candidate for term " << storage->current_term()+1 << std::endl;
  // set role to candidate
  state.role = State::Role::Candidate;

  // increment current term
  storage->current_term(storage->current_term() + 1);

  // clear any previous votes and vote for self
  std::for_each(
      state.known_peers.begin(), state.known_peers.end(),
      [this](auto peer) mutable { peer.voted_for_me = (state.id == peer.id); });

  storage->voted_for(state.id);
  state.leader_id = state.id;

  // return messages
  auto info = storage->get_last_entry_info();
  RPC::VoteRequest vote_req = {state.id, "", 0, storage->current_term(), state.id,
                               info.index, info.term};
  std::for_each(state.known_peers.begin(), state.known_peers.end(),
                [&](auto &peer) {
                  if (peer.id != state.id) {
                    vote_req.destination_id = peer.id;
                    vote_req.message_id = ++peer.last_message_id;
                    peer.waiting_for_response = true;
                    callbacks.send(peer.id, vote_req);
                  }
                });

  if (state.role == raft::State::Role::Leader) {
    callbacks.set_heartbeat_timeout();
  } else {
    callbacks.set_vote_timeout();
  }
}

void Server::on(RPC::HeartbeatRequest) {
  std::for_each(
      state.known_peers.begin(), state.known_peers.end(), [&](auto &peer) {
        if (peer.id == state.id || peer.waiting_for_response) {
          //don't send a heartbeat to self or to a node that hasn't sent a
          //response to a previous RPC back.
          return;
       }
       peer.waiting_for_response = true;
       RPC::AppendEntriesRequest ret;
       ret.peer_id = state.id;
       ret.destination_id = peer.id;
       ret.message_id = ++peer.last_message_id;
       ret.leader_commit = storage->last_commit().index;
       ret.term = storage->current_term();
       ret.prev_log_index = peer.match_index;
       ret.prev_log_term = storage->get_entry_info(ret.prev_log_index).term;
       ret.entries = {};
       callbacks.send(peer.id, ret);
      });
  callbacks.set_heartbeat_timeout();
}

void Server::on(const std::string &, RPC::VoteRequest request) {
  if (request.destination_id != state.id) {
    callbacks.drop(request.peer_id);
    return;
  }
  std::string vote_for;
  if (request.term > storage->current_term()) {
    step_down(request.term);
  }
  RPC::VoteResponse ret = {state.id, request.peer_id, request.message_id, storage->current_term(),
                           false};
  if (state.minimum_timeout_reached &&
      request.term >= storage->current_term() &&
      (storage->voted_for().empty() ||
       storage->voted_for() == request.candidate_id) &&
      request.last_log_index >= storage->get_last_entry_info().index) {
    ret = {state.id, request.peer_id, request.message_id, request.term, true};
    storage->voted_for(request.candidate_id);
    state.leader_id = request.candidate_id;
  }

  if (ret.vote_granted == true) {
    callbacks.set_vote_timeout();
  } else {
    //if the leader recieves a vote request, it could be because the node crashed during an RPC request and failed to send
    //a response. In such a case we want to reset the status of the node so that it isn't waiting for a response and can
    //recieve updates
    if (state.role == raft::State::Role::Leader) {
      auto *peer = state.find_peer(request.peer_id);
      peer->waiting_for_response = false;
    }
  }
  callbacks.send(request.peer_id, std::move(ret));
}

void Server::on(const std::string &, RPC::AppendEntriesRequest request) {  
  if (request.destination_id != state.id) {
    callbacks.drop(request.peer_id);
    return;
  }
  RPC::AppendEntriesResponse response = {state.id, request.peer_id, request.message_id,
                                         storage->current_term(), false, 0 };
  if (request.term < storage->current_term()) {
    callbacks.send(request.peer_id, std::move(response));
    return;
  }

  if (request.term > storage->current_term() ||
      (request.term == storage->current_term() &&
       state.role == State::Role::Candidate)) {
    step_down(request.term);
  }

  state.leader_id = request.peer_id;
  auto last_commit = storage->last_commit();

  auto info = storage->get_entry_info(request.prev_log_index);
  if ( request.prev_log_index == info.index && request.prev_log_term == info.term) {
    if ( request.prev_log_index < last_commit.index || request.prev_log_term < last_commit.term ) {
      if ( request.entries.empty() ) {
        return;
      }
    } else {
      response.success = true;
      response.match_index = storage->append(std::move(request.entries));
      if ( request.leader_commit > last_commit.index ) {
        std::cout << state.id << " EntryRequest commit " << request.leader_commit << std::endl;
        storage->commit_until(request.leader_commit);
        callbacks.commit_advanced(request.leader_commit);
      }
    }
  }
  if ( response.success == false ) {
    std::cout << state.id << " EntryRequest after I" << request.prev_log_index << " T" << request.prev_log_term << " now at I" << storage->get_last_entry_info().index << " commit at I" << storage->last_commit().index << " T" << storage->last_commit().term << std::endl;
  }

  if (response.term == request.term) {
    state.minimum_timeout_reached = false;
    callbacks.set_minimum_timeout();
    callbacks.set_vote_timeout();
  }
  callbacks.send(request.peer_id, std::move(response));
}

void Server::on(const std::string &, RPC::VoteResponse response) {
  auto *peer = state.find_peer(response.peer_id);
  if (response.destination_id != state.id || 
      peer == nullptr || 
      peer->waiting_for_response == false) {
    if(peer->last_message_id >= response.message_id) {
      //it's pretty common that an old vote response comes in after a heartbeat was sent to claim the leadership.
      //Instead of dropping the connection we just ignore the message as it's harmless and an artifact of 
      //network latency
      callbacks.drop(response.peer_id);
    }
    return;
  }

  if (response.term > storage->current_term()) {
    step_down(response.term);
  }

  if (state.role == State::Role::Candidate &&
      response.term == storage->current_term()) {
    if (response.vote_granted) {
      // add vote
      peer->voted_for_me = true;
      size_t num_votes =
          std::count_if(state.known_peers.begin(), state.known_peers.end(),
                        [](auto &peer) { return peer.voted_for_me; });
      if (num_votes >= (1 + state.known_peers.size() / 2)) {
        std::cout << state.id << " leader" << std::endl;
        state.role = State::Role::Leader;
        state.leader_id = state.id;

        auto info = storage->get_last_entry_info();
        
        RPC::AppendEntriesRequest request;
        request.peer_id = state.id;
        request.leader_commit = storage->last_commit().index;
        request.peer_id = state.leader_id;
        request.prev_log_index = info.index;
        request.prev_log_term = info.term;
        request.term = storage->current_term();

        std::for_each(state.known_peers.begin(), state.known_peers.end(),
                      [&](auto &peer) {
                        peer.waiting_for_response = true;
                        peer.next_index = info.index + 1;
                        peer.match_index = 0;
                        if ( peer.id == state.id ) {
                          peer.match_index = info.index;
                        } else {
                          request.destination_id = peer.id;
                          request.message_id = ++peer.last_message_id;
                          callbacks.send(peer.id, request);
                        }
                      });
        state.minimum_timeout_reached = false;
        callbacks.set_minimum_timeout();
        callbacks.set_heartbeat_timeout();
      }
    }
  }
}

void Server::on(const std::string &, RPC::AppendEntriesResponse response) {
  auto *peer = state.find_peer(response.peer_id);
  if (response.destination_id != state.id || 
      peer == nullptr || 
      peer->waiting_for_response == false ||
      peer->last_message_id != response.message_id) {
    callbacks.drop(response.peer_id);
    return;
  }
  peer->waiting_for_response = false;

  if ( response.term > storage->current_term() ) {
    step_down(response.term);
  } else if ( response.term < storage->current_term() ) {
    return;
  }


  if ( state.role != State::Role::Leader ) {
    return;
  }

  if ( response.success == false ) {
    peer->next_index -= 1;
    if ( peer->next_index == 0 ) {
      peer->next_index = 1;
    }
    std::cout << "Peer " << response.peer_id << " failure term " << response.term << " (leader term " << storage->current_term() << ") next index set to " << peer->next_index << std::endl;

    RPC::AppendEntriesRequest request;
    peer->waiting_for_response = true;
    request.peer_id = state.leader_id;
    request.destination_id = response.peer_id;
    request.message_id = ++peer->last_message_id;
    request.leader_commit = storage->last_commit().index;
    request.term = storage->current_term();
    request.prev_log_index = peer->next_index - 1;
    request.prev_log_term =
      storage->get_entry_info(request.prev_log_index).term;
    request.entries = storage->entries_since(request.prev_log_index);
    callbacks.send(response.peer_id, std::move(request));
    return;
  } else if ( peer->match_index < response.match_index ) {
    // the match_index in the response doesn't correspond to what the leader
    // has observed so there's potential to commit
    peer->next_index = std::max(peer->next_index, response.match_index + 1);
    peer->match_index = response.match_index;
    std::vector<uint64_t> indexes;
    indexes.reserve(state.known_peers.size());
    
    std::for_each(
      std::begin(state.known_peers), std::end(state.known_peers),
      [&indexes](auto &it) mutable { 
      indexes.push_back(it.match_index); 
    });
    // sort in reverse order
    std::sort(std::begin(indexes), std::end(indexes),
              std::greater<uint64_t>());
    // commit the latest commit index
    uint64_t new_commit = indexes[state.known_peers.size() / 2];
    if ( new_commit > storage->last_commit().index ) {
      std::cout << state.id << " New Commit with id " << new_commit << " term " << storage->current_term() << std::endl;
      storage->commit_until(new_commit);
      callbacks.commit_advanced(new_commit);
    }
  }
}

void Server::on(const std::string &, RPC::ClientRequest request) {
  if (state.role != State::Role::Leader) {
    auto *leader = state.find_peer(state.leader_id);
    if(leader == nullptr) {
      //if no leader is available, send self
      leader = state.find_peer(state.id);
    }
    RPC::NotLeaderResponse ret = {
      leader->id,
      leader->ip_port
    };
    callbacks.client_send(request.client_id, std::move(ret));
    return;
  }
    uint64_t new_index = storage->get_last_entry_info().index + 1;
    // commit locally
    RPC::ClientResponse ret = {request.message_id, {new_index, storage->current_term()}};
    // storage->append is blocking -- make sure it doesn't take too long!
    // The length of time spent here is the amount of time to write to disk.
    // If append does more IO than a simple sequential write to disk you
    // are not using this tool correctly.
    if (storage->append({{ret.entry_info, request.data}}) != new_index) {
      //TODO abort?
      return;
    }
    
    callbacks.client_send(request.client_id, std::move(ret));

    raft::RPC::AppendEntriesRequest req;
    req.peer_id = state.id;
    req.leader_commit = storage->last_commit().index;
    req.term = storage->current_term();
    req.prev_log_index = std::numeric_limits<uint64_t>::max();

    std::for_each(
        state.known_peers.begin(), state.known_peers.end(), [&](auto &peer) {
          if ( peer.id == state.id ) {
            peer.match_index = new_index;
            peer.next_index = new_index + 1;
            return;
          }

          if ( peer.next_index != new_index  || peer.waiting_for_response) {
            return; //Don't send more requests to a peer that's not up-to-date
          }

          req.message_id = ++peer.last_message_id;
          peer.waiting_for_response = true;

          req.destination_id = peer.id;
          if ( req.prev_log_index != peer.next_index - 1 ) {
            req.prev_log_index = peer.next_index - 1;
            req.prev_log_term = storage->get_entry_info(req.prev_log_index).term;
            req.entries = storage->entries_since(req.prev_log_index);
          }
          if ( peer.next_index == new_index ) {
            peer.next_index++;
          }
          callbacks.send(peer.id, req);
        });
    callbacks.set_heartbeat_timeout();
}

void Server::on(RPC::MinimumTimeoutRequest) {
  state.minimum_timeout_reached = true;
}

void Server::timeout() {
  if (state.role == raft::State::Role::Leader) {
    on(raft::RPC::HeartbeatRequest{});
  } else {
    on(raft::RPC::TimeoutRequest{});
  }
}
  
void Server::on(const std::string &, RPC::ConfigChangeRequest /*request*/) {
}

void Server::step_down(uint64_t new_term) {
  // out of sync
  // sync term
  if (storage->current_term() != new_term) {
    storage->current_term(new_term);
  }
  storage->voted_for("");

  if (state.role != State::Role::Follower) {
    // fall - back to follower
    state.role = State::Role::Follower;
    std::for_each(state.known_peers.begin(), state.known_peers.end(),
                  [](auto &peer) { peer.reset(); });
  }
}
}
