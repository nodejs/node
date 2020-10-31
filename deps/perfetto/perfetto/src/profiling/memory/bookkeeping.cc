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

#include "src/profiling/memory/bookkeeping.h"

#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "src/profiling/common/callstack_trie.h"

namespace perfetto {
namespace profiling {

void HeapTracker::RecordMalloc(const std::vector<FrameData>& callstack,
                               uint64_t address,
                               uint64_t sample_size,
                               uint64_t alloc_size,
                               uint64_t sequence_number,
                               uint64_t timestamp) {
  std::vector<Interned<Frame>> frames;
  frames.reserve(callstack.size());
  for (const FrameData& loc : callstack) {
    auto frame_it = frame_cache_.find(loc.frame.pc);
    if (frame_it != frame_cache_.end()) {
      frames.emplace_back(frame_it->second);
    } else {
      frames.emplace_back(callsites_->InternCodeLocation(loc));
      frame_cache_.emplace(loc.frame.pc, frames.back());
    }
  }

  auto it = allocations_.find(address);
  if (it != allocations_.end()) {
    Allocation& alloc = it->second;
    PERFETTO_DCHECK(alloc.sequence_number != sequence_number);
    if (alloc.sequence_number < sequence_number) {
      // As we are overwriting the previous allocation, the previous allocation
      // must have been freed.
      //
      // This makes the sequencing a bit incorrect. We are overwriting this
      // allocation, so we prentend both the alloc and the free for this have
      // already happened at committed_sequence_number_, while in fact the free
      // might not have happened until right before this operation.

      if (alloc.sequence_number > committed_sequence_number_) {
        // Only count the previous allocation if it hasn't already been
        // committed to avoid double counting it.
        AddToCallstackAllocations(timestamp, alloc);
      }

      SubtractFromCallstackAllocations(alloc);
      GlobalCallstackTrie::Node* node = callsites_->CreateCallsite(frames);
      alloc.sample_size = sample_size;
      alloc.alloc_size = alloc_size;
      alloc.sequence_number = sequence_number;
      alloc.SetCallstackAllocations(MaybeCreateCallstackAllocations(node));
    }
  } else {
    GlobalCallstackTrie::Node* node = callsites_->CreateCallsite(frames);
    allocations_.emplace(address,
                         Allocation(sample_size, alloc_size, sequence_number,
                                    MaybeCreateCallstackAllocations(node)));
  }

  RecordOperation(sequence_number, {address, timestamp});
}

void HeapTracker::RecordOperation(uint64_t sequence_number,
                                  const PendingOperation& operation) {
  if (sequence_number != committed_sequence_number_ + 1) {
    pending_operations_.emplace(sequence_number, operation);
    return;
  }

  CommitOperation(sequence_number, operation);

  // At this point some other pending operations might be eligible to be
  // committed.
  auto it = pending_operations_.begin();
  while (it != pending_operations_.end() &&
         it->first == committed_sequence_number_ + 1) {
    CommitOperation(it->first, it->second);
    it = pending_operations_.erase(it);
  }
}

void HeapTracker::CommitOperation(uint64_t sequence_number,
                                  const PendingOperation& operation) {
  committed_sequence_number_++;
  committed_timestamp_ = operation.timestamp;

  uint64_t address = operation.allocation_address;

  // We will see many frees for addresses we do not know about.
  auto leaf_it = allocations_.find(address);
  if (leaf_it == allocations_.end())
    return;

  Allocation& value = leaf_it->second;
  if (value.sequence_number == sequence_number) {
    AddToCallstackAllocations(operation.timestamp, value);
  } else if (value.sequence_number < sequence_number) {
    SubtractFromCallstackAllocations(value);
    allocations_.erase(leaf_it);
  }
  // else (value.sequence_number > sequence_number:
  //  This allocation has been replaced by a newer one in RecordMalloc.
  //  This code commits ther previous allocation's malloc (and implicit free
  //  that must have happened, as there is now a new allocation at the same
  //  address). This means that this operation, be it a malloc or a free, must
  //  be treated as a no-op.
}

uint64_t HeapTracker::GetSizeForTesting(const std::vector<FrameData>& stack) {
  PERFETTO_DCHECK(!dump_at_max_mode_);
  GlobalCallstackTrie::Node* node = callsites_->CreateCallsite(stack);
  // Hack to make it go away again if it wasn't used before.
  // This is only good because this is used for testing only.
  GlobalCallstackTrie::IncrementNode(node);
  GlobalCallstackTrie::DecrementNode(node);
  auto it = callstack_allocations_.find(node);
  if (it == callstack_allocations_.end()) {
    return 0;
  }
  const CallstackAllocations& alloc = it->second;
  return alloc.value.totals.allocated - alloc.value.totals.freed;
}

uint64_t HeapTracker::GetMaxForTesting(const std::vector<FrameData>& stack) {
  PERFETTO_DCHECK(dump_at_max_mode_);
  GlobalCallstackTrie::Node* node = callsites_->CreateCallsite(stack);
  // Hack to make it go away again if it wasn't used before.
  // This is only good because this is used for testing only.
  GlobalCallstackTrie::IncrementNode(node);
  GlobalCallstackTrie::DecrementNode(node);
  auto it = callstack_allocations_.find(node);
  if (it == callstack_allocations_.end()) {
    return 0;
  }
  const CallstackAllocations& alloc = it->second;
  return alloc.value.retain_max.max;
}


}  // namespace profiling
}  // namespace perfetto
