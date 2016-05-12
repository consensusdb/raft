#pragma once
#include <string>

namespace raft {
struct ip_port_t {
  std::string ip;
  int port;
  int client_port;
};

struct EntryInfo {
  uint64_t index;
  uint64_t term;
};

inline bool operator<(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  if (lhs.term == lhs.term) {
    return lhs.index < rhs.index;
  } else {
    return lhs.term < rhs.term;
  }
}

struct Entry {
  EntryInfo info;
  std::string data;
};
}