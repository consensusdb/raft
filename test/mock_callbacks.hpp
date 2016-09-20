#pragma once

#include <raft_callbacks.hpp>
#include <map>
#include <string>
#include <vector>

namespace raft {
namespace test {
class Callbacks : public raft::Callbacks{
 public:

  inline Callbacks(): raft::Callbacks() {
    reset();
  }

  void reset() {
    set_heartbeat_timeout_ = set_vote_timeout_ = set_minimum_timeout_ = commit_index_calls_ = append_entries_request_calls_ = append_entries_response_calls_ = vote_request_calls_ = vote_response_calls_ = client_response_calls_ = not_leader_response_calls_ = current_entry_response_calls_ = id_calls_ = drop_calls_ = 0;
    append_entries_request_.clear();
    append_entries_response_.clear();
    vote_request_.clear();
    vote_response_.clear();
    client_response_.clear();
    not_leader_response_.clear();
    current_entry_response_.clear();
    id_.clear();
    drop_.clear();
  }

  int set_heartbeat_timeout_;
  int set_vote_timeout_;
  int set_minimum_timeout_;

  uint64_t commit_index_;
  int commit_index_calls_;

  std::map<std::string, raft::RPC::AppendEntriesRequest> append_entries_request_;
  int append_entries_request_calls_;

  std::map<std::string, raft::RPC::AppendEntriesResponse> append_entries_response_;
  int append_entries_response_calls_;

  std::map<std::string,raft::RPC::VoteRequest> vote_request_;
  int vote_request_calls_;

  std::map<std::string,raft::RPC::VoteResponse> vote_response_;
  int vote_response_calls_;

  std::map<std::string, raft::RPC::ClientResponse> client_response_;
  int client_response_calls_;

  std::map<std::string, raft::RPC::NotLeaderResponse> not_leader_response_;
  int not_leader_response_calls_;

  std::map<std::string, raft::RPC::CurrentEntryResponse> current_entry_response_;
  int current_entry_response_calls_;

  std::map<std::string, std::string> id_;
  int id_calls_;

  std::map<std::string, bool> drop_;
  int drop_calls_;

  inline virtual void set_heartbeat_timeout() {
    ++set_heartbeat_timeout_; 
  }

  inline virtual void set_vote_timeout() {
    ++set_vote_timeout_; 
  }

  inline virtual void set_minimum_timeout() {
    ++set_minimum_timeout_; 
  }

  inline virtual void send(const std::string& peer_id, const raft::RPC::AppendEntriesRequest& request) {
    append_entries_request_[peer_id] = request;
    ++append_entries_request_calls_;
  }

  inline virtual void send(const std::string& peer_id, const raft::RPC::AppendEntriesResponse& response) {
    append_entries_response_[peer_id] = response;
    ++append_entries_response_calls_;
  }

  inline virtual void send(const std::string& peer_id, const raft::RPC::VoteRequest& request) {
    vote_request_[peer_id] = request;
    ++vote_request_calls_;
  }

  inline virtual void send(const std::string& peer_id, const raft::RPC::VoteResponse& response) {
    vote_response_[peer_id] = response;
    ++vote_response_calls_;
  }

  inline virtual void client_send(const std::string& peer_id, const raft::RPC::ClientResponse& response) {
    client_response_[peer_id] = response;
    client_response_calls_++;
  
  }
  inline virtual void client_send(const std::string& peer_id, const raft::RPC::NotLeaderResponse& response) {
    not_leader_response_[peer_id] = response;
    not_leader_response_calls_++;
  }

  inline virtual void client_send(const std::string& peer_id, const raft::RPC::CurrentEntryResponse& response) {
    current_entry_response_[peer_id] = response;
    current_entry_response_calls_++;
  }

  inline virtual bool identify(const std::string& temp_id, const std::string& id) {
    id_[id] = temp_id;
    id_calls_++;
    return true;
  }

  inline virtual void drop(const std::string& peer_id)  {
    drop_[peer_id] = true;
    drop_calls_++;
  }

  virtual void commit_advanced(uint64_t commit_index) {
    commit_index_ = commit_index;
    ++commit_index_calls_;
  }

  virtual void new_leader_elected(const std::string &) {
  }

  inline virtual ~Callbacks() {}
};
}
}
