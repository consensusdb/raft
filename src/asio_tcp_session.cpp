#include <asio_tcp_session.hpp>
#include <asio_tcp_server.hpp>
#include <network_message_processor.hpp>
#include <asio.hpp>

namespace network {
namespace asio {
Session::Session(std::shared_ptr<Server> server, tcp::socket socket,
                 std::unique_ptr<MessageProcessor> message_processor,
                 std::string id)
    : socket_(std::move(socket)),
      message_processor_(std::move(message_processor)),
      server_(std::move(server)),
      ending_(false),
      id_(std::move(id)) {}

void Session::start() {
  if (ending_ == true) {
    return;
  }
  do_read(message_processor_->process_read(id_, 0, server().raft_server()));
}

void Session::start(const std::string &id) {
  if (ending_ == true) {
    return;
  }
  std::ostringstream oss;
  oss << "ID " << id << "\n";
  send(oss.str());
  do_read(message_processor_->process_read(id_, 0, server().raft_server()));
}

void Session::do_read(buffer_t buffer) {
  if (ending_ == true) {
    return;
  }
  auto self = shared_from_this();
  socket_.async_read_some(
      ::asio::buffer(buffer.ptr, buffer.size),
      [this, self](std::error_code ec, std::size_t bytes_recvd) mutable {
        if (ending_ == true) {
          return;
        }
        if (ec) {
          // unregister
          // TODO
        }
        buffer_t buffer = message_processor_->process_read(
            id_, bytes_recvd, server().raft_server());
        do_read(buffer);
      });
}

std::string &Session::id() { return id_; }

void Session::end() { ending_ = true; }

void Session::send(std::string message) {
  write_queue_.emplace_back(std::move(message));
  if (write_queue_.size() == 1) {
    do_write();
  }
}

MessageProcessor &Session::message_processor() { return *message_processor_; }

void Session::do_write() {
  if (ending_ == true) {
    return;
  }
  auto self = shared_from_this();
  ::asio::async_write(socket_, ::asio::buffer(write_queue_.front().data(),
                                              write_queue_.front().size()),
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                          write_queue_.pop_front();
                          if (!write_queue_.empty()) {
                            this->do_write();
                          }
                        } else {
                          // TODO
                        }
                      });
}

tcp::socket &Session::socket() { return socket_; }

Server &Session::server() { return *server_; }
}
}