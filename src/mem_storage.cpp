#include <mem_storage.hpp>
#include <raft.hpp>
#include <algorithm>
#include <fstream>
#include <simple_serialize.hpp>

namespace avery {

MemStorage::MemStorage(const char *  filename) : raft::Storage(), current_term_(0), last_entry_info_{0,0}, commit_info_{0,0}, log_file_(filename, std::ios::app) {
}

uint64_t MemStorage::append(const std::vector<raft::Entry> &entries) {
  int ret = 0;
  if(entries.empty()) {
    return last_entry_info_.index;
  }
  if ( last_entry_info_.index >= entries.front().info.index ) {
    if ( entries_[last_entry_info_.index - 1].info.index != last_entry_info_.index ) {
      std::cout << "WTF" << std::endl;
    }
    entries_.erase(entries_.begin() + entries.front().info.index - 1, entries_.end());
  }
  entries_.insert(entries_.end(), entries.begin(), entries.end());

  std::for_each(entries.begin(), entries.end(), [this](auto &entry) {
    log_file_ << "E " << entry.info.index << " " << entry.data << "\n";
  });
  log_file_ << std::flush;
  last_entry_info_ = entries.back().info;
  return last_entry_info_.index;
}

void MemStorage::voted_for(std::string id) {
  if ( voted_for_ == id ) {
    return;
  }
  log_file_ << "V " << id << "\n" << std::flush;
  voted_for_ = id; 
}

void MemStorage::current_term(uint64_t current_term) { 
  if ( current_term_ == current_term ) {
    return;
  }
  log_file_ << "T " << current_term << "\n" << std::flush;
  current_term_ = current_term;
}

uint64_t MemStorage::commit_until(uint64_t commit_index) {
  commit_index = std::min(last_entry_info_.index, commit_index);
  if ( commit_info_.index == commit_index ) {
    return commit_info_.index;
  }
  log_file_ << "C " << commit_index << "\n" << std::flush;
  commit_info_ = get_entry_info(commit_index);
  return commit_info_.index;
}

std::string MemStorage::voted_for() { return voted_for_; }

uint64_t MemStorage::current_term() { return current_term_; }

raft::EntryInfo MemStorage::last_commit() { return commit_info_;  }

std::vector<raft::Entry> MemStorage::entries_since(uint64_t index) {
  if ( index >= entries_.size() ) {
    return{};
  }
  if ( entries_[index].info.index != index+1 ) {
    std::cout << "WTF" << std::endl;
  }
  return std::vector<raft::Entry>{entries_.begin() + index, entries_.end()};
}

raft::EntryInfo MemStorage::get_last_entry_info() {
  return last_entry_info_;
}

raft::EntryInfo MemStorage::get_entry_info(uint64_t index) {
  if ( index == 0 || index > entries_.size() ) { return {0,0}; }
  if ( entries_[index-1].info.index != index ) {
    std::cout << "WTF" << std::endl;
  }
  return entries_[index - 1].info;
}

}