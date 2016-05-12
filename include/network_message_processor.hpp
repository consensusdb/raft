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
class MessageProcessor {
 public:
  virtual buffer_t process_read(std::string& id, size_t bytes_recieved,
                                raft::Server& server) = 0;

  virtual std::string serialize(raft::RPC::AppendEntriesRequest) const {
    throw MessageProcessorException("AppendEntriesRequest not implemented");
  }

  virtual std::string serialize(raft::RPC::AppendEntriesResponse) const {
    throw MessageProcessorException("AppendEntriesResponse not implemented");
  }

  virtual std::string serialize(raft::RPC::VoteRequest) const {
    throw MessageProcessorException("VoteRequest not implemented");
  }

  virtual std::string serialize(raft::RPC::VoteResponse) const {
    throw MessageProcessorException("VoteResponse not implemented");
  }

  virtual std::string serialize(raft::RPC::ClientRequest) const {
    throw MessageProcessorException("ClientRequest not implemented");
  }

  virtual std::string serialize(raft::RPC::ClientResponse) const {
    throw MessageProcessorException("ClientResponse not implemented");
  }

  inline virtual ~MessageProcessor() {}
};

// since the server takes requests from clients and peers, we need 2 different
// message parsers.
class MessageProcessorFactory {
 public:
  virtual std::unique_ptr<network::MessageProcessor> peer() const = 0;
  virtual std::unique_ptr<network::MessageProcessor> client() const = 0;
  inline virtual ~MessageProcessorFactory() {}
};
}
