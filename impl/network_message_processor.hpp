#pragma once
#include <raft_rpc.hpp>
#include <raft_server.hpp>
#include <exception>
#include <string>
#include <memory>

namespace network {

class MessageProcessorException : public std::exception {
 public:
  explicit MessageProcessorException(const char* message) : msg_(message) {}
  explicit MessageProcessorException(const std::string& message)
      : msg_(message) {}
  virtual ~MessageProcessorException() throw() {}
  virtual const char* what() const throw() { return msg_.c_str(); }

 protected:
  std::string msg_;
};

class Session;

struct buffer_t {
  char* ptr;
  size_t size;
};

enum MessageCategory {
  Client,
  Server
};

class MessageProcessor {
 public:
  virtual buffer_t process_read(std::string& id, size_t bytes_recieved,
                                raft::Server<std::string>& server) = 0;

  virtual std::string serialize(raft::RPC::AppendEntriesRequest<std::string>) const = 0;
  virtual std::string serialize(raft::RPC::AppendEntriesResponse) const = 0;
  virtual std::string serialize(raft::RPC::VoteRequest) const = 0;
  virtual std::string serialize(raft::RPC::VoteResponse) const = 0;
  virtual std::string serialize(raft::RPC::ClientRequest) const = 0;
  virtual std::string serialize(raft::RPC::ClientResponse) const = 0;
  virtual std::string serialize(raft::RPC::LocalFailureResponse) const = 0;
  virtual std::string serialize(raft::RPC::NotLeaderResponse) const = 0;
  virtual std::string serialize(raft::RPC::CurrentEntryResponse) const = 0;

  virtual ~MessageProcessor() {
  }
};

// since the server takes requests from clients and peers, we need 2 different
// message parsers.
class MessageProcessorFactory {
 public:
  virtual std::unique_ptr<network::MessageProcessor> operator()(MessageCategory category) const = 0;
  inline virtual ~MessageProcessorFactory() {}
};
}
