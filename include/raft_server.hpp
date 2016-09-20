#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <raft_rpc.hpp>
#include <raft_entry.hpp>
#include <raft_state.hpp>
#include <raft_storage.hpp>
#include <raft_callbacks.hpp>

// Server is the main object that represents a raft node/server.

namespace raft {

template <typename data_t>
class Server {
 public:
  Server(std::string id, PeerInfo known_peers,
         std::unique_ptr<Storage<data_t> > a_storage, Callbacks<data_t> &callbacks)
      : state(id, std::move(known_peers)),
        storage(std::move(a_storage)),
        callbacks(callbacks) {}
  // the volatile state
  State state;
  // the non-volatile state
  std::unique_ptr<Storage<data_t> > storage;
  Callbacks<data_t> &callbacks;

  // will always return a response. If the machine has already voted for someone
  // in the
  // present term or has more comitted messages it will return a negative
  // response.
  // Otherwise it will return a positive response
  void on(const std::string &peer_id, RPC::VoteRequest request);

  // may return a heartbeat to all the peers claiming leadership
  // if the vote tally doesn't create a concensus/majority then no heartbeat
  // will be sent
  void on(const std::string &peer_id, RPC::VoteResponse response);

  // will always return a response
  void on(const std::string &peer_id, RPC::AppendEntriesRequest<data_t> request);

  // may return a request if more synchonization is needed
  void on(const std::string &peer_id, RPC::AppendEntriesResponse response);

  void on(const std::string &client_id, RPC::ClientRequest request);
  
  void on(const std::string &peer_id, RPC::ConfigChangeRequest request);

  // will return a vote request to all the peers to vote on
  void on(RPC::TimeoutRequest request);

  // will return a heartbeat to all the peers claiming leadership
  void on(RPC::HeartbeatRequest request);

  // Once a client gets a heartbeat or Votes, it will ignore any further vote
  // requests
  // until the minimum time for a vote timeout to occur passes. See the last
  // paragraph in
  // section 6 of the raft paper page 11.
  void on(RPC::MinimumTimeoutRequest request);

  void timeout();

 private:
  // any request where the term is further along will make the current node
  // fallback
  // to being a Follower. This function does the necessary operations to step
  // down.
  void step_down(uint64_t new_term);
};

}

#include <raft_server.ipp>
