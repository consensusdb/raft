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
        next_index(0),
        match_index(0),
        voted_for_me(false) {}

  void reset() { next_index = 0, match_index = 0, voted_for_me = false; }
  ip_port_t ip_port;
  std::string id;
  uint64_t next_index;
  uint64_t match_index;
  bool voted_for_me;
};

// because the number of peers
// usually hovers around 5, 7 or 9, a simple iteration over a
// vector is good enough and simplifies the memory layout
// which in turn offers great cache locality and performance.
typedef std::vector<peer_info_t> PeerInfo;

class UnknownPeerException : public std::exception {
 public:
  explicit UnknownPeerException(const std::string& id) {
    std::ostringstream oss;
    oss << "unknown peer " << id;
    msg_ = oss.str();
  }

  virtual ~UnknownPeerException() throw() {}
  virtual const char* what() const throw() { return msg_.c_str(); }

 protected:
  std::string msg_;
};

struct State {
  enum class Role { Candidate, Follower, Leader };
  // node state
  State(std::string id, PeerInfo known_peers)
      : role(Role::Follower),
        id(id),
        known_peers(known_peers),
        minimum_timeout_reached(true) {}
  peer_info_t& find_peer(const std::string& id) {
    auto it = std::find_if(known_peers.begin(), known_peers.end(),
                           [&id](auto& peer) { return peer.id == id; });
    if (it != known_peers.end()) {
      return *it;
    }
    throw UnknownPeerException(id);
  }

  Role role;
  std::string id;
  std::string leader_id;
  PeerInfo known_peers;

  std::vector<std::string> cluster_new;
  std::vector<std::string> cluster_old;

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
