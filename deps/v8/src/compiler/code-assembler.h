// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_ASSEMBLER_H_
#define V8_COMPILER_CODE_ASSEMBLER_H_

#include <map>
#include <memory>
#include <initializer_list>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/base/macros.h"
#include "src/base/type-traits.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/source-position.h"
#include "src/heap/heap.h"
#include "src/objects/arguments.h"
#include "src/objects/data-handler.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/js-collection.h"
#include "src/objects/js-proxy.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects.h"
#include "src/objects/oddball.h"
#include "src/runtime/runtime.h"
#include "src/utils/allocation.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AsmWasmData;
class AsyncGeneratorRequest;
struct AssemblerOptions;
class BigInt;
class CallInterfaceDescriptor;
class Callable;
class Factory;
class FinalizationGroupCleanupJobTask;
class InterpreterData;
class Isolate;
class JSAsyncFunctionObject;
class JSAsyncGeneratorObject;
class JSCollator;
class JSCollection;
class JSDateTimeFormat;
class JSListFormat;
class JSLocale;
class JSNumberFormat;
class JSPluralRules;
class JSRegExpStringIterator;
class JSRelativeTimeFormat;
class JSSegmentIterator;
class JSSegmenter;
class JSV8BreakIterator;
class JSWeakCollection;
class JSFinalizationGroup;
class JSFinalizationGroupCleanupIterator;
class JSWeakMap;
class JSWeakRef;
class JSWeakSet;
class PromiseCapability;
class PromiseFulfillReactionJobTask;
class PromiseReaction;
class PromiseReactionJobTask;
class PromiseRejectReactionJobTask;
class WasmDebugInfo;
class Zone;
#define MAKE_FORWARD_DECLARATION(V, NAME, Name, name) class Name;
TORQUE_STRUCT_LIST_GENERATOR(MAKE_FORWARD_DECLARATION, UNUSED)
#undef MAKE_FORWARD_DECLARATION

template <typename T>
class Signature;

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

#define ENUM_ELEMENT(Name) k##Name,
#define ENUM_STRUCT_ELEMENT(NAME, Name, name) k##Name,
enum class ObjectType {
  ENUM_ELEMENT(Object)                 //
  ENUM_ELEMENT(Smi)                    //
  ENUM_ELEMENT(HeapObject)             //
  OBJECT_TYPE_LIST(ENUM_ELEMENT)       //
  HEAP_OBJECT_TYPE_LIST(ENUM_ELEMENT)  //
  STRUCT_LIST(ENUM_STRUCT_ELEMENT)     //
};
#undef ENUM_ELEMENT
#undef ENUM_STRUCT_ELEMENT

enum class CheckBounds { kAlways, kDebugOnly };
inline bool NeedsBoundsCheck(CheckBounds check_bounds) {
  switch (check_bounds) {
    case CheckBounds::kAlways:
      return true;
    case CheckBounds::kDebugOnly:
      return DEBUG_BOOL;
  }
}

enum class StoreToObjectWriteBarrier { kNone, kMap, kFull };

class AccessCheckNeeded;
class BigIntWrapper;
class ClassBoilerplate;
class BooleanWrapper;
class CompilationCacheTable;
class Constructor;
class Filler;
class FunctionTemplateRareData;
class InternalizedString;
class JSArgumentsObject;
class JSArrayBufferView;
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
class WasmCapiFunctionData;
class WasmExceptionObject;
class WasmExceptionTag;
class WasmExportedFunctionData;
class WasmGlobalObject;
class WasmIndirectFunctionTable;
class WasmJSFunctionData;
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
OBJECT_TYPE_CASE(Smi)
OBJECT_TYPE_CASE(HeapObject)
OBJECT_TYPE_LIST(OBJECT_TYPE_CASE)
HEAP_OBJECT_ORDINARY_TYPE_LIST(OBJECT_TYPE_CASE)
STRUCT_LIST(OBJECT_TYPE_STRUCT_CASE)
HEAP_OBJECT_TEMPLATE_TYPE_LIST(OBJECT_TYPE_TEMPLATE_CASE)
#undef OBJECT_TYPE_CASE
#undef OBJECT_TYPE_STRUCT_CASE
#undef OBJECT_TYPE_TEMPLATE_CASE

// {raw_value} must be a tagged Object.
// {raw_type} must be a tagged Smi.
// {raw_location} must be a tagged String.
// Returns a tagged Smi.
Address CheckObjectType(Address raw_value, Address raw_type,
                        Address raw_location);

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
class SourcePositionTable;

using CodeAssemblerVariableList = ZoneVector<CodeAssemblerVariable*>;

using CodeAssemblerCallback = std::function<void()>;

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

template <class... Types>
class CodeAssemblerParameterizedLabel;

// This macro alias allows to use PairT<T1, T2> as a macro argument.
#define PAIR_TYPE(T1, T2) PairT<T1, T2>

#define CODE_ASSEMBLER_COMPARE_BINARY_OP_LIST(V)          \
  V(Float32Equal, BoolT, Float32T, Float32T)              \
  V(Float32LessThan, BoolT, Float32T, Float32T)           \
  V(Float32LessThanOrEqual, BoolT, Float32T, Float32T)    \
  V(Float32GreaterThan, BoolT, Float32T, Float32T)        \
  V(Float32GreaterThanOrEqual, BoolT, Float32T, Float32T) \
  V(Float64Equal, BoolT, Float64T, Float64T)              \
  V(Float64NotEqual, BoolT, Float64T, Float64T)           \
  V(Float64LessThan, BoolT, Float64T, Float64T)           \
  V(Float64LessThanOrEqual, BoolT, Float64T, Float64T)    \
  V(Float64GreaterThan, BoolT, Float64T, Float64T)        \
  V(Float64GreaterThanOrEqual, BoolT, Float64T, Float64T) \
  /* Use Word32Equal if you need Int32Equal */            \
  V(Int32GreaterThan, BoolT, Word32T, Word32T)            \
  V(Int32GreaterThanOrEqual, BoolT, Word32T, Word32T)     \
  V(Int32LessThan, BoolT, Word32T, Word32T)               \
  V(Int32LessThanOrEqual, BoolT, Word32T, Word32T)        \
  /* Use WordEqual if you need IntPtrEqual */             \
  V(IntPtrLessThan, BoolT, WordT, WordT)                  \
  V(IntPtrLessThanOrEqual, BoolT, WordT, WordT)           \
  V(IntPtrGreaterThan, BoolT, WordT, WordT)               \
  V(IntPtrGreaterThanOrEqual, BoolT, WordT, WordT)        \
  /* Use Word32Equal if you need Uint32Equal */           \
  V(Uint32LessThan, BoolT, Word32T, Word32T)              \
  V(Uint32LessThanOrEqual, BoolT, Word32T, Word32T)       \
  V(Uint32GreaterThan, BoolT, Word32T, Word32T)           \
  V(Uint32GreaterThanOrEqual, BoolT, Word32T, Word32T)    \
  /* Use WordEqual if you need UintPtrEqual */            \
  V(UintPtrLessThan, BoolT, WordT, WordT)                 \
  V(UintPtrLessThanOrEqual, BoolT, WordT, WordT)          \
  V(UintPtrGreaterThan, BoolT, WordT, WordT)              \
  V(UintPtrGreaterThanOrEqual, BoolT, WordT, WordT)

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
  V(Int32SubWithOverflow, PAIR_TYPE(Int32T, BoolT), Int32T, Int32T)     \
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
  V(Float64ExtractLowWord32, Uint32T, Float64T)                \
  V(Float64ExtractHighWord32, Uint32T, Float64T)               \
  V(BitcastTaggedToWord, IntPtrT, Object)                      \
  V(BitcastTaggedSignedToWord, IntPtrT, Smi)                   \
  V(BitcastMaybeObjectToWord, IntPtrT, MaybeObject)            \
  V(BitcastWordToTagged, Object, WordT)                        \
  V(BitcastWordToTaggedSigned, Smi, WordT)                     \
  V(TruncateFloat64ToFloat32, Float32T, Float64T)              \
  V(TruncateFloat64ToWord32, Uint32T, Float64T)                \
  V(TruncateInt64ToInt32, Int32T, Int64T)                      \
  V(ChangeFloat32ToFloat64, Float64T, Float32T)                \
  V(ChangeFloat64ToUint32, Uint32T, Float64T)                  \
  V(ChangeFloat64ToUint64, Uint64T, Float64T)                  \
  V(ChangeInt32ToFloat64, Float64T, Int32T)                    \
  V(ChangeInt32ToInt64, Int64T, Int32T)                        \
  V(ChangeUint32ToFloat64, Float64T, Word32T)                  \
  V(ChangeUint32ToUint64, Uint64T, Word32T)                    \
  V(BitcastInt32ToFloat32, Float32T, Word32T)                  \
  V(BitcastFloat32ToInt32, Uint32T, Float32T)                  \
  V(RoundFloat64ToInt32, Int32T, Float64T)                     \
  V(RoundInt32ToFloat32, Int32T, Float32T)                     \
  V(Float64SilenceNaN, Float64T, Float64T)                     \
  V(Float64RoundDown, Float64T, Float64T)                      \
  V(Float64RoundUp, Float64T, Float64T)                        \
  V(Float64RoundTiesEven, Float64T, Float64T)                  \
  V(Float64RoundTruncate, Float64T, Float64T)                  \
  V(Word32Clz, Int32T, Word32T)                                \
  V(Word32BitwiseNot, Word32T, Word32T)                        \
  V(WordNot, WordT, WordT)                                     \
  V(Int32AbsWithOverflow, PAIR_TYPE(Int32T, BoolT), Int32T)    \
  V(Int64AbsWithOverflow, PAIR_TYPE(Int64T, BoolT), Int64T)    \
  V(IntPtrAbsWithOverflow, PAIR_TYPE(IntPtrT, BoolT), IntPtrT) \
  V(Word32BinaryNot, BoolT, Word32T)

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

  static Handle<Code> GenerateCode(CodeAssemblerState* state,
                                   const AssemblerOptions& options);

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
      static_assert(
          !std::is_same<A, MaybeObject>::value,
          "Can't cast to MaybeObject, use explicit conversion functions. ");

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
        if (std::is_same<PreviousType, MaybeObject>::value) {
          code_assembler_->GenerateCheckMaybeObjectIsObject(node_, location_);
        }
        Node* function = code_assembler_->ExternalConstant(
            ExternalReference::check_object_type());
        code_assembler_->CallCFunction(
            function, MachineType::AnyTagged(),
            std::make_pair(MachineType::AnyTagged(), node_),
            std::make_pair(MachineType::TaggedSigned(),
                           code_assembler_->SmiConstant(
                               static_cast<int>(ObjectTypeOf<A>::value))),
            std::make_pair(MachineType::AnyTagged(),
                           code_assembler_->StringConstant(location_)));
      }
#endif
      return TNode<A>::UncheckedCast(node_);
    }

    template <class A>
    operator SloppyTNode<A>() {
      return implicit_cast<TNode<A>>(*this);
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

  CheckedNode<Object, false> Cast(Node* value, const char* location = "") {
    return {value, this, location};
  }

  template <class T>
  CheckedNode<T, true> Cast(TNode<T> value, const char* location = "") {
    return {value, this, location};
  }

#ifdef DEBUG
#define STRINGIFY(x) #x
#define TO_STRING_LITERAL(x) STRINGIFY(x)
#define CAST(x) \
  Cast(x, "CAST(" #x ") at " __FILE__ ":" TO_STRING_LITERAL(__LINE__))
#define TORQUE_CAST(x) \
  ca_.Cast(x, "CAST(" #x ") at " __FILE__ ":" TO_STRING_LITERAL(__LINE__))
#else
#define CAST(x) Cast(x)
#define TORQUE_CAST(x) ca_.Cast(x)
#endif

#ifdef DEBUG
  void GenerateCheckMaybeObjectIsObject(Node* node, const char* location);
#endif

  // Constants.
  TNode<Int32T> Int32Constant(int32_t value);
  TNode<Int64T> Int64Constant(int64_t value);
  TNode<IntPtrT> IntPtrConstant(intptr_t value);
  TNode<Uint32T> Uint32Constant(uint32_t value) {
    return Unsigned(Int32Constant(bit_cast<int32_t>(value)));
  }
  TNode<UintPtrT> UintPtrConstant(uintptr_t value) {
    return Unsigned(IntPtrConstant(bit_cast<intptr_t>(value)));
  }
  TNode<RawPtrT> PointerConstant(void* value) {
    return ReinterpretCast<RawPtrT>(IntPtrConstant(bit_cast<intptr_t>(value)));
  }
  TNode<Number> NumberConstant(double value);
  TNode<Smi> SmiConstant(Smi value);
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
  TNode<BoolT> BoolConstant(bool value) {
    return value ? Int32TrueConstant() : Int32FalseConstant();
  }

  // TODO(jkummerow): The style guide wants pointers for output parameters.
  // https://google.github.io/styleguide/cppguide.html#Output_Parameters
  bool ToInt32Constant(Node* node,
                       int32_t& out_value);  // NOLINT(runtime/references)
  bool ToInt64Constant(Node* node,
                       int64_t& out_value);  // NOLINT(runtime/references)
  bool ToSmiConstant(Node* node, Smi* out_value);
  bool ToIntPtrConstant(Node* node,
                        intptr_t& out_value);  // NOLINT(runtime/references)

  bool IsUndefinedConstant(TNode<Object> node);
  bool IsNullConstant(TNode<Object> node);

  TNode<Int32T> Signed(TNode<Word32T> x) { return UncheckedCast<Int32T>(x); }
  TNode<IntPtrT> Signed(TNode<WordT> x) { return UncheckedCast<IntPtrT>(x); }
  TNode<Uint32T> Unsigned(TNode<Word32T> x) {
    return UncheckedCast<Uint32T>(x);
  }
  TNode<UintPtrT> Unsigned(TNode<WordT> x) {
    return UncheckedCast<UintPtrT>(x);
  }

  static constexpr int kTargetParameterIndex = -1;

  Node* Parameter(int value);

  TNode<Context> GetJSContextParameter();
  void Return(SloppyTNode<Object> value);
  void Return(SloppyTNode<Object> value1, SloppyTNode<Object> value2);
  void Return(SloppyTNode<Object> value1, SloppyTNode<Object> value2,
              SloppyTNode<Object> value3);
  void PopAndReturn(Node* pop, Node* value);

  void ReturnIf(Node* condition, Node* value);

  void ReturnRaw(Node* value);

  void AbortCSAAssert(Node* message);
  void DebugBreak();
  void Unreachable();
  void Comment(const char* msg) {
    if (!FLAG_code_comments) return;
    Comment(std::string(msg));
  }
  void Comment(std::string msg);
  template <class... Args>
  void Comment(Args&&... args) {
    if (!FLAG_code_comments) return;
    std::ostringstream s;
    USE((s << std::forward<Args>(args))...);
    Comment(s.str());
  }

  void StaticAssert(TNode<BoolT> value);

  void SetSourcePosition(const char* file, int line);

  void Bind(Label* label);
#if DEBUG
  void Bind(Label* label, AssemblerDebugInfo debug_info);
#endif  // DEBUG
  void Goto(Label* label);
  void GotoIf(SloppyTNode<IntegralT> condition, Label* true_label);
  void GotoIfNot(SloppyTNode<IntegralT> condition, Label* false_label);
  void Branch(SloppyTNode<IntegralT> condition, Label* true_label,
              Label* false_label);

  template <class T>
  TNode<T> Uninitialized() {
    return {};
  }

  template <class... T>
  void Bind(CodeAssemblerParameterizedLabel<T...>* label, TNode<T>*... phis) {
    Bind(label->plain_label());
    label->CreatePhis(phis...);
  }
  template <class... T, class... Args>
  void Branch(TNode<BoolT> condition,
              CodeAssemblerParameterizedLabel<T...>* if_true,
              CodeAssemblerParameterizedLabel<T...>* if_false, Args... args) {
    if_true->AddInputs(args...);
    if_false->AddInputs(args...);
    Branch(condition, if_true->plain_label(), if_false->plain_label());
  }

  template <class... T, class... Args>
  void Goto(CodeAssemblerParameterizedLabel<T...>* label, Args... args) {
    label->AddInputs(args...);
    Goto(label->plain_label());
  }

  void Branch(TNode<BoolT> condition, const std::function<void()>& true_body,
              const std::function<void()>& false_body);
  void Branch(TNode<BoolT> condition, Label* true_label,
              const std::function<void()>& false_body);
  void Branch(TNode<BoolT> condition, const std::function<void()>& true_body,
              Label* false_label);

  void Switch(Node* index, Label* default_label, const int32_t* case_values,
              Label** case_labels, size_t case_count);

  // Access to the frame pointer
  TNode<RawPtrT> LoadFramePointer();
  TNode<RawPtrT> LoadParentFramePointer();

  // Access to the stack pointer
  TNode<RawPtrT> LoadStackPointer();

  // Poison |value| on speculative paths.
  TNode<Object> TaggedPoisonOnSpeculation(SloppyTNode<Object> value);
  TNode<WordT> WordPoisonOnSpeculation(SloppyTNode<WordT> value);

  // Load raw memory location.
  Node* Load(MachineType type, Node* base,
             LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);
  template <class Type>
  TNode<Type> Load(MachineType type, TNode<RawPtr<Type>> base) {
    DCHECK(
        IsSubtype(type.representation(), MachineRepresentationOf<Type>::value));
    return UncheckedCast<Type>(Load(type, static_cast<Node*>(base)));
  }
  Node* Load(MachineType type, Node* base, Node* offset,
             LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);
  Node* AtomicLoad(MachineType type, Node* base, Node* offset);
  // Load uncompressed tagged value from (most likely off JS heap) memory
  // location.
  Node* LoadFullTagged(
      Node* base, LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);
  Node* LoadFullTagged(
      Node* base, Node* offset,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);

  Node* LoadFromObject(MachineType type, TNode<HeapObject> object,
                       TNode<IntPtrT> offset);

  // Load a value from the root array.
  TNode<Object> LoadRoot(RootIndex root_index);

  // Store value to raw memory location.
  Node* Store(Node* base, Node* value);
  Node* Store(Node* base, Node* offset, Node* value);
  Node* StoreEphemeronKey(Node* base, Node* offset, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* offset,
                            Node* value);
  Node* UnsafeStoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                  Node* value);
  Node* UnsafeStoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                  Node* offset, Node* value);

  // Stores uncompressed tagged value to (most likely off JS heap) memory
  // location without write barrier.
  Node* StoreFullTaggedNoWriteBarrier(Node* base, Node* tagged_value);
  Node* StoreFullTaggedNoWriteBarrier(Node* base, Node* offset,
                                      Node* tagged_value);

  // Optimized memory operations that map to Turbofan simplified nodes.
  TNode<HeapObject> OptimizedAllocate(TNode<IntPtrT> size,
                                      AllocationType allocation,
                                      AllowLargeObjects allow_large_objects);
  void StoreToObject(MachineRepresentation rep, TNode<HeapObject> object,
                     TNode<IntPtrT> offset, Node* value,
                     StoreToObjectWriteBarrier write_barrier);
  void OptimizedStoreField(MachineRepresentation rep, TNode<HeapObject> object,
                           int offset, Node* value);
  void OptimizedStoreFieldAssertNoWriteBarrier(MachineRepresentation rep,
                                               TNode<HeapObject> object,
                                               int offset, Node* value);
  void OptimizedStoreFieldUnsafeNoWriteBarrier(MachineRepresentation rep,
                                               TNode<HeapObject> object,
                                               int offset, Node* value);
  void OptimizedStoreMap(TNode<HeapObject> object, TNode<Map>);
  // {value_high} is used for 64-bit stores on 32-bit platforms, must be
  // nullptr in other cases.
  Node* AtomicStore(MachineRepresentation rep, Node* base, Node* offset,
                    Node* value, Node* value_high = nullptr);

  // Exchange value at raw memory location
  Node* AtomicExchange(MachineType type, Node* base, Node* offset, Node* value,
                       Node* value_high = nullptr);

  // Compare and Exchange value at raw memory location
  Node* AtomicCompareExchange(MachineType type, Node* base, Node* offset,
                              Node* old_value, Node* new_value,
                              Node* old_value_high = nullptr,
                              Node* new_value_high = nullptr);

  Node* AtomicAdd(MachineType type, Node* base, Node* offset, Node* value,
                  Node* value_high = nullptr);

  Node* AtomicSub(MachineType type, Node* base, Node* offset, Node* value,
                  Node* value_high = nullptr);

  Node* AtomicAnd(MachineType type, Node* base, Node* offset, Node* value,
                  Node* value_high = nullptr);

  Node* AtomicOr(MachineType type, Node* base, Node* offset, Node* value,
                 Node* value_high = nullptr);

  Node* AtomicXor(MachineType type, Node* base, Node* offset, Node* value,
                  Node* value_high = nullptr);

  // Store a value to the root array.
  Node* StoreRoot(RootIndex root_index, Node* value);

// Basic arithmetic operations.
#define DECLARE_CODE_ASSEMBLER_BINARY_OP(name, ResType, Arg1Type, Arg2Type) \
  TNode<ResType> name(SloppyTNode<Arg1Type> a, SloppyTNode<Arg2Type> b);
  CODE_ASSEMBLER_BINARY_OP_LIST(DECLARE_CODE_ASSEMBLER_BINARY_OP)
#undef DECLARE_CODE_ASSEMBLER_BINARY_OP

  TNode<UintPtrT> WordShr(TNode<UintPtrT> left, TNode<IntegralT> right) {
    return Unsigned(
        WordShr(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<IntPtrT> WordSar(TNode<IntPtrT> left, TNode<IntegralT> right) {
    return Signed(WordSar(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<IntPtrT> WordShl(TNode<IntPtrT> left, TNode<IntegralT> right) {
    return Signed(WordShl(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<UintPtrT> WordShl(TNode<UintPtrT> left, TNode<IntegralT> right) {
    return Unsigned(
        WordShl(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Int32T> Word32Shl(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Word32Shl(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<Uint32T> Word32Shl(TNode<Uint32T> left, TNode<Uint32T> right) {
    return Unsigned(
        Word32Shl(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<Uint32T> Word32Shr(TNode<Uint32T> left, TNode<Uint32T> right) {
    return Unsigned(
        Word32Shr(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<IntPtrT> WordAnd(TNode<IntPtrT> left, TNode<IntPtrT> right) {
    return Signed(WordAnd(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<UintPtrT> WordAnd(TNode<UintPtrT> left, TNode<UintPtrT> right) {
    return Unsigned(
        WordAnd(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Int32T> Word32And(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Word32And(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<Uint32T> Word32And(TNode<Uint32T> left, TNode<Uint32T> right) {
    return Unsigned(
        Word32And(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Int32T> Word32Or(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Word32Or(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<Uint32T> Word32Or(TNode<Uint32T> left, TNode<Uint32T> right) {
    return Unsigned(
        Word32Or(static_cast<Node*>(left), static_cast<Node*>(right)));
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

  TNode<BoolT> IntPtrEqual(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<BoolT> WordEqual(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<BoolT> WordNotEqual(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<BoolT> Word32Equal(SloppyTNode<Word32T> left,
                           SloppyTNode<Word32T> right);
  TNode<BoolT> Word32NotEqual(SloppyTNode<Word32T> left,
                              SloppyTNode<Word32T> right);
  TNode<BoolT> Word64Equal(SloppyTNode<Word64T> left,
                           SloppyTNode<Word64T> right);
  TNode<BoolT> Word64NotEqual(SloppyTNode<Word64T> left,
                              SloppyTNode<Word64T> right);

  TNode<BoolT> Word32Or(TNode<BoolT> left, TNode<BoolT> right) {
    return UncheckedCast<BoolT>(
        Word32Or(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<BoolT> Word32And(TNode<BoolT> left, TNode<BoolT> right) {
    return UncheckedCast<BoolT>(
        Word32And(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Int32T> Int32Add(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Int32Add(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Uint32T> Uint32Add(TNode<Uint32T> left, TNode<Uint32T> right) {
    return Unsigned(
        Int32Add(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Int32T> Int32Sub(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Int32Sub(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<Int32T> Int32Mul(TNode<Int32T> left, TNode<Int32T> right) {
    return Signed(
        Int32Mul(static_cast<Node*>(left), static_cast<Node*>(right)));
  }

  TNode<WordT> IntPtrAdd(SloppyTNode<WordT> left, SloppyTNode<WordT> right);
  TNode<IntPtrT> IntPtrDiv(TNode<IntPtrT> left, TNode<IntPtrT> right);
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
  TNode<UintPtrT> UintPtrAdd(TNode<UintPtrT> left, TNode<UintPtrT> right) {
    return Unsigned(
        IntPtrAdd(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<UintPtrT> UintPtrSub(TNode<UintPtrT> left, TNode<UintPtrT> right) {
    return Unsigned(
        IntPtrSub(static_cast<Node*>(left), static_cast<Node*>(right)));
  }
  TNode<RawPtrT> RawPtrAdd(TNode<RawPtrT> left, TNode<IntPtrT> right) {
    return ReinterpretCast<RawPtrT>(IntPtrAdd(left, right));
  }
  TNode<RawPtrT> RawPtrAdd(TNode<IntPtrT> left, TNode<RawPtrT> right) {
    return ReinterpretCast<RawPtrT>(IntPtrAdd(left, right));
  }

  TNode<WordT> WordShl(SloppyTNode<WordT> value, int shift);
  TNode<WordT> WordShr(SloppyTNode<WordT> value, int shift);
  TNode<WordT> WordSar(SloppyTNode<WordT> value, int shift);
  TNode<IntPtrT> WordShr(TNode<IntPtrT> value, int shift) {
    return UncheckedCast<IntPtrT>(WordShr(static_cast<Node*>(value), shift));
  }
  TNode<IntPtrT> WordSar(TNode<IntPtrT> value, int shift) {
    return UncheckedCast<IntPtrT>(WordSar(static_cast<Node*>(value), shift));
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

  template <class Dummy = void>
  TNode<IntPtrT> BitcastTaggedToWord(TNode<Smi> node) {
    static_assert(sizeof(Dummy) < 0,
                  "Should use BitcastTaggedSignedToWord instead.");
  }

  // Changes a double to an inptr_t for pointer arithmetic outside of Smi range.
  // Assumes that the double can be exactly represented as an int.
  TNode<UintPtrT> ChangeFloat64ToUintPtr(SloppyTNode<Float64T> value);
  // Same in the opposite direction.
  TNode<Float64T> ChangeUintPtrToFloat64(TNode<UintPtrT> value);

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
  TNode<Object> CallRuntime(Runtime::FunctionId function,
                            SloppyTNode<Object> context, TArgs... args) {
    return CallRuntimeImpl(function, context,
                           {implicit_cast<SloppyTNode<Object>>(args)...});
  }

  template <class... TArgs>
  TNode<Object> CallRuntimeWithCEntry(Runtime::FunctionId function,
                                      TNode<Code> centry,
                                      SloppyTNode<Object> context,
                                      TArgs... args) {
    return CallRuntimeWithCEntryImpl(function, centry, context, {args...});
  }

  template <class... TArgs>
  void TailCallRuntime(Runtime::FunctionId function,
                       SloppyTNode<Object> context, TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    TNode<Int32T> arity = Int32Constant(argc);
    return TailCallRuntimeImpl(function, arity, context,
                               {implicit_cast<SloppyTNode<Object>>(args)...});
  }

  template <class... TArgs>
  void TailCallRuntime(Runtime::FunctionId function, TNode<Int32T> arity,
                       SloppyTNode<Object> context, TArgs... args) {
    return TailCallRuntimeImpl(function, arity, context,
                               {implicit_cast<SloppyTNode<Object>>(args)...});
  }

  template <class... TArgs>
  void TailCallRuntimeWithCEntry(Runtime::FunctionId function,
                                 TNode<Code> centry, TNode<Object> context,
                                 TArgs... args) {
    int argc = sizeof...(args);
    TNode<Int32T> arity = Int32Constant(argc);
    return TailCallRuntimeWithCEntryImpl(
        function, arity, centry, context,
        {implicit_cast<SloppyTNode<Object>>(args)...});
  }

  //
  // If context passed to CallStub is nullptr, it won't be passed to the stub.
  //

  template <class T = Object, class... TArgs>
  TNode<T> CallStub(Callable const& callable, SloppyTNode<Object> context,
                    TArgs... args) {
    TNode<Code> target = HeapConstant(callable.code());
    return CallStub<T>(callable.descriptor(), target, context, args...);
  }

  template <class T = Object, class... TArgs>
  TNode<T> CallStub(const CallInterfaceDescriptor& descriptor,
                    SloppyTNode<Code> target, SloppyTNode<Object> context,
                    TArgs... args) {
    return UncheckedCast<T>(CallStubR(StubCallMode::kCallCodeObject, descriptor,
                                      1, target, context, args...));
  }

  template <class... TArgs>
  Node* CallStubR(StubCallMode call_mode,
                  const CallInterfaceDescriptor& descriptor, size_t result_size,
                  SloppyTNode<Object> target, SloppyTNode<Object> context,
                  TArgs... args) {
    return CallStubRImpl(call_mode, descriptor, result_size, target, context,
                         {args...});
  }

  Node* CallStubN(StubCallMode call_mode,
                  const CallInterfaceDescriptor& descriptor, size_t result_size,
                  int input_count, Node* const* inputs);

  template <class T = Object, class... TArgs>
  TNode<T> CallBuiltinPointer(const CallInterfaceDescriptor& descriptor,
                              TNode<BuiltinPtr> target, TNode<Object> context,
                              TArgs... args) {
    return UncheckedCast<T>(CallStubR(StubCallMode::kCallBuiltinPointer,
                                      descriptor, 1, target, context, args...));
  }

  template <class... TArgs>
  void TailCallStub(Callable const& callable, SloppyTNode<Object> context,
                    TArgs... args) {
    TNode<Code> target = HeapConstant(callable.code());
    return TailCallStub(callable.descriptor(), target, context, args...);
  }

  template <class... TArgs>
  void TailCallStub(const CallInterfaceDescriptor& descriptor,
                    SloppyTNode<Code> target, SloppyTNode<Object> context,
                    TArgs... args) {
    return TailCallStubImpl(descriptor, target, context, {args...});
  }

  template <class... TArgs>
  Node* TailCallBytecodeDispatch(const CallInterfaceDescriptor& descriptor,
                                 Node* target, TArgs... args);

  template <class... TArgs>
  Node* TailCallStubThenBytecodeDispatch(
      const CallInterfaceDescriptor& descriptor, Node* target, Node* context,
      TArgs... args) {
    return TailCallStubThenBytecodeDispatchImpl(descriptor, target, context,
                                                {args...});
  }

  // Tailcalls to the given code object with JSCall linkage. The JS arguments
  // (including receiver) are supposed to be already on the stack.
  // This is a building block for implementing trampoline stubs that are
  // installed instead of code objects with JSCall linkage.
  // Note that no arguments adaption is going on here - all the JavaScript
  // arguments are left on the stack unmodified. Therefore, this tail call can
  // only be used after arguments adaptation has been performed already.
  TNode<Object> TailCallJSCode(TNode<Code> code, TNode<Context> context,
                               TNode<JSFunction> function,
                               TNode<Object> new_target,
                               TNode<Int32T> arg_count);

  template <class... TArgs>
  Node* CallJS(Callable const& callable, Node* context, Node* function,
               Node* receiver, TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    Node* arity = Int32Constant(argc);
    return CallStub(callable, context, function, arity, receiver, args...);
  }

  template <class... TArgs>
  Node* ConstructJSWithTarget(Callable const& callable, Node* context,
                              Node* target, Node* new_target, TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    Node* arity = Int32Constant(argc);
    Node* receiver = LoadRoot(RootIndex::kUndefinedValue);

    // Construct(target, new_target, arity, receiver, arguments...)
    return CallStub(callable, context, target, new_target, arity, receiver,
                    args...);
  }
  template <class... TArgs>
  Node* ConstructJS(Callable const& callable, Node* context, Node* new_target,
                    TArgs... args) {
    return ConstructJSWithTarget(callable, context, new_target, new_target,
                                 args...);
  }

  Node* CallCFunctionN(Signature<MachineType>* signature, int input_count,
                       Node* const* inputs);

  // Type representing C function argument with type info.
  using CFunctionArg = std::pair<MachineType, Node*>;

  // Call to a C function.
  template <class... CArgs>
  Node* CallCFunction(Node* function, MachineType return_type, CArgs... cargs) {
    static_assert(v8::internal::conjunction<
                      std::is_convertible<CArgs, CFunctionArg>...>::value,
                  "invalid argument types");
    return CallCFunction(function, return_type, {cargs...});
  }

  // Call to a C function, while saving/restoring caller registers.
  template <class... CArgs>
  Node* CallCFunctionWithCallerSavedRegisters(Node* function,
                                              MachineType return_type,
                                              SaveFPRegsMode mode,
                                              CArgs... cargs) {
    static_assert(v8::internal::conjunction<
                      std::is_convertible<CArgs, CFunctionArg>...>::value,
                  "invalid argument types");
    return CallCFunctionWithCallerSavedRegisters(function, return_type, mode,
                                                 {cargs...});
  }

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

  bool IsExceptionHandlerActive() const;

 protected:
  void RegisterCallGenerationCallbacks(
      const CodeAssemblerCallback& call_prologue,
      const CodeAssemblerCallback& call_epilogue);
  void UnregisterCallGenerationCallbacks();

  bool Word32ShiftIsSafe() const;
  PoisoningMitigationLevel poisoning_level() const;

  bool IsJSFunctionCall() const;

 private:
  void HandleException(Node* result);

  Node* CallCFunction(Node* function, MachineType return_type,
                      std::initializer_list<CFunctionArg> args);

  Node* CallCFunctionWithCallerSavedRegisters(
      Node* function, MachineType return_type, SaveFPRegsMode mode,
      std::initializer_list<CFunctionArg> args);

  TNode<Object> CallRuntimeImpl(Runtime::FunctionId function,
                                TNode<Object> context,
                                std::initializer_list<TNode<Object>> args);

  TNode<Object> CallRuntimeWithCEntryImpl(
      Runtime::FunctionId function, TNode<Code> centry, TNode<Object> context,
      std::initializer_list<TNode<Object>> args);

  void TailCallRuntimeImpl(Runtime::FunctionId function, TNode<Int32T> arity,
                           TNode<Object> context,
                           std::initializer_list<TNode<Object>> args);

  void TailCallRuntimeWithCEntryImpl(Runtime::FunctionId function,
                                     TNode<Int32T> arity, TNode<Code> centry,
                                     TNode<Object> context,
                                     std::initializer_list<TNode<Object>> args);

  void TailCallStubImpl(const CallInterfaceDescriptor& descriptor,
                        TNode<Code> target, TNode<Object> context,
                        std::initializer_list<Node*> args);

  Node* TailCallStubThenBytecodeDispatchImpl(
      const CallInterfaceDescriptor& descriptor, Node* target, Node* context,
      std::initializer_list<Node*> args);

  Node* CallStubRImpl(StubCallMode call_mode,
                      const CallInterfaceDescriptor& descriptor,
                      size_t result_size, Node* target,
                      SloppyTNode<Object> context,
                      std::initializer_list<Node*> args);

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

class V8_EXPORT_PRIVATE CodeAssemblerVariable {
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
  struct ImplComparator {
    bool operator()(const CodeAssemblerVariable::Impl* a,
                    const CodeAssemblerVariable::Impl* b) const;
  };
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

class V8_EXPORT_PRIVATE CodeAssemblerLabel {
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
  inline bool is_used() const { return merge_count_ != 0; }

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
  std::map<CodeAssemblerVariable::Impl*, Node*,
           CodeAssemblerVariable::ImplComparator>
      variable_phis_;
  // Map of variables to the list of value nodes that have been added from each
  // merge path in their order of merging.
  std::map<CodeAssemblerVariable::Impl*, std::vector<Node*>,
           CodeAssemblerVariable::ImplComparator>
      variable_merges_;

  // Cannot be copied because the destructor explicitly call the destructor of
  // the underlying {RawMachineLabel}, hence only one pointer can point to it.
  DISALLOW_COPY_AND_ASSIGN(CodeAssemblerLabel);
};

class CodeAssemblerParameterizedLabelBase {
 public:
  bool is_used() const { return plain_label_.is_used(); }
  explicit CodeAssemblerParameterizedLabelBase(CodeAssembler* assembler,
                                               size_t arity,
                                               CodeAssemblerLabel::Type type)
      : state_(assembler->state()),
        phi_inputs_(arity),
        plain_label_(assembler, type) {}

 protected:
  CodeAssemblerLabel* plain_label() { return &plain_label_; }
  void AddInputs(std::vector<Node*> inputs);
  Node* CreatePhi(MachineRepresentation rep, const std::vector<Node*>& inputs);
  const std::vector<Node*>& CreatePhis(
      std::vector<MachineRepresentation> representations);

 private:
  CodeAssemblerState* state_;
  std::vector<std::vector<Node*>> phi_inputs_;
  std::vector<Node*> phi_nodes_;
  CodeAssemblerLabel plain_label_;
};

template <class... Types>
class CodeAssemblerParameterizedLabel
    : public CodeAssemblerParameterizedLabelBase {
 public:
  static constexpr size_t kArity = sizeof...(Types);
  explicit CodeAssemblerParameterizedLabel(CodeAssembler* assembler,
                                           CodeAssemblerLabel::Type type)
      : CodeAssemblerParameterizedLabelBase(assembler, kArity, type) {}

 private:
  friend class CodeAssembler;

  void AddInputs(TNode<Types>... inputs) {
    CodeAssemblerParameterizedLabelBase::AddInputs(
        std::vector<Node*>{inputs...});
  }
  void CreatePhis(TNode<Types>*... results) {
    const std::vector<Node*>& phi_nodes =
        CodeAssemblerParameterizedLabelBase::CreatePhis(
            {MachineRepresentationOf<Types>::value...});
    auto it = phi_nodes.begin();
    USE(it);
    ITERATE_PACK(AssignPhi(results, *(it++)));
  }
  template <class T>
  static void AssignPhi(TNode<T>* result, Node* phi) {
    if (phi != nullptr) *result = TNode<T>::UncheckedCast(phi);
  }
};

using CodeAssemblerExceptionHandlerLabel =
    CodeAssemblerParameterizedLabel<Object>;

class V8_EXPORT_PRIVATE CodeAssemblerState {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeAssemblerState(Isolate* isolate, Zone* zone,
                     const CallInterfaceDescriptor& descriptor, Code::Kind kind,
                     const char* name, PoisoningMitigationLevel poisoning_level,
                     int32_t builtin_index = Builtins::kNoBuiltinId);

  // Create with JSCall linkage.
  CodeAssemblerState(Isolate* isolate, Zone* zone, int parameter_count,
                     Code::Kind kind, const char* name,
                     PoisoningMitigationLevel poisoning_level,
                     int32_t builtin_index = Builtins::kNoBuiltinId);

  ~CodeAssemblerState();

  const char* name() const { return name_; }
  int parameter_count() const;

#if DEBUG
  void PrintCurrentBlock(std::ostream& os);
#endif  // DEBUG
  bool InsideBlock();
  void SetInitialDebugInformation(const char* msg, const char* file, int line);

 private:
  friend class CodeAssembler;
  friend class CodeAssemblerLabel;
  friend class CodeAssemblerVariable;
  friend class CodeAssemblerTester;
  friend class CodeAssemblerParameterizedLabelBase;
  friend class CodeAssemblerScopedExceptionHandler;

  CodeAssemblerState(Isolate* isolate, Zone* zone,
                     CallDescriptor* call_descriptor, Code::Kind kind,
                     const char* name, PoisoningMitigationLevel poisoning_level,
                     int32_t builtin_index);

  void PushExceptionHandler(CodeAssemblerExceptionHandlerLabel* label);
  void PopExceptionHandler();

  std::unique_ptr<RawMachineAssembler> raw_assembler_;
  Code::Kind kind_;
  const char* name_;
  int32_t builtin_index_;
  bool code_generated_;
  ZoneSet<CodeAssemblerVariable::Impl*, CodeAssemblerVariable::ImplComparator>
      variables_;
  CodeAssemblerCallback call_prologue_;
  CodeAssemblerCallback call_epilogue_;
  std::vector<CodeAssemblerExceptionHandlerLabel*> exception_handler_labels_;
  using VariableId = uint32_t;
  VariableId next_variable_id_ = 0;

  VariableId NextVariableId() { return next_variable_id_++; }

  DISALLOW_COPY_AND_ASSIGN(CodeAssemblerState);
};

class V8_EXPORT_PRIVATE CodeAssemblerScopedExceptionHandler {
 public:
  CodeAssemblerScopedExceptionHandler(
      CodeAssembler* assembler, CodeAssemblerExceptionHandlerLabel* label);

  // Use this constructor for compatability/ports of old CSA code only. New code
  // should use the CodeAssemblerExceptionHandlerLabel version.
  CodeAssemblerScopedExceptionHandler(
      CodeAssembler* assembler, CodeAssemblerLabel* label,
      TypedCodeAssemblerVariable<Object>* exception);

  ~CodeAssemblerScopedExceptionHandler();

 private:
  bool has_handler_;
  CodeAssembler* assembler_;
  CodeAssemblerLabel* compatibility_label_;
  std::unique_ptr<CodeAssemblerExceptionHandlerLabel> label_;
  TypedCodeAssemblerVariable<Object>* exception_;
};

}  // namespace compiler

#if defined(V8_HOST_ARCH_32_BIT)
using BInt = Smi;
#elif defined(V8_HOST_ARCH_64_BIT)
using BInt = IntPtrT;
#else
#error Unknown architecture.
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_ASSEMBLER_H_
