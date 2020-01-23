// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_TNODE_H_
#define V8_CODEGEN_TNODE_H_

#include "src/codegen/machine-type.h"

namespace v8 {
namespace internal {

class HeapNumber;
class BigInt;
class Object;

namespace compiler {

class Node;

}

struct UntaggedT {};

struct IntegralT : UntaggedT {};

struct WordT : IntegralT {
  static const MachineRepresentation kMachineRepresentation =
      (kSystemPointerSize == 4) ? MachineRepresentation::kWord32
                                : MachineRepresentation::kWord64;
};

struct RawPtrT : WordT {
  static constexpr MachineType kMachineType = MachineType::Pointer();
};

template <class To>
struct RawPtr : RawPtrT {};

struct Word32T : IntegralT {
  static const MachineRepresentation kMachineRepresentation =
      MachineRepresentation::kWord32;
};
struct Int32T : Word32T {
  static constexpr MachineType kMachineType = MachineType::Int32();
};
struct Uint32T : Word32T {
  static constexpr MachineType kMachineType = MachineType::Uint32();
};
struct Int16T : Int32T {
  static constexpr MachineType kMachineType = MachineType::Int16();
};
struct Uint16T : Uint32T, Int32T {
  static constexpr MachineType kMachineType = MachineType::Uint16();
};
struct Int8T : Int16T {
  static constexpr MachineType kMachineType = MachineType::Int8();
};
struct Uint8T : Uint16T, Int16T {
  static constexpr MachineType kMachineType = MachineType::Uint8();
};

struct Word64T : IntegralT {
  static const MachineRepresentation kMachineRepresentation =
      MachineRepresentation::kWord64;
};
struct Int64T : Word64T {
  static constexpr MachineType kMachineType = MachineType::Int64();
};
struct Uint64T : Word64T {
  static constexpr MachineType kMachineType = MachineType::Uint64();
};

struct IntPtrT : WordT {
  static constexpr MachineType kMachineType = MachineType::IntPtr();
};
struct UintPtrT : WordT {
  static constexpr MachineType kMachineType = MachineType::UintPtr();
};

struct Float32T : UntaggedT {
  static const MachineRepresentation kMachineRepresentation =
      MachineRepresentation::kFloat32;
  static constexpr MachineType kMachineType = MachineType::Float32();
};

struct Float64T : UntaggedT {
  static const MachineRepresentation kMachineRepresentation =
      MachineRepresentation::kFloat64;
  static constexpr MachineType kMachineType = MachineType::Float64();
};

#ifdef V8_COMPRESS_POINTERS
using TaggedT = Int32T;
#else
using TaggedT = IntPtrT;
#endif

// Result of a comparison operation.
struct BoolT : Word32T {};

// Value type of a Turbofan node with two results.
template <class T1, class T2>
struct PairT {};

inline constexpr MachineType CommonMachineType(MachineType type1,
                                               MachineType type2) {
  return (type1 == type2) ? type1
                          : ((type1.IsTagged() && type2.IsTagged())
                                 ? MachineType::AnyTagged()
                                 : MachineType::None());
}

template <class Type, class Enable = void>
struct MachineTypeOf {
  static constexpr MachineType value = Type::kMachineType;
};

template <class Type, class Enable>
constexpr MachineType MachineTypeOf<Type, Enable>::value;

template <>
struct MachineTypeOf<Object> {
  static constexpr MachineType value = MachineType::AnyTagged();
};
template <>
struct MachineTypeOf<MaybeObject> {
  static constexpr MachineType value = MachineType::AnyTagged();
};
template <>
struct MachineTypeOf<Smi> {
  static constexpr MachineType value = MachineType::TaggedSigned();
};
template <class HeapObjectSubtype>
struct MachineTypeOf<HeapObjectSubtype,
                     typename std::enable_if<std::is_base_of<
                         HeapObject, HeapObjectSubtype>::value>::type> {
  static constexpr MachineType value = MachineType::TaggedPointer();
};

template <class HeapObjectSubtype>
constexpr MachineType MachineTypeOf<
    HeapObjectSubtype, typename std::enable_if<std::is_base_of<
                           HeapObject, HeapObjectSubtype>::value>::type>::value;

template <class Type, class Enable = void>
struct MachineRepresentationOf {
  static const MachineRepresentation value = Type::kMachineRepresentation;
};
template <class T>
struct MachineRepresentationOf<
    T, typename std::enable_if<std::is_base_of<Object, T>::value>::type> {
  static const MachineRepresentation value =
      MachineTypeOf<T>::value.representation();
};
template <class T>
struct MachineRepresentationOf<
    T, typename std::enable_if<std::is_base_of<MaybeObject, T>::value>::type> {
  static const MachineRepresentation value =
      MachineTypeOf<T>::value.representation();
};
template <>
struct MachineRepresentationOf<ExternalReference> {
  static const MachineRepresentation value = RawPtrT::kMachineRepresentation;
};

template <class T>
struct is_valid_type_tag {
  static const bool value = std::is_base_of<Object, T>::value ||
                            std::is_base_of<UntaggedT, T>::value ||
                            std::is_base_of<MaybeObject, T>::value ||
                            std::is_same<ExternalReference, T>::value;
  static const bool is_tagged = std::is_base_of<Object, T>::value ||
                                std::is_base_of<MaybeObject, T>::value;
};

template <class T1, class T2>
struct is_valid_type_tag<PairT<T1, T2>> {
  static const bool value =
      is_valid_type_tag<T1>::value && is_valid_type_tag<T2>::value;
  static const bool is_tagged = false;
};

template <class T1, class T2>
struct UnionT;

template <class T1, class T2>
struct is_valid_type_tag<UnionT<T1, T2>> {
  static const bool is_tagged =
      is_valid_type_tag<T1>::is_tagged && is_valid_type_tag<T2>::is_tagged;
  static const bool value = is_tagged;
};

template <class T1, class T2>
struct UnionT {
  static constexpr MachineType kMachineType =
      CommonMachineType(MachineTypeOf<T1>::value, MachineTypeOf<T2>::value);
  static const MachineRepresentation kMachineRepresentation =
      kMachineType.representation();
  static_assert(kMachineRepresentation != MachineRepresentation::kNone,
                "no common representation");
  static_assert(is_valid_type_tag<T1>::is_tagged &&
                    is_valid_type_tag<T2>::is_tagged,
                "union types are only possible for tagged values");
};

using AnyTaggedT = UnionT<Object, MaybeObject>;
using Number = UnionT<Smi, HeapNumber>;
using Numeric = UnionT<Number, BigInt>;

// A pointer to a builtin function, used by Torque's function pointers.
using BuiltinPtr = Smi;

class int31_t {
 public:
  int31_t() : value_(0) {}
  int31_t(int value) : value_(value) {  // NOLINT(runtime/explicit)
    DCHECK_EQ((value & 0x80000000) != 0, (value & 0x40000000) != 0);
  }
  int31_t& operator=(int value) {
    DCHECK_EQ((value & 0x80000000) != 0, (value & 0x40000000) != 0);
    value_ = value;
    return *this;
  }
  int32_t value() const { return value_; }
  operator int32_t() const { return value_; }

 private:
  int32_t value_;
};

template <class T, class U>
struct is_subtype {
  static const bool value = std::is_base_of<U, T>::value;
};
template <class T1, class T2, class U>
struct is_subtype<UnionT<T1, T2>, U> {
  static const bool value =
      is_subtype<T1, U>::value && is_subtype<T2, U>::value;
};
template <class T, class U1, class U2>
struct is_subtype<T, UnionT<U1, U2>> {
  static const bool value =
      is_subtype<T, U1>::value || is_subtype<T, U2>::value;
};
template <class T1, class T2, class U1, class U2>
struct is_subtype<UnionT<T1, T2>, UnionT<U1, U2>> {
  static const bool value =
      (is_subtype<T1, U1>::value || is_subtype<T1, U2>::value) &&
      (is_subtype<T2, U1>::value || is_subtype<T2, U2>::value);
};

template <class T, class U>
struct types_have_common_values {
  static const bool value = is_subtype<T, U>::value || is_subtype<U, T>::value;
};
template <class U>
struct types_have_common_values<BoolT, U> {
  static const bool value = types_have_common_values<Word32T, U>::value;
};
template <class U>
struct types_have_common_values<Uint32T, U> {
  static const bool value = types_have_common_values<Word32T, U>::value;
};
template <class U>
struct types_have_common_values<Int32T, U> {
  static const bool value = types_have_common_values<Word32T, U>::value;
};
template <class U>
struct types_have_common_values<Uint64T, U> {
  static const bool value = types_have_common_values<Word64T, U>::value;
};
template <class U>
struct types_have_common_values<Int64T, U> {
  static const bool value = types_have_common_values<Word64T, U>::value;
};
template <class U>
struct types_have_common_values<IntPtrT, U> {
  static const bool value = types_have_common_values<WordT, U>::value;
};
template <class U>
struct types_have_common_values<UintPtrT, U> {
  static const bool value = types_have_common_values<WordT, U>::value;
};
template <class T1, class T2, class U>
struct types_have_common_values<UnionT<T1, T2>, U> {
  static const bool value = types_have_common_values<T1, U>::value ||
                            types_have_common_values<T2, U>::value;
};

template <class T, class U1, class U2>
struct types_have_common_values<T, UnionT<U1, U2>> {
  static const bool value = types_have_common_values<T, U1>::value ||
                            types_have_common_values<T, U2>::value;
};
template <class T1, class T2, class U1, class U2>
struct types_have_common_values<UnionT<T1, T2>, UnionT<U1, U2>> {
  static const bool value = types_have_common_values<T1, U1>::value ||
                            types_have_common_values<T1, U2>::value ||
                            types_have_common_values<T2, U1>::value ||
                            types_have_common_values<T2, U2>::value;
};

template <class T>
struct types_have_common_values<T, MaybeObject> {
  static const bool value = types_have_common_values<T, Object>::value;
};

template <class T>
struct types_have_common_values<MaybeObject, T> {
  static const bool value = types_have_common_values<Object, T>::value;
};

// TNode<T> is an SSA value with the static type tag T, which is one of the
// following:
//   - a subclass of internal::Object represents a tagged type
//   - a subclass of internal::UntaggedT represents an untagged type
//   - ExternalReference
//   - PairT<T1, T2> for an operation returning two values, with types T1
//     and T2
//   - UnionT<T1, T2> represents either a value of type T1 or of type T2.
template <class T>
class TNode {
 public:
  template <class U,
            typename std::enable_if<is_subtype<U, T>::value, int>::type = 0>
  TNode(const TNode<U>& other) : node_(other) {
    LazyTemplateChecks();
  }
  TNode() : TNode(nullptr) {}

  TNode operator=(TNode other) {
    DCHECK_NOT_NULL(other.node_);
    node_ = other.node_;
    return *this;
  }

  bool is_null() { return node_ == nullptr; }

  operator compiler::Node*() const { return node_; }

  static TNode UncheckedCast(compiler::Node* node) { return TNode(node); }

 protected:
  explicit TNode(compiler::Node* node) : node_(node) { LazyTemplateChecks(); }

 private:
  // These checks shouldn't be checked before TNode is actually used.
  void LazyTemplateChecks() {
    static_assert(is_valid_type_tag<T>::value, "invalid type tag");
  }

  compiler::Node* node_;
};

// SloppyTNode<T> is a variant of TNode<T> and allows implicit casts from
// Node*. It is intended for function arguments as long as some call sites
// still use untyped Node* arguments.
// TODO(tebbi): Delete this class once transition is finished.
template <class T>
class SloppyTNode : public TNode<T> {
 public:
  SloppyTNode(compiler::Node* node)  // NOLINT(runtime/explicit)
      : TNode<T>(node) {}
  template <class U, typename std::enable_if<is_subtype<U, T>::value,
                                             int>::type = 0>
  SloppyTNode(const TNode<U>& other)  // NOLINT(runtime/explicit)
      : TNode<T>(other) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_TNODE_H_
