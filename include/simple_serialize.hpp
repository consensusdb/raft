#pragma once
#include <iostream>
#include <string>
#include <raft.hpp>
std::ostream &operator<<(std::ostream &os, const raft::Entry &entry);
std::istream &operator>>(std::istream &is, raft::Entry &entry);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesRequest &request);
std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesRequest &request);

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
std::istream &operator>>(std::istream &is, raft::RPC::ClientResponse &response);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ConfigChangeRequest &request);
std::istream &operator>>(std::istream &is, raft::RPC::ConfigChangeRequest &request);

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::PeerConfig& config);
std::istream &operator>>(std::istream &is, raft::RPC::PeerConfig &config);
