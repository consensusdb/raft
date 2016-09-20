#include <asio_tcp_server.hpp>

#include <asio.hpp>
#include <raft.hpp>
#include <algorithm>
#include <random>

#include <asio_tcp_session.hpp>
#include <iostream>

namespace network {
namespace asio {
using ::asio::ip::tcp;
Server::Server(::asio::io_service &io_service, short port, short client_port,
               std::string id, raft::PeerInfo known_peers,
               std::unique_ptr<raft::Storage<std::string> > storage,
               const network::MessageProcessorFactory &factory,
               int heartbeat_ms)
    : mt_(std::random_device{}()),
      dist_(static_cast<int>(0.7 * heartbeat_ms),
            static_cast<int>(1.3 * heartbeat_ms)),
      io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
      client_acceptor_(io_service, tcp::endpoint(tcp::v4(), client_port)),
      timer_(io_service, std::chrono::milliseconds(dist_(mt_))),
      minimum_timer_(io_service, std::chrono::milliseconds(dist_.min())),
      message_factory_(factory),
      raft_server_(id, std::move(known_peers), std::move(storage), *this),
      next_id_(0),
      stop_(true),
      watermark_index_(0){}

void Server::start() {
  if (!stop_) {
    return;
  }
  stop_ = false;
  set_vote_timeout();
  raft_server().on(raft::RPC::MinimumTimeoutRequest{});
  do_accept_peer();
  do_accept_client();
}

void Server::stop() {
  if (stop_) {
    return;
  }
  stop_ = true;
  for_each(peers_.begin(), peers_.end(), [](auto& peer) { 
    peer->stop_and_close();
  });
  peers_.clear();

  for_each(all_clients_.begin(), all_clients_.end(), [](auto& peer) {
    peer.second->stop_and_close();
  });
  all_clients_.clear();
}

Server::~Server() { stop(); }

void Server::set_heartbeat_timeout() {
  if (stop_) {
    return;
  }
  timer_.expires_from_now(std::chrono::milliseconds(dist_(mt_)));
  timer_.async_wait([this](const std::error_code &e) {
    if (!e && !stop_) {
      raft_server().timeout();
    }
  });
}

void Server::set_vote_timeout() {
  if (stop_) {
    return;
  }
  timer_.expires_from_now(std::chrono::milliseconds(10 * dist_(mt_)));
  timer_.async_wait([this](const std::error_code &e) {
    if (!e && !stop_) {
      raft_server().timeout();
    }
  });
}

void Server::set_minimum_timeout() {
  if (stop_) {
    return;
  }
  minimum_timer_.expires_from_now(std::chrono::milliseconds(10*dist_.min()));
  minimum_timer_.async_wait([this](const std::error_code &e) {
    if (!e && !stop_) {
      raft_server().on(raft::RPC::MinimumTimeoutRequest{});
    }
  });
}

bool Server::identify(const std::string &temp_id, const std::string &peer_id) {
  if (stop_) {
    return false;
  }
  int identify_count = 0;
  std::for_each(peers_.begin(), peers_.end(), [&temp_id, &peer_id,&identify_count](auto& peer) {
    if (peer->id() == temp_id) {
      peer->id() = peer_id;
      ++identify_count;
    }
  });
  if ( raft_server().state.find_peer(peer_id) == nullptr ) {
    return false;
  }
  return identify_count == 1;
}

void Server::drop(const std::string &temp_id) {
  if (stop_) {
    return;
  }
  if ( temp_id[0] == '-' ) {
    //client
    auto it = all_clients_.find(temp_id);
    if ( it != all_clients_.end() ) {
      it->second->stop_and_close();
      all_clients_.erase(it);
      return;
    }
  }

  peers_.erase(
      std::remove_if(peers_.begin(), peers_.end(), [&temp_id](auto& peer) {
        if (peer->id() == temp_id) {
          peer->stop_and_close();
          return true;
        }
        return false;
      }), peers_.end());
}
raft::Server<std::string> &Server::raft_server() { return raft_server_; }

void Server::do_accept_peer() {
  if ( stop_ || accepting_peer_session_ != nullptr ) {
    return;
  }
  auto self = shared_from_this();

  std::ostringstream oss;
  oss << "+" << ++next_id_;

  accepting_peer_session_ = std::make_shared<Session>(self, tcp::socket(io_service_),
                                                      message_factory_(MessageCategory::Server), oss.str());

  acceptor_.async_accept(accepting_peer_session_->socket(),
                         [this, self](std::error_code ec) {
    if ( stop_ ) {
      accepting_peer_session_ = nullptr;
      return;
    }
    if ( !ec ) {
      peers_.emplace_back(std::move(accepting_peer_session_));
      peers_.back()->start();
    }
    do_accept_peer();
  });
}

void Server::do_accept_client() {
  if ( stop_ || accepting_client_session_ != nullptr ) {
    return;
  }
  auto self = shared_from_this();
  std::ostringstream oss;
  oss << "-" << ++next_id_;
  accepting_client_session_ = std::make_shared<Session>(
    self, tcp::socket(io_service_), message_factory_(MessageCategory::Client), oss.str());
  client_acceptor_.async_accept(
    accepting_client_session_->socket(), [this, self](std::error_code ec) {
    if ( stop_ ) {
      accepting_client_session_ = nullptr;
      return;
    }
    if ( !ec ) {
      auto it = all_clients_.emplace(accepting_client_session_->id(), std::move(accepting_client_session_));
      it.first->second->start();

    }
    do_accept_client();
  });
}

void Server::commit_advanced(uint64_t commit_index) {
  if (stop_ || all_clients_.empty() ) {
    return;
  }
  while ( watermark_index_ < commit_index ) {
    auto message = all_clients_.begin()->second->message_processor().serialize(raft::RPC::CurrentEntryResponse{raft_server_.storage->get_entry_info(watermark_index_)});
    std::for_each(all_clients_.begin(), all_clients_.end(), [&message](auto &client) {
      client.second->send(message);
    });
    ++watermark_index_;
  }
}

template <class M>
void Server::session_send(const std::string &id, M message) {
  if (stop_) {
    return;
  }
  auto it = std::find_if(peers_.begin(), peers_.end(),
                         [&id](auto &peer) { return peer->id() == id; });
  if (it == peers_.end()) {
    auto &peer = *raft_server().state.find_peer(id);
    auto self = shared_from_this();
    auto session = std::make_shared<Session>(self, tcp::socket(io_service_),
                                             message_factory_(MessageCategory::Server), id);
    auto shared_message = std::make_shared<M>(std::move(message));
    tcp::endpoint endpoint(
        ::asio::ip::address::from_string(peer.ip_port.ip.c_str()),
        peer.ip_port.port);
    session->socket().async_connect(
        endpoint, [this, self, session, shared_message](std::error_code ec) {
          if (!ec && !stop_) {
            session->socket().set_option(tcp::no_delay(true));
            peers_.emplace_back(std::move(session));
            session->start();
            session->send(session->message_processor().serialize(
                std::move(*shared_message)));
          }
        });
  } else {
    (*it)->send((*it)->message_processor().serialize(std::move(message)));
  }
}

template <class M>
void Server::client_session_send(const std::string &id, M message) {
  if ( stop_ ) {
    return;
  }
  auto it = all_clients_.find(id);
  if ( it == all_clients_.end() ) return;
  auto &session = *it->second;
  session.send(session.message_processor().serialize(std::move(message)));
}

void Server::send(const std::string &id,
                  const raft::RPC::AppendEntriesRequest<std::string> &request) {
  session_send(id, request);
}

void Server::send(const std::string &id,
                  const raft::RPC::AppendEntriesResponse &response) {
  session_send(id, response);
}

void Server::send(const std::string &id,
                  const raft::RPC::VoteRequest &request) {
  session_send(id, request);
}

void Server::send(const std::string &id,
                  const raft::RPC::VoteResponse &response) {
  session_send(id, response);
}

void Server::client_send(const std::string &id,
                  const raft::RPC::ClientResponse &response) {
  client_session_send(id, response);
}

void Server::client_send(const std::string &id,
                         const raft::RPC::NotLeaderResponse &response) {
  client_session_send(id, response);
}
void Server::client_send(const std::string &id,
                         const raft::RPC::LocalFailureResponse &response) {
  client_session_send(id, response);
}
void Server::client_send(const std::string &id,
                         const raft::RPC::CurrentEntryResponse &response) {
  client_session_send(id, response);
}
  
void Server::new_leader_elected(const std::string &id) {
  std::cout << raft_server().state.id <<  " has new leader " << id << " elected" << std::endl;
}

}
}
