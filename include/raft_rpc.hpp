#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <raft_entry.hpp>

namespace raft {

namespace RPC {

struct AppendEntriesRequest {
  uint64_t term;
  std::string leader_id;
  uint64_t prev_log_index;
  uint64_t prev_log_term;
  uint64_t leader_commit;
  std::vector<Entry> entries;
};

struct AppendEntriesResponse {
  std::string peer_id;
  uint64_t term;
  bool success;
  uint64_t match_index;
};

struct VoteRequest {
  uint64_t term;
  std::string candidate_id;
  uint64_t last_log_index;
  uint64_t last_log_term;
};

struct VoteResponse {
  std::string peer_id;
  uint64_t term;
  bool vote_granted;
};

// these aren't RPC structs per-se but they are events that get generated on
// timeouts
//
struct TimeoutRequest {};

struct HeartbeatRequest {};

struct MinimumTimeoutRequest {};

struct ClientRequest {
  std::string data;
};

// respond to the client. if the server is not the leader, success will be false
// the leader information will contain the address of the leader to retry the
// request.
struct ClientResponse {
  std::string error_message;
  EntryInfo entry_info;
  std::string leader_id;
  ip_port_t leader_info;
};
}
}