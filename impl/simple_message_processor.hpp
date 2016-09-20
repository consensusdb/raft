#pragma once
#include <asio_tcp_session.hpp>
#include <network_message_processor.hpp>
#include <raft.hpp>
#include <string>
#include <simple_serialize.hpp>

namespace avery {

class MessageProcessor : public network::MessageProcessor {
 public:
  MessageProcessor(network::MessageCategory category);
  network::buffer_t process_read(std::string &id, size_t bytes_recieved,
                                 raft::Server<std::string> &server);

  std::string serialize(raft::RPC::AppendEntriesRequest<std::string> request) const { return serialize_impl(std::move(request)); }
  std::string serialize(raft::RPC::AppendEntriesResponse response) const { return serialize_impl(std::move(response)); }
  std::string serialize(raft::RPC::VoteRequest request) const { return serialize_impl(std::move(request)); }
  std::string serialize(raft::RPC::VoteResponse response) const { return serialize_impl(std::move(response)); }

  std::string serialize(raft::RPC::ClientRequest request) const  { return serialize_impl(std::move(request)); }
  std::string serialize(raft::RPC::ClientResponse response) const  { return serialize_impl(std::move(response)); }
  std::string serialize(raft::RPC::LocalFailureResponse response) const  { return serialize_impl(std::move(response)); }
  std::string serialize(raft::RPC::NotLeaderResponse response) const  { return serialize_impl(std::move(response)); }
  std::string serialize(raft::RPC::CurrentEntryResponse response) const  { return serialize_impl(std::move(response)); }

 private:

  template <typename rpc_message_t>
  std::string serialize_impl(rpc_message_t &&message) const {
    std::ostringstream oss;
    oss << message;
    return oss.str();
  }

  template <typename rpc_message_t>
  void process_impl(const std::string &id, raft::Server<std::string> &server, std::istringstream &s) {
    rpc_message_t message;
    s >> message;
    if (message.header.peer_id != id && !server.callbacks.identify(id, message.header.peer_id)) {
      server.callbacks.drop(message.header.peer_id);
      return;
    }
    server.on(message.header.peer_id, std::move(message));
  }

  void process_message(std::string &id, std::string message,
                       raft::Server<std::string> &server);
  void process_server_message(std::string &id, std::string message,
                       raft::Server<std::string> &server);
  void process_client_message(std::string &id, std::string message,
                       raft::Server<std::string> &server);

  enum { max_length = 64 * 1024 };
  char data_[max_length];
  std::string incomplete_message_;
  network::MessageCategory category_;
};

class MyMessageProcessoryFactory : public network::MessageProcessorFactory {
 public:
  std::unique_ptr<network::MessageProcessor> operator()(network::MessageCategory category) const {
    return std::unique_ptr<network::MessageProcessor>(
        new MessageProcessor{category});
  }
};

}
