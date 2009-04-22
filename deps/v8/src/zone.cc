// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "zone-inl.h"

namespace v8 { namespace internal {


Address Zone::position_ = 0;
Address Zone::limit_ = 0;
int Zone::zone_excess_limit_ = 256 * MB;
int Zone::segment_bytes_allocated_ = 0;

bool AssertNoZoneAllocation::allow_allocation_ = true;

int ZoneScope::nesting_ = 0;

// Segments represent chunks of memory: They have starting address
// (encoded in the this pointer) and a size in bytes. Segments are
// chained together forming a LIFO structure with the newest segment
// available as Segment::head(). Segments are allocated using malloc()
// and de-allocated using free().

class Segment {
 public:
  Segment* next() const { return next_; }
  void clear_next() { next_ = NULL; }

  int size() const { return size_; }
  int capacity() const { return size_ - sizeof(Segment); }

  Address start() const { return address(sizeof(Segment)); }
  Address end() const { return address(size_); }

  static Segment* head() { return head_; }
  static void set_head(Segment* head) { head_ = head; }

  // Creates a new segment, sets it size, and pushes it to the front
  // of the segment chain. Returns the new segment.
  static Segment* New(int size) {
    Segment* result = reinterpret_cast<Segment*>(Malloced::New(size));
    Zone::adjust_segment_bytes_allocated(size);
    if (result != NULL) {
      result->next_ = head_;
      result->size_ = size;
      head_ = result;
    }
    return result;
  }

  // Deletes the given segment. Does not touch the segment chain.
  static void Delete(Segment* segment, int size) {
    Zone::adjust_segment_bytes_allocated(-size);
    Malloced::Delete(segment);
  }

  static int bytes_allocated() { return bytes_allocated_; }

 private:
  // Computes the address of the nth byte in this segment.
  Address address(int n) const {
    return Address(this) + n;
  }

  static Segment* head_;
  static int bytes_allocated_;
  Segment* next_;
  int size_;
};


Segment* Segment::head_ = NULL;
int Segment::bytes_allocated_ = 0;


void Zone::DeleteAll() {
#ifdef DEBUG
  // Constant byte value used for zapping dead memory in debug mode.
  static const unsigned char kZapDeadByte = 0xcd;
#endif

  // Find a segment with a suitable size to keep around.
  Segment* keep = Segment::head();
  while (keep != NULL && keep->size() > kMaximumKeptSegmentSize) {
    keep = keep->next();
  }

  // Traverse the chained list of segments, zapping (in debug mode)
  // and freeing every segment except the one we wish to keep.
  Segment* current = Segment::head();
  while (current != NULL) {
    Segment* next = current->next();
    if (current == keep) {
      // Unlink the segment we wish to keep from the list.
      current->clear_next();
    } else {
      int size = current->size();
#ifdef DEBUG
      // Zap the entire current segment (including the header).
      memset(current, kZapDeadByte, size);
#endif
      Segment::Delete(current, size);
    }
    current = next;
  }

  // If we have found a segment we want to keep, we must recompute the
  // variables 'position' and 'limit' to prepare for future allocate
  // attempts. Otherwise, we must clear the position and limit to
  // force a new segment to be allocated on demand.
  if (keep != NULL) {
    Address start = keep->start();
    position_ = RoundUp(start, kAlignment);
    limit_ = keep->end();
#ifdef DEBUG
    // Zap the contents of the kept segment (but not the header).
    memset(start, kZapDeadByte, keep->capacity());
#endif
  } else {
    position_ = limit_ = 0;
  }

  // Update the head segment to be the kept segment (if any).
  Segment::set_head(keep);
}


Address Zone::NewExpand(int size) {
  // Make sure the requested size is already properly aligned and that
  // there isn't enough room in the Zone to satisfy the request.
  ASSERT(size == RoundDown(size, kAlignment));
  ASSERT(position_ + size > limit_);

  // Compute the new segment size. We use a 'high water mark'
  // strategy, where we increase the segment size every time we expand
  // except that we employ a maximum segment size when we delete. This
  // is to avoid excessive malloc() and free() overhead.
  Segment* head = Segment::head();
  int old_size = (head == NULL) ? 0 : head->size();
  static const int kSegmentOverhead = sizeof(Segment) + kAlignment;
  int new_size = kSegmentOverhead + size + (old_size << 1);
  if (new_size < kMinimumSegmentSize) {
    new_size = kMinimumSegmentSize;
  } else if (new_size > kMaximumSegmentSize) {
    // Limit the size of new segments to avoid growing the segment size
    // exponentially, thus putting pressure on contiguous virtual address space.
    // All the while making sure to allocate a segment large enough to hold the
    // requested size.
    new_size = Max(kSegmentOverhead + size, kMaximumSegmentSize);
  }
  Segment* segment = Segment::New(new_size);
  if (segment == NULL) V8::FatalProcessOutOfMemory("Zone");

  // Recompute 'top' and 'limit' based on the new segment.
  Address result = RoundUp(segment->start(), kAlignment);
  position_ = result + size;
  limit_ = segment->end();
  ASSERT(position_ <= limit_);
  return result;
}


} }  // namespace v8::internal
