// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_OBJECT_HEADER_H_
#define V8_HEAP_CPPGC_HEAP_OBJECT_HEADER_H_

#include <stdint.h>

#include <atomic>

#include "include/cppgc/internal/gc-info.h"
#include "src/base/bit-field.h"
#include "src/heap/cppgc/globals.h"

namespace cppgc {
namespace internal {

// HeapObjectHeader contains meta data per object and is prepended to each
// object.
//
// +-----------------+------+------------------------------------------+
// | name            | bits |                                          |
// +-----------------+------+------------------------------------------+
// | padding         |   32 | Only present on 64-bit platform.         |
// +-----------------+------+------------------------------------------+
// | GCInfoIndex     |   14 |                                          |
// | unused          |    1 |                                          |
// | in construction |    1 | In construction encoded as |false|.      |
// +-----------------+------+------------------------------------------+
// | size            |   14 | 17 bits because allocations are aligned. |
// | unused          |    1 |                                          |
// | mark bit        |    1 |                                          |
// +-----------------+------+------------------------------------------+
//
// Notes:
// - See |GCInfoTable| for constraints on GCInfoIndex.
// - |size| for regular objects is encoded with 14 bits but can actually
//   represent sizes up to |kBlinkPageSize| (2^17) because allocations are
//   always 8 byte aligned (see kAllocationGranularity).
// - |size| for large objects is encoded as 0. The size of a large object is
//   stored in |LargeObjectPage::PayloadSize()|.
// - |mark bit| and |in construction| bits are located in separate 16-bit halves
//    to allow potentially accessing them non-atomically.
class HeapObjectHeader {
 public:
  enum class AccessMode : uint8_t { kNonAtomic, kAtomic };

  static constexpr size_t kSizeLog2 = 17;
  static constexpr size_t kMaxSize = (size_t{1} << kSizeLog2) - 1;
  static constexpr uint16_t kLargeObjectSizeInHeader = 0;

  inline static HeapObjectHeader& FromPayload(void* address);
  inline static const HeapObjectHeader& FromPayload(const void* address);

  inline HeapObjectHeader(size_t size, GCInfoIndex gc_info_index);

  // The payload starts directly after the HeapObjectHeader.
  inline Address Payload() const;

  template <AccessMode mode = AccessMode::kNonAtomic>
  inline GCInfoIndex GetGCInfoIndex() const;

  template <AccessMode mode = AccessMode::kNonAtomic>
  inline size_t GetSize() const;
  inline void SetSize(size_t size);

  template <AccessMode mode = AccessMode::kNonAtomic>
  inline bool IsLargeObject() const;

  template <AccessMode = AccessMode::kNonAtomic>
  bool IsInConstruction() const;
  inline void MarkAsFullyConstructed();
  // Use MarkObjectAsFullyConstructed() to mark an object as being constructed.

  template <AccessMode = AccessMode::kNonAtomic>
  bool IsMarked() const;
  template <AccessMode = AccessMode::kNonAtomic>
  void Unmark();
  inline bool TryMarkAtomic();

  template <AccessMode = AccessMode::kNonAtomic>
  bool IsFree() const;

  inline bool IsFinalizable() const;
  void Finalize();

 private:
  enum class EncodedHalf : uint8_t { kLow, kHigh };

  // Used in |encoded_high_|.
  using FullyConstructedField = v8::base::BitField16<bool, 0, 1>;
  using UnusedField1 = FullyConstructedField::Next<bool, 1>;
  using GCInfoIndexField = UnusedField1::Next<GCInfoIndex, 14>;
  // Used in |encoded_low_|.
  using MarkBitField = v8::base::BitField16<bool, 0, 1>;
  using UnusedField2 = MarkBitField::Next<bool, 1>;
  using SizeField = void;  // Use EncodeSize/DecodeSize instead.

  static constexpr size_t DecodeSize(uint16_t encoded) {
    // Essentially, gets optimized to << 1.
    using SizeField = UnusedField2::Next<size_t, 14>;
    return SizeField::decode(encoded) * kAllocationGranularity;
  }

  static constexpr uint16_t EncodeSize(size_t size) {
    // Essentially, gets optimized to >> 1.
    using SizeField = UnusedField2::Next<size_t, 14>;
    return SizeField::encode(size / kAllocationGranularity);
  }

  V8_EXPORT_PRIVATE void CheckApiConstants();

  template <AccessMode, EncodedHalf part,
            std::memory_order memory_order = std::memory_order_seq_cst>
  inline uint16_t LoadEncoded() const;
  template <AccessMode mode, EncodedHalf part,
            std::memory_order memory_order = std::memory_order_seq_cst>
  inline void StoreEncoded(uint16_t bits, uint16_t mask);

#if defined(V8_TARGET_ARCH_64_BIT)
  uint32_t padding_ = 0;
#endif  // defined(V8_TARGET_ARCH_64_BIT)
  uint16_t encoded_high_;
  uint16_t encoded_low_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_OBJECT_HEADER_H_
