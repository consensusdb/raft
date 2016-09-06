#pragma once

#include <raft_storage.hpp>
namespace raft {
namespace test {
class Storage : public raft::Storage {
 public:
  std::string voted_for_;
  int voted_for_calls_;

  uint64_t current_term_;
  int current_term_calls_;
  
  uint64_t commit_index_;
  int commit_index_calls_;

  std::vector< Entry> entries_;
  int append_calls_;

  inline Storage(): raft::Storage() {}

  inline virtual uint64_t append(const std::vector< Entry>& entries) {
    append_calls_++;
    entries_ = entries;
    if(entries_.empty()) {
      return 0;
    }
    return entries_.back().info.index;
  }

  inline virtual void voted_for(std::string id) {
    voted_for_ = id;
    voted_for_calls_++;
  }

  inline virtual void current_term(uint64_t current_term) {
    current_term_ = current_term;
    current_term_calls_++;
  }

  inline virtual uint64_t commit_until(uint64_t commit_index) {
    if(entries_.empty()) {
      return 0;
    }
    commit_index_ = std::min(commit_index, entries_.back().info.index);
    commit_index_calls_++;
    return commit_index_;
  }

  inline virtual std::string voted_for() { 
    return voted_for_; 
  }

  inline virtual uint64_t current_term() {
    return current_term_;
  }

  inline virtual EntryInfo last_commit() {
    for(auto &entry : entries_) {
      if(entry.info.index == commit_index_) {
        return entry.info;
      }
    }
    return {0,0};
  }

  inline virtual std::vector<Entry> entries_since(uint64_t index) {
    for(auto it = entries_.begin(); it != entries_.end(); ++it) {
      if(it->info.index == index) {
        return {it, entries_.end()};
      }
    }
    return {};
  }

  inline virtual EntryInfo get_last_entry_info() {
    if(entries_.empty()) {
      return {0,0};
    }
    return entries_.back().info;
  }

  inline virtual EntryInfo get_entry_info(uint64_t index) {
    for(auto &entry : entries_) {
      if(entry.info.index == index) {
        return entry.info;
      }
    }
    return {0,0};
  }

  inline virtual ~Storage() {}
};
}
}
