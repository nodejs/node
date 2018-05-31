// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_ASSEMBLER_H_
#define V8_COMPILER_CODE_ASSEMBLER_H_

#include <map>
#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/allocation.h"
#include "src/base/template-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/machine-type.h"
#include "src/objects/data-handler.h"
#include "src/objects/map.h"
#include "src/runtime/runtime.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Callable;
class CallInterfaceDescriptor;
class Isolate;
class JSCollection;
class JSRegExpStringIterator;
class JSWeakCollection;
class JSWeakMap;
class JSWeakSet;
class PromiseCapability;
class PromiseFulfillReactionJobTask;
class PromiseReaction;
class PromiseReactionJobTask;
class PromiseRejectReactionJobTask;
class InterpreterData;
class Factory;
class Zone;

struct UntaggedT {};

struct IntegralT : UntaggedT {};

struct WordT : IntegralT {
  static const MachineRepresentation kMachineRepresentation =
      (kPointerSize == 4) ? MachineRepresentation::kWord32
                          : MachineRepresentation::kWord64;
};

struct RawPtrT : WordT {};

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
struct is_valid_type_tag {
  static const bool value = std::is_base_of<Object, T>::value ||
                            std::is_base_of<UntaggedT, T>::value ||
                            std::is_same<ExternalReference, T>::value;
  static const bool is_tagged = std::is_base_of<Object, T>::value;
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

using Number = UnionT<Smi, HeapNumber>;
using Numeric = UnionT<Number, BigInt>;

#define ENUM_ELEMENT(Name) k##Name,
#define ENUM_STRUCT_ELEMENT(NAME, Name, name) k##Name,
enum class ObjectType {
  kObject,
  OBJECT_TYPE_LIST(ENUM_ELEMENT) HEAP_OBJECT_TYPE_LIST(ENUM_ELEMENT)
      STRUCT_LIST(ENUM_STRUCT_ELEMENT)
};
#undef ENUM_ELEMENT
#undef ENUM_STRUCT_ELEMENT

class AccessCheckNeeded;
class BigIntWrapper;
class ClassBoilerplate;
class BooleanWrapper;
class CompilationCacheTable;
class Constructor;
class Filler;
class InternalizedString;
class JSArgumentsObject;
class JSContextExtensionObject;
class JSError;
class JSSloppyArgumentsObject;
class MapCache;
class MutableHeapNumber;
class NativeContext;
class NumberWrapper;
class ScriptWrapper;
class SloppyArgumentsElements;
class StringWrapper;
class SymbolWrapper;
class Undetectable;
class UniqueName;
class WasmGlobalObject;
class WasmMemoryObject;
class WasmModuleObject;
class WasmTableObject;

template <class T>
struct ObjectTypeOf {};

#define OBJECT_TYPE_CASE(Name)                           \
  template <>                                            \
  struct ObjectTypeOf<Name> {                            \
    static const ObjectType value = ObjectType::k##Name; \
  };
#define OBJECT_TYPE_STRUCT_CASE(NAME, Name, name)        \
  template <>                                            \
  struct ObjectTypeOf<Name> {                            \
    static const ObjectType value = ObjectType::k##Name; \
  };
#define OBJECT_TYPE_TEMPLATE_CASE(Name)                  \
  template <class... Args>                               \
  struct ObjectTypeOf<Name<Args...>> {                   \
    static const ObjectType value = ObjectType::k##Name; \
  };
OBJECT_TYPE_CASE(Object)
OBJECT_TYPE_LIST(OBJECT_TYPE_CASE)
HEAP_OBJECT_ORDINARY_TYPE_LIST(OBJECT_TYPE_CASE)
STRUCT_LIST(OBJECT_TYPE_STRUCT_CASE)
HEAP_OBJECT_TEMPLATE_TYPE_LIST(OBJECT_TYPE_TEMPLATE_CASE)
#undef OBJECT_TYPE_CASE
#undef OBJECT_TYPE_STRUCT_CASE
#undef OBJECT_TYPE_TEMPLATE_CASE

Smi* CheckObjectType(Object* value, Smi* type, String* location);

namespace compiler {

class CallDescriptor;
class CodeAssemblerLabel;
class CodeAssemblerVariable;
template <class T>
class TypedCodeAssemblerVariable;
class CodeAssemblerState;
class Node;
class RawMachineAssembler;
class RawMachineLabel;

typedef ZoneVector<CodeAssemblerVariable*> CodeAssemblerVariableList;

typedef std::function<void()> CodeAssemblerCallback;

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
  static_assert(is_valid_type_tag<T>::value, "invalid type tag");

  template <class U,
            typename std::enable_if<is_subtype<U, T>::value, int>::type = 0>
  TNode(const TNode<U>& other) : node_(other) {}
  TNode() : node_(nullptr) {}

  TNode operator=(TNode other) {
    DCHECK_NULL(node_);
    node_ = other.node_;
    return *this;
  }

  operator compiler::Node*() const { return node_; }

  static TNode UncheckedCast(compiler::Node* node) { return TNode(node); }

 protected:
  explicit TNode(compiler::Node* node) : node_(node) {}

 private:
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

// This macro alias allows to use PairT<T1, T2> as a macro argument.
#define PAIR_TYPE(T1, T2) PairT<T1, T2>

#define CODE_ASSEMBLER_COMPARE_BINARY_OP_LIST(V)          \
  V(Float32Equal, BoolT, Float32T, Float32T)              \
  V(Float32LessThan, BoolT, Float32T, Float32T)           \
  V(Float32LessThanOrEqual, BoolT, Float32T, Float32T)    \
  V(Float32GreaterThan, BoolT, Float32T, Float32T)        \
  V(Float32GreaterThanOrEqual, BoolT, Float32T, Float32T) \
  V(Float64Equal, BoolT, Float64T, Float64T)              \
  V(Float64LessThan, BoolT, Float64T, Float64T)           \
  V(Float64LessThanOrEqual, BoolT, Float64T, Float64T)    \
  V(Float64GreaterThan, BoolT, Float64T, Float64T)        \
  V(Float64GreaterThanOrEqual, BoolT, Float64T, Float64T) \
  V(Int32GreaterThan, BoolT, Word32T, Word32T)            \
  V(Int32GreaterThanOrEqual, BoolT, Word32T, Word32T)     \
  V(Int32LessThan, BoolT, Word32T, Word32T)               \
  V(Int32LessThanOrEqual, BoolT, Word32T, Word32T)        \
  V(IntPtrLessThan, BoolT, WordT, WordT)                  \
  V(IntPtrLessThanOrEqual, BoolT, WordT, WordT)           \
  V(IntPtrGreaterThan, BoolT, WordT, WordT)               \
  V(IntPtrGreaterThanOrEqual, BoolT, WordT, WordT)        \
  V(IntPtrEqual, BoolT, WordT, WordT)                     \
  V(Uint32LessThan, BoolT, Word32T, Word32T)              \
  V(Uint32LessThanOrEqual, BoolT, Word32T, Word32T)       \
  V(Uint32GreaterThan, BoolT, Word32T, Word32T)           \
  V(Uint32GreaterThanOrEqual, BoolT, Word32T, Word32T)    \
  V(UintPtrLessThan, BoolT, WordT, WordT)                 \
  V(UintPtrLessThanOrEqual, BoolT, WordT, WordT)          \
  V(UintPtrGreaterThan, BoolT, WordT, WordT)              \
  V(UintPtrGreaterThanOrEqual, BoolT, WordT, WordT)       \
  V(WordEqual, BoolT, WordT, WordT)                       \
  V(WordNotEqual, BoolT, WordT, WordT)                    \
  V(Word32Equal, BoolT, Word32T, Word32T)                 \
  V(Word32NotEqual, BoolT, Word32T, Word32T)              \
  V(Word64Equal, BoolT, Word64T, Word64T)                 \
  V(Word64NotEqual, BoolT, Word64T, Word64T)

#define CODE_ASSEMBLER_BINARY_OP_LIST(V)                                \
  CODE_ASSEMBLER_COMPARE_BINARY_OP_LIST(V)                              \
  V(Float64Add, Float64T, Float64T, Float64T)                           \
  V(Float64Sub, Float64T, Float64T, Float64T)                           \
  V(Float64Mul, Float64T, Float64T, Float64T)                           \
  V(Float64Div, Float64T, Float64T, Float64T)                           \
  V(Float64Mod, Float64T, Float64T, Float64T)                           \
  V(Float64Atan2, Float64T, Float64T, Float64T)                         \
  V(Float64Pow, Float64T, Float64T, Float64T)                           \
  V(Float64Max, Float64T, Float64T, Float64T)                           \
  V(Float64Min, Float64T, Float64T, Float64T)                           \
  V(Float64InsertLowWord32, Float64T, Float64T, Word32T)                \
  V(Float64InsertHighWord32, Float64T, Float64T, Word32T)               \
  V(IntPtrAddWithOverflow, PAIR_TYPE(IntPtrT, BoolT), IntPtrT, IntPtrT) \
  V(IntPtrSubWithOverflow, PAIR_TYPE(IntPtrT, BoolT), IntPtrT, IntPtrT) \
  V(Int32Add, Word32T, Word32T, Word32T)                                \
  V(Int32AddWithOverflow, PAIR_TYPE(Int32T, BoolT), Int32T, Int32T)     \
  V(Int32Sub, Word32T, Word32T, Word32T)                                \
  V(Int32Mul, Word32T, Word32T, Word32T)                                \
  V(Int32MulWithOverflow, PAIR_TYPE(Int32T, BoolT), Int32T, Int32T)     \
  V(Int32Div, Int32T, Int32T, Int32T)                                   \
  V(Int32Mod, Int32T, Int32T, Int32T)                                   \
  V(WordRor, WordT, WordT, IntegralT)                                   \
  V(Word32Ror, Word32T, Word32T, Word32T)                               \
  V(Word64Ror, Word64T, Word64T, Word64T)

TNode<Float64T> Float64Add(TNode<Float64T> a, TNode<Float64T> b);

#define CODE_ASSEMBLER_UNARY_OP_LIST(V)                        \
  V(Float64Abs, Float64T, Float64T)                            \
  V(Float64Acos, Float64T, Float64T)                           \
  V(Float64Acosh, Float64T, Float64T)                          \
  V(Float64Asin, Float64T, Float64T)                           \
  V(Float64Asinh, Float64T, Float64T)                          \
  V(Float64Atan, Float64T, Float64T)                           \
  V(Float64Atanh, Float64T, Float64T)                          \
  V(Float64Cos, Float64T, Float64T)                            \
  V(Float64Cosh, Float64T, Float64T)                           \
  V(Float64Exp, Float64T, Float64T)                            \
  V(Float64Expm1, Float64T, Float64T)                          \
  V(Float64Log, Float64T, Float64T)                            \
  V(Float64Log1p, Float64T, Float64T)                          \
  V(Float64Log2, Float64T, Float64T)                           \
  V(Float64Log10, Float64T, Float64T)                          \
  V(Float64Cbrt, Float64T, Float64T)                           \
  V(Float64Neg, Float64T, Float64T)                            \
  V(Float64Sin, Float64T, Float64T)                            \
  V(Float64Sinh, Float64T, Float64T)                           \
  V(Float64Sqrt, Float64T, Float64T)                           \
  V(Float64Tan, Float64T, Float64T)                            \
  V(Float64Tanh, Float64T, Float64T)                           \
  V(Float64ExtractLowWord32, Word32T, Float64T)                \
  V(Float64ExtractHighWord32, Word32T, Float64T)               \
  V(BitcastTaggedToWord, IntPtrT, Object)                      \
  V(BitcastWordToTagged, Object, WordT)                        \
  V(BitcastWordToTaggedSigned, Smi, WordT)                     \
  V(TruncateFloat64ToFloat32, Float32T, Float64T)              \
  V(TruncateFloat64ToWord32, Word32T, Float64T)                \
  V(TruncateInt64ToInt32, Int32T, Int64T)                      \
  V(ChangeFloat32ToFloat64, Float64T, Float32T)                \
  V(ChangeFloat64ToUint32, Uint32T, Float64T)                  \
  V(ChangeFloat64ToUint64, Uint64T, Float64T)                  \
  V(ChangeInt32ToFloat64, Float64T, Int32T)                    \
  V(ChangeInt32ToInt64, Int64T, Int32T)                        \
  V(ChangeUint32ToFloat64, Float64T, Word32T)                  \
  V(ChangeUint32ToUint64, Uint64T, Word32T)                    \
  V(RoundFloat64ToInt32, Int32T, Float64T)                     \
  V(RoundInt32ToFloat32, Int32T, Float32T)                     \
  V(Float64SilenceNaN, Float64T, Float64T)                     \
  V(Float64RoundDown, Float64T, Float64T)                      \
  V(Float64RoundUp, Float64T, Float64T)                        \
  V(Float64RoundTiesEven, Float64T, Float64T)                  \
  V(Float64RoundTruncate, Float64T, Float64T)                  \
  V(Word32Clz, Int32T, Word32T)                                \
  V(Word32Not, Word32T, Word32T)                               \
  V(WordNot, WordT, WordT)                                     \
  V(Int32AbsWithOverflow, PAIR_TYPE(Int32T, BoolT), Int32T)    \
  V(Int64AbsWithOverflow, PAIR_TYPE(Int64T, BoolT), Int64T)    \
  V(IntPtrAbsWithOverflow, PAIR_TYPE(IntPtrT, BoolT), IntPtrT) \
  V(Word32BinaryNot, Word32T, Word32T)

// A "public" interface used by components outside of compiler directory to
// create code objects with TurboFan's backend. This class is mostly a thin
// shim around the RawMachineAssembler, and its primary job is to ensure that
// the innards of the RawMachineAssembler and other compiler implementation
// details don't leak outside of the the compiler directory..
//
// V8 components that need to generate low-level code using this interface
// should include this header--and this header only--from the compiler
// directory (this is actually enforced). Since all interesting data
// structures are forward declared, it's not possible for clients to peek
// inside the compiler internals.
//
// In addition to providing isolation between TurboFan and code generation
// clients, CodeAssembler also provides an abstraction for creating variables
// and enhanced Label functionality to merge variable values along paths where
// they have differing values, including loops.
//
// The CodeAssembler itself is stateless (and instances are expected to be
// temporary-scoped and short-lived); all its state is encapsulated into
// a CodeAssemblerState instance.
class V8_EXPORT_PRIVATE CodeAssembler {
 public:
  explicit CodeAssembler(CodeAssemblerState* state) : state_(state) {}
  ~CodeAssembler();

  static Handle<Code> GenerateCode(CodeAssemblerState* state);

  bool Is64() const;
  bool IsFloat64RoundUpSupported() const;
  bool IsFloat64RoundDownSupported() const;
  bool IsFloat64RoundTiesEvenSupported() const;
  bool IsFloat64RoundTruncateSupported() const;
  bool IsInt32AbsWithOverflowSupported() const;
  bool IsInt64AbsWithOverflowSupported() const;
  bool IsIntPtrAbsWithOverflowSupported() const;

  // Shortened aliases for use in CodeAssembler subclasses.
  using Label = CodeAssemblerLabel;
  using Variable = CodeAssemblerVariable;
  template <class T>
  using TVariable = TypedCodeAssemblerVariable<T>;
  using VariableList = CodeAssemblerVariableList;

  // ===========================================================================
  // Base Assembler
  // ===========================================================================

  template <class PreviousType, bool FromTyped>
  class CheckedNode {
   public:
#ifdef DEBUG
    CheckedNode(Node* node, CodeAssembler* code_assembler, const char* location)
        : node_(node), code_assembler_(code_assembler), location_(location) {}
#else
    CheckedNode(compiler::Node* node, CodeAssembler*, const char*)
        : node_(node) {}
#endif

    template <class A>
    operator TNode<A>() {
      static_assert(types_have_common_values<A, PreviousType>::value,
                    "Incompatible types: this cast can never succeed.");
      static_assert(std::is_convertible<TNode<A>, TNode<Object>>::value,
                    "Coercion to untagged values cannot be "
                    "checked.");
      static_assert(
          !FromTyped ||
              !std::is_convertible<TNode<PreviousType>, TNode<A>>::value,
          "Unnecessary CAST: types are convertible.");
#ifdef DEBUG
      if (FLAG_debug_code) {
        Node* function = code_assembler_->ExternalConstant(
            ExternalReference::check_object_type(code_assembler_->isolate()));
        code_assembler_->CallCFunction3(
            MachineType::AnyTagged(), MachineType::AnyTagged(),
            MachineType::TaggedSigned(), MachineType::AnyTagged(), function,
            node_,
            code_assembler_->SmiConstant(
                static_cast<int>(ObjectTypeOf<A>::value)),
            code_assembler_->StringConstant(location_));
      }
#endif
      return TNode<A>::UncheckedCast(node_);
    }

    template <class A>
    operator SloppyTNode<A>() {
      return base::implicit_cast<TNode<A>>(*this);
    }

    Node* node() const { return node_; }

   private:
    Node* node_;
#ifdef DEBUG
    CodeAssembler* code_assembler_;
    const char* location_;
#endif
  };

  template <class T>
  TNode<T> UncheckedCast(Node* value) {
    return TNode<T>::UncheckedCast(value);
  }
  template <class T, class U>
  TNode<T> UncheckedCast(TNode<U> value) {
    static_assert(types_have_common_values<T, U>::value,
                  "Incompatible types: this cast can never succeed.");
    return TNode<T>::UncheckedCast(value);
  }

  // ReinterpretCast<T>(v) has the power to cast even when the type of v is
  // unrelated to T. Use with care.
  template <class T>
  TNode<T> ReinterpretCast(Node* value) {
    return TNode<T>::UncheckedCast(value);
  }

  CheckedNode<Object, false> Cast(Node* value, const char* location) {
    return {value, this, location};
  }

  template <class T>
  CheckedNode<T, true> Cast(TNode<T> value, const char* location) {
    return {value, this, location};
  }

#ifdef DEBUG
#define STRINGIFY(x) #x
#define TO_STRING_LITERAL(x) STRINGIFY(x)
#define CAST(x) \
  Cast(x, "CAST(" #x ") at " __FILE__ ":" TO_STRING_LITERAL(__LINE__))
#else
#define CAST(x) Cast(x, "")
#endif

#ifdef V8_EMBEDDED_BUILTINS
  // Off-heap builtins cannot embed constants within the code object itself,
  // and thus need to load them from the root list.
  bool ShouldLoadConstantsFromRootList() const {
    return (isolate()->serializer_enabled() &&
            isolate()->builtins_constants_table_builder() != nullptr);
  }

  TNode<HeapObject> LookupConstant(Handle<HeapObject> object);
  TNode<ExternalReference> LookupExternalReference(ExternalReference reference);
#endif

  // Constants.
  TNode<Int32T> Int32Constant(int32_t value);
  TNode<Int64T> Int64Constant(int64_t value);
  TNode<IntPtrT> IntPtrConstant(intptr_t value);
  TNode<Number> NumberConstant(double value);
  TNode<Smi> SmiConstant(Smi* value);
  TNode<Smi> SmiConstant(int value);
  template <typename E,
            typename = typename std::enable_if<std::is_enum<E>::value>::type>
  TNode<Smi> SmiConstant(E value) {
    STATIC_ASSERT(sizeof(E) <= sizeof(int));
    return SmiConstant(static_cast<int>(value));
  }
  TNode<HeapObject> UntypedHeapConstant(Handle<HeapObject> object);
  template <class Type>
  TNode<Type> HeapConstant(Handle<Type> object) {
    return UncheckedCast<Type>(UntypedHeapConstant(object));
  }
  TNode<String> StringConstant(const char* str);
  TNode<Oddball> BooleanConstant(bool value);
  TNode<ExternalReference> ExternalConstant(ExternalReference address);
  TNode<Float64T> Float64Constant(double value);
  TNode<HeapNumber> NaNConstant();
  TNode<BoolT> Int32TrueConstant() {
    return ReinterpretCast<BoolT>(Int32Constant(1));
  }
  TNode<BoolT> Int32FalseConstant() {
    return ReinterpretCast<BoolT>(Int32Constant(0));
  }

  bool ToInt32Constant(Node* node, int32_t& out_value);
  bool ToInt64Constant(Node* node, int64_t& out_value);
  bool ToSmiConstant(Node* node, Smi*& out_value);
  bool ToIntPtrConstant(Node* node, intptr_t& out_value);

  TNode<Int32T> Signed(TNode<Word32T> x) { return UncheckedCast<Int32T>(x); }
  TNode<IntPtrT> Signed(TNode<WordT> x) { return UncheckedCast<IntPtrT>(x); }
  TNode<Uint32T> Unsigned(TNode<Word32T> x) {
    return UncheckedCast<Uint32T>(x);
  }
  TNode<UintPtrT> Unsigned(TNode<WordT> x) {
    return UncheckedCast<UintPtrT>(x);
  }

  Node* Parameter(int value);

  TNode<Context> GetJSContextParameter();
  void Return(SloppyTNode<Object> value);
  void Return(SloppyTNode<Object> value1, SloppyTNode<Object> value2);
  void Return(SloppyTNode<Object> value1, SloppyTNode<Object> value2,
              SloppyTNode<Object> value3);
  void PopAndReturn(Node* pop, Node* value);

  void ReturnIf(Node* condition, Node* value);

  void DebugAbort(Node* message);
  void DebugBreak();
  void Unreachable();
  void Comment(const char* format, ...);

  void Bind(Label* label);
#if DEBUG
  void Bind(Label* label, AssemblerDebugInfo debug_info);
#endif  // DEBUG
  void Goto(Label* label);
  void GotoIf(SloppyTNode<IntegralT> condition, Label* true_label);
  void GotoIfNot(SloppyTNode<IntegralT> condition, Label* false_label);
  void Branch(SloppyTNode<IntegralT> condition, Label* true_label,
              Label* false_label);

  void Switch(Node* index, Label* default_label, const int32_t* case_values,
              Label** case_labels, size_t case_count);

  // Access to the frame pointer
  Node* LoadFramePointer();
  Node* LoadParentFramePointer();

  // Access to the roots pointer.
  TNode<IntPtrT> LoadRootsPointer();

  // Access to the stack pointer
  Node* LoadStackPointer();

  // Poison |value| on speculative paths.
  TNode<Object> PoisonOnSpeculationTagged(SloppyTNode<Object> value);
  TNode<WordT> PoisonOnSpeculationWord(SloppyTNode<WordT> value);

  // Load raw memory location.
  Node* Load(MachineType rep, Node* base,
             LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);
  template <class Type>
  TNode<Type> Load(MachineType rep, TNode<RawPtr<Type>> base) {
    DCHECK(
        IsSubtype(rep.representation(), MachineRepresentationOf<Type>::value));
    return UncheckedCast<Type>(Load(rep, static_cast<Node*>(base)));
  }
  Node* Load(MachineType rep, Node* base, Node* offset,
             LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);
  Node* AtomicLoad(MachineType rep, Node* base, Node* offset);

  // Load a value from the root array.
  TNode<Object> LoadRoot(Heap::RootListIndex root_index);

  // Store value to raw memory location.
  Node* Store(Node* base, Node* value);
  Node* Store(Node* base, Node* offset, Node* value);
  Node* StoreWithMapWriteBarrier(Node* base, Node* offset, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* offset,
                            Node* value);
  Node* AtomicStore(MachineRepresentation rep, Node* base, Node* offset,
                    Node* value);

  // Exchange value at raw memory location
  Node* AtomicExchange(MachineType type, Node* base, Node* offset, Node* value);

  // Compare and Exchange value at raw memory location
  Node* AtomicCompareExchange(MachineType type, Node* base, Node* offset,
                              Node* old_value, Node* new_value);

  Node* AtomicAdd(MachineType type, Node* base, Node* offset, Node* value);

  Node* AtomicSub(MachineType type, Node* base, Node* offset, Node* value);

  Node* AtomicAnd(MachineType type, Node* base, Node* offset, Node* value);

  Node* AtomicOr(MachineType type, Node* base, Node* offset, Node* value);

  Node* AtomicXor(MachineType type, Node* base, Node* offset, Node* value);

  // Store a value to the root array.
  Node* StoreRoot(Heap::RootListIndex root_index, Node* value);

// Basic arithmetic operations.
#define DECLARE_CODE_ASSEMBLER_BINARY_OP(name, ResType, Arg1Type, Arg2Type) \
  TNode<ResType> name(SloppyTNode<Arg1Type> a, SloppyTNode<Arg2Type> b);
  CODE_ASSEMBLER_BINARY_OP_LIST(DECLARE_CODE_ASSEMBLER_BINARY_OP)
#undef DECLARE_CODE_ASSEMBLER_BINARY_OP

  TNode<IntPtrT> WordShr(TNode<IntPtrT> left, TNode<IntegralT> right) {
    return UncheckedCast<IntPtrT>(
        WordShr(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<IntPtrT> WordAnd(TNode<IntPtrT> left, TNode<IntPtrT> right) {
    return UncheckedCast<IntPtrT>(
        WordAnd(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  template <class Left, class Right,
            class = typename std::enable_if<
                std::is_base_of<Object, Left>::value &&
                std::is_base_of<Object, Right>::value>::type>
  TNode<BoolT> WordEqual(TNode<Left> left, TNode<Right> right) {
    return WordEqual(ReinterpretCast<WordT>(left),
                     ReinterpretCast<WordT>(right));
  }
  TNode<BoolT> WordEqual(TNode<Object> left, Node* right) {
    return WordEqual(ReinterpretCast<WordT>(left),
                     ReinterpretCast<WordT>(right));
  }
  TNode<BoolT> WordEqual(Node* left, TNode<Object> right) {
    return WordEqual(ReinterpretCast<WordT>(left),
                     ReinterpretCast<WordT>(right));
  }
  template <class Left, class Right,
            class = typename std::enable_if<
                std::is_base_of<Object, Left>::value &&
                std::is_base_of<Object, Right>::value>::type>
  TNode<BoolT> WordNotEqual(TNode<Left> left, TNode<Right> right) {
    return WordNotEqual(ReinterpretCast<WordT>(left),
                        ReinterpretCast<WordT>(right));
  }
  TNode<BoolT> WordNotEqual(TNode<Object> left, Node* right) {
    return WordNotEqual(ReinterpretCast<WordT>(left),
                        ReinterpretCast<WordT>(right));
  }
  TNode<BoolT> WordNotEqual(Node* left, TNode<Object> right) {
    return WordNotEqual(ReinterpretCast<WordT>(left),
                        ReinterpretCast<WordT>(right));
  }

  TNode<Int32T> Int32Add(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Int32Add(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<WordT> IntPtrAdd(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<WordT> IntPtrSub(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<WordT> IntPtrMul(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<IntPtrT> IntPtrAdd(TNode<IntPtrT> left, TNode<IntPtrT> right) {
    return Signed(
        IntPtrAdd(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<IntPtrT> IntPtrSub(TNode<IntPtrT> left, TNode<IntPtrT> right) {
    return Signed(
        IntPtrSub(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<IntPtrT> IntPtrMul(TNode<IntPtrT> left, TNode<IntPtrT> right) {
    return Signed(
        IntPtrMul(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<WordT> WordShl(SloppyTNode<WordT> value, int shift);
  TNode<WordT> WordShr(SloppyTNode<WordT> value, int shift);
  TNode<WordT> WordSar(SloppyTNode<WordT> value, int shift);
  TNode<IntPtrT> WordShr(TNode<IntPtrT> value, int shift) {
    return UncheckedCast<IntPtrT>(WordShr(static_cast<Node*>(value), shift));
  }
  TNode<Word32T> Word32Shr(SloppyTNode<Word32T> value, int shift);

  TNode<WordT> WordOr(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<WordT> WordAnd(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<WordT> WordXor(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<WordT> WordShl(SloppyTNode<WordT> left, SloppyTNode<IntegralT> right);
  TNode<WordT> WordShr(SloppyTNode<WordT> left, SloppyTNode<IntegralT> right);
  TNode<WordT> WordSar(SloppyTNode<WordT> left, SloppyTNode<IntegralT> right);
  TNode<Word32T> Word32Or(SloppyTNode<Word32T> left,
                          SloppyTNode<Word32T> right);
  TNode<Word32T> Word32And(SloppyTNode<Word32T> left,
                           SloppyTNode<Word32T> right);
  TNode<Word32T> Word32Xor(SloppyTNode<Word32T> left,
                           SloppyTNode<Word32T> right);
  TNode<Word32T> Word32Shl(SloppyTNode<Word32T> left,
                           SloppyTNode<Word32T> right);
  TNode<Word32T> Word32Shr(SloppyTNode<Word32T> left,
                           SloppyTNode<Word32T> right);
  TNode<Word32T> Word32Sar(SloppyTNode<Word32T> left,
                           SloppyTNode<Word32T> right);
  TNode<Word64T> Word64Or(SloppyTNode<Word64T> left,
                          SloppyTNode<Word64T> right);
  TNode<Word64T> Word64And(SloppyTNode<Word64T> left,
                           SloppyTNode<Word64T> right);
  TNode<Word64T> Word64Xor(SloppyTNode<Word64T> left,
                           SloppyTNode<Word64T> right);
  TNode<Word64T> Word64Shl(SloppyTNode<Word64T> left,
                           SloppyTNode<Word64T> right);
  TNode<Word64T> Word64Shr(SloppyTNode<Word64T> left,
                           SloppyTNode<Word64T> right);
  TNode<Word64T> Word64Sar(SloppyTNode<Word64T> left,
                           SloppyTNode<Word64T> right);

// Unary
#define DECLARE_CODE_ASSEMBLER_UNARY_OP(name, ResType, ArgType) \
  TNode<ResType> name(SloppyTNode<ArgType> a);
  CODE_ASSEMBLER_UNARY_OP_LIST(DECLARE_CODE_ASSEMBLER_UNARY_OP)
#undef DECLARE_CODE_ASSEMBLER_UNARY_OP

  // Changes a double to an inptr_t for pointer arithmetic outside of Smi range.
  // Assumes that the double can be exactly represented as an int.
  TNode<UintPtrT> ChangeFloat64ToUintPtr(SloppyTNode<Float64T> value);

  // Changes an intptr_t to a double, e.g. for storing an element index
  // outside Smi range in a HeapNumber. Lossless on 32-bit,
  // rounds on 64-bit (which doesn't affect valid element indices).
  Node* RoundIntPtrToFloat64(Node* value);
  // No-op on 32-bit, otherwise zero extend.
  TNode<UintPtrT> ChangeUint32ToWord(SloppyTNode<Word32T> value);
  // No-op on 32-bit, otherwise sign extend.
  TNode<IntPtrT> ChangeInt32ToIntPtr(SloppyTNode<Word32T> value);

  // No-op that guarantees that the value is kept alive till this point even
  // if GC happens.
  Node* Retain(Node* value);

  // Projections
  Node* Projection(int index, Node* value);

  template <int index, class T1, class T2>
  TNode<typename std::tuple_element<index, std::tuple<T1, T2>>::type>
  Projection(TNode<PairT<T1, T2>> value) {
    return UncheckedCast<
        typename std::tuple_element<index, std::tuple<T1, T2>>::type>(
        Projection(index, value));
  }

  // Calls
  template <class... TArgs>
  TNode<Object> CallRuntimeImpl(Runtime::FunctionId function,
                                SloppyTNode<Object> context, TArgs... args);
  template <class... TArgs>
  TNode<Object> CallRuntime(Runtime::FunctionId function,
                            SloppyTNode<Object> context, TArgs... args) {
    return CallRuntimeImpl(function, context,
                           base::implicit_cast<SloppyTNode<Object>>(args)...);
  }

  template <class... TArgs>
  TNode<Object> TailCallRuntimeImpl(Runtime::FunctionId function,
                                    SloppyTNode<Object> context, TArgs... args);
  template <class... TArgs>
  TNode<Object> TailCallRuntime(Runtime::FunctionId function,
                                SloppyTNode<Object> context, TArgs... args) {
    return TailCallRuntimeImpl(
        function, context, base::implicit_cast<SloppyTNode<Object>>(args)...);
  }

  //
  // If context passed to CallStub is nullptr, it won't be passed to the stub.
  //

  template <class... TArgs>
  Node* CallStub(Callable const& callable, Node* context, TArgs... args) {
    Node* target = HeapConstant(callable.code());
    return CallStub(callable.descriptor(), target, context,
                    base::implicit_cast<Node*>(args)...);
  }

  template <class... TArgs>
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, TArgs... args) {
    return CallStubR(descriptor, 1, target, context,
                     base::implicit_cast<Node*>(args)...);
  }

  template <class... TArgs>
  Node* CallStubR(const CallInterfaceDescriptor& descriptor, size_t result_size,
                  Node* target, Node* context, TArgs... args);

  Node* CallStubN(const CallInterfaceDescriptor& descriptor, size_t result_size,
                  int input_count, Node* const* inputs,
                  bool pass_context = true);

  template <class... TArgs>
  Node* TailCallStub(Callable const& callable, Node* context, TArgs... args) {
    Node* target = HeapConstant(callable.code());
    return TailCallStub(callable.descriptor(), target, context, args...);
  }

  template <class... TArgs>
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, TArgs... args) {
    return TailCallStubImpl(descriptor, target, context,
                            base::implicit_cast<Node*>(args)...);
  }
  template <class... TArgs>
  Node* TailCallStubImpl(const CallInterfaceDescriptor& descriptor,
                         Node* target, Node* context, TArgs... args);

  template <class... TArgs>
  Node* TailCallBytecodeDispatch(const CallInterfaceDescriptor& descriptor,
                                 Node* target, TArgs... args);

  template <class... TArgs>
  Node* TailCallStubThenBytecodeDispatch(
      const CallInterfaceDescriptor& descriptor, Node* context, Node* target,
      TArgs... args);

  template <class... TArgs>
  Node* CallJS(Callable const& callable, Node* context, Node* function,
               Node* receiver, TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    Node* arity = Int32Constant(argc);
    return CallStub(callable, context, function, arity, receiver, args...);
  }

  template <class... TArgs>
  Node* ConstructJS(Callable const& callable, Node* context, Node* new_target,
                    TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    Node* arity = Int32Constant(argc);
    Node* receiver = LoadRoot(Heap::kUndefinedValueRootIndex);

    // Construct(target, new_target, arity, receiver, arguments...)
    return CallStub(callable, context, new_target, new_target, arity, receiver,
                    args...);
  }

  Node* CallCFunctionN(Signature<MachineType>* signature, int input_count,
                       Node* const* inputs);

  // Call to a C function with one argument.
  Node* CallCFunction1(MachineType return_type, MachineType arg0_type,
                       Node* function, Node* arg0);

  // Call to a C function with one argument, while saving/restoring caller
  // registers except the register used for return value.
  Node* CallCFunction1WithCallerSavedRegisters(MachineType return_type,
                                               MachineType arg0_type,
                                               Node* function, Node* arg0,
                                               SaveFPRegsMode mode);

  // Call to a C function with two arguments.
  Node* CallCFunction2(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, Node* function, Node* arg0,
                       Node* arg1);

  // Call to a C function with three arguments.
  Node* CallCFunction3(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2);

  // Call to a C function with three arguments, while saving/restoring caller
  // registers except the register used for return value.
  Node* CallCFunction3WithCallerSavedRegisters(
      MachineType return_type, MachineType arg0_type, MachineType arg1_type,
      MachineType arg2_type, Node* function, Node* arg0, Node* arg1, Node* arg2,
      SaveFPRegsMode mode);

  // Call to a C function with four arguments.
  Node* CallCFunction4(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, Node* function, Node* arg0,
                       Node* arg1, Node* arg2, Node* arg3);

  // Call to a C function with five arguments.
  Node* CallCFunction5(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2,
                       Node* arg3, Node* arg4);

  // Call to a C function with six arguments.
  Node* CallCFunction6(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       MachineType arg5_type, Node* function, Node* arg0,
                       Node* arg1, Node* arg2, Node* arg3, Node* arg4,
                       Node* arg5);

  // Call to a C function with nine arguments.
  Node* CallCFunction9(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       MachineType arg5_type, MachineType arg6_type,
                       MachineType arg7_type, MachineType arg8_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2,
                       Node* arg3, Node* arg4, Node* arg5, Node* arg6,
                       Node* arg7, Node* arg8);

  // Exception handling support.
  void GotoIfException(Node* node, Label* if_exception,
                       Variable* exception_var = nullptr);

  // Helpers which delegate to RawMachineAssembler.
  Factory* factory() const;
  Isolate* isolate() const;
  Zone* zone() const;

  CodeAssemblerState* state() { return state_; }

  void BreakOnNode(int node_id);

  bool UnalignedLoadSupported(MachineRepresentation rep) const;
  bool UnalignedStoreSupported(MachineRepresentation rep) const;

 protected:
  void RegisterCallGenerationCallbacks(
      const CodeAssemblerCallback& call_prologue,
      const CodeAssemblerCallback& call_epilogue);
  void UnregisterCallGenerationCallbacks();

  bool Word32ShiftIsSafe() const;
  PoisoningMitigationLevel poisoning_enabled() const;

 private:
  // These two don't have definitions and are here only for catching use cases
  // where the cast is not necessary.
  TNode<Int32T> Signed(TNode<Int32T> x);
  TNode<Uint32T> Unsigned(TNode<Uint32T> x);

  RawMachineAssembler* raw_assembler() const;

  // Calls respective callback registered in the state.
  void CallPrologue();
  void CallEpilogue();

  CodeAssemblerState* state_;

  DISALLOW_COPY_AND_ASSIGN(CodeAssembler);
};

class CodeAssemblerVariable {
 public:
  explicit CodeAssemblerVariable(CodeAssembler* assembler,
                                 MachineRepresentation rep);
  CodeAssemblerVariable(CodeAssembler* assembler, MachineRepresentation rep,
                        Node* initial_value);
#if DEBUG
  CodeAssemblerVariable(CodeAssembler* assembler, AssemblerDebugInfo debug_info,
                        MachineRepresentation rep);
  CodeAssemblerVariable(CodeAssembler* assembler, AssemblerDebugInfo debug_info,
                        MachineRepresentation rep, Node* initial_value);
#endif  // DEBUG

  ~CodeAssemblerVariable();
  void Bind(Node* value);
  Node* value() const;
  MachineRepresentation rep() const;
  bool IsBound() const;

 private:
  class Impl;
  friend class CodeAssemblerLabel;
  friend class CodeAssemblerState;
  friend std::ostream& operator<<(std::ostream&, const Impl&);
  friend std::ostream& operator<<(std::ostream&, const CodeAssemblerVariable&);
  Impl* impl_;
  CodeAssemblerState* state_;
  DISALLOW_COPY_AND_ASSIGN(CodeAssemblerVariable);
};

std::ostream& operator<<(std::ostream&, const CodeAssemblerVariable&);
std::ostream& operator<<(std::ostream&, const CodeAssemblerVariable::Impl&);

template <class T>
class TypedCodeAssemblerVariable : public CodeAssemblerVariable {
 public:
  TypedCodeAssemblerVariable(TNode<T> initial_value, CodeAssembler* assembler)
      : CodeAssemblerVariable(assembler, MachineRepresentationOf<T>::value,
                              initial_value) {}
  explicit TypedCodeAssemblerVariable(CodeAssembler* assembler)
      : CodeAssemblerVariable(assembler, MachineRepresentationOf<T>::value) {}
#if DEBUG
  TypedCodeAssemblerVariable(AssemblerDebugInfo debug_info,
                             CodeAssembler* assembler)
      : CodeAssemblerVariable(assembler, debug_info,
                              MachineRepresentationOf<T>::value) {}
  TypedCodeAssemblerVariable(AssemblerDebugInfo debug_info,
                             TNode<T> initial_value, CodeAssembler* assembler)
      : CodeAssemblerVariable(assembler, debug_info,
                              MachineRepresentationOf<T>::value,
                              initial_value) {}
#endif  // DEBUG

  TNode<T> value() const {
    return TNode<T>::UncheckedCast(CodeAssemblerVariable::value());
  }

  void operator=(TNode<T> value) { Bind(value); }
  void operator=(const TypedCodeAssemblerVariable<T>& variable) {
    Bind(variable.value());
  }

 private:
  using CodeAssemblerVariable::Bind;
};

class CodeAssemblerLabel {
 public:
  enum Type { kDeferred, kNonDeferred };

  explicit CodeAssemblerLabel(
      CodeAssembler* assembler,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, 0, nullptr, type) {}
  CodeAssemblerLabel(
      CodeAssembler* assembler,
      const CodeAssemblerVariableList& merged_variables,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, merged_variables.size(),
                           &(merged_variables[0]), type) {}
  CodeAssemblerLabel(
      CodeAssembler* assembler, size_t count,
      CodeAssemblerVariable* const* vars,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred);
  CodeAssemblerLabel(
      CodeAssembler* assembler,
      std::initializer_list<CodeAssemblerVariable*> vars,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, vars.size(), vars.begin(), type) {}
  CodeAssemblerLabel(
      CodeAssembler* assembler, CodeAssemblerVariable* merged_variable,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, 1, &merged_variable, type) {}
  ~CodeAssemblerLabel();

  inline bool is_bound() const { return bound_; }

 private:
  friend class CodeAssembler;

  void Bind();
#if DEBUG
  void Bind(AssemblerDebugInfo debug_info);
#endif  // DEBUG
  void UpdateVariablesAfterBind();
  void MergeVariables();

  bool bound_;
  size_t merge_count_;
  CodeAssemblerState* state_;
  RawMachineLabel* label_;
  // Map of variables that need to be merged to their phi nodes (or placeholders
  // for those phis).
  std::map<CodeAssemblerVariable::Impl*, Node*> variable_phis_;
  // Map of variables to the list of value nodes that have been added from each
  // merge path in their order of merging.
  std::map<CodeAssemblerVariable::Impl*, std::vector<Node*>> variable_merges_;
};

class V8_EXPORT_PRIVATE CodeAssemblerState {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeAssemblerState(Isolate* isolate, Zone* zone,
                     const CallInterfaceDescriptor& descriptor, Code::Kind kind,
                     const char* name,
                     PoisoningMitigationLevel poisoning_enabled,
                     size_t result_size = 1, uint32_t stub_key = 0,
                     int32_t builtin_index = Builtins::kNoBuiltinId);

  // Create with JSCall linkage.
  CodeAssemblerState(Isolate* isolate, Zone* zone, int parameter_count,
                     Code::Kind kind, const char* name,
                     PoisoningMitigationLevel poisoning_enabled,
                     int32_t builtin_index = Builtins::kNoBuiltinId);

  ~CodeAssemblerState();

  const char* name() const { return name_; }
  int parameter_count() const;

#if DEBUG
  void PrintCurrentBlock(std::ostream& os);
  bool InsideBlock();
#endif  // DEBUG
  void SetInitialDebugInformation(const char* msg, const char* file, int line);

 private:
  friend class CodeAssembler;
  friend class CodeAssemblerLabel;
  friend class CodeAssemblerVariable;
  friend class CodeAssemblerTester;

  CodeAssemblerState(Isolate* isolate, Zone* zone,
                     CallDescriptor* call_descriptor, Code::Kind kind,
                     const char* name,
                     PoisoningMitigationLevel poisoning_enabled,
                     uint32_t stub_key, int32_t builtin_index);

  std::unique_ptr<RawMachineAssembler> raw_assembler_;
  Code::Kind kind_;
  const char* name_;
  uint32_t stub_key_;
  int32_t builtin_index_;
  bool code_generated_;
  ZoneSet<CodeAssemblerVariable::Impl*> variables_;
  CodeAssemblerCallback call_prologue_;
  CodeAssemblerCallback call_epilogue_;

  DISALLOW_COPY_AND_ASSIGN(CodeAssemblerState);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_ASSEMBLER_H_
