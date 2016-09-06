#include <mem_storage.hpp>
#include <raft.hpp>
#include <algorithm>
#include <sstream>
#include <simple_serialize.hpp>

namespace avery {

MemStorage::MemStorage(const char *  filename) : raft::Storage(), current_term_(0), last_entry_info_{0,0}, commit_info_{0,0}, log_file_(filename) {
}

uint64_t MemStorage::append(const std::vector<raft::Entry> &entries) {
  if(entries.empty()) {
    return last_entry_info_.index;
  }
  if ( last_entry_info_.index >= entries.front().info.index ) {
    if ( entries_[last_entry_info_.index - 1].info.index != last_entry_info_.index ) {
      //how can this occur? It can't/shouldn't
      abort();
    }
    entries_.erase(entries_.begin() + entries.front().info.index - 1, entries_.end());
  }
  entries_.insert(entries_.end(), entries.begin(), entries.end());

  std::ostringstream oss;
  std::for_each(entries.begin(), entries.end(), [this, &oss](auto &entry) {
    oss << "E " << entry.info.index << " " << entry.data << "\n";
  });
  std::string temp(oss.str());
  log_file_.write(temp.c_str(), temp.size());
  last_entry_info_ = entries.back().info;
  return last_entry_info_.index;
}

void MemStorage::voted_for(std::string id) {
  if ( voted_for_ == id ) {
    return;
  }
  std::ostringstream oss;
  oss << "C " << id << "\n";
  std::string temp(oss.str());
  log_file_.write(temp.c_str(), temp.size());
  voted_for_ = id; 
}

void MemStorage::current_term(uint64_t current_term) { 
  if ( current_term_ == current_term ) {
    return;
  }
  std::ostringstream oss;
  oss << "T " << current_term << "\n";
  std::string temp(oss.str());
  log_file_.write(temp.c_str(), temp.size());
  current_term_ = current_term;
}

uint64_t MemStorage::commit_until(uint64_t commit_index) {
  commit_index = min(last_entry_info_.index, commit_index);
  if ( commit_info_.index == commit_index ) {
    return commit_info_.index;
  }
  std::ostringstream oss;
  oss << "C " << commit_index << "\n";
  std::string temp(oss.str());
  log_file_.write(temp.c_str(), temp.size());
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
    //how can this occur? It can't/shouldn't
    abort();
  }
  return entries_[index - 1].info;
}

}
