#include <mem_storage.hpp>
#include <raft.hpp>
#include <algorithm>
#include <sstream>
#include <simple_serialize.hpp>

namespace avery {

MemStorage::MemStorage(const char *  filename) : raft::Storage<std::string>(), current_term_(0), uncommit_{0,0}, commit_{0,0}, log_file_(filename) {
}

raft::EntryInfo MemStorage::append(const std::vector<raft::Entry<std::string> > &entries) {
  if(entries.empty()) {
    return uncommit_;
  }
  if ( uncommit_.index >= entries.front().info.index ) {
    if ( entries_[uncommit_.index - 1].info.index != uncommit_.index ) {
      //how can this occur? It can't/shouldn't
      exit(1);
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
  uncommit_ = entries.back().info;
  return uncommit_;
}

void MemStorage::voted_for(std::string id) {
  if ( voted_for_ == id ) {
    return;
  }
  if(!id.empty()) {
    std::ostringstream oss;
    oss << "V " << id << "\n";
    std::string temp(oss.str());
    log_file_.write(temp.c_str(), temp.size());
  }
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

raft::EntryInfo MemStorage::commit_until(uint64_t commit_index) {
  commit_index = std::min(uncommit_.index, commit_index);
  if ( commit_.index == commit_index ) {
    return commit_;
  }
  std::ostringstream oss;
  oss << "C " << commit_index << "\n";
  std::string temp(oss.str());
  log_file_.write(temp.c_str(), temp.size());
  commit_ = get_entry_info(commit_index);
  return commit_;
}

std::string MemStorage::voted_for() { return voted_for_; }

uint64_t MemStorage::current_term() { return current_term_; }

raft::LogState MemStorage::log_state() { return { commit_, uncommit_ };  }

std::vector<raft::Entry<std::string> > MemStorage::entries_since(uint64_t index) {
  if ( index >= entries_.size() ) {
    return{};
  }
  if ( entries_[index].info.index != index+1 ) {
    exit(1);
  }
  return std::vector<raft::Entry<std::string> >{entries_.begin() + index, entries_.end()};
}

raft::EntryInfo MemStorage::get_entry_info(uint64_t index) {
  if ( index == 0 || index > entries_.size() ) { return {0,0}; }
  if ( entries_[index-1].info.index != index ) {
    //how can this occur? It can't/shouldn't
    exit(1);
  }
  return entries_[index - 1].info;
}

}
