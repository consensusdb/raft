#pragma once

#include <raft_rpc.hpp>

namespace raft {
// abstract interface that needs to be implemented
class Storage {
 public:
  // write operations
  virtual uint64_t append(const std::vector<
      Entry>& entries) = 0;  // return the index of the highest appended index
  virtual void voted_for(std::string id) = 0;
  virtual void current_term(uint64_t current_term) = 0;
  virtual uint64_t commit_until(
      uint64_t commit_index) = 0;  // return the index of highest index comitted

  // read operations
  virtual std::string voted_for() = 0;
  virtual uint64_t current_term() = 0;
  virtual EntryInfo last_commit() = 0;
  virtual std::vector<Entry> entries_since(uint64_t index) = 0;
  virtual EntryInfo get_last_entry_info() = 0;
  virtual EntryInfo get_entry_info(uint64_t index) = 0;
  inline virtual ~Storage() {}
};
}
