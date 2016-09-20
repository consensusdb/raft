#include <simple_message_processor.hpp>

#include <asio_tcp_session.hpp>
#include <network_message_processor.hpp>
#include <raft.hpp>
#include <algorithm>
#include <list>
#include <string>
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

  MessageProcessor::MessageProcessor(network::MessageCategory category): network::MessageProcessor{},
    category_(category) {
  }

network::buffer_t MessageProcessor::process_read(std::string &id,
                                                     size_t bytes_recieved,
                                                     raft::Server<std::string> &server) {
  newline_tokenizer(
      data_, data_ + bytes_recieved, incomplete_message_,
      std::bind(&MessageProcessor::process_message, this, std::ref(id),
                std::placeholders::_1, std::ref(server)));
  return {data_, max_length};
}

void MessageProcessor::process_message(std::string &id, std::string message,
                                           raft::Server<std::string> &server) {
  if(category_ == network::MessageCategory::Client) {
    process_client_message(id, std::move(message), server);
  } else {
    process_server_message(id, std::move(message), server);
  }
}

void MessageProcessor::process_server_message(std::string &id, std::string message,
                                           raft::Server<std::string> &server) {
  std::istringstream stream(std::move(message));
  stream.exceptions(std::istringstream::failbit | std::istringstream::badbit);
  try {
    std::string name;
    stream >> name;
    if (name == "AppendEntriesRequest") {
      process_impl<raft::RPC::AppendEntriesRequest<std::string> >(id, server, stream);
    } else if (name == "AppendEntriesResponse") {
      process_impl<raft::RPC::AppendEntriesResponse>(id, server, stream);
    } else if (name == "VoteResponse") {
      process_impl<raft::RPC::VoteResponse>(id, server, stream);
    } else if (name == "VoteRequest") {
      process_impl<raft::RPC::VoteRequest>(id, server, stream);
    } else if ( name == "ConfigChangeRequest" ) {
      raft::RPC::ConfigChangeRequest request;
      stream >> request;
      server.on(request.header.peer_id, std::move(request));
    } else {
      // failed to parse header
      server.callbacks.drop(id);
    }
  } catch (const std::istringstream::failure &) {
    // failed to parse the message after a valid header
    server.callbacks.drop(id);
  }
}

void MessageProcessor::process_client_message(std::string &id,
                                             std::string message,
                                             raft::Server<std::string> &server) {
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
      server.callbacks.client_send(id, raft::RPC::CurrentEntryResponse{server.storage->log_state().commit});
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
