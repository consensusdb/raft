#pragma once
#include <asio_tcp_session.hpp>
#include <network_message_processor.hpp>
#include <raft.hpp>
#include <string>

namespace avery {

class PeerMessageProcessor : public network::MessageProcessor {
 public:
  network::buffer_t process_read(std::string &id, size_t bytes_recieved,
                                 raft::Server &server);

  std::string serialize(raft::RPC::AppendEntriesRequest request) const;
  std::string serialize(raft::RPC::AppendEntriesResponse request) const;
  std::string serialize(raft::RPC::VoteRequest request) const;
  std::string serialize(raft::RPC::VoteResponse request) const;
 private:
  void process_message(std::string &id, std::string message,
                       raft::Server &server);

  enum { max_length = 64 * 1024 };
  char data_[max_length];
  std::string incomplete_message_;
};

class ClientMessageProcessor : public network::MessageProcessor {
 public:
  network::buffer_t process_read(std::string &id, size_t bytes_recieved,
                                 raft::Server &server);

  std::string serialize(raft::RPC::ClientRequest request) const;
  std::string serialize(raft::RPC::ClientResponse response) const;
  std::string serialize(raft::RPC::LocalFailureResponse response) const;
  std::string serialize(raft::RPC::NotLeaderResponse response) const;
  std::string serialize(raft::RPC::CurrentEntryResponse response) const;
 private:
  void process_message(std::string &id, std::string message,
                       raft::Server &server);

  enum { max_length = 64 * 1024 };
  char data_[max_length];
  std::string incomplete_message_;
};

class MyMessageProcessoryFactory : public network::MessageProcessorFactory {
 public:
  std::unique_ptr<network::MessageProcessor> peer() const {
    return std::unique_ptr<network::MessageProcessor>(
        new PeerMessageProcessor{});
  }

  std::unique_ptr<network::MessageProcessor> client() const {
    return std::unique_ptr<network::MessageProcessor>(
        new ClientMessageProcessor{});
  }
};
}
