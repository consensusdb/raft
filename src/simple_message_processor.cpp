#include <simple_message_processor.hpp>

#include <asio_tcp_session.hpp>
#include <network_message_processor.hpp>
#include <raft.hpp>
#include <algorithm>
#include <list>
#include <string>
#include <simple_serialize.hpp>
#include <sstream>
#include <functional>

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

network::buffer_t PeerMessageProcessor::process_read(std::string &id,
                                                     size_t bytes_recieved,
                                                     raft::Server &server) {
  newline_tokenizer(
      data_, data_ + bytes_recieved, incomplete_message_,
      std::bind(&PeerMessageProcessor::process_message, this, std::ref(id),
                std::placeholders::_1, std::ref(server)));
  return {data_, max_length};
}

std::string PeerMessageProcessor::serialize(
    raft::RPC::AppendEntriesRequest request) const {
  std::ostringstream oss;
  oss << request;
  return oss.str();
}
std::string PeerMessageProcessor::serialize(
    raft::RPC::AppendEntriesResponse request) const {
  std::ostringstream oss;
  oss << request;
  return oss.str();
}
std::string PeerMessageProcessor::serialize(
    raft::RPC::VoteRequest request) const {
  std::ostringstream oss;
  oss << request;
  return oss.str();
}
std::string PeerMessageProcessor::serialize(
    raft::RPC::VoteResponse request) const {
  std::ostringstream oss;
  oss << request;
  return oss.str();
}

void PeerMessageProcessor::process_message(std::string &id, std::string message,
                                           raft::Server &server) {
  std::istringstream s(std::move(message));
  s.exceptions(std::istringstream::failbit | std::istringstream::badbit);
  try {
    std::string name;
    s >> name;
    if (name == "AppendEntriesRequest") {
      raft::RPC::AppendEntriesRequest request;
      s >> request;
      if (request.peer_id != id && !server.callbacks.identify(id, request.peer_id)) {
        server.callbacks.drop(request.peer_id);
        return;
      }
      server.on(request.peer_id, std::move(request));
    } else if (name == "AppendEntriesResponse") {
      raft::RPC::AppendEntriesResponse response;
      s >> response;
      if ( response.peer_id != id && !server.callbacks.identify(id, response.peer_id) ) {
        server.callbacks.drop(response.peer_id);
        return;
      }
      server.on(response.peer_id, std::move(response));
    } else if (name == "VoteResponse") {
      raft::RPC::VoteResponse response;
      s >> response;
      if ( response.peer_id != id && !server.callbacks.identify(id, response.peer_id) ) {
        server.callbacks.drop(response.peer_id);
        return;
      }
      server.on(response.peer_id, std::move(response));
    } else if (name == "VoteRequest") {
      raft::RPC::VoteRequest request;
      s >> request;
      if ( request.peer_id != id && !server.callbacks.identify(id, request.peer_id) ) {
        server.callbacks.drop(request.peer_id);
        return;
      }
      server.on(request.peer_id, std::move(request));
    } else if ( name == "ConfigChangeRequest" ) {
      raft::RPC::ConfigChangeRequest request;
      s >> request;
      server.on(request.peer_id, std::move(request));
    } else {
      // failed to parse header
      server.callbacks.drop(id);
    }
  } catch (const std::istringstream::failure &) {
    // failed to parse the message after a valid header
    server.callbacks.drop(id);
  }
}

network::buffer_t ClientMessageProcessor::process_read(std::string &id,
                                                       size_t bytes_recieved,
                                                       raft::Server &server) {
  newline_tokenizer(
      data_, data_ + bytes_recieved, incomplete_message_,
      std::bind(&ClientMessageProcessor::process_message, this, std::ref(id),
                std::placeholders::_1, std::ref(server)));
  return {data_, max_length};
}

std::string ClientMessageProcessor::serialize(
    raft::RPC::ClientRequest request) const {
  std::ostringstream oss;
  oss << request;
  return oss.str();
}
std::string ClientMessageProcessor::serialize(
    raft::RPC::ClientResponse response) const {
  std::ostringstream oss;
  oss << response;
  return oss.str();
}

std::string ClientMessageProcessor::serialize(
  raft::RPC::LocalFailureResponse response) const {
  std::ostringstream oss;
  oss << response;
  return oss.str();
}

std::string ClientMessageProcessor::serialize(
  raft::RPC::NotLeaderResponse response) const {
  std::ostringstream oss;
  oss << response;
  return oss.str();
}

std::string ClientMessageProcessor::serialize(raft::RPC::CurrentEntryResponse response) const {
  std::ostringstream oss;
  oss << response;
  return oss.str();
}

void ClientMessageProcessor::process_message(std::string &id,
                                             std::string message,
                                             raft::Server &server) {
  std::istringstream s(std::move(message));
  s.exceptions(std::istringstream::failbit | std::istringstream::badbit);
  try {
    std::string name;
    s >> name;
    if (name == "ClientRequest") {
      raft::RPC::ClientRequest request;
      s >> request;
      request.client_id = id;
      server.on(id, std::move(request));
    } else if ( name == "CurrentEntryRequest" ) {
      server.callbacks.client_send(id, raft::RPC::CurrentEntryResponse{server.storage->get_last_entry_info()});
    } else {
      // failed to parse header
      server.callbacks.drop(id);
    }
  } catch (const std::istringstream::failure &) {
    // failed to parse the message after a valid header
    server.callbacks.drop(id);
  }
}
}
