#include <asio_tcp_server.hpp>

#include <asio.hpp>
#include <raft.hpp>
#include <algorithm>
#include <iostream>
#include <random>

#include <asio_tcp_session.hpp>

namespace network {
namespace asio {
using ::asio::ip::tcp;
Server::Server(::asio::io_service &io_service, short port, short client_port,
               std::string id, raft::PeerInfo known_peers,
               std::unique_ptr<raft::Storage> storage,
               const network::MessageProcessorFactory &factory,
               int heartbeat_ms)
    : mt_(std::random_device{}()),
      dist_(static_cast<int>((0.8 * heartbeat_ms + 1) / 2),
            static_cast<int>((1.2 * heartbeat_ms + 2) / 2)),
      io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
      client_acceptor_(io_service, tcp::endpoint(tcp::v4(), client_port)),
      timer_(io_service, std::chrono::milliseconds(dist_(mt_))),
      minimum_timer_(io_service, std::chrono::milliseconds(dist_.min())),
      message_factory_(factory),
      raft_server_(id, std::move(known_peers), std::move(storage), *this),
      next_id_(0),
      stop_(true) {}

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
  for_each(peers_.begin(), peers_.end(), [](auto peer) { peer->stop(); });
  peers_.clear();
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
  minimum_timer_.expires_from_now(std::chrono::milliseconds(10 * dist_.min()));
  minimum_timer_.async_wait([this](const std::error_code &e) {
    if (!e && !stop_) {
      raft_server().on(raft::RPC::MinimumTimeoutRequest{});
    }
  });
}

void Server::identify(const std::string &temp_id, const std::string &peer_id) {
  if (stop_) {
    return;
  }
  std::for_each(peers_.begin(), peers_.end(), [&temp_id, &peer_id](auto peer) {
    if (peer->id() == temp_id) {
      peer->id() = peer_id;
    }
  });
}

void Server::drop(const std::string &temp_id) {
  if (stop_) {
    return;
  }
  peers_.erase(
      std::remove_if(peers_.begin(), peers_.end(), [&temp_id](auto peer) {
        if (peer->id() == temp_id) {
          peer->stop();
          return true;
        }
        return false;
      }), peers_.end());
}

raft::Server &Server::raft_server() { return raft_server_; }

void Server::do_accept_peer() {
  if (stop_) {
    return;
  }
  auto self = shared_from_this();

  std::ostringstream oss;
  oss << "!" << ++next_id_;

  auto session = std::make_shared<Session>(self, tcp::socket(io_service_),
                                           message_factory_.peer(), oss.str());
  acceptor_.async_accept(session->socket(),
                         [this, self, session](std::error_code ec) {
                           if (!ec && !stop_) {
                             peers_.emplace_back(std::move(session));
                             peers_.back()->start();
                           }
                           if (!stop_) {
                             do_accept_peer();
                           }
                         });
}

void Server::do_accept_client() {
  if (stop_) {
    return;
  }
  auto self = shared_from_this();
  std::ostringstream oss;
  oss << "C" << ++next_id_;
  auto session = std::make_shared<Session>(
      self, tcp::socket(io_service_), message_factory_.client(), oss.str());
  client_acceptor_.async_accept(
      session->socket(), [this, self, session](std::error_code ec) {
        if (!ec && !stop_) {
          all_clients_.emplace(session->id(), session);
          session->start();
        }
        if (!stop_) {
          do_accept_client();
        }
      });
}

void Server::client_waiting(const std::string &id,
                            const raft::EntryInfo &info) {
  if (stop_) {
    return;
  }
  waiting_clients_.emplace(std::move(info), id);
}

void Server::commit_advanced(uint64_t commit_index) {
  if (stop_) {
    return;
  }
  auto it = waiting_clients_.begin();
  while (it != waiting_clients_.end()) {
    const raft::EntryInfo &e = it->first;
    auto it2 = all_clients_.find(it->second);
    if (it2 == all_clients_.end()) {
      it = waiting_clients_.erase(it);
    } else {
      std::shared_ptr<Session> &session = it2->second;
      if (commit_index >= e.index) {
        raft::RPC::ClientResponse ret;
        ret.entry_info = e;
        ret.leader_id = raft_server().state.leader_id;
        ret.leader_info = raft_server().state.find_peer(ret.leader_id).ip_port;
        ret.error_message = "OK";
        session->send(session->message_processor().serialize(std::move(ret)));
        it = waiting_clients_.erase(it);
      } else {
        break;
      }
    }
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
    auto &peer = raft_server().state.find_peer(id);
    auto self = shared_from_this();
    auto session = std::make_shared<Session>(self, tcp::socket(io_service_),
                                             message_factory_.peer(), id);
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

void Server::send(const std::string &id,
                  const raft::RPC::AppendEntriesRequest &request) {
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

void Server::send(const std::string &id,
                  const raft::RPC::ClientRequest &request) {
  session_send(id, request);
}

void Server::send(const std::string &id,
                  const raft::RPC::ClientResponse &response) {
  session_send(id, response);
}
}
}
