#pragma once

#include <string>
#include <algorithm>
#include <raft_entry.hpp>

namespace raft {

struct peer_info_t {
  peer_info_t(std::string id, std::string ip, int port, int client_port)
      : ip_port{std::move(ip), port, client_port},
        id(std::move(id)),
        next_entry({1,1}),
        match_entry({0,0}),
        last_message_id(0),
        voted_for_me(false),
        waiting_for_response(false) {}

  void reset() { 
    voted_for_me = false; 
    ++last_message_id;
    waiting_for_response = false;
  }
  ip_port_t ip_port;
  std::string id;
  EntryInfo next_entry;
  EntryInfo match_entry;
  uint64_t last_message_id;
  bool voted_for_me;
  bool waiting_for_response;
};

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

//helper
inline const char * to_string(const raft::State::Role& role) {
  switch (role) {
    case (raft::State::Role::Candidate):
      return "candidate";
    case (raft::State::Role::Leader):
      return "leader";
    case (raft::State::Role::Follower):
      return "follower";
    default:
      return "????";
  }
}
