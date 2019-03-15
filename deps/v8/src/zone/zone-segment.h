// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_SEGMENT_H_
#define V8_ZONE_ZONE_SEGMENT_H_

#include "src/v8.h"

// Segments represent chunks of memory: They have starting address
// (encoded in the this pointer) and a size in bytes. Segments are
// chained together forming a LIFO structure with the newest segment
// available as segment_head_. Segments are allocated using malloc()
// and de-allocated using free().
namespace v8 {
namespace internal {

// Forward declarations.
class AccountingAllocator;
class Zone;

class Segment {
 public:
  Zone* zone() const { return zone_; }
  void set_zone(Zone* const zone) { zone_ = zone; }

  Segment* next() const { return next_; }
  void set_next(Segment* const next) { next_ = next; }

  // {total_size} returns the allocated size including the bookkeeping bytes of
  // the {Segment}.
  size_t total_size() const { return size_; }

  // {capacity} returns the number of storage bytes in this {Segment}, i.e.
  // {end() - start()}.
  size_t capacity() const { return size_ - sizeof(Segment); }

  Address start() const { return address(sizeof(Segment)); }
  Address end() const { return address(size_); }

  // Zap the contents of the segment (but not the header).
  void ZapContents();
  // Zaps the header and makes the segment unusable this way.
  void ZapHeader();

 private:
  // Segments are only created by the AccountingAllocator.
  friend class AccountingAllocator;

  explicit Segment(size_t size) : size_(size) {}

#ifdef DEBUG
  // Constant byte value used for zapping dead memory in debug mode.
  static const unsigned char kZapDeadByte = 0xcd;
#endif

  // Computes the address of the nth byte in this segment.
  Address address(size_t n) const {
    return reinterpret_cast<Address>(this) + n;
  }

  Zone* zone_ = nullptr;
  Segment* next_ = nullptr;
  const size_t size_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_SEGMENT_H_
