#pragma once
#include <asio_tcp_session.hpp>
#include <network_message_processor.hpp>
#include <raft.hpp>
#include <algorithm>
#include <list>
#include <string>
#include <simple_serialize.hpp>
#include <sstream>

namespace {
// generic function that tokenizes on new-line. returns the remaining data
template <typename Callback>
void newline_tokenizer(char *begin, char *buffer_end,
                       std::string &incomplete_message, Callback callback) {
  char *end;
  while ((end = std::find(begin, buffer_end, '\n')) != buffer_end) {
    incomplete_message.append(begin, end);
    begin = end + 1;
    // strip off '\r'
    if (!incomplete_message.empty() && incomplete_message.back() == '\r') {
      incomplete_message.resize(incomplete_message.size() - 1);
    }
    std::string message;
    incomplete_message.swap(message);
    callback(std::move(message));
  }
  // add any excess bytes to the incomplete message as those excess bytes
  // represent the start
  // of the next message
  incomplete_message.append(begin, std::distance(begin, buffer_end));
}
}

namespace avery {

class PeerMessageProcessor : public network::MessageProcessor {
 public:
  network::buffer_t process_read(std::string &id, size_t bytes_recieved,
                                 raft::Server &server) {
    newline_tokenizer(
        data_, data_ + bytes_recieved, incomplete_message_,
        std::bind(&PeerMessageProcessor::process_message, this, std::ref(id),
                  std::placeholders::_1, std::ref(server)));
    return {data_, max_length};
  }

  std::string serialize(raft::RPC::AppendEntriesRequest request) const {
    std::ostringstream oss;
    oss << request;
    return oss.str();
  }
  std::string serialize(raft::RPC::AppendEntriesResponse request) const {
    std::ostringstream oss;
    oss << request;
    return oss.str();
  }
  std::string serialize(raft::RPC::VoteRequest request) const {
    std::ostringstream oss;
    oss << request;
    return oss.str();
  }
  std::string serialize(raft::RPC::VoteResponse request) const {
    std::ostringstream oss;
    oss << request;
    return oss.str();
  }

 private:
  void process_message(std::string &id, std::string message,
                       raft::Server &server) {
    std::istringstream s(std::move(message));
    s.exceptions(std::istringstream::failbit | std::istringstream::badbit);
    try {
      std::string name;
      s >> name;
      if (name == "ID") {
        s >> name;
        server.callbacks.identify(id, name);
      } else if (name == "AppendEntriesRequest") {
        raft::RPC::AppendEntriesRequest request;
        s >> request;
        server.on(id, std::move(request));
      } else if (name == "AppendEntriesResponse") {
        raft::RPC::AppendEntriesResponse response;
        s >> response;
        server.on(id, std::move(response));
      } else if (name == "VoteResponse") {
        raft::RPC::VoteResponse response;
        s >> response;
        server.on(id, std::move(response));
      } else if (name == "VoteRequest") {
        raft::RPC::VoteRequest request;
        s >> request;
        server.on(id, std::move(request));
      } else {
        // TODO failed to parse error
      }
    } catch (const std::istringstream::failure &) {
      // TODO failed to parse error
    }
  }

  enum { max_length = 64 * 1024 };
  char data_[max_length];
  std::string incomplete_message_;
};

class ClientMessageProcessor : public network::MessageProcessor {
 public:
  network::buffer_t process_read(std::string &id, size_t bytes_recieved,
                                 raft::Server &server) {
    newline_tokenizer(
        data_, data_ + bytes_recieved, incomplete_message_,
        std::bind(&ClientMessageProcessor::process_message, this, std::ref(id),
                  std::placeholders::_1, std::ref(server)));
    return {data_, max_length};
  }

  std::string serialize(raft::RPC::ClientRequest request) const {
    std::ostringstream oss;
    oss << request;
    return oss.str();
  }
  std::string serialize(raft::RPC::ClientResponse request) const {
    std::ostringstream oss;
    oss << request;
    return oss.str();
  }

 private:
  void process_message(std::string &id, std::string message,
                       raft::Server &server) {
    // since we use the space as a delimter, messages can't contain spaces or be
    // empty
    server.on(id, raft::RPC::ClientRequest{std::move(message)});
  }

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
