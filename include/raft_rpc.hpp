#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <raft_entry.hpp>

namespace raft {

namespace RPC {

struct AppendEntriesRequest {
  std::string peer_id;  // also leader_id
  std::string destination_id;
  uint64_t term;
  uint64_t prev_log_index;
  uint64_t prev_log_term;
  uint64_t leader_commit;
  std::vector<Entry> entries;
};

struct AppendEntriesResponse {
  std::string peer_id;
  std::string destination_id;
  uint64_t term;
  bool success;
  uint64_t match_index;
};

struct VoteRequest {
  std::string peer_id;
  std::string destination_id;
  uint64_t term;
  std::string candidate_id;
  uint64_t last_log_index;
  uint64_t last_log_term;
};

struct VoteResponse {
  std::string peer_id;
  std::string destination_id;
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
  std::string client_id;
  std::string leader_id;
  std::string data;
};

// respond to the client. if the server is not the leader, success will be false
// the leader information will contain the address of the leader to retry the
// request.
struct ClientResponse {
  std::string peer_id;
  std::string client_id;
  std::string leader_id;
  std::string error_message;
  EntryInfo entry_info;
  ip_port_t leader_info;
};

struct PeerConfig {
  std::string name;
  std::string address;
  int peer_port;
  int client_port;
  bool is_new;
};

struct ConfigChangeRequest {
  std::string peer_id;  // also leader_id
  std::string destination_id;
  uint64_t term;
  uint64_t prev_log_index;
  uint64_t prev_log_term;
  uint64_t leader_commit;
  uint64_t index;
  std::vector<PeerConfig> peer_configs;
};

}
}
