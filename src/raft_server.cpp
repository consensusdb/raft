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
  std::cout << state.id << " is a candidate" << std::endl;
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
  RPC::VoteRequest vote_req = {state.id, "", storage->current_term(), state.id,
                               info.index, info.term};
  std::for_each(state.known_peers.begin(), state.known_peers.end(),
                [&](auto &peer) {
                  if (peer.id != state.id) {
                    vote_req.destination_id = peer.id;
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
        if (peer.id != state.id) {
          RPC::AppendEntriesRequest ret;
          ret.peer_id = state.id;
          ret.destination_id = peer.id;
          ret.leader_commit = storage->last_commit().index;
          ret.term = storage->current_term();
          ret.prev_log_index = peer.match_index;
          ret.prev_log_term = storage->get_entry_info(ret.prev_log_index).term;
          ret.request_watermark = peer.request_watermark;
          ret.entries = {};
          callbacks.send(peer.id, ret);
        }
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
  RPC::VoteResponse ret = {state.id, request.peer_id, storage->current_term(),
                           false};
  if (state.minimum_timeout_reached &&
      request.term >= storage->current_term() &&
      (storage->voted_for().empty() ||
       storage->voted_for() == request.candidate_id) &&
      request.last_log_index >= storage->get_last_entry_info().index) {
    ret = {state.id, request.peer_id, request.term, true};
    storage->voted_for(request.candidate_id);
    state.leader_id = request.candidate_id;
  }

  if (ret.vote_granted == true) {
    callbacks.set_vote_timeout();
  }
  callbacks.send(request.peer_id, std::move(ret));
}

void Server::on(const std::string &, RPC::AppendEntriesRequest request) {  
  if (request.destination_id != state.id) {
    callbacks.drop(request.peer_id);
    return;
  }
  RPC::AppendEntriesResponse response = {state.id, request.peer_id,
                                         storage->current_term(), false, 0, request.request_watermark };
  auto size = request.entries.size();
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
    std::cout << state.id << " " << request.request_watermark << " EntryRequest after I" << request.prev_log_index << " T" << request.prev_log_term << " now at I" << storage->get_last_entry_info().index << " commit at I" << storage->last_commit().index << " T" << storage->last_commit().term << std::endl;
  }

  if (response.term == request.term) {
    state.minimum_timeout_reached = false;
    callbacks.set_minimum_timeout();
    callbacks.set_vote_timeout();
  }
  callbacks.send(request.peer_id, std::move(response));
}

void Server::on(const std::string &, RPC::VoteResponse response) {
  if (response.destination_id != state.id) {
    callbacks.drop(response.peer_id);
    return;
  }
  if (response.term > storage->current_term()) {
    step_down(response.term);
  }
  if (state.role == State::Role::Candidate &&
      response.term == storage->current_term()) {
    if (response.vote_granted) {
      // add vote
      state.find_peer(response.peer_id)->voted_for_me = true;
      size_t num_votes =
          std::count_if(state.known_peers.begin(), state.known_peers.end(),
                        [](auto &peer) { return peer.voted_for_me; });
      if (num_votes >= (1 + state.known_peers.size() / 2)) {
        std::cout << state.id << " leader" << std::endl;
        state.role = State::Role::Leader;
        state.leader_id = state.id;

        auto info = storage->get_last_entry_info();

        std::for_each(state.known_peers.begin(), state.known_peers.end(),
                      [&](auto &peer) {
                        peer.next_index = info.index + 1;
                        peer.match_index = 0;
                        if ( peer.id == state.id ) {
                          peer.match_index = info.index;
                        }
                      });

        RPC::AppendEntriesRequest request;
        request.peer_id = state.id;
        request.leader_commit = storage->last_commit().index;
        request.peer_id = state.leader_id;
        request.prev_log_index = info.index;
        request.prev_log_term = info.term;
        request.term = storage->current_term();

        std::for_each(state.known_peers.begin(), state.known_peers.end(),
                      [&](auto &peer) {
                        if (peer.id != state.id) {
                          request.destination_id = peer.id;
                          request.request_watermark = peer.request_watermark;
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
  if ( response.destination_id != state.id ) {
    callbacks.drop(response.peer_id);
    return;
  }
  if ( response.term > storage->current_term() ) {
    step_down(response.term);
  } else if ( response.term < storage->current_term() ) {
    return;
  }


  if ( state.role != State::Role::Leader ) {
    return;
  }
  auto &peer = *state.find_peer(response.peer_id);

  if ( response.success == false ) {
    if ( response.request_watermark < peer.request_watermark ) {
      std::cout << "discarding old request" << std::endl;
      return;
    }
    peer.next_index -= 1;
    if ( peer.next_index == 0 ) {
      peer.next_index = 1;
    }
    std::cout << "Peer " << response.peer_id << " failure term " << response.term << " (leader term " << storage->current_term() << ") next index set to " << peer.next_index << std::endl;

    RPC::AppendEntriesRequest request;
    request.peer_id = state.leader_id;
    request.destination_id = response.peer_id;
    request.leader_commit = storage->last_commit().index;
    request.term = storage->current_term();
    request.prev_log_index = peer.next_index - 1;
    request.prev_log_term =
      storage->get_entry_info(request.prev_log_index).term;
    request.entries = storage->entries_since(request.prev_log_index);
    request.request_watermark = ++peer.request_watermark;
    callbacks.send(response.peer_id, std::move(request));
    return;
  } else if ( peer.match_index < response.match_index ) {
    // the match_index in the response doesn't correspond to what the leader
    // has observed so there's potential to commit
    peer.next_index = std::max(peer.next_index, response.match_index + 1);
    peer.match_index = response.match_index;
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
      leader = state.find_peer(state.id);
    }
    RPC::NotLeaderResponse ret = {
      state.id,
      request.client_id,
      leader->id,
      leader->ip_port
    };
    callbacks.client_send(request.client_id, std::move(ret));
    return;
  }
    uint64_t new_index = storage->get_last_entry_info().index + 1;
    // commit locally
    RPC::ClientResponse ret = {{new_index, storage->current_term()}};
    // storage->append is blocking -- make sure it doesn't take too long!
    // The length of time spent here is the amount of time to write to disk.
    // If append does more IO than a simple sequential write to disk you
    // are not using this tool correctly.
    if (storage->append({{ret.entry_info, request.data}}) != new_index) {
      callbacks.client_send(request.client_id, RPC::LocalFailureResponse{});
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
          if ( peer.next_index != new_index ) {
            return; //Don't send more requests to a peer that's not up-to-date
          }
          req.destination_id = peer.id;
          if ( req.prev_log_index != peer.next_index - 1 ) {
            req.prev_log_index = peer.next_index - 1;
            req.prev_log_term = storage->get_entry_info(req.prev_log_index).term;
            req.entries = storage->entries_since(req.prev_log_index);
          }
          req.request_watermark = peer.request_watermark;
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
  
void Server::on(const std::string &, RPC::ConfigChangeRequest request) {
  if ( state.cluster_old.empty() ) {
    state.cluster_old = state.known_peers;
  }
  state.cluster_new = request.peer_configs;
  state.cluster_old
  if(state.new_peers.empty()) {
    //this is phase 2 where we swap out the old peers with the new peers
  } else {
    state.new_peers = request.peer_configs;
  }
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
