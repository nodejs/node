// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_COMPRESSION_H_
#define V8_ZONE_ZONE_COMPRESSION_H_

#include "src/base/bits.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// This struct provides untyped implementation of zone compression scheme.
//
// The compression scheme relies on the following assumptions:
// 1) all zones containing compressed pointers are allocated in the same "zone
//    cage" of kReservationSize size and kReservationAlignment-aligned.
//    Attempt to compress pointer to an object stored outside of the "cage"
//    will silently succeed but it will later produce wrong result after
//    decompression.
// 2) compression is just a masking away bits above kReservationAlignment.
// 3) nullptr is compressed to 0, thus there must be no valid objects allocated
//    at the beginning of the "zone cage". Ideally, the first page of the cage
//    should be unmapped in order to catch attempts to use decompressed nullptr
//    value earlier.
// 4) decompression requires "zone cage" address value, which is computed on
//    the fly from an arbitrary address pointing somewhere to the "zone cage".
// 5) decompression requires special casing for nullptr.
struct ZoneCompression {
  static const size_t kReservationSize = size_t{2} * GB;
  static const size_t kReservationAlignment =
      COMPRESS_ZONES_BOOL ? size_t{4} * GB : 1;

  static_assert(base::bits::IsPowerOfTwo(kReservationAlignment),
                "Bad zone alignment");

  static const size_t kOffsetMask = kReservationAlignment - 1;

  inline static Address base_of(const void* zone_pointer) {
    return reinterpret_cast<Address>(zone_pointer) & ~kOffsetMask;
  }

  inline static bool CheckSameBase(const void* p1, const void* p2) {
    if (p1 == nullptr || p2 == nullptr) return true;
    CHECK_EQ(base_of(p1), base_of(p2));
    return true;
  }

  inline static uint32_t Compress(const void* value) {
    Address raw_value = reinterpret_cast<Address>(value);
    uint32_t compressed_value = static_cast<uint32_t>(raw_value & kOffsetMask);
    DCHECK_IMPLIES(compressed_value == 0, value == nullptr);
    DCHECK_LT(compressed_value, kReservationSize);
    return compressed_value;
  }

  inline static Address Decompress(const void* zone_pointer,
                                   uint32_t compressed_value) {
    if (compressed_value == 0) return kNullAddress;
    return base_of(zone_pointer) + compressed_value;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_COMPRESSION_H_
