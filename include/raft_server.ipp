#pragma once
#include <raft_server.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <iterator>
#include <functional>

namespace raft {

template <typename data_t>
void Server<data_t>::on(RPC::TimeoutRequest) {
  // set role to candidate
  state.role = State::Role::Candidate;

  // increment current term
  storage->current_term(storage->current_term() + 1);

  // clear any previous votes and vote for self
  std::for_each(
      state.known_peers.begin(), state.known_peers.end(),
      [this](auto& peer) { peer.voted_for_me = (state.id == peer.id); });

  storage->voted_for(state.id);
  
  // return messages
  RPC::VoteRequest vote_req = { 
    {state.id, "", 0, storage->current_term(), storage->log_state()},
  };

  std::for_each(state.known_peers.begin(), state.known_peers.end(),
                [&](auto &peer) {
                  if (peer.id == state.id) {
                    return;
                  }
                  vote_req.header.destination_id = peer.id;
                  vote_req.header.message_id = ++peer.last_message_id;
                  peer.waiting_for_response = true;
                  callbacks.send(peer.id, vote_req);
                });
  callbacks.set_vote_timeout();
}

template <typename data_t>
void Server<data_t>::on(RPC::HeartbeatRequest) {
  std::for_each(
      state.known_peers.begin(), state.known_peers.end(), [&](auto &peer) {
        if (peer.id == state.id || peer.waiting_for_response) {
          return;
       }
       peer.waiting_for_response = true;
       RPC::AppendEntriesRequest<data_t> request = {
         { state.id, //peer_id
           peer.id, //destination_id
           +peer.last_message_id, //message_id
           storage->current_term(), //term
           storage->log_state()
         }, //header
         peer.match_entry, //previous entry
         {} //entries
       };
       callbacks.send(peer.id, std::move(request));
      });
  callbacks.set_heartbeat_timeout();
}

template <typename data_t>
void Server<data_t>::on(const std::string &, RPC::VoteRequest request) {
  auto *peer = state.find_peer(request.header.peer_id);
  if (request.header.destination_id != state.id || peer == nullptr) {
    callbacks.drop(request.header.peer_id);
    return;
  }

  if(state.role == State::Role::Leader) {
    //when a peer that's dead comes back online, it will announce itself eventually with a vote request. The peer may have crashed without sending a response to an in-flight request, so the leader resets the state of the peer to start recieving updates from the leader again.
    peer->waiting_for_response = false;
  }
  
  RPC::VoteResponse response = { { state.id, request.header.peer_id, request.header.message_id, std::max(storage->current_term(),request.header.term), storage->log_state() }, false };

  if(state.minimum_timeout_reached) {
    //ignore a peer that times out before it's possible. This can occur during a configuration change where a peer gets dropped but doesn't know it's being dropped and starts spamming peers that no longer want to communicate with it
    return;
  }

  if (request.header.term <= storage->current_term()) {
    //peer is old. This can occur if a peer loses network conectivity to the majority of nodes except this one
    callbacks.send(request.header.peer_id, std::move(response));
    return;
  } else {
    step_down(request.header.term);
    state.leader_id = "";
  }

  if ( request.header.log_state.uncommit.index >= storage->log_state().uncommit.index) {
    response.vote_granted = true;
    storage->voted_for(request.header.peer_id);
    callbacks.set_vote_timeout();
  } 
  callbacks.send(request.header.peer_id, std::move(response));
}

template <typename data_t>
void Server<data_t>::on(const std::string &, RPC::AppendEntriesRequest<data_t> request) {
  auto *peer = state.find_peer(request.header.peer_id);
  if (request.header.destination_id != state.id || peer == nullptr) {
    callbacks.drop(request.header.peer_id);
    return;
  }

  RPC::AppendEntriesResponse response = { {state.id, request.header.peer_id, request.header.message_id,
                                         std::max(storage->current_term(), request.header.term), storage->log_state()}, false};

  if (request.header.term < storage->current_term()) {
    //peer is old. This can occur if a peer loses network conectivity to the majority of nodes except this one
    callbacks.send(request.header.peer_id, std::move(response));
    return;
  } else if(request.header.term > storage->current_term()) {
    //peer probably just won leadership and is sending a heartbeat
    step_down(request.header.term);
    if(state.leader_id != request.header.peer_id) {
      state.leader_id = request.header.peer_id;
      callbacks.new_leader_elected(state.leader_id);
    }
  } else if(state.leader_id != request.header.peer_id) {
    if(state.leader_id.empty() && request.entries.empty() ) {
      //heartbeat from the new leader.
      state.leader_id = state.id;
      callbacks.new_leader_elected(state.leader_id);
    } else {
      //peer isn't leader for this term. why are we getting a message from it?
      callbacks.drop(request.header.peer_id);
      return;
    }
  }

  //check to see if the previous entry is the last uncomitted entry. This is the typical case where followers
  //are up-to-date with their leader. this allows us to skip a call to get_entry_info which may require disk I/O
  //if entry meta-data is not in-memory
  auto log_state = storage->log_state();
  if(request.previous_entry != log_state.uncommit) {
    if(request.previous_entry.index < log_state.uncommit.index) {
      //peer is trying to over-write comitted data. committed data is immutable. why did we get this message?
      callbacks.drop(request.header.peer_id);
      return;
    }

    if(request.previous_entry.index > log_state.uncommit.index) {
      //there are entries missing in the middle, the leader needs to go further in the past and get entries
      callbacks.send(request.header.peer_id, std::move(response));
      return;
    }

    if ( request.previous_entry.term != storage->get_entry_info(request.previous_entry.index).term ) {
      //previous entry term doesn't match. This only occurs when a previous leader writes an uncomitted change to
      //a minority of followers of which this machine is part of, the leader dies, and then a new leader is elected
      //by the majority that doesn't have the log. This machine would have voted against the new leader but 
      //lost the vote.
      //the leader needs to back-off some more until the the previous entry is an uncomitted entry on the same term and index 
      //or until the previous_entry reaches the last comitted entry.
      callbacks.send(request.header.peer_id, std::move(response));
      return;
    }
  }

  if ( request.header.log_state.commit.index > storage->log_state().commit.index ) {
    assert(request.header.log_state.commit.term >= storage->log_state().commit.term);
    storage->commit_until(request.header.log_state.commit.index);
    callbacks.commit_advanced(request.header.log_state.commit.index);
  }

  response.success = true;
  response.header.log_state = storage->log_state();

  state.minimum_timeout_reached = false;
  callbacks.set_minimum_timeout();
  callbacks.set_vote_timeout();

  callbacks.send(request.header.peer_id, std::move(response));
}

template <typename data_t>
void Server<data_t>::on(const std::string &, RPC::VoteResponse response) {
  auto *peer = state.find_peer(response.header.peer_id);
  if (response.header.destination_id != state.id || 
      peer == nullptr || 
      peer->waiting_for_response == false) {
    if(peer->last_message_id >= response.header.message_id) {
      //only drop the connection if the message_id is in the future
      callbacks.drop(response.header.peer_id);
    }
    return;
  }

  if (response.header.term > storage->current_term()) {
    step_down(response.header.term);
    return;
  }

  if (state.role != State::Role::Candidate ||
      !response.vote_granted) {
    return;
  }
  
  peer->waiting_for_response = false;

  // add vote
  peer->voted_for_me = true;
  size_t num_votes =
      std::count_if(state.known_peers.begin(), state.known_peers.end(),
                    [](auto &peer) { return peer.voted_for_me; });
  if (num_votes >= (1 + state.known_peers.size() / 2)) {
    state.role = State::Role::Leader;

    if(state.leader_id != state.id) {
      state.leader_id = state.id;
      callbacks.new_leader_elected(state.leader_id);
    }
    
    RPC::AppendEntriesRequest<data_t> request = {
      { 
         state.id, //from
         "", //to(placeholder)
         0, //message_id(placeholder)
         storage->current_term(), //term
         storage->log_state()
      }, //header
      storage->log_state().uncommit,
      {} //entries
    };

    std::for_each(state.known_peers.begin(), state.known_peers.end(),
                  [&](auto &peer) {
                    peer.waiting_for_response = true;
                    peer.next_entry.index = request.previous_entry.index + 1;
                    peer.match_entry.index = 0;
                    if ( peer.id == state.id ) {
                      peer.match_entry = request.previous_entry;
                    } else {
                      request.header.destination_id = peer.id;
                      request.header.message_id = ++peer.last_message_id;
                      callbacks.send(peer.id, request);
                    }
                  });
    state.minimum_timeout_reached = false;
    callbacks.set_minimum_timeout();
    callbacks.set_heartbeat_timeout();
  }
}

template <typename data_t>
void Server<data_t>::on(const std::string &, RPC::AppendEntriesResponse response) {
  auto *peer = state.find_peer(response.header.peer_id);
  if (response.header.destination_id != state.id || 
      peer == nullptr) {
    callbacks.drop(response.header.peer_id);
    return;
  }

  if ( peer->waiting_for_response == false ||
      peer->last_message_id != response.header.message_id ||
      response.header.term < storage->current_term() ) {
    return;
  }

  peer->waiting_for_response = false;

  if ( response.header.term > storage->current_term() ) {
    step_down(response.header.term);
    return;
  }

  if ( state.role != State::Role::Leader ) {
    return;
  }

  if ( response.success == false ) {
    peer->next_entry.index -= 1;
    if ( peer->next_entry.index == 0 ) {
      peer->next_entry.index = 1;
    }
    RPC::AppendEntriesRequest<data_t> request = {
      {
        state.id, //from
        response.header.peer_id, //to
        ++peer->last_message_id, //message id
        storage->current_term(), //term
        storage->log_state()
      }, //header
      storage->get_entry_info(peer->next_entry.index - 1), //previous entry
      storage->entries_since(peer->next_entry.index - 1) // entries
    };
    peer->waiting_for_response = true;
    callbacks.send(response.header.peer_id, std::move(request));
    return;
  } else if ( peer->match_entry.index < response.header.log_state.uncommit.index ) {
    // the uncommitted index in the response doesn't correspond to what the leader
    // has observed so there's potential to commit
    peer->next_entry.index = std::max(peer->next_entry.index, response.header.log_state.uncommit.index + 1);
    peer->match_entry.index = response.header.log_state.uncommit.index;
    std::vector<uint64_t> indexes;
    indexes.reserve(state.known_peers.size());
    
    std::for_each(
      std::begin(state.known_peers), std::end(state.known_peers),
      [&indexes](auto &it) mutable { 
      indexes.push_back(it.match_entry.index); 
    });
    // sort in reverse order
    std::sort(std::begin(indexes), std::end(indexes),
              std::greater<uint64_t>());
    // commit the latest commit index
    uint64_t new_commit = indexes[state.known_peers.size() / 2];
    if ( new_commit > storage->log_state().commit.index ) {
      storage->commit_until(new_commit);
      callbacks.commit_advanced(new_commit);
    }
  }
}

template <typename data_t>
void Server<data_t>::on(const std::string &, RPC::ClientRequest request) {
  if (state.role != State::Role::Leader) {
    auto *leader = state.find_peer(state.leader_id);
    if(leader == nullptr) {
      leader = state.find_peer(state.id);
    }
    callbacks.client_send(request.client_id, {leader->id, leader->ip_port });
    return;
  }
    uint64_t new_index = storage->log_state().uncommit.index + 1;
    // commit locally
    RPC::ClientResponse ret = {request.message_id, {new_index, storage->current_term()}};
    // storage->append is blocking -- make sure it doesn't take too long!
    // The length of time spent here is the amount of time to write to disk.
    // If append does more IO than a simple sequential write to disk you
    // are not using this tool correctly.
    if (storage->append({{ret.entry_info, request.data}}).index != new_index) {
      exit(1);
    }
    
    callbacks.client_send(request.client_id, std::move(ret));

    raft::RPC::AppendEntriesRequest<data_t> req = {
      { state.id, "", 0, storage->current_term(), storage->log_state() },
      {std::numeric_limits<uint64_t>::max(),std::numeric_limits<uint64_t>::max() },
      {}
    };

    std::for_each(
        state.known_peers.begin(), state.known_peers.end(), [&](auto &peer) {
          if ( peer.id == state.id ) {
            peer.match_entry.index = new_index;
            peer.next_entry.index = new_index + 1;
            return;
          }

          if ( peer.next_entry.index != new_index  || peer.waiting_for_response) {
            return; //Don't send more requests to a peer that's not up-to-date
          }

          req.header.message_id = ++peer.last_message_id;
          req.header.destination_id = peer.id;
          peer.waiting_for_response = true;

          if ( req.previous_entry.index != peer.next_entry.index - 1 ) {
            req.previous_entry.index = peer.next_entry.index - 1;
            req.previous_entry.term = storage->get_entry_info(req.previous_entry.index).term;
            req.entries = storage->entries_since(req.previous_entry.index);
          }
          if ( peer.next_entry.index == new_index ) {
            peer.next_entry.index++;
          }
          callbacks.send(peer.id, req);
        });
    callbacks.set_heartbeat_timeout();
}

template <typename data_t>
void Server<data_t>::on(RPC::MinimumTimeoutRequest) {
  state.minimum_timeout_reached = true;
}

template <typename data_t>
void Server<data_t>::timeout() {
  if (state.role == raft::State::Role::Leader) {
    on(raft::RPC::HeartbeatRequest{});
  } else {
    on(raft::RPC::TimeoutRequest{});
  }
}
  
template <typename data_t>
void Server<data_t>::on(const std::string &, RPC::ConfigChangeRequest /*request*/) {
}

template <typename data_t>
void Server<data_t>::step_down(uint64_t new_term) {
  // out of sync
  // sync term
  if (storage->current_term() != new_term) {
    storage->current_term(new_term);
  }

  if(!storage->voted_for().empty()) {
    storage->voted_for("");
  }

  if (state.role != State::Role::Follower) {
    // fall - back to follower
    state.role = State::Role::Follower;
    std::for_each(state.known_peers.begin(), state.known_peers.end(),
                  [](auto &peer) { peer.reset(); });
  }
}

}
