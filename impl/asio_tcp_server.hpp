#pragma once
#include <asio.hpp>
#include <raft.hpp>
#include <asio/high_resolution_timer.hpp>
#include <random>
#include <map>

namespace network {
class MessageProcessor;
class MessageProcessorFactory;
namespace asio {
using ::asio::ip::tcp;
class Session;

class Server : public raft::Callbacks,
               public std::enable_shared_from_this<Server> {
 public:
  Server(::asio::io_service& io_service, short server_port, short client_port,
         std::string id, raft::PeerInfo known_peers,
         std::unique_ptr<raft::Storage> storage,
         const network::MessageProcessorFactory& factory, int heartbeat_ms);
  ~Server();
  void start();
  void stop();
  bool identify(const std::string& temp_id, const std::string& peer_id);
  void drop(const std::string& temp_id);

  void send(const std::string& peer_id,
            const raft::RPC::AppendEntriesRequest& request);
  void send(const std::string& peer_id,
            const raft::RPC::AppendEntriesResponse& request);
  void send(const std::string& peer_id, const raft::RPC::VoteRequest& request);
  void send(const std::string& peer_id, const raft::RPC::VoteResponse& request);

  void client_send(const std::string& peer_id,
            const raft::RPC::ClientResponse& request);

  void client_send(const std::string& peer_id,
                   const raft::RPC::NotLeaderResponse& request);
  
  void client_send(const std::string& peer_id,
                   const raft::RPC::LocalFailureResponse& request);

  void client_send(const std::string& peer_id,
                   const raft::RPC::CurrentEntryResponse& response);

  void set_heartbeat_timeout();
  void set_vote_timeout();
  void set_minimum_timeout();

  raft::Server& raft_server();

  void commit_advanced(uint64_t commit_index);

 private:
  void do_accept_client();
  void do_accept_peer();

  template <class M>
  void session_send(const std::string& peer_id, M message);

  template <class M>
  void client_session_send(const std::string& client_id, M message);

  std::mt19937 mt_;
  std::uniform_int_distribution<int> dist_;
  ::asio::io_service& io_service_;
  tcp::acceptor acceptor_;
  tcp::acceptor client_acceptor_;

  std::vector<std::shared_ptr<Session> > peers_;

  std::shared_ptr<Session> accepting_client_session_;
  std::shared_ptr<Session> accepting_peer_session_;
  std::unordered_map<std::string, std::shared_ptr<Session> > all_clients_;

  ::asio::high_resolution_timer timer_;
  ::asio::high_resolution_timer minimum_timer_;
  const network::MessageProcessorFactory& message_factory_;
  raft::Server raft_server_;
  uint64_t next_id_;
  bool stop_;

  uint64_t watermark_index_;
};
}
}
