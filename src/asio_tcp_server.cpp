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
    : mt_(std::random_device()()),
      dist_(static_cast<int>((0.8 * heartbeat_ms + 1) / 2),
            static_cast<int>((1.2 * heartbeat_ms + 2) / 2)),
      io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
      client_acceptor_(io_service, tcp::endpoint(tcp::v4(), client_port)),
      timer_(io_service, 10 * std::chrono::milliseconds(dist_(mt_))),
      minimum_timer_(io_service, 10 * std::chrono::milliseconds(dist_.min())),
      message_factory_(factory),
      raft_server_(id, std::move(known_peers), std::move(storage), *this),
      next_id_(0) {}

void Server::start() {
  // we add an initial penalty when the server starts up before it can try to be
  // a leader
  timer_.async_wait([this](const std::error_code &e) {
    if (!e) {
      raft_server().timeout();
    }
  });
  do_accept_peer();
  do_accept_client();
}

void Server::set_heartbeat_timeout() {
  timer_.expires_from_now(std::chrono::milliseconds(dist_(mt_)));
  timer_.async_wait([this](const std::error_code &e) {
    if (!e) {
      raft_server().timeout();
    }
  });
}

void Server::set_vote_timeout() {
  timer_.expires_from_now(std::chrono::milliseconds(10 * dist_(mt_)));
  timer_.async_wait([this](const std::error_code &e) {
    if (!e) {
      raft_server().timeout();
    }
  });
}

void Server::set_minimum_timeout() {
  minimum_timer_.expires_from_now(std::chrono::milliseconds(10 * dist_.min()));
  minimum_timer_.async_wait([this](const std::error_code &e) {
    if (!e) {
      raft_server().on(raft::RPC::MinimumTimeoutRequest{});
    }
  });
}

void Server::identify(const std::string &temp_id, const std::string &id) {
  {
    auto it = std::find_if(raft_server().state.known_peers.begin(),
                           raft_server().state.known_peers.end(),
                           [&id](auto &peer) { return peer.id == id; });

    if (it == raft_server().state.known_peers.end()) {
      // unknown peer tried to connect
      return;
    }
  }
  {
    auto it = std::find_if(identified_peers_.begin(), identified_peers_.end(),
                           [&id](auto &peer) { return peer->id() == id; });
    if (it != identified_peers_.end()) {
      (*it)->end();
      all_peers_.erase(std::remove_if(all_peers_.begin(), all_peers_.end(),
                                      [&it](auto peer) {
                                        return peer->id() == (*it)->id();
                                      }),
                       all_peers_.end());
    }
  }

  auto it =
      std::find_if(all_peers_.begin(), all_peers_.end(),
                   [&temp_id](auto &peer) { return peer->id() == temp_id; });
  if (it != all_peers_.end()) {
    identified_peers_.emplace_back(*it);
  }
}

raft::Server &Server::raft_server() { return raft_server_; }

void Server::do_accept_peer() {
  auto self = shared_from_this();
  auto session = std::make_shared<Session>(self, tcp::socket(io_service_),
                                           message_factory_.peer());
  acceptor_.async_accept(session->socket(),
                         [self, session](std::error_code ec) {
                           if (!ec) {
                             std::ostringstream oss;
                             oss << "peer_" << ++self->next_id_;
                             session->id() = oss.str();
                             self->all_peers_.emplace_back(std::move(session));
                             self->all_peers_.back()->start();
                           }

                           self->do_accept_peer();
                         });
}

void Server::do_accept_client() {
  auto self = shared_from_this();
  auto session = std::make_shared<Session>(self, tcp::socket(io_service_),
                                           message_factory_.client());
  client_acceptor_.async_accept(
      session->socket(), [self, session](std::error_code ec) {
        if (!ec) {
          std::ostringstream oss;
          oss << "client_" << ++self->next_id_;
          session->id() = oss.str();
          self->all_clients_.emplace(session->id(), session);
          session->start();
        }

        self->do_accept_client();
      });
}

void Server::client_waiting(const std::string &id,
                            const raft::EntryInfo &info) {
  waiting_clients_.emplace(std::move(info), id);
}

void Server::commit_advanced(uint64_t commit_index) {
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
  auto it = std::find_if(identified_peers_.begin(), identified_peers_.end(),
                         [&id](auto &peer) { return peer->id() == id; });
  if (it == identified_peers_.end()) {
    auto &peer = raft_server().state.find_peer(id);
    auto self = shared_from_this();
    auto session = std::make_shared<Session>(self, tcp::socket(io_service_),
                                             message_factory_.peer(), id);
    auto shared_message = std::make_shared<M>(std::move(message));
    tcp::endpoint endpoint(
        ::asio::ip::address::from_string(peer.ip_port.ip.c_str()),
        peer.ip_port.port);
    session->socket().async_connect(
        endpoint, [self, session, shared_message](std::error_code ec) {
          session->socket().set_option(tcp::no_delay(true));
          if (!ec) {
            self->identified_peers_.emplace_back(std::move(session));
            session->start(self->raft_server().state.id);
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
