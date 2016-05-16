#pragma once
#include <raft.hpp>

namespace avery {
class MemStorage : public raft::Storage {
 public:
  MemStorage();
  uint64_t append(const std::vector<raft::Entry> &entries);
  void voted_for(std::string id);
  void current_term(uint64_t current_term);
  uint64_t commit_until(uint64_t commit_index);
  std::string voted_for();
  uint64_t current_term();
  raft::EntryInfo last_commit();
  std::vector<raft::Entry> entries_since(uint64_t index);
  raft::EntryInfo get_last_entry_info();
  raft::EntryInfo get_entry_info(uint64_t index);

 private:
  std::vector<raft::Entry> entries_;
  std::string voted_for_;
  uint64_t current_term_;
  raft::EntryInfo commit_info_;
};
}