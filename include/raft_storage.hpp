#pragma once

#include <raft_rpc.hpp>

//raft storage is simple. You must keep a log of entries. Once committed, they are immutable.
//There will always be a certain amount of logs that have been appended and not comitted.
//When appending new logs, if a non-comitted log is already stored, the newly appended log
//takes precedence and deletes the previously uncommitted logs.
//i.e:
//
//index 1 entry 1. <-committed
//index 2 entry 2. <-committed
//index 3 entry 3. <-committed
//index 4 entry 4. <-appended
//index 5 entry 5. <-appended
//
//If append tries to append logs below index 4, there's a serious problem because the entries are comitted.
//If append adds an entry at index 6, just append it.
//If append adds an entry at index 4, then delete entries including and after 4 (4 and 5),
//and append the new entries


namespace raft {
// abstract interface that needs to be implemented
template <typename data_t>
class Storage {
 public:
  // write operations
  virtual EntryInfo append(const std::vector<
      Entry<data_t> >& entries) = 0;  // return the highest appended entry
  virtual void voted_for(std::string id) = 0; //save the vote
  virtual void current_term(uint64_t current_term) = 0; //save the current term
  virtual EntryInfo commit_until( uint64_t commit_index) = 0;  // return the highest entry committed

  // read operations
  virtual std::string voted_for() = 0; //last voted_for entry
  virtual uint64_t current_term() = 0;
  virtual LogState log_state() = 0; //entry index and term of the last entry comitted 
  virtual std::vector<Entry<data_t> > entries_since(uint64_t index) = 0; //get entries after a certain index
  virtual EntryInfo get_entry_info(uint64_t index) = 0; //get the index and term of a certain index
  inline virtual ~Storage() {}
};
}
