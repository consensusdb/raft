#include <raft.hpp>
#include <map>
#include <asio_tcp_server.hpp>
#include <mem_storage.hpp>
#include <simple_message_processor.hpp>
#include <asio.hpp>
#include <iostream>
#include <functional>


int main() {
  //name, ip, peer port(server) client port
  raft::PeerInfo peers = {
    {"Armada", "127.0.0.1", 8000, 3000 },
    {"Banner", "127.0.0.1", 8001, 3001 },
    {"Cobble", "127.0.0.1", 8002, 3002 },
    {"Dragon", "127.0.0.1", 8003, 3003 },
    {"Eiffel", "127.0.0.1", 8004, 3004 },
    {"Farter", "127.0.0.1", 8005, 3005 },
    {"Deadly", "127.0.0.1", 8006, 3006}
  };
  int heartbeat_ms = 100;

  asio::io_service io_service;

  std::vector<std::shared_ptr<network::asio::Server> > servers;
  avery::MyMessageProcessoryFactory message_factory;

  std::for_each(peers.begin(), peers.end(), [&](auto peer) {
    servers.emplace_back(std::make_shared<network::asio::Server>(io_service, peer.ip_port.port, peer.ip_port.client_port, peer.id, peers, std::unique_ptr<raft::Storage >{ new avery::MemStorage() }, message_factory, heartbeat_ms));
    servers.back()->start();
  });

  std::mt19937 mt(std::random_device{}());
  std::uniform_int_distribution<int> dist(static_cast<int>((5 * heartbeat_ms + 1) / 2),
                                          static_cast<int>((10 * heartbeat_ms + 2) / 2));
  std::uniform_int_distribution<int> server_dist(0,servers.size()-1);
  std::uniform_int_distribution<int> chaos_dist(0, 100);
  
  std::vector<bool> is_server_on(servers.size(), true);

  asio::high_resolution_timer shark_attack(io_service, std::chrono::milliseconds(heartbeat_ms));
  
  std::function<void(const std::error_code&)> on_shark_attack;
  bool quorum_broken = false;
  on_shark_attack = [&](const std::error_code &e) {
    if ( !e ) {
      int idx = server_dist(mt);
      auto &server = servers[idx];
      int percent = chaos_dist(mt);
      if ( percent >= 60 ) {
        if ( is_server_on[idx] ) {
          bool killing_leader = (server->raft_server().state.role == raft::State::Role::Leader);
          size_t servers_running = (std::count(is_server_on.begin(), is_server_on.end(), true)-1);
          std::cout << "><(((('> shark attack on " << server->raft_server().state.id << " OFF,"
            << servers_running << "/" << is_server_on.size() << std::endl;
          server->stop();
          if ( servers_running < (1 + (is_server_on.size() / 2)) ) {
            if ( killing_leader ) {
              quorum_broken = true;
              std::cout << "><(((('> shark attack quorum is broken no leader should be elected" << std::endl;
            }
          } else if ( killing_leader ) {
            std::cout << "><(((('> shark attack new leader will be elected" << std::endl;
          }
          is_server_on[idx] = !is_server_on[idx];
        }
      } else if ( !is_server_on[idx] ) {
        size_t servers_running = (std::count(is_server_on.begin(), is_server_on.end(), true)+1);
        std::cout << "><(((('> shark attack on " << server->raft_server().state.id << " ON,"
          << servers_running << "/" << is_server_on.size() << std::endl;
        server->start();
        if ( ( servers_running >= (1 + (is_server_on.size() / 2))) && quorum_broken) {
          quorum_broken = false;
          std::cout << "><(((('> shark attack quorum possible again, a new leader will be elected" << std::endl;
        }
        is_server_on[idx] = !is_server_on[idx];
      }
      shark_attack.expires_from_now(std::chrono::milliseconds(dist(mt)));
      shark_attack.async_wait(on_shark_attack);
    }
  };

  shark_attack.async_wait(on_shark_attack);




  io_service.run();
  return 0;
}
