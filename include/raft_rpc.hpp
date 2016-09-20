#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <raft_entry.hpp>

namespace raft {

namespace RPC {

template <typename data_t>
struct AppendEntriesRequest {
  MessageHeader header;
  EntryInfo previous_entry;
  std::vector<Entry<data_t> > entries;
};

struct AppendEntriesResponse {
  MessageHeader header;
  bool success;
};

struct VoteRequest {
  MessageHeader header;
};

struct VoteResponse {
  MessageHeader header;
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
  std::string message_id;
  std::string data;
};

struct ClientResponse {
  std::string message_id;
  EntryInfo entry_info;
};

struct NotLeaderResponse {
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
  MessageHeader header;
  uint64_t previous_entry;
  std::vector<PeerConfig> peer_configs;
  uint64_t request_watermark;
};

}
}
