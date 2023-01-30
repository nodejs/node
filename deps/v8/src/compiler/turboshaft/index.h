// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INDEX_H_
#define V8_COMPILER_TURBOSHAFT_INDEX_H_

#include <cstddef>
#include <type_traits>

#include "src/base/logging.h"
#include "src/compiler/turboshaft/fast-hash.h"

namespace v8::internal::compiler::turboshaft {
// Operations are stored in possibly muliple sequential storage slots.
using OperationStorageSlot = std::aligned_storage_t<8, 8>;
// Operations occupy at least 2 slots, therefore we assign one id per two slots.
constexpr size_t kSlotsPerId = 2;

// `OpIndex` is an offset from the beginning of the operations buffer.
// Compared to `Operation*`, it is more memory efficient (32bit) and stable when
// the operations buffer is re-allocated.
class OpIndex {
 public:
  explicit constexpr OpIndex(uint32_t offset) : offset_(offset) {
    DCHECK_EQ(offset % sizeof(OperationStorageSlot), 0);
  }
  constexpr OpIndex() : offset_(std::numeric_limits<uint32_t>::max()) {}

  uint32_t id() const {
    // Operations are stored at an offset that's a multiple of
    // `sizeof(OperationStorageSlot)`. In addition, an operation occupies at
    // least `kSlotsPerId` many `OperationSlot`s. Therefore, we can assign id's
    // by dividing by `kSlotsPerId`. A compact id space is important, because it
    // makes side-tables smaller.
    DCHECK_EQ(offset_ % sizeof(OperationStorageSlot), 0);
    return offset_ / sizeof(OperationStorageSlot) / kSlotsPerId;
  }
  uint32_t offset() const {
    DCHECK_EQ(offset_ % sizeof(OperationStorageSlot), 0);
    return offset_;
  }

  bool valid() const { return *this != Invalid(); }

  static constexpr OpIndex Invalid() { return OpIndex(); }

  // Encode a sea-of-nodes node id in the `OpIndex` type.
  // Only used for node origins that actually point to sea-of-nodes graph nodes.
  static OpIndex EncodeTurbofanNodeId(uint32_t id) {
    OpIndex result = OpIndex(id * sizeof(OperationStorageSlot));
    result.offset_ += kTurbofanNodeIdFlag;
    return result;
  }
  uint32_t DecodeTurbofanNodeId() const {
    DCHECK(IsTurbofanNodeId());
    return offset_ / sizeof(OperationStorageSlot);
  }
  bool IsTurbofanNodeId() const {
    return offset_ % sizeof(OperationStorageSlot) == kTurbofanNodeIdFlag;
  }

  bool operator==(OpIndex other) const { return offset_ == other.offset_; }
  bool operator!=(OpIndex other) const { return offset_ != other.offset_; }
  bool operator<(OpIndex other) const { return offset_ < other.offset_; }
  bool operator>(OpIndex other) const { return offset_ > other.offset_; }
  bool operator<=(OpIndex other) const { return offset_ <= other.offset_; }
  bool operator>=(OpIndex other) const { return offset_ >= other.offset_; }

 private:
  uint32_t offset_;

  static constexpr uint32_t kTurbofanNodeIdFlag = 1;
};

std::ostream& operator<<(std::ostream& os, OpIndex idx);

template <>
struct fast_hash<OpIndex> {
  V8_INLINE size_t operator()(OpIndex op) const { return op.id(); }
};

V8_INLINE size_t hash_value(OpIndex op) { return base::hash_value(op.id()); }

// `BlockIndex` is the index of a bound block.
// A dominating block always has a smaller index.
// It corresponds to the ordering of basic blocks in the operations buffer.
class BlockIndex {
 public:
  explicit constexpr BlockIndex(uint32_t id) : id_(id) {}
  constexpr BlockIndex() : id_(std::numeric_limits<uint32_t>::max()) {}

  uint32_t id() const { return id_; }
  bool valid() const { return *this != Invalid(); }

  static constexpr BlockIndex Invalid() { return BlockIndex(); }

  bool operator==(BlockIndex other) const { return id_ == other.id_; }
  bool operator!=(BlockIndex other) const { return id_ != other.id_; }
  bool operator<(BlockIndex other) const { return id_ < other.id_; }
  bool operator>(BlockIndex other) const { return id_ > other.id_; }
  bool operator<=(BlockIndex other) const { return id_ <= other.id_; }
  bool operator>=(BlockIndex other) const { return id_ >= other.id_; }

 private:
  uint32_t id_;
};

template <>
struct fast_hash<BlockIndex> {
  V8_INLINE size_t operator()(BlockIndex op) const { return op.id(); }
};

V8_INLINE size_t hash_value(BlockIndex op) { return base::hash_value(op.id()); }

std::ostream& operator<<(std::ostream& os, BlockIndex b);

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_INDEX_H_
