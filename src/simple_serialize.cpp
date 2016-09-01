#include <simple_serialize.hpp>

std::ostream &operator<<(std::ostream &os, const raft::Entry &entry) {
  return os << entry.info.index << " " << entry.info.term << " " << entry.data;
}

std::istream &operator>>(std::istream &is, raft::Entry &entry) {
  is >> entry.info.index >> entry.info.term >> entry.data;
  return is;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesRequest &request) {
  os << "AppendEntriesRequest " << request.peer_id << " "
    << request.destination_id << " " << request.term << " "
    << request.prev_log_index << " " << request.prev_log_term << " "
    << request.leader_commit << " " <<  request.request_watermark << " "
    << request.entries.size() << " ";
  std::for_each(request.entries.begin(), request.entries.end(),
                [&os](auto entry) { os << entry << " "; });
  os << "\n";
  return os;
}

std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesRequest &request) {
  size_t num_entries;
  is >> request.peer_id >> request.destination_id >> request.term >>
      request.prev_log_index >> request.prev_log_term >>
      request.leader_commit >> request.request_watermark >> num_entries;
  request.entries.reserve(num_entries);
  for (size_t i = 0; i < num_entries; ++i) {
    raft::Entry entry;
    is >> entry;
    request.entries.emplace_back(std::move(entry));
  }
  return is;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::AppendEntriesResponse &request) {
  return os << "AppendEntriesResponse " << request.peer_id << " "
            << request.destination_id << " " << request.term << " "
            << request.success << " " << request.match_index << " " 
            << request.request_watermark << " \n";
}

std::istream &operator>>(std::istream &is,
                         raft::RPC::AppendEntriesResponse &request) {
  return is >> request.peer_id >> request.destination_id >> request.term >>
    request.success >> request.match_index >> request.request_watermark;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::VoteRequest &request) {
  return os << "VoteRequest " << request.peer_id << " "
            << request.destination_id << " " << request.term << " "
            << request.candidate_id << " " << request.last_log_index << " "
            << request.last_log_term << " \n";
}

std::istream &operator>>(std::istream &is, raft::RPC::VoteRequest &request) {
  return is >> request.peer_id >> request.destination_id >> request.term >>
         request.candidate_id >> request.last_log_index >>
         request.last_log_term;
}

std::ostream &operator<<(std::ostream &os,
                         const raft::RPC::VoteResponse &response) {
  return os << "VoteResponse " << response.peer_id << " "
            << response.destination_id << " " << response.term << " "
            << response.vote_granted << " \n";
}

std::istream &operator>>(std::istream &is, raft::RPC::VoteResponse &response) {
  return is >> response.peer_id >> response.destination_id >> response.term >>
         response.vote_granted;
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
  return os << "ClientResponse " << response.entry_info.index << " " <<response.entry_info.term << " \n";
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
   os << "ConfigChangeRequest " << request.peer_id
      << " " <<  request.destination_id << " " <<  request.term << " " <<  request.prev_log_index
      << " " <<  request.prev_log_term << " " <<  request.leader_commit << " " <<  request.index
      << " " <<  request.term << " " << request.peer_configs.size() << " ";
  std::for_each(request.peer_configs.begin(), request.peer_configs.end(),
                [&os](auto peer) { os << peer<< " "; });
  os << "\n";
  return os;
}

std::istream &operator>>(std::istream &is, raft::RPC::ConfigChangeRequest &request) {
  size_t num_peers;
   is >>  request.peer_id
      >>  request.destination_id >>  request.term >>  request.prev_log_index
      >>  request.prev_log_term >>  request.leader_commit >>  request.index
      >>  request.term >> num_peers;
  request.peer_configs.reserve(num_peers);
  for (size_t i = 0; i < num_peers; ++i) {
    raft::RPC::PeerConfig peer;
    is >> peer;
    request.peer_configs.emplace_back(std::move(peer));
  }
  return is;
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
