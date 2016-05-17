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
          ret.prev_log_index = state.find_peer(peer.id).match_index;
          ret.prev_log_term = storage->get_entry_info(ret.prev_log_index).term;
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
    state.minimum_timeout_reached = false;
    callbacks.set_minimum_timeout();
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
                                         storage->current_term(), false, 0};
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

  auto info = storage->get_last_entry_info();

  if ((request.prev_log_index == 0 || request.prev_log_index <= info.index) &&
      info.term == request.prev_log_term) {
    response.success = true;
    response.match_index = storage->append(request.entries);
  }
  if (request.leader_commit > storage->last_commit().index) {
    storage->commit_until(request.leader_commit);
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
      state.find_peer(response.peer_id).voted_for_me = true;
      size_t num_votes =
          std::count_if(state.known_peers.begin(), state.known_peers.end(),
                        [](auto &peer) { return peer.voted_for_me; });
      if (num_votes >= (1 + state.known_peers.size() / 2)) {
        std::cout << state.id << " leader" << std::endl;
        state.role = State::Role::Leader;
        state.leader_id = state.id;

        auto info = storage->get_last_entry_info();

        std::for_each(state.known_peers.begin(), state.known_peers.end(),
                      [&info](auto &peer) {
                        peer.next_index = info.index + 1;
                        peer.match_index = 0;
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
  if (response.destination_id != state.id) {
    callbacks.drop(response.peer_id);
    return;
  }
  if (response.term > storage->current_term()) {
    step_down(response.term);
  } else if (response.term < storage->current_term()) {
    return;
  }

  if (state.role == State::Role::Leader) {
    auto &peer = state.find_peer(response.peer_id);
    if (response.success == false) {
      peer.next_index = peer.next_index - 1;
      if (peer.next_index == 0) {
        peer.next_index = 1;
      }
      RPC::AppendEntriesRequest request;
      request.peer_id = state.leader_id;
      request.destination_id = response.peer_id;
      request.leader_commit = storage->last_commit().index;
      request.term = storage->current_term();
      request.prev_log_index = peer.next_index - 1;
      request.prev_log_term =
          storage->get_entry_info(request.prev_log_index).term;
      request.entries = storage->entries_since(request.prev_log_index);
      callbacks.send(response.peer_id, std::move(request));
      return;
    } else if (peer.match_index != response.match_index) {
      // the match_index in the response doesn't correspond to what the leader
      // has observed so there's potential to commit
      peer.next_index = response.match_index + 1;
      peer.match_index = response.match_index;
      std::vector<uint64_t> indexes;
      indexes.reserve(state.known_peers.size());
      std::for_each(
          std::begin(state.known_peers), std::end(state.known_peers),
          [&indexes](auto &it) mutable { indexes.push_back(it.match_index); });
      // sort in reverse order
      std::sort(std::begin(indexes), std::end(indexes),
                std::greater<uint64_t>());
      // commit the latest commit index
      uint64_t new_commit = indexes[state.known_peers.size() / 2];
      if (new_commit > storage->last_commit().index) {
        storage->commit_until(new_commit);
        callbacks.commit_advanced(new_commit);
      }
    }
  }
}

void Server::on(const std::string &, RPC::ClientRequest request) {
  RPC::ClientResponse ret = {state.id,
                             request.client_id,
                             state.leader_id,
                             "OK",
                             {0, 0},
                             state.find_peer(state.leader_id).ip_port};
  if (state.role != State::Role::Leader || request.leader_id != state.id) {
    ret.error_message = "NotLeader";
  } else {
    uint64_t new_index = storage->get_last_entry_info().index + 1;
    // commit locally
    ret.entry_info = {new_index, storage->current_term()};
    // storage->append is blocking -- make sure it doesn't take too long!
    // The length of time spent here is the amount of time to write to disk.
    // If append does more IO than a simple sequential write to disk you
    // are not using this tool correctly.
    if (storage->append({{ret.entry_info, request.data}}) != new_index) {
      // local failure
      ret.error_message = "StorageFailure";
    }
  }
  if (ret.error_message != "OK") {
    callbacks.send(request.client_id, std::move(ret));
  } else {
    callbacks.client_waiting(request.client_id, ret.entry_info);

    raft::RPC::AppendEntriesRequest req;
    req.peer_id = state.id;
    req.leader_commit = storage->last_commit().index;
    req.term = storage->current_term();

    std::for_each(
        state.known_peers.begin(), state.known_peers.end(), [&](auto &peer) {
          if (peer.id == state.id) return;
          req.destination_id = peer.id;
          req.prev_log_index = peer.next_index - 1;
          req.prev_log_term = storage->get_entry_info(req.prev_log_index).term;
          req.entries = storage->entries_since(req.prev_log_index);
          callbacks.send(peer.id, req);
        });
    callbacks.set_heartbeat_timeout();
  }
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
  
void Server::on(const std::string &, RPC::ConfigChangeRequest) {
  //TODO
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
