#include "gtest/gtest.h"
#include "mock_storage.hpp"
#include "mock_callbacks.hpp"

#include <raft.hpp>

namespace {
  raft::PeerInfo peers = {
    {"Armada", "127.0.0.1", 8000, 3000 },
    {"Banner", "127.0.0.1", 8001, 3001 },
    {"Cobble", "127.0.0.1", 8002, 3002 }
  };
}

TEST(Raft, send_vote_request) {
  using namespace raft;
  test::Callbacks callbacks;
  Server server(peers[0].id, peers, std::unique_ptr<Storage>(new test::Storage()), callbacks);
  server.on(RPC::TimeoutRequest{});
  ASSERT_EQ(peers.size()-1, callbacks.vote_request_calls_);
  callbacks.reset();
  server.on(RPC::TimeoutRequest{});
  ASSERT_EQ(peers.size()-1, callbacks.vote_request_calls_);
}
