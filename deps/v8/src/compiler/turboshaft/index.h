// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INDEX_H_
#define V8_COMPILER_TURBOSHAFT_INDEX_H_

#include <cstddef>
#include <type_traits>

#include "src/base/logging.h"
#include "src/compiler/turboshaft/fast-hash.h"
#include "src/compiler/turboshaft/representations.h"

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

  uint32_t offset_;

  static constexpr uint32_t kTurbofanNodeIdFlag = 1;
};

std::ostream& operator<<(std::ostream& os, OpIndex idx);

// V<> represents an SSA-value that is parameterized with the values
// representation (see `representations.h` for the different classes to pass as
// the `Rep` argument). Prefer using V<> instead of a plain OpIndex where
// possible.
template <typename Rep>
class V : public OpIndex {
  static_assert(std::is_base_of_v<Any, Rep>,
                "V<> requires a representation tag");

 public:
  constexpr V() : OpIndex() {}

  // V<Rep> is implicitly constructible from plain OpIndex.
  template <typename T, typename = std::enable_if_t<std::is_same_v<T, OpIndex>>>
  V(T index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  // V<Rep> is implicitly constructible from V<R> iff R == Rep or R is a
  // subclass of Rep.
  template <typename R, typename = std::enable_if_t<std::is_base_of_v<Rep, R>>>
  V(V<R> index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  template <typename R>
  static V<Rep> Cast(V<R> index) {
    return V<Rep>(OpIndex{index});
  }
};

// V<Word32> is a specialization of V<> for Word32 representation in order to
// allow implicit conversion from V<Word64>, which is valid in Turboshaft.
template <>
class V<Word32> : public OpIndex {
 public:
  constexpr V() : OpIndex() {}

  template <typename T, typename = std::enable_if_t<std::is_same_v<T, OpIndex>>>
  V(T index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  template <typename R,
            typename = std::enable_if_t<std::is_base_of_v<Word32, R>>>
  V(V<R> index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  V(V<Word64> index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  template <typename R>
  static V<Word32> Cast(V<R> index) {
    return V<Word32>(OpIndex{index});
  }
};

// ConstOrV<> is a generalization of V<> that allows constexpr values
// (constants) to be passed implicitly. This allows reducers to write things
// like
//
// __ Word32Add(value, 1)
//
// instead of having to write
//
// __ Word32Add(value, __ Word32Constant(1))
//
// which makes overall code more compact and easier to read. Functions need to
// call `resolve` on the assembler in order to convert to V<> (which will then
// construct the corresponding ConstantOp if the given ConstOrV<> holds a
// constexpr value).
// NOTICE: ConstOrV<Rep> can only be used for `Rep`s that provide a
// `constexpr_type`.
template <typename Rep, typename C = typename Rep::constexpr_type>
class ConstOrV {
  static_assert(std::is_base_of_v<Any, Rep>,
                "ConstOrV<> requires a representation tag");

 public:
  using constant_type = C;

  ConstOrV(constant_type value)  // NOLINT(runtime/explicit)
      : constant_value_(value), value_() {}

  // ConstOrV<Rep> is implicitly constructible from plain OpIndex.
  template <typename T, typename = std::enable_if_t<std::is_same_v<T, OpIndex>>>
  ConstOrV(T index)  // NOLINT(runtime/explicit)
      : constant_value_(), value_(index) {}

  // ConstOrV<Rep> is implicitly constructible from V<R> iff V<Rep> is
  // constructible from V<R>.
  template <typename R,
            typename = std::enable_if_t<std::is_constructible_v<V<Rep>, V<R>>>>
  ConstOrV(V<R> index)  // NOLINT(runtime/explicit)
      : constant_value_(), value_(index) {}

  bool is_constant() const { return constant_value_.has_value(); }
  constant_type constant_value() const {
    DCHECK(is_constant());
    return *constant_value_;
  }
  V<Rep> value() const {
    DCHECK(!is_constant());
    return value_;
  }

 private:
  base::Optional<constant_type> constant_value_;
  V<Rep> value_;
};

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
