#pragma once
#include <raft.hpp>
#include <map>
#include <list>
#include <fstream>

namespace avery {
class MemStorage : public raft::Storage {
 public:
  MemStorage(const char * filename);
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
  typedef std::vector<raft::Entry> entries_t;
  entries_t entries_;
  std::ofstream log_file_;
  std::string voted_for_;
  raft::EntryInfo last_entry_info_;
  raft::EntryInfo commit_info_;
  uint64_t current_term_;
};
}