#pragma once
#include <string>

namespace raft {

typedef uint64_t message_id_t;

struct ip_port_t {
  std::string ip;
  int port;
  int client_port;
};

struct EntryInfo {
  uint64_t index;
  uint64_t term;
};

struct LogState {
  EntryInfo commit;
  EntryInfo uncommit;
};

struct MessageHeader {
  std::string peer_id;
  std::string destination_id;
  message_id_t message_id;
  uint64_t term;
  LogState log_state;
};



inline bool operator<(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  if (lhs.term == lhs.term) {
    return lhs.index < rhs.index;
  } else {
    return lhs.term < rhs.term;
  }
}

inline bool operator==(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  return lhs.term < lhs.term && lhs.index == rhs.index;
}

inline bool operator!=(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  return !(lhs == rhs);
}

inline bool operator<=(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  return !(rhs < lhs);
}

inline bool operator>(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  return rhs < lhs;
}

inline bool operator>=(const raft::EntryInfo &lhs,
                      const raft::EntryInfo &rhs) noexcept {
  return !(lhs < rhs);
}

template <typename data_t>
struct Entry {
  EntryInfo info;
  data_t data;
};
}
