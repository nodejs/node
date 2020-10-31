/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PROFILING_MEMORY_BOOKKEEPING_H_
#define SRC_PROFILING_MEMORY_BOOKKEEPING_H_

#include <map>
#include <vector>

#include "perfetto/base/time.h"
#include "src/profiling/common/callstack_trie.h"
#include "src/profiling/common/interner.h"
#include "src/profiling/memory/unwound_messages.h"

// Below is an illustration of the bookkeeping system state where
// PID 1 does the following allocations:
// 0x123: 128 bytes at [bar main]
// 0x234: 128 bytes at [bar main]
// 0xf00: 512 bytes at [foo main]
// PID 1 allocated but previously freed 1024 bytes at [bar main]
//
// PID 2 does the following allocations:
// 0x345: 512 bytes at [foo main]
// 0x456:  32 bytes at [foo main]
// PID 2 allocated but already freed 1235 bytes at [foo main]
// PID 2 allocated and freed 2048 bytes in main.
//
// +---------------------------------+   +-------------------+
// | +---------+    HeapTracker PID 1|   | GlobalCallstackTri|
// | |0x123 128+---+    +----------+ |   |           +---+   |
// | |         |   +---->alloc:1280+----------------->bar|   |
// | |0x234 128+---+    |free: 1024| |   |           +-^-+   |
// | |         |        +----------+ |   |   +---+     ^     |
// | |0xf00 512+---+                 | +----->foo|     |     |
// | +--------+|   |    +----------+ | | |   +-^-+     |     |
// |               +---->alloc: 512+---+ |     |       |     |
// |                    |free:    0| | | |     +--+----+     |
// |                    +----------+ | | |        |          |
// |                                 | | |      +-+--+       |
// +---------------------------------+ | |      |main|       |
//                                     | |      +--+-+       |
// +---------------------------------+ | |         ^         |
// | +---------+    HeapTracker PID 2| | +-------------------+
// | |0x345 512+---+    +----------+ | |           |
// | |         |   +---->alloc:1779+---+           |
// | |0x456  32+---+    |free: 1235| |             |
// | +---------+        +----------+ |             |
// |                                 |             |
// |                    +----------+ |             |
// |                    |alloc:2048+---------------+
// |                    |free: 2048| |
// |                    +----------+ |
// |                                 |
// +---------------------------------+
//   Allocation    CallstackAllocations        Node
//
// The active allocations are on the leftmost side, modeled as the class
// HeapTracker::Allocation.
//
// The total allocated and freed bytes per callsite are in the middle, modeled
// as the HeapTracker::CallstackAllocations class.
// Note that (1280 - 1024) = 256, so alloc - free is equal to the total of the
// currently active allocations.
// Note in PID 2 there is a CallstackAllocations with 2048 allocated and 2048
// freed bytes. This is not currently referenced by any Allocations (as it
// should, as 2048 - 2048 = 0, which would mean that the total size of the
// allocations referencing it should be 0). This is because we haven't dumped
// this state yet, so the CallstackAllocations will be kept around until the
// next dump, written to the trace, and then destroyed.
//
// On the right hand side is the GlobalCallstackTrie, with nodes representing
// distinct callstacks. They have no information about the currently allocated
// or freed bytes, they only contain a reference count to destroy them as
// soon as they are no longer referenced by a HeapTracker.

namespace perfetto {
namespace profiling {

// Snapshot for memory allocations of a particular process. Shares callsites
// with other processes.
class HeapTracker {
 public:
  struct CallstackMaxAllocations {
    uint64_t max;
    uint64_t cur;
  };
  struct CallstackTotalAllocations {
    uint64_t allocated;
    uint64_t freed;
  };

  // Sum of all the allocations for a given callstack.
  struct CallstackAllocations {
    CallstackAllocations(GlobalCallstackTrie::Node* n) : node(n) {}

    uint64_t allocs = 0;
    uint64_t allocation_count = 0;
    uint64_t free_count = 0;
    union {
      CallstackMaxAllocations retain_max;
      CallstackTotalAllocations totals;
    } value = {};

    GlobalCallstackTrie::Node* const node;

    ~CallstackAllocations() { GlobalCallstackTrie::DecrementNode(node); }

    bool operator<(const CallstackAllocations& other) const {
      return node < other.node;
    }
  };

  // Caller needs to ensure that callsites outlives the HeapTracker.
  explicit HeapTracker(GlobalCallstackTrie* callsites, bool dump_at_max_mode)
      : callsites_(callsites), dump_at_max_mode_(dump_at_max_mode) {}

  void RecordMalloc(const std::vector<FrameData>& stack,
                    uint64_t address,
                    uint64_t sample_size,
                    uint64_t alloc_size,
                    uint64_t sequence_number,
                    uint64_t timestamp);

  template <typename F>
  void GetCallstackAllocations(F fn) {
    // There are two reasons we remove the unused callstack allocations on the
    // next iteration of Dump:
    // * We need to remove them after the callstacks were dumped, which
    //   currently happens after the allocations are dumped.
    // * This way, we do not destroy and recreate callstacks as frequently.
    for (auto it_and_alloc : dead_callstack_allocations_) {
      auto& it = it_and_alloc.first;
      uint64_t allocated = it_and_alloc.second;
      const CallstackAllocations& alloc = it->second;
      if (alloc.allocs == 0 && alloc.allocation_count == allocated) {
        // TODO(fmayer): We could probably be smarter than throw away
        // our whole frames cache.
        ClearFrameCache();
        callstack_allocations_.erase(it);
      }
    }
    dead_callstack_allocations_.clear();

    for (auto it = callstack_allocations_.begin();
         it != callstack_allocations_.end(); ++it) {
      const CallstackAllocations& alloc = it->second;
      fn(alloc);

      if (alloc.allocs == 0)
        dead_callstack_allocations_.emplace_back(it, alloc.allocation_count);
    }
  }

  template <typename F>
  void GetAllocations(F fn) {
    for (const auto& addr_and_allocation : allocations_) {
      const Allocation& alloc = addr_and_allocation.second;
      fn(addr_and_allocation.first, alloc.sample_size, alloc.alloc_size,
         alloc.callstack_allocations()->node->id());
    }
  }

  void RecordFree(uint64_t address,
                  uint64_t sequence_number,
                  uint64_t timestamp) {
    RecordOperation(sequence_number, {address, timestamp});
  }

  void ClearFrameCache() { frame_cache_.clear(); }

  uint64_t committed_timestamp() { return committed_timestamp_; }
  uint64_t max_timestamp() { return max_timestamp_; }

  uint64_t GetSizeForTesting(const std::vector<FrameData>& stack);
  uint64_t GetMaxForTesting(const std::vector<FrameData>& stack);
  uint64_t GetTimestampForTesting() { return committed_timestamp_; }

 private:
  struct Allocation {
    Allocation(uint64_t size,
               uint64_t asize,
               uint64_t seq,
               CallstackAllocations* csa)
        : sample_size(size), alloc_size(asize), sequence_number(seq) {
      SetCallstackAllocations(csa);
    }

    Allocation() = default;
    Allocation(const Allocation&) = delete;
    Allocation(Allocation&& other) noexcept {
      sample_size = other.sample_size;
      alloc_size = other.alloc_size;
      sequence_number = other.sequence_number;
      callstack_allocations_ = other.callstack_allocations_;
      other.callstack_allocations_ = nullptr;
    }

    ~Allocation() { SetCallstackAllocations(nullptr); }

    void SetCallstackAllocations(CallstackAllocations* callstack_allocations) {
      if (callstack_allocations_)
        callstack_allocations_->allocs--;
      callstack_allocations_ = callstack_allocations;
      if (callstack_allocations_)
        callstack_allocations_->allocs++;
    }

    CallstackAllocations* callstack_allocations() const {
      return callstack_allocations_;
    }

    uint64_t sample_size;
    uint64_t alloc_size;
    uint64_t sequence_number;

   private:
    CallstackAllocations* callstack_allocations_ = nullptr;
  };

  struct PendingOperation {
    uint64_t allocation_address;
    uint64_t timestamp;
  };

  CallstackAllocations* MaybeCreateCallstackAllocations(
      GlobalCallstackTrie::Node* node) {
    auto callstack_allocations_it = callstack_allocations_.find(node);
    if (callstack_allocations_it == callstack_allocations_.end()) {
      GlobalCallstackTrie::IncrementNode(node);
      bool inserted;
      std::tie(callstack_allocations_it, inserted) =
          callstack_allocations_.emplace(node, node);
      PERFETTO_DCHECK(inserted);
    }
    return &callstack_allocations_it->second;
  }

  void RecordOperation(uint64_t sequence_number,
                       const PendingOperation& operation);

  // Commits a malloc or free operation.
  // See comment of pending_operations_ for encoding of malloc and free
  // operations.
  //
  // Committing a malloc operation: Add the allocations size to
  // CallstackAllocation::allocated.
  // Committing a free operation: Add the allocation's size to
  // CallstackAllocation::freed and delete the allocation.
  void CommitOperation(uint64_t sequence_number,
                       const PendingOperation& operation);

  void AddToCallstackAllocations(uint64_t ts, const Allocation& alloc) {
    alloc.callstack_allocations()->allocation_count++;
    if (dump_at_max_mode_) {
      current_unfreed_ += alloc.sample_size;
      alloc.callstack_allocations()->value.retain_max.cur += alloc.sample_size;

      if (current_unfreed_ <= max_unfreed_)
        return;

      if (max_sequence_number_ == alloc.sequence_number - 1) {
        alloc.callstack_allocations()->value.retain_max.max =
            // We know the only CallstackAllocation that has max != cur is the
            // one we just updated.
            alloc.callstack_allocations()->value.retain_max.cur;
      } else {
        for (auto& p : callstack_allocations_) {
          // We need to reset max = cur for every CallstackAllocation, as we
          // do not know which ones have changed since the last max.
          // TODO(fmayer): Add an index to speed this up
          CallstackAllocations& csa = p.second;
          csa.value.retain_max.max = csa.value.retain_max.cur;
        }
      }
      max_sequence_number_ = alloc.sequence_number;
      max_unfreed_ = current_unfreed_;
      max_timestamp_ = ts;
    } else {
      alloc.callstack_allocations()->value.totals.allocated +=
          alloc.sample_size;
    }
  }

  void SubtractFromCallstackAllocations(const Allocation& alloc) {
    alloc.callstack_allocations()->free_count++;
    if (dump_at_max_mode_) {
      current_unfreed_ -= alloc.sample_size;
      alloc.callstack_allocations()->value.retain_max.cur -= alloc.sample_size;
    } else {
      alloc.callstack_allocations()->value.totals.freed += alloc.sample_size;
    }
  }

  // We cannot use an interner here, because after the last allocation goes
  // away, we still need to keep the CallstackAllocations around until the next
  // dump.
  std::map<GlobalCallstackTrie::Node*, CallstackAllocations>
      callstack_allocations_;

  std::vector<std::pair<decltype(callstack_allocations_)::iterator, uint64_t>>
      dead_callstack_allocations_;

  std::map<uint64_t /* allocation address */, Allocation> allocations_;

  // An operation is either a commit of an allocation or freeing of an
  // allocation. An operation is a free if its seq_id is larger than
  // the sequence_number of the corresponding allocation. It is a commit if its
  // seq_id is equal to the sequence_number of the corresponding allocation.
  //
  // If its seq_id is less than the sequence_number of the corresponding
  // allocation it could be either, but is ignored either way.
  std::map<uint64_t /* seq_id */, PendingOperation /* allocation address */>
      pending_operations_;

  uint64_t committed_timestamp_ = 0;
  // The sequence number all mallocs and frees have been handled up to.
  uint64_t committed_sequence_number_ = 0;
  GlobalCallstackTrie* callsites_;

  const bool dump_at_max_mode_ = false;
  // The following members are only used if dump_at_max_mode_ == true.
  uint64_t max_sequence_number_ = 0;
  uint64_t current_unfreed_ = 0;
  uint64_t max_unfreed_ = 0;
  uint64_t max_timestamp_ = 0;

  // We index by abspc, which is unique as long as the maps do not change.
  // This is why we ClearFrameCache after we reparsed maps.
  std::unordered_map<uint64_t /* abs pc */, Interned<Frame>> frame_cache_;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_BOOKKEEPING_H_
