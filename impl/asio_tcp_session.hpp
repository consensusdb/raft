#pragma once
#include <memory>
#include <string>
#include <list>
#include <asio.hpp>
#include <network_message_processor.hpp>

namespace network {
namespace asio {
using ::asio::ip::tcp;
class Server;

class Session : public std::enable_shared_from_this<Session> {
 public:
  Session(std::shared_ptr<Server> server, tcp::socket socket,
          std::unique_ptr<MessageProcessor> message_processor, std::string id);
  ~Session();

  tcp::socket &socket();
  Server &server();
  void start();
  void stop();
  void stop_and_close();
  void send(std::string message);
  std::string &id();
  MessageProcessor &message_processor();

 private:
  void do_read(buffer_t buffer);
  void do_write();

  std::shared_ptr<Server> server_;
  tcp::socket socket_;
  std::unique_ptr<MessageProcessor> message_processor_;
  std::list<std::string> write_queue_;
  std::string id_;
  bool stop_;
  bool close_;
};
}
}