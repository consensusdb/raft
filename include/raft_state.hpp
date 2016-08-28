#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <raft_entry.hpp>
#include <sstream>
#include <algorithm>

namespace raft {

struct peer_info_t {
  peer_info_t(std::string id, std::string ip, int port, int client_port)
      : ip_port{std::move(ip), port, client_port},
        id(std::move(id)),
        next_index(1),
        match_index(0),
        voted_for_me(false),
        request_watermark(0) {}

  void reset() { voted_for_me = false; }
  ip_port_t ip_port;
  std::string id;
  uint64_t next_index;
  uint64_t match_index;
  bool voted_for_me;
  uint64_t request_watermark;
};

// because the number of peers
// usually hovers around 5, 7 or 9, a simple iteration over a
// vector is good enough and simplifies the memory layout
// which in turn offers greater cache locality and performance.
typedef std::vector<peer_info_t> PeerInfo;

struct State {
  enum class Role { Candidate, Follower, Leader };
  // node state
  State(std::string id, PeerInfo known_peers)
      : role(Role::Follower),
        id(id),
        leader_id(id),
        known_peers(known_peers),
        minimum_timeout_reached(true) {}

  //returns non-owning pointer
  peer_info_t* find_peer(const std::string& id) {
    auto it = std::find_if(known_peers.begin(), known_peers.end(),
                           [&id](auto& peer) { return peer.id == id; });
    if (it != known_peers.end()) {
      return &(*it);
    }
    return nullptr;
  }

  Role role;
  std::string id;
  std::string leader_id;
  PeerInfo known_peers;

  PeerInfo cluster_new;
  PeerInfo cluster_old;

  bool minimum_timeout_reached;
};
}

inline std::ostream& operator<<(std::ostream& os, raft::State::Role& role) {
  switch (role) {
    case (raft::State::Role::Candidate):
      return os << "candidate";
    case (raft::State::Role::Leader):
      return os << "leader";
    case (raft::State::Role::Follower):
      return os << "follower";
    default:
      return os << "????";
  }
}
