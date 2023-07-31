// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_SERIALIZER_DESERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_SERIALIZER_DESERIALIZER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace ro {

// Common functionality for RO serialization and deserialization.

// Serializer format:
//
// --------------------------------------------------------------------
// page_content[1..n]      - content of each page
// kReadOnlyRootsTable
// #ifndef V8_STATIC_ROOTS
// read_only_roots[1..n]   - all entries of the ro roots table
// #endif  // V8_STATIC_ROOTS
// kFinalizeReadOnlySpace  - end mark
// --------------------------------------------------------------------
//
// where page_content is:
// ------------------------------------------------------------------
// kPage                   - page begin mark
// segment_content[1..n]   - content of each segment
// kFinalizePage           - page end mark
// ------------------------------------------------------------------
//
// where segment_content is:
// ----------------------------------------------------------------
// kSegment                - segment mark
// offset                  - start of segment rel. to area_start
// size                    - size of segment in bytes
// bytes[1..size]          - content
// #ifndef V8_STATIC_ROOTS
// kRelocateSegment        - segment relocation mark
// tagged_slots_bitfield[] - bitfield of tagged slots
// #endif  // V8_STATIC_ROOTS
// ----------------------------------------------------------------
enum Bytecode {
  kPage,
  kSegment,
  kRelocateSegment,
  kFinalizePage,
  kReadOnlyRootsTable,
  kFinalizeReadOnlySpace,
};
static constexpr int kNumberOfBytecodes =
    static_cast<int>(kFinalizeReadOnlySpace) + 1;

// Like std::vector<bool> but with a known underlying encoding.
class BitSet final {
 public:
  explicit BitSet(size_t size_in_bits)
      : size_in_bits_(size_in_bits),
        data_(new uint8_t[size_in_bytes()]()),
        owns_data_(true) {}

  explicit BitSet(uint8_t* data, size_t size_in_bits)
      : size_in_bits_(size_in_bits), data_(data), owns_data_(false) {}

  ~BitSet() {
    if (owns_data_) delete[] data_;
  }

  bool contains(int i) const {
    DCHECK(0 <= i && i < static_cast<int>(size_in_bits_));
    return (data_[chunk_index(i)] & bit_mask(i)) != 0;
  }

  void set(int i) {
    DCHECK(0 <= i && i < static_cast<int>(size_in_bits_));
    data_[chunk_index(i)] |= bit_mask(i);
  }

  size_t size_in_bits() const { return size_in_bits_; }
  size_t size_in_bytes() const {
    return RoundUp<kBitsPerByte>(size_in_bits_) / kBitsPerByte;
  }

  const uint8_t* data() const { return data_; }

 private:
  static constexpr int kBitsPerChunk = kUInt8Size * kBitsPerByte;
  static constexpr int chunk_index(int i) { return i / kBitsPerChunk; }
  static constexpr int bit_index(int i) { return i % kBitsPerChunk; }
  static constexpr uint32_t bit_mask(int i) { return 1 << bit_index(i); }

  const size_t size_in_bits_;
  uint8_t* const data_;
  const bool owns_data_;
};

// Tagged slots need relocation after deserialization when V8_STATIC_ROOTS is
// disabled.
//
// Note this encoding works for all remaining build configs, in particular for
// all supported kTaggedSize values.
struct EncodedTagged_t {
  static constexpr int kPageIndexBits = 5;  // Max 32 RO pages.
  static constexpr int kOffsetBits = 27;
  static constexpr int kSize = kUInt32Size;

  uint32_t ToUint32() const { return *reinterpret_cast<const uint32_t*>(this); }
  static EncodedTagged_t FromUint32(uint32_t v) {
    return FromAddress(reinterpret_cast<Address>(&v));
  }
  static EncodedTagged_t FromAddress(Address address) {
    return *reinterpret_cast<EncodedTagged_t*>(address);
  }

  int page_index : kPageIndexBits;
  int offset : kOffsetBits;  // Shifted by kTaggedSizeLog2.
};
static_assert(EncodedTagged_t::kSize == sizeof(EncodedTagged_t));

}  // namespace ro
}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_SERIALIZER_DESERIALIZER_H_
