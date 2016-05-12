#include <raft_rpc.hpp>
namespace raft {
class Callbacks {
 public:
  virtual void set_heartbeat_timeout() = 0;
  virtual void set_vote_timeout() = 0;
  virtual void set_minimum_timeout() = 0;

  virtual void send(const std::string& peer_id,
                    const raft::RPC::AppendEntriesRequest& request) = 0;
  virtual void send(const std::string& peer_id,
                    const raft::RPC::AppendEntriesResponse& response) = 0;
  virtual void send(const std::string& peer_id,
                    const raft::RPC::VoteRequest& request) = 0;
  virtual void send(const std::string& peer_id,
                    const raft::RPC::VoteResponse& response) = 0;
  virtual void send(const std::string& peer_id,
                    const raft::RPC::ClientRequest& request) = 0;
  virtual void send(const std::string& peer_id,
                    const raft::RPC::ClientResponse& response) = 0;

  virtual void identify(const std::string& temp_id,
                        const std::string& peer_id) = 0;

  virtual void client_waiting(const std::string& peer_id,
                              const raft::EntryInfo& info) = 0;
  virtual void commit_advanced(uint64_t commit_index) = 0;
  inline virtual ~Callbacks() {}
};
}