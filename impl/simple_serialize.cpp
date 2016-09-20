#include <simple_serialize.hpp>

std::ostream &operator<<(std::ostream &os, const raft::EntryInfo &info) {
  return os << info.index << " " << info.term;
}

std::istream &operator>>(std::istream &is, raft::EntryInfo &info) {
  return is >> info.index >> info.term;
}

std::ostream &operator<<(std::ostream &os, const raft::MessageHeader &header) {
  return os << header.peer_id << " " << header.destination_id << " " << header.message_id << " " << header.term << " " << header.log_state.commit << " " << header.log_state.uncommit;
}

std::istream &operator>>(std::istream &is, raft::MessageHeader &header) {
  return is >> header.peer_id >> header.destination_id >> header.message_id >> header.term >> header.log_state.commit >> header.log_state.uncommit;
}

std::ostream &operator<<(std::ostream &os, const raft::Entry<std::string> &entry) {
  return os << entry.info << " " << entry.data;
}

std::istream &operator>>(std::istream &is, raft::Entry<std::string> &entry) {
  return is >> entry.info >> entry.data;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesRequest<std::string> &request) {
  return os << "AppendEntriesRequest " << request.header << " "
    << request.previous_entry << " " <<  request.entries << "\n";
}

std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesRequest<std::string> &request) {
  return is >> request.header  >> request.previous_entry >> request.entries;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesResponse &response) {
  return os << "AppendEntriesResponse " << response.success << " \n";
}

std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesResponse &response) {
  return is >> response.header >> response.success;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::VoteRequest &request) {
  return os << "VoteRequest " << request.header << " \n";
}

std::istream &operator>>(std::istream &is, raft::RPC::VoteRequest &request) {
  return is >> request.header;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::VoteResponse &response) {
  return os << "VoteResponse " << response.header << " " << response.vote_granted << " \n";
}

std::istream &operator>>(std::istream &is, raft::RPC::VoteResponse &response) {
  return is >> response.header >> response.vote_granted;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ClientRequest &request) {
  return os << "ClientRequest " << request.data << " \n";
}

std::istream &operator>>(std::istream &is, raft::RPC::ClientRequest &request) {
  return is >> request.data;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ClientResponse &response) {
  return os << "ClientResponse " << response.entry_info << " \n";
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::LocalFailureResponse &/*response*/) {
  return os << "LocalFailureResponse \n";
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::NotLeaderResponse &response) {
  return os << "NotLeaderResponse " << response.leader_id << " " <<  response.leader_info.ip << " " << response.leader_info.client_port << " \n";
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::ConfigChangeRequest &request) {
   return os << "ConfigChangeRequest " << request.header << " " << request.previous_entry << " " << request.peer_configs <<"\n";
}

std::istream &operator>>(std::istream &is, raft::RPC::ConfigChangeRequest &request) {
  return is >> request.header >> request.previous_entry >> request.peer_configs;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::PeerConfig& config) {
  return os << config.name << " " <<config.address << " " <<config.peer_port
            << " " <<config.client_port << " " <<config.is_new;
}

std::istream &operator>>(std::istream &is, raft::RPC::PeerConfig &config) {
  return is >> config.name >> config.address >> config.peer_port
            >> config.client_port >> config.is_new;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::CurrentEntryResponse &response) {
  return os << "CurrentEntryResponse " << response.entry_info.index << " " << response.entry_info.term << " \n";
}
