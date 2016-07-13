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
  uint64_t request_watermark;
};

struct AppendEntriesResponse {
  std::string peer_id;
  std::string destination_id;
  uint64_t term;
  bool success;
  uint64_t match_index;
  uint64_t request_watermark;
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
  std::string data;
};

struct ClientResponse {
  EntryInfo entry_info;
};

struct NotLeaderResponse {
  std::string peer_id;
  std::string client_id;
  std::string leader_id;
  ip_port_t leader_info;
};

struct LocalFailureResponse {

};

struct CurrentEntryRequest {
};

struct CurrentEntryResponse {
  EntryInfo entry_info;
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
