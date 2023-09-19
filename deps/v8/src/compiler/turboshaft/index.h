// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INDEX_H_
#define V8_COMPILER_TURBOSHAFT_INDEX_H_

#include <cstddef>
#include <type_traits>

#include "src/base/logging.h"
#include "src/codegen/tnode.h"
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

// Dummy value for abstract representation classes that don't have a
// RegisterRepresentation.
struct nullrep_t {};
constexpr nullrep_t nullrep;

// Abstract tag classes for V<>.
struct Any {};

template <size_t Bits>
struct WordWithBits : public Any {
  static constexpr int bits = Bits;
  static_assert(Bits == 32 || Bits == 64);
};

using Word32 = WordWithBits<32>;
using Word64 = WordWithBits<64>;
using WordPtr = std::conditional_t<Is64(), Word64, Word32>;

template <size_t Bits>
struct FloatWithBits : public Any {  // FloatAny {
  static constexpr int bits = Bits;
  static_assert(Bits == 32 || Bits == 64);
};

using Float32 = FloatWithBits<32>;
using Float64 = FloatWithBits<64>;

// TODO(nicohartmann@): Replace all uses of `V<Tagged>` by `V<Object>`.
using Tagged = Object;

struct Compressed : public Any {};

// Traits classes `v_traits<T>` to provide additional T-specific information for
// V<T> and ConstOrV<T>. If you need to provide non-default conversion behavior
// for a specific type, specialize the corresponding v_traits<>.
template <typename T, typename = void>
struct v_traits;

template <>
struct v_traits<Any> {
  static constexpr bool is_abstract_tag = true;
  static constexpr auto rep = nullrep;
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return true;
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_same_v<U, Any>> {};
};

template <>
struct v_traits<Compressed> {
  static constexpr bool is_abstract_tag = true;
  static constexpr auto rep = nullrep;
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Compressed();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_base_of_v<U, Compressed>> {};
};

template <>
struct v_traits<Word32> {
  static constexpr bool is_abstract_tag = true;
  static constexpr WordRepresentation rep = WordRepresentation::Word32();
  using constexpr_type = uint32_t;
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Word32();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_base_of_v<U, Word32>> {};
};

template <>
struct v_traits<Word64> {
  static constexpr bool is_abstract_tag = true;
  static constexpr WordRepresentation rep = WordRepresentation::Word64();
  using constexpr_type = uint64_t;
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Word64();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_base_of_v<U, Word64> ||
                           std::is_same_v<U, Word32>> {};
};

template <>
struct v_traits<Float32> {
  static constexpr bool is_abstract_tag = true;
  static constexpr FloatRepresentation rep = FloatRepresentation::Float32();
  using constexpr_type = float;
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Float32();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_base_of_v<U, Float32>> {};
};

template <>
struct v_traits<Float64> {
  static constexpr bool is_abstract_tag = true;
  static constexpr FloatRepresentation rep = FloatRepresentation::Float64();
  using constexpr_type = double;
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Float64();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_base_of_v<U, Float64>> {};
};

template <typename T>
struct v_traits<T, typename std::enable_if_t<std::is_base_of_v<Object, T>>> {
  static constexpr bool is_abstract_tag = false;
  static constexpr auto rep = RegisterRepresentation::Tagged();
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Tagged();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<std::is_base_of_v<U, T> || std::is_same_v<U, Any> ||
                           is_subtype<T, U>::value> {};
};

template <typename T1, typename T2>
struct v_traits<UnionT<T1, T2>,
                typename std::enable_if_t<std::is_base_of_v<Object, T1> &&
                                          std::is_base_of_v<Object, T2>>> {
  static constexpr bool is_abstract_tag = false;
  static constexpr auto rep = RegisterRepresentation::Tagged();
  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return rep == RegisterRepresentation::Tagged();
  }

  template <typename U>
  struct implicitly_convertible_to
      : std::bool_constant<
            (std::is_base_of_v<U, T1> && std::is_base_of_v<U, T2>) ||
            std::is_same_v<U, Any> || is_subtype<UnionT<T1, T2>, U>::value> {};
};

// V<> represents an SSA-value that is parameterized with the type of the value.
// Types from the `Object` hierarchy can be provided as well as the abstract
// representation classes (`Word32`, ...) defined above.
// Prefer using V<> instead of a plain OpIndex where possible.
template <typename T>
class V : public OpIndex {
 public:
  using type = T;
  static constexpr auto rep = v_traits<type>::rep;
  constexpr V() : OpIndex() {}

  // V<T> is implicitly constructible from plain OpIndex.
  template <typename U, typename = std::enable_if_t<std::is_same_v<U, OpIndex>>>
  V(U index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  // V<T> is implicitly constructible from V<U> iff
  // `v_traits<U>::implicitly_convertible_to<T>::value`. This is typically the
  // case if T == U or T is a subclass of U. Different types may specify
  // different conversion rules in the corresponding `v_traits` when necessary.
  template <typename U, typename = std::enable_if_t<v_traits<
                            U>::template implicitly_convertible_to<T>::value>>
  V(V<U> index) : OpIndex(index) {}  // NOLINT(runtime/explicit)

  template <typename U>
  static V<T> Cast(V<U> index) {
    return V<T>(OpIndex{index});
  }

  static constexpr bool allows_representation(RegisterRepresentation rep) {
    return v_traits<T>::allows_representation(rep);
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
// NOTICE: `ConstOrV<T>` can only be used if `v_traits<T>` provides a
// `constexpr_type`.
template <typename T, typename C = typename v_traits<T>::constexpr_type>
class ConstOrV {
 public:
  using type = T;
  using constant_type = C;

  ConstOrV(constant_type value)  // NOLINT(runtime/explicit)
      : constant_value_(value), value_() {}

  // ConstOrV<T> is implicitly constructible from plain OpIndex.
  template <typename U, typename = std::enable_if_t<std::is_same_v<U, OpIndex>>>
  ConstOrV(U index)  // NOLINT(runtime/explicit)
      : constant_value_(), value_(index) {}

  // ConstOrV<T> is implicitly constructible from V<U> iff V<T> is
  // constructible from V<U>.
  template <typename U,
            typename = std::enable_if_t<std::is_constructible_v<V<T>, V<U>>>>
  ConstOrV(V<U> index)  // NOLINT(runtime/explicit)
      : constant_value_(), value_(index) {}

  bool is_constant() const { return constant_value_.has_value(); }
  constant_type constant_value() const {
    DCHECK(is_constant());
    return *constant_value_;
  }
  V<type> value() const {
    DCHECK(!is_constant());
    return value_;
  }

 private:
  base::Optional<constant_type> constant_value_;
  V<type> value_;
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
