#pragma once
#include <iostream>
#include <string>
#include <raft.hpp>
std::ostream &operator<<(std::ostream &os, const raft::EntryInfo &);
std::istream &operator>>(std::istream &is, raft::EntryInfo &);

std::ostream &operator<<(std::ostream &os, const raft::MessageHeader &);
std::istream &operator>>(std::istream &is, raft::MessageHeader &);

std::ostream &operator<<(std::ostream &os, const raft::Entry<std::string> &);
std::istream &operator>>(std::istream &is, raft::Entry<std::string> &);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesRequest<std::string> &request);
std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesRequest<std::string> &request);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesResponse &response);
std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesResponse &response);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::VoteRequest &request);
std::istream &operator>>(std::istream &is, raft::RPC::VoteRequest &request);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::VoteResponse &response);
std::istream &operator>>(std::istream &is, raft::RPC::VoteResponse &response);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ClientRequest &request);
std::istream &operator>>(std::istream &is, raft::RPC::ClientRequest &request);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ClientResponse &response);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ConfigChangeRequest &request);
std::istream &operator>>(std::istream &is, raft::RPC::ConfigChangeRequest &request);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::PeerConfig& config);
std::istream &operator>>(std::istream &is, raft::RPC::PeerConfig &config);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::LocalFailureResponse &response);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::NotLeaderResponse &response);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::CurrentEntryResponse &response);


template< typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
  os << vec.size() << " ";
  std::for_each(vec.begin(), vec.end(), [&os](auto &elem) { os << elem << " "; });
  return os;
}

template< typename T>
std::istream &operator>>(std::istream &is, std::vector<T> &vec) {
  size_t num_elems;
  is >> num_elems;
  vec.reserve(num_elems);
  for (size_t i = 0; i < num_elems; ++i) {
    T elem;
    is >> elem;
    vec.emplace_back(std::move(elem));
  }
  return is;
}
