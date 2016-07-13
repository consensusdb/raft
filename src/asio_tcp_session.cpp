#include <asio_tcp_session.hpp>
#include <asio_tcp_server.hpp>
#include <network_message_processor.hpp>
#include <asio.hpp>
#include <iostream>

namespace network {
namespace asio {
Session::Session(std::shared_ptr<Server> server, tcp::socket socket,
                 std::unique_ptr<MessageProcessor> message_processor,
                 std::string id)
    : socket_(std::move(socket)),
      message_processor_(std::move(message_processor)),
      server_(std::move(server)),
      id_(std::move(id)),
      stop_(true),
      close_(false){}

Session::~Session() {}

void Session::start() {
  if (!stop_) {
    return;
  }
  stop_ = false;
  do_read(message_processor_->process_read(id_, 0, server().raft_server()));
}

void Session::do_read(buffer_t buffer) {
  if (stop_) {
    return;
  }
  auto self = shared_from_this();
  socket_.async_read_some(
      ::asio::buffer(buffer.ptr, buffer.size),
      [this, self](std::error_code ec, std::size_t bytes_recvd) mutable {
        if (!ec && !stop_) {
          buffer_t buffer = message_processor_->process_read(
              id_, bytes_recvd, server().raft_server());
          do_read(buffer);
        } else {
          stop_ = true;
          if (write_queue_.empty()) {
            if ( close_ ) {
              socket_.close();
            }
            // if the write queue isn't empty,
            // let the write callback drop the connection
            server().drop(id());
          }
        }
      });
}

std::string &Session::id() { return id_; }

void Session::stop() {
  stop_ = true;
}

void Session::stop_and_close() {
  if ( close_ == true ) {
    return;
  }
  stop();
  close_ = true;
  std::error_code ec;
  socket_.shutdown(tcp::socket::shutdown_both,ec);
}

void Session::send(std::string message) {
  if (stop_) {
    return;
  }
  write_queue_.emplace_back(std::move(message));
  if (write_queue_.size() == 1) {
    do_write();
  }
}

MessageProcessor &Session::message_processor() { return *message_processor_; }

void Session::do_write() {
  if (stop_) {
    return;
  }
  auto self = shared_from_this();
  ::asio::async_write(socket_, ::asio::buffer(write_queue_.front().data(),
                                              write_queue_.front().size()),
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec && !stop_) {
                          write_queue_.pop_front();
                          if (!write_queue_.empty()) {
                            do_write();
                          }
                        } else {
                          stop_ = true;
                          if ( close_ ) {
                            socket_.close();
                          }
                          write_queue_.clear();
                          server().drop(id());
                        }
                      });
}

tcp::socket &Session::socket() { return socket_; }

Server &Session::server() { return *server_; }
}
}