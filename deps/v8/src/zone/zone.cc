// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone.h"

#include <cstring>

#include "src/utils.h"
#include "src/v8.h"

#ifdef V8_USE_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif  // V8_USE_ADDRESS_SANITIZER

namespace v8 {
namespace internal {

namespace {

#if V8_USE_ADDRESS_SANITIZER

const size_t kASanRedzoneBytes = 24;  // Must be a multiple of 8.

#else

#define ASAN_POISON_MEMORY_REGION(start, size) \
  do {                                         \
    USE(start);                                \
    USE(size);                                 \
  } while (false)

#define ASAN_UNPOISON_MEMORY_REGION(start, size) \
  do {                                           \
    USE(start);                                  \
    USE(size);                                   \
  } while (false)

const size_t kASanRedzoneBytes = 0;

#endif  // V8_USE_ADDRESS_SANITIZER

}  // namespace

Zone::Zone(AccountingAllocator* allocator, const char* name)
    : allocation_size_(0),
      segment_bytes_allocated_(0),
      position_(0),
      limit_(0),
      allocator_(allocator),
      segment_head_(nullptr),
      name_(name),
      sealed_(false) {
  allocator_->ZoneCreation(this);
}

Zone::~Zone() {
  allocator_->ZoneDestruction(this);

  DeleteAll();

  DCHECK(segment_bytes_allocated_ == 0);
}

void* Zone::New(size_t size) {
  CHECK(!sealed_);

  // Round up the requested size to fit the alignment.
  size = RoundUp(size, kAlignmentInBytes);

  // Check if the requested size is available without expanding.
  Address result = position_;

  const size_t size_with_redzone = size + kASanRedzoneBytes;
  const uintptr_t limit = reinterpret_cast<uintptr_t>(limit_);
  const uintptr_t position = reinterpret_cast<uintptr_t>(position_);
  // position_ > limit_ can be true after the alignment correction above.
  if (limit < position || size_with_redzone > limit - position) {
    result = NewExpand(size_with_redzone);
  } else {
    position_ += size_with_redzone;
  }

  Address redzone_position = result + size;
  DCHECK(redzone_position + kASanRedzoneBytes == position_);
  ASAN_POISON_MEMORY_REGION(redzone_position, kASanRedzoneBytes);

  // Check that the result has the proper alignment and return it.
  DCHECK(IsAddressAligned(result, kAlignmentInBytes, 0));
  allocation_size_ += size;
  return reinterpret_cast<void*>(result);
}

void Zone::DeleteAll() {
  // Traverse the chained list of segments and return them all to the allocator.
  for (Segment* current = segment_head_; current;) {
    Segment* next = current->next();
    size_t size = current->size();

    // Un-poison the segment content so we can re-use or zap it later.
    ASAN_UNPOISON_MEMORY_REGION(current->start(), current->capacity());

    segment_bytes_allocated_ -= size;
    allocator_->ReturnSegment(current);
    current = next;
  }

  position_ = limit_ = 0;
  allocation_size_ = 0;
  segment_head_ = nullptr;
}

// Creates a new segment, sets it size, and pushes it to the front
// of the segment chain. Returns the new segment.
Segment* Zone::NewSegment(size_t requested_size) {
  Segment* result = allocator_->GetSegment(requested_size);
  if (result != nullptr) {
    DCHECK_GE(result->size(), requested_size);
    segment_bytes_allocated_ += result->size();
    result->set_zone(this);
    result->set_next(segment_head_);
    segment_head_ = result;
  }
  return result;
}

Address Zone::NewExpand(size_t size) {
  // Make sure the requested size is already properly aligned and that
  // there isn't enough room in the Zone to satisfy the request.
  DCHECK_EQ(size, RoundDown(size, kAlignmentInBytes));
  DCHECK(limit_ < position_ ||
         reinterpret_cast<uintptr_t>(limit_) -
                 reinterpret_cast<uintptr_t>(position_) <
             size);

  // Compute the new segment size. We use a 'high water mark'
  // strategy, where we increase the segment size every time we expand
  // except that we employ a maximum segment size when we delete. This
  // is to avoid excessive malloc() and free() overhead.
  Segment* head = segment_head_;
  const size_t old_size = (head == nullptr) ? 0 : head->size();
  static const size_t kSegmentOverhead = sizeof(Segment) + kAlignmentInBytes;
  const size_t new_size_no_overhead = size + (old_size << 1);
  size_t new_size = kSegmentOverhead + new_size_no_overhead;
  const size_t min_new_size = kSegmentOverhead + size;
  // Guard against integer overflow.
  if (new_size_no_overhead < size || new_size < kSegmentOverhead) {
    V8::FatalProcessOutOfMemory("Zone");
    return nullptr;
  }
  if (new_size < kMinimumSegmentSize) {
    new_size = kMinimumSegmentSize;
  } else if (new_size > kMaximumSegmentSize) {
    // Limit the size of new segments to avoid growing the segment size
    // exponentially, thus putting pressure on contiguous virtual address space.
    // All the while making sure to allocate a segment large enough to hold the
    // requested size.
    new_size = Max(min_new_size, kMaximumSegmentSize);
  }
  if (new_size > INT_MAX) {
    V8::FatalProcessOutOfMemory("Zone");
    return nullptr;
  }
  Segment* segment = NewSegment(new_size);
  if (segment == nullptr) {
    V8::FatalProcessOutOfMemory("Zone");
    return nullptr;
  }

  // Recompute 'top' and 'limit' based on the new segment.
  Address result = RoundUp(segment->start(), kAlignmentInBytes);
  position_ = result + size;
  // Check for address overflow.
  // (Should not happen since the segment is guaranteed to accomodate
  // size bytes + header and alignment padding)
  DCHECK(reinterpret_cast<uintptr_t>(position_) >=
         reinterpret_cast<uintptr_t>(result));
  limit_ = segment->end();
  DCHECK(position_ <= limit_);
  return result;
}

}  // namespace internal
}  // namespace v8
