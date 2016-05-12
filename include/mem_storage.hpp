#pragma once
#include <raft.hpp>
#include <algorithm>

namespace avery {
class MemStorage : public raft::Storage {
 public:
  MemStorage() : raft::Storage(), current_term_(0), commit_info_{0, 0} {}
  uint64_t append(const std::vector<raft::Entry> &entries) {
    if (entries.empty()) {
      if (entries_.empty()) {
        return 0;
      }
      return entries_.back().info.index;
    }
    std::remove_if(std::rbegin(entries_), std::rend(entries_),
                   [&entries](const auto &e) {
                     return entries.front().info.index <= e.info.index;
                   });
    entries_.insert(std::end(entries_), std::begin(entries), std::end(entries));
    return entries_.back().info.index;
  }

  void voted_for(std::string id) { voted_for_ = id; }

  void current_term(uint64_t current_term) { current_term_ = current_term; }

  uint64_t commit_until(uint64_t commit_index) {
    if (entries_.empty()) return 0;
    auto it = std::find_if(
        std::rbegin(entries_), std::rend(entries_),
        [commit_index](const auto &e) { return e.info.index <= commit_index; });
    if (it == std::rend(entries_)) {
      return 0;
    }
    commit_info_ = it->info;
    return commit_info_.index;
  }

  std::string voted_for() { return voted_for_; }

  uint64_t current_term() { return current_term_; }

  raft::EntryInfo last_commit() { return commit_info_; }

  std::vector<raft::Entry> entries_since(uint64_t index) {
    if (entries_.empty()) return {};
    auto it =
        std::find_if(std::rbegin(entries_), std::rend(entries_),
                     [index](const auto &e) { return e.info.index == index; });

    return {it.base(), std::end(entries_)};
  }

  raft::EntryInfo get_last_entry_info() {
    if (entries_.empty()) {
      return {0, 0};
    } else {
      return entries_.back().info;
    }
  }

  raft::EntryInfo get_entry_info(uint64_t index) {
    auto it = std::find_if(
        entries_.rbegin(), entries_.rend(),
        [&index](const raft::Entry &e) { return e.info.index == index; });
    if (it != entries_.rend()) {
      return it->info;
    } else {
      return {0, 0};
    }
  }

 private:
  std::vector<raft::Entry> entries_;
  std::string voted_for_;
  uint64_t current_term_;
  raft::EntryInfo commit_info_;
};
}