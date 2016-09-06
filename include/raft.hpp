#pragma once
//convenient header that adds all the required raft headers

#include <raft_rpc.hpp>  //structs that represent the RPC requests and responses
#include <raft_entry.hpp>  //structs that represent an entry
#include <raft_state.hpp>  //struct that represents the volatile state
#include <raft_storage.hpp>  //abstact interface that represents the non-volatile state
#include <raft_server.hpp>  //event handlers your networking code calls
