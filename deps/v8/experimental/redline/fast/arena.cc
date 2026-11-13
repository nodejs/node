// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/arena.h"

#include <algorithm>
#include <cstring>

namespace redline {
namespace fast {

namespace {

// Segments of kSegmentSize released by dead arenas, kept for reuse by the
// thread that freed them. Bounded so that a thread which only frees (e.g. the
// coordinator evicting corpus programs) cannot hoard memory.
class SegmentPool {
 public:
  static constexpr size_t kMaxSegments = 64;  // 4 MB per thread

  char* Take() {
    if (free_.empty()) {
      return new char[Arena::kSegmentSize];
    }
    char* segment = free_.back();
    free_.pop_back();
    return segment;
  }

  void Give(char* segment) {
    if (free_.size() >= kMaxSegments) {
      delete[] segment;
      return;
    }
    free_.push_back(segment);
  }

 private:
  std::vector<char*> free_;
};

SegmentPool& ThreadPool() {
  // Deliberately leaked: an arena with static storage duration may be
  // destroyed after the thread's thread_local objects, and must still find
  // a live pool. Threads are long-lived in redline, so nothing accumulates.
  thread_local SegmentPool* pool = new SegmentPool;
  return *pool;
}

}  // namespace

Arena::~Arena() {
  for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it) {
    it->destroy(it->object);
  }
  for (const Segment& segment : segments_) {
    if (segment.size == kSegmentSize) {
      ThreadPool().Give(segment.memory);
    } else {
      delete[] segment.memory;
    }
  }
}

void* Arena::Allocate(size_t size, size_t alignment) {
  if (size == 0) {
    size = 1;
  }
  uintptr_t aligned = (reinterpret_cast<uintptr_t>(cursor_) + alignment - 1) &
                      ~(static_cast<uintptr_t>(alignment) - 1);
  if (cursor_ == nullptr ||
      aligned + size > reinterpret_cast<uintptr_t>(limit_)) {
    Segment segment{nullptr, 0};
    if (size + alignment <= kSegmentSize) {
      segment = {ThreadPool().Take(), kSegmentSize};
    } else {
      segment = {new char[size + alignment], size + alignment};
    }
    segments_.push_back(segment);
    cursor_ = segment.memory;
    limit_ = cursor_ + segment.size;
    aligned = (reinterpret_cast<uintptr_t>(cursor_) + alignment - 1) &
              ~(static_cast<uintptr_t>(alignment) - 1);
  }
  // Pointer arithmetic on an aligned bump allocator: the alignment is
  // computed on the integer value of the cursor, so the round-trip through
  // uintptr_t is intentional.
  cursor_ = reinterpret_cast<char*>(aligned +
                                    size);  // NOLINT(performance-no-int-to-ptr)
  bytes_allocated_ += size;
  return reinterpret_cast<void*>(aligned);  // NOLINT(performance-no-int-to-ptr)
}

std::string_view Arena::CopyString(std::string_view text) {
  if (text.empty()) {
    return std::string_view();
  }
  char* memory = static_cast<char*>(Allocate(text.size(), 1));
  memcpy(memory, text.data(), text.size());
  return std::string_view(memory, text.size());
}

}  // namespace fast
}  // namespace redline
