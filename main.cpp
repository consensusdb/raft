#include <raft.hpp>
#include <map>
#include <asio_tcp_server.hpp>
#include <mem_storage.hpp>
#include <simple_message_processor.hpp>


int main() {
  //name, ip, peer port(server) client port
  raft::PeerInfo peers = {
    {"Armada", "127.0.0.1", 8000, 3000 },
    {"Banner", "127.0.0.1", 8001, 3001 },
    {"Cobble", "127.0.0.1", 8002, 3002 },
    {"Dragon", "127.0.0.1", 8003, 3003 },
    {"Eiffel", "127.0.0.1", 8004, 3004 },
    {"Farter", "127.0.0.1", 8005, 3005 },
    {"Deadly", "127.0.0.1", 8006, 3006},
  };

  asio::io_service io_service;

  std::vector<std::shared_ptr<network::asio::Server> > servers;
  avery::MyMessageProcessoryFactory message_factory;

  for ( auto &peer : peers ) {
    if ( peer.id == "Deadly" ) {
      continue;
    }
    servers.emplace_back(std::make_shared<network::asio::Server>(io_service, peer.ip_port.port, peer.ip_port.client_port, peer.id, peers, std::unique_ptr<raft::Storage >{ new avery::MemStorage() }, message_factory, 100));
    servers.back()->start();
  }

  io_service.run();
  return 0;
}
