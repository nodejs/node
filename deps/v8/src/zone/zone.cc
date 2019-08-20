// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone.h"

#include <cstring>

#include "src/init/v8.h"
#include "src/sanitizer/asan.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

namespace {

#ifdef V8_USE_ADDRESS_SANITIZER

constexpr size_t kASanRedzoneBytes = 24;  // Must be a multiple of 8.

#else  // !V8_USE_ADDRESS_SANITIZER

constexpr size_t kASanRedzoneBytes = 0;

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

  DCHECK_EQ(segment_bytes_allocated_, 0);
}

void* Zone::AsanNew(size_t size) {
  CHECK(!sealed_);

  // Round up the requested size to fit the alignment.
  size = RoundUp(size, kAlignmentInBytes);

  // Check if the requested size is available without expanding.
  Address result = position_;

  const size_t size_with_redzone = size + kASanRedzoneBytes;
  DCHECK_LE(position_, limit_);
  if (size_with_redzone > limit_ - position_) {
    result = NewExpand(size_with_redzone);
  } else {
    position_ += size_with_redzone;
  }

  Address redzone_position = result + size;
  DCHECK_EQ(redzone_position + kASanRedzoneBytes, position_);
  ASAN_POISON_MEMORY_REGION(reinterpret_cast<void*>(redzone_position),
                            kASanRedzoneBytes);

  // Check that the result has the proper alignment and return it.
  DCHECK(IsAligned(result, kAlignmentInBytes));
  return reinterpret_cast<void*>(result);
}

void Zone::ReleaseMemory() {
  allocator_->ZoneDestruction(this);
  DeleteAll();
  allocator_->ZoneCreation(this);
}

void Zone::DeleteAll() {
  // Traverse the chained list of segments and return them all to the allocator.
  for (Segment* current = segment_head_; current;) {
    Segment* next = current->next();
    size_t size = current->total_size();

    // Un-poison the segment content so we can re-use or zap it later.
    ASAN_UNPOISON_MEMORY_REGION(reinterpret_cast<void*>(current->start()),
                                current->capacity());

    segment_bytes_allocated_ -= size;
    allocator_->ReturnSegment(current);
    current = next;
  }

  position_ = limit_ = 0;
  allocation_size_ = 0;
  segment_head_ = nullptr;
}

// Creates a new segment, sets its size, and pushes it to the front
// of the segment chain. Returns the new segment.
Segment* Zone::NewSegment(size_t requested_size) {
  Segment* result = allocator_->AllocateSegment(requested_size);
  if (!result) return nullptr;
  DCHECK_GE(result->total_size(), requested_size);
  segment_bytes_allocated_ += result->total_size();
  result->set_zone(this);
  result->set_next(segment_head_);
  segment_head_ = result;
  return result;
}

Address Zone::NewExpand(size_t size) {
  // Make sure the requested size is already properly aligned and that
  // there isn't enough room in the Zone to satisfy the request.
  DCHECK_EQ(size, RoundDown(size, kAlignmentInBytes));
  DCHECK(limit_ - position_ < size);

  // Commit the allocation_size_ of segment_head_ if any.
  allocation_size_ = allocation_size();
  // Compute the new segment size. We use a 'high water mark'
  // strategy, where we increase the segment size every time we expand
  // except that we employ a maximum segment size when we delete. This
  // is to avoid excessive malloc() and free() overhead.
  Segment* head = segment_head_;
  const size_t old_size = head ? head->total_size() : 0;
  static const size_t kSegmentOverhead = sizeof(Segment) + kAlignmentInBytes;
  const size_t new_size_no_overhead = size + (old_size << 1);
  size_t new_size = kSegmentOverhead + new_size_no_overhead;
  const size_t min_new_size = kSegmentOverhead + size;
  // Guard against integer overflow.
  if (new_size_no_overhead < size || new_size < kSegmentOverhead) {
    V8::FatalProcessOutOfMemory(nullptr, "Zone");
    return kNullAddress;
  }
  if (new_size < kMinimumSegmentSize) {
    new_size = kMinimumSegmentSize;
  } else if (new_size >= kMaximumSegmentSize) {
    // Limit the size of new segments to avoid growing the segment size
    // exponentially, thus putting pressure on contiguous virtual address space.
    // All the while making sure to allocate a segment large enough to hold the
    // requested size.
    new_size = Max(min_new_size, kMaximumSegmentSize);
  }
  if (new_size > INT_MAX) {
    V8::FatalProcessOutOfMemory(nullptr, "Zone");
    return kNullAddress;
  }
  Segment* segment = NewSegment(new_size);
  if (segment == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "Zone");
    return kNullAddress;
  }

  // Recompute 'top' and 'limit' based on the new segment.
  Address result = RoundUp(segment->start(), kAlignmentInBytes);
  position_ = result + size;
  // Check for address overflow.
  // (Should not happen since the segment is guaranteed to accommodate
  // size bytes + header and alignment padding)
  DCHECK(position_ >= result);
  limit_ = segment->end();
  DCHECK(position_ <= limit_);
  return result;
}

}  // namespace internal
}  // namespace v8
