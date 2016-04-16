// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPES_H_
#define V8_TYPES_H_

#include "src/conversions.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

// SUMMARY
//
// A simple type system for compiler-internal use. It is based entirely on
// union types, and all subtyping hence amounts to set inclusion. Besides the
// obvious primitive types and some predefined unions, the type language also
// can express class types (a.k.a. specific maps) and singleton types (i.e.,
// concrete constants).
//
// Types consist of two dimensions: semantic (value range) and representation.
// Both are related through subtyping.
//
//
// SEMANTIC DIMENSION
//
// The following equations and inequations hold for the semantic axis:
//
//   None <= T
//   T <= Any
//
//   Number = Signed32 \/ Unsigned32 \/ Double
//   Smi <= Signed32
//   Name = String \/ Symbol
//   UniqueName = InternalizedString \/ Symbol
//   InternalizedString < String
//
//   Receiver = Object \/ Proxy
//   Array < Object
//   Function < Object
//   RegExp < Object
//   Undetectable < Object
//   Detectable = Receiver \/ Number \/ Name - Undetectable
//
//   Class(map) < T   iff instance_type(map) < T
//   Constant(x) < T  iff instance_type(map(x)) < T
//   Array(T) < Array
//   Function(R, S, T0, T1, ...) < Function
//   Context(T) < Internal
//
// Both structural Array and Function types are invariant in all parameters;
// relaxing this would make Union and Intersect operations more involved.
// There is no subtyping relation between Array, Function, or Context types
// and respective Constant types, since these types cannot be reconstructed
// for arbitrary heap values.
// Note also that Constant(x) < Class(map(x)) does _not_ hold, since x's map can
// change! (Its instance type cannot, however.)
// TODO(rossberg): the latter is not currently true for proxies, because of fix,
// but will hold once we implement direct proxies.
// However, we also define a 'temporal' variant of the subtyping relation that
// considers the _current_ state only, i.e., Constant(x) <_now Class(map(x)).
//
//
// REPRESENTATIONAL DIMENSION
//
// For the representation axis, the following holds:
//
//   None <= R
//   R <= Any
//
//   UntaggedInt = UntaggedInt1 \/ UntaggedInt8 \/
//                 UntaggedInt16 \/ UntaggedInt32
//   UntaggedFloat = UntaggedFloat32 \/ UntaggedFloat64
//   UntaggedNumber = UntaggedInt \/ UntaggedFloat
//   Untagged = UntaggedNumber \/ UntaggedPtr
//   Tagged = TaggedInt \/ TaggedPtr
//
// Subtyping relates the two dimensions, for example:
//
//   Number <= Tagged \/ UntaggedNumber
//   Object <= TaggedPtr \/ UntaggedPtr
//
// That holds because the semantic type constructors defined by the API create
// types that allow for all possible representations, and dually, the ones for
// representation types initially include all semantic ranges. Representations
// can then e.g. be narrowed for a given semantic type using intersection:
//
//   SignedSmall /\ TaggedInt       (a 'smi')
//   Number /\ TaggedPtr            (a heap number)
//
//
// RANGE TYPES
//
// A range type represents a continuous integer interval by its minimum and
// maximum value.  Either value may be an infinity, in which case that infinity
// itself is also included in the range.   A range never contains NaN or -0.
//
// If a value v happens to be an integer n, then Constant(v) is considered a
// subtype of Range(n, n) (and therefore also a subtype of any larger range).
// In order to avoid large unions, however, it is usually a good idea to use
// Range rather than Constant.
//
//
// PREDICATES
//
// There are two main functions for testing types:
//
//   T1->Is(T2)     -- tests whether T1 is included in T2 (i.e., T1 <= T2)
//   T1->Maybe(T2)  -- tests whether T1 and T2 overlap (i.e., T1 /\ T2 =/= 0)
//
// Typically, the former is to be used to select representations (e.g., via
// T->Is(SignedSmall())), and the latter to check whether a specific case needs
// handling (e.g., via T->Maybe(Number())).
//
// There is no functionality to discover whether a type is a leaf in the
// lattice. That is intentional. It should always be possible to refine the
// lattice (e.g., splitting up number types further) without invalidating any
// existing assumptions or tests.
// Consequently, do not normally use Equals for type tests, always use Is!
//
// The NowIs operator implements state-sensitive subtying, as described above.
// Any compilation decision based on such temporary properties requires runtime
// guarding!
//
//
// PROPERTIES
//
// Various formal properties hold for constructors, operators, and predicates
// over types. For example, constructors are injective and subtyping is a
// complete partial order.
//
// See test/cctest/test-types.cc for a comprehensive executable specification,
// especially with respect to the properties of the more exotic 'temporal'
// constructors and predicates (those prefixed 'Now').
//
//
// IMPLEMENTATION
//
// Internally, all 'primitive' types, and their unions, are represented as
// bitsets. Bit 0 is reserved for tagging. Class is a heap pointer to the
// respective map. Only structured types require allocation.
// Note that the bitset representation is closed under both Union and Intersect.


// -----------------------------------------------------------------------------
// Values for bitset types

// clang-format off

#define MASK_BITSET_TYPE_LIST(V) \
  V(Representation, 0xffc00000u) \
  V(Semantic,       0x003ffffeu)

#define REPRESENTATION(k) ((k) & BitsetType::kRepresentation)
#define SEMANTIC(k)       ((k) & BitsetType::kSemantic)

#define REPRESENTATION_BITSET_TYPE_LIST(V)    \
  V(None,               0)                    \
  V(UntaggedBit,        1u << 22 | kSemantic) \
  V(UntaggedIntegral8,  1u << 23 | kSemantic) \
  V(UntaggedIntegral16, 1u << 24 | kSemantic) \
  V(UntaggedIntegral32, 1u << 25 | kSemantic) \
  V(UntaggedFloat32,    1u << 26 | kSemantic) \
  V(UntaggedFloat64,    1u << 27 | kSemantic) \
  V(UntaggedSimd128,    1u << 28 | kSemantic) \
  V(UntaggedPointer,    1u << 29 | kSemantic) \
  V(TaggedSigned,       1u << 30 | kSemantic) \
  V(TaggedPointer,      1u << 31 | kSemantic) \
  \
  V(UntaggedIntegral,   kUntaggedBit | kUntaggedIntegral8 |        \
                        kUntaggedIntegral16 | kUntaggedIntegral32) \
  V(UntaggedFloat,      kUntaggedFloat32 | kUntaggedFloat64)       \
  V(UntaggedNumber,     kUntaggedIntegral | kUntaggedFloat)        \
  V(Untagged,           kUntaggedNumber | kUntaggedPointer)        \
  V(Tagged,             kTaggedSigned | kTaggedPointer)

#define INTERNAL_BITSET_TYPE_LIST(V)                                      \
  V(OtherUnsigned31, 1u << 1 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherUnsigned32, 1u << 2 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherSigned32,   1u << 3 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherNumber,     1u << 4 | REPRESENTATION(kTagged | kUntaggedNumber))

#define SEMANTIC_BITSET_TYPE_LIST(V) \
  V(Negative31,          1u << 5  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Null,                1u << 6  | REPRESENTATION(kTaggedPointer)) \
  V(Undefined,           1u << 7  | REPRESENTATION(kTaggedPointer)) \
  V(Boolean,             1u << 8  | REPRESENTATION(kTaggedPointer)) \
  V(Unsigned30,          1u << 9  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(MinusZero,           1u << 10 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(NaN,                 1u << 11 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Symbol,              1u << 12 | REPRESENTATION(kTaggedPointer)) \
  V(InternalizedString,  1u << 13 | REPRESENTATION(kTaggedPointer)) \
  V(OtherString,         1u << 14 | REPRESENTATION(kTaggedPointer)) \
  V(Simd,                1u << 15 | REPRESENTATION(kTaggedPointer)) \
  V(Undetectable,        1u << 16 | REPRESENTATION(kTaggedPointer)) \
  V(OtherObject,         1u << 17 | REPRESENTATION(kTaggedPointer)) \
  V(Proxy,               1u << 18 | REPRESENTATION(kTaggedPointer)) \
  V(Function,            1u << 19 | REPRESENTATION(kTaggedPointer)) \
  V(Internal,            1u << 20 | REPRESENTATION(kTagged | kUntagged)) \
  \
  V(Signed31,                 kUnsigned30 | kNegative31) \
  V(Signed32,                 kSigned31 | kOtherUnsigned31 | kOtherSigned32) \
  V(Negative32,               kNegative31 | kOtherSigned32) \
  V(Unsigned31,               kUnsigned30 | kOtherUnsigned31) \
  V(Unsigned32,               kUnsigned30 | kOtherUnsigned31 | \
                              kOtherUnsigned32) \
  V(Integral32,               kSigned32 | kUnsigned32) \
  V(PlainNumber,              kIntegral32 | kOtherNumber) \
  V(OrderedNumber,            kPlainNumber | kMinusZero) \
  V(MinusZeroOrNaN,           kMinusZero | kNaN) \
  V(Number,                   kOrderedNumber | kNaN) \
  V(String,                   kInternalizedString | kOtherString) \
  V(UniqueName,               kSymbol | kInternalizedString) \
  V(Name,                     kSymbol | kString) \
  V(BooleanOrNumber,          kBoolean | kNumber) \
  V(BooleanOrNullOrUndefined, kBoolean | kNull | kUndefined) \
  V(NullOrUndefined,          kNull | kUndefined) \
  V(NumberOrString,           kNumber | kString) \
  V(NumberOrUndefined,        kNumber | kUndefined) \
  V(PlainPrimitive,           kNumberOrString | kBoolean | kNullOrUndefined) \
  V(Primitive,                kSymbol | kSimd | kPlainPrimitive) \
  V(DetectableReceiver,       kFunction | kOtherObject | kProxy) \
  V(Detectable,               kDetectableReceiver | kNumber | kName) \
  V(Object,                   kFunction | kOtherObject | kUndetectable) \
  V(Receiver,                 kObject | kProxy) \
  V(StringOrReceiver,         kString | kReceiver) \
  V(Unique,                   kBoolean | kUniqueName | kNull | kUndefined | \
                              kReceiver) \
  V(NonNumber,                kUnique | kString | kInternal) \
  V(Any,                      0xfffffffeu)

// clang-format on

/*
 * The following diagrams show how integers (in the mathematical sense) are
 * divided among the different atomic numerical types.
 *
 *   ON    OS32     N31     U30     OU31    OU32     ON
 * ______[_______[_______[_______[_______[_______[_______
 *     -2^31   -2^30     0      2^30    2^31    2^32
 *
 * E.g., OtherUnsigned32 (OU32) covers all integers from 2^31 to 2^32-1.
 *
 * Some of the atomic numerical bitsets are internal only (see
 * INTERNAL_BITSET_TYPE_LIST).  To a types user, they should only occur in
 * union with certain other bitsets.  For instance, OtherNumber should only
 * occur as part of PlainNumber.
 */

#define PROPER_BITSET_TYPE_LIST(V) \
  REPRESENTATION_BITSET_TYPE_LIST(V) \
  SEMANTIC_BITSET_TYPE_LIST(V)

#define BITSET_TYPE_LIST(V)          \
  MASK_BITSET_TYPE_LIST(V)           \
  REPRESENTATION_BITSET_TYPE_LIST(V) \
  INTERNAL_BITSET_TYPE_LIST(V)       \
  SEMANTIC_BITSET_TYPE_LIST(V)

class Type;

// -----------------------------------------------------------------------------
// Bitset types (internal).

class BitsetType {
 public:
  typedef uint32_t bitset;  // Internal

  enum : uint32_t {
#define DECLARE_TYPE(type, value) k##type = (value),
    BITSET_TYPE_LIST(DECLARE_TYPE)
#undef DECLARE_TYPE
        kUnusedEOL = 0
  };

  static bitset SignedSmall();
  static bitset UnsignedSmall();

  bitset Bitset() {
    return static_cast<bitset>(reinterpret_cast<uintptr_t>(this) ^ 1u);
  }

  static bool IsInhabited(bitset bits) {
    return SEMANTIC(bits) != kNone && REPRESENTATION(bits) != kNone;
  }

  static bool SemanticIsInhabited(bitset bits) {
    return SEMANTIC(bits) != kNone;
  }

  static bool Is(bitset bits1, bitset bits2) {
    return (bits1 | bits2) == bits2;
  }

  static double Min(bitset);
  static double Max(bitset);

  static bitset Glb(Type* type);  // greatest lower bound that's a bitset
  static bitset Glb(double min, double max);
  static bitset Lub(Type* type);  // least upper bound that's a bitset
  static bitset Lub(i::Map* map);
  static bitset Lub(i::Object* value);
  static bitset Lub(double value);
  static bitset Lub(double min, double max);
  static bitset ExpandInternals(bitset bits);

  static const char* Name(bitset);
  static void Print(std::ostream& os, bitset);  // NOLINT
#ifdef DEBUG
  static void Print(bitset);
#endif

  static bitset NumberBits(bitset bits);

  static bool IsBitset(Type* type) {
    return reinterpret_cast<uintptr_t>(type) & 1;
  }

  static Type* NewForTesting(bitset bits) { return New(bits); }

 private:
  friend class Type;

  static Type* New(bitset bits) {
    return reinterpret_cast<Type*>(static_cast<uintptr_t>(bits | 1u));
  }

  struct Boundary {
    bitset internal;
    bitset external;
    double min;
  };
  static const Boundary BoundariesArray[];
  static inline const Boundary* Boundaries();
  static inline size_t BoundariesSize();
};

// -----------------------------------------------------------------------------
// Superclass for non-bitset types (internal).
class TypeBase {
 protected:
  friend class Type;

  enum Kind {
    kClass,
    kConstant,
    kContext,
    kArray,
    kFunction,
    kTuple,
    kUnion,
    kRange
  };

  Kind kind() const { return kind_; }
  explicit TypeBase(Kind kind) : kind_(kind) {}

  static bool IsKind(Type* type, Kind kind) {
    if (BitsetType::IsBitset(type)) return false;
    TypeBase* base = reinterpret_cast<TypeBase*>(type);
    return base->kind() == kind;
  }

  // The hacky conversion to/from Type*.
  static Type* AsType(TypeBase* type) { return reinterpret_cast<Type*>(type); }
  static TypeBase* FromType(Type* type) {
    return reinterpret_cast<TypeBase*>(type);
  }

 private:
  Kind kind_;
};

// -----------------------------------------------------------------------------
// Class types.

class ClassType : public TypeBase {
 public:
  i::Handle<i::Map> Map() { return map_; }

 private:
  friend class Type;
  friend class BitsetType;

  static Type* New(i::Handle<i::Map> map, Zone* zone) {
    return AsType(new (zone->New(sizeof(ClassType)))
                      ClassType(BitsetType::Lub(*map), map));
  }

  static ClassType* cast(Type* type) {
    DCHECK(IsKind(type, kClass));
    return static_cast<ClassType*>(FromType(type));
  }

  ClassType(BitsetType::bitset bitset, i::Handle<i::Map> map)
      : TypeBase(kClass), bitset_(bitset), map_(map) {}

  BitsetType::bitset Lub() { return bitset_; }

  BitsetType::bitset bitset_;
  Handle<i::Map> map_;
};

// -----------------------------------------------------------------------------
// Constant types.

class ConstantType : public TypeBase {
 public:
  i::Handle<i::Object> Value() { return object_; }

 private:
  friend class Type;
  friend class BitsetType;

  static Type* New(i::Handle<i::Object> value, Zone* zone) {
    BitsetType::bitset bitset = BitsetType::Lub(*value);
    return AsType(new (zone->New(sizeof(ConstantType)))
                      ConstantType(bitset, value));
  }

  static ConstantType* cast(Type* type) {
    DCHECK(IsKind(type, kConstant));
    return static_cast<ConstantType*>(FromType(type));
  }

  ConstantType(BitsetType::bitset bitset, i::Handle<i::Object> object)
      : TypeBase(kConstant), bitset_(bitset), object_(object) {}

  BitsetType::bitset Lub() { return bitset_; }

  BitsetType::bitset bitset_;
  Handle<i::Object> object_;
};
// TODO(neis): Also cache value if numerical.
// TODO(neis): Allow restricting the representation.

// -----------------------------------------------------------------------------
// Range types.

class RangeType : public TypeBase {
 public:
  struct Limits {
    double min;
    double max;
    Limits(double min, double max) : min(min), max(max) {}
    explicit Limits(RangeType* range) : min(range->Min()), max(range->Max()) {}
    bool IsEmpty();
    static Limits Empty() { return Limits(1, 0); }
    static Limits Intersect(Limits lhs, Limits rhs);
    static Limits Union(Limits lhs, Limits rhs);
  };

  double Min() { return limits_.min; }
  double Max() { return limits_.max; }

 private:
  friend class Type;
  friend class BitsetType;
  friend class UnionType;

  static Type* New(double min, double max, BitsetType::bitset representation,
                   Zone* zone) {
    return New(Limits(min, max), representation, zone);
  }

  static bool IsInteger(double x) {
    return nearbyint(x) == x && !i::IsMinusZero(x);  // Allows for infinities.
  }

  static Type* New(Limits lim, BitsetType::bitset representation, Zone* zone) {
    DCHECK(IsInteger(lim.min) && IsInteger(lim.max));
    DCHECK(lim.min <= lim.max);
    DCHECK(REPRESENTATION(representation) == representation);
    BitsetType::bitset bits =
        SEMANTIC(BitsetType::Lub(lim.min, lim.max)) | representation;

    return AsType(new (zone->New(sizeof(RangeType))) RangeType(bits, lim));
  }

  static RangeType* cast(Type* type) {
    DCHECK(IsKind(type, kRange));
    return static_cast<RangeType*>(FromType(type));
  }

  RangeType(BitsetType::bitset bitset, Limits limits)
      : TypeBase(kRange), bitset_(bitset), limits_(limits) {}

  BitsetType::bitset Lub() { return bitset_; }

  BitsetType::bitset bitset_;
  Limits limits_;
};

// -----------------------------------------------------------------------------
// Context types.

class ContextType : public TypeBase {
 public:
  Type* Outer() { return outer_; }

 private:
  friend class Type;

  static Type* New(Type* outer, Zone* zone) {
    return AsType(new (zone->New(sizeof(ContextType))) ContextType(outer));
  }

  static ContextType* cast(Type* type) {
    DCHECK(IsKind(type, kContext));
    return static_cast<ContextType*>(FromType(type));
  }

  explicit ContextType(Type* outer) : TypeBase(kContext), outer_(outer) {}

  Type* outer_;
};

// -----------------------------------------------------------------------------
// Array types.

class ArrayType : public TypeBase {
 public:
  Type* Element() { return element_; }

 private:
  friend class Type;

  explicit ArrayType(Type* element) : TypeBase(kArray), element_(element) {}

  static Type* New(Type* element, Zone* zone) {
    return AsType(new (zone->New(sizeof(ArrayType))) ArrayType(element));
  }

  static ArrayType* cast(Type* type) {
    DCHECK(IsKind(type, kArray));
    return static_cast<ArrayType*>(FromType(type));
  }

  Type* element_;
};

// -----------------------------------------------------------------------------
// Superclass for types with variable number of type fields.
class StructuralType : public TypeBase {
 public:
  int LengthForTesting() { return Length(); }

 protected:
  friend class Type;

  int Length() { return length_; }

  Type* Get(int i) {
    DCHECK(0 <= i && i < this->Length());
    return elements_[i];
  }

  void Set(int i, Type* type) {
    DCHECK(0 <= i && i < this->Length());
    elements_[i] = type;
  }

  void Shrink(int length) {
    DCHECK(2 <= length && length <= this->Length());
    length_ = length;
  }

  StructuralType(Kind kind, int length, i::Zone* zone)
      : TypeBase(kind), length_(length) {
    elements_ = reinterpret_cast<Type**>(zone->New(sizeof(Type*) * length));
  }

 private:
  int length_;
  Type** elements_;
};

// -----------------------------------------------------------------------------
// Function types.

class FunctionType : public StructuralType {
 public:
  int Arity() { return this->Length() - 2; }
  Type* Result() { return this->Get(0); }
  Type* Receiver() { return this->Get(1); }
  Type* Parameter(int i) { return this->Get(2 + i); }

  void InitParameter(int i, Type* type) { this->Set(2 + i, type); }

 private:
  friend class Type;

  FunctionType(Type* result, Type* receiver, int arity, Zone* zone)
      : StructuralType(kFunction, 2 + arity, zone) {
    Set(0, result);
    Set(1, receiver);
  }

  static Type* New(Type* result, Type* receiver, int arity, Zone* zone) {
    return AsType(new (zone->New(sizeof(FunctionType)))
                      FunctionType(result, receiver, arity, zone));
  }

  static FunctionType* cast(Type* type) {
    DCHECK(IsKind(type, kFunction));
    return static_cast<FunctionType*>(FromType(type));
  }
};

// -----------------------------------------------------------------------------
// Tuple types.

class TupleType : public StructuralType {
 public:
  int Arity() { return this->Length(); }
  Type* Element(int i) { return this->Get(i); }

  void InitElement(int i, Type* type) { this->Set(i, type); }

 private:
  friend class Type;

  TupleType(int length, Zone* zone) : StructuralType(kTuple, length, zone) {}

  static Type* New(int length, Zone* zone) {
    return AsType(new (zone->New(sizeof(TupleType))) TupleType(length, zone));
  }

  static TupleType* cast(Type* type) {
    DCHECK(IsKind(type, kTuple));
    return static_cast<TupleType*>(FromType(type));
  }
};

// -----------------------------------------------------------------------------
// Union types (internal).
// A union is a structured type with the following invariants:
// - its length is at least 2
// - at most one field is a bitset, and it must go into index 0
// - no field is a union
// - no field is a subtype of any other field
class UnionType : public StructuralType {
 private:
  friend Type;
  friend BitsetType;

  UnionType(int length, Zone* zone) : StructuralType(kUnion, length, zone) {}

  static Type* New(int length, Zone* zone) {
    return AsType(new (zone->New(sizeof(UnionType))) UnionType(length, zone));
  }

  static UnionType* cast(Type* type) {
    DCHECK(IsKind(type, kUnion));
    return static_cast<UnionType*>(FromType(type));
  }

  bool Wellformed();
};

class Type {
 public:
  typedef BitsetType::bitset bitset;  // Internal

// Constructors.
#define DEFINE_TYPE_CONSTRUCTOR(type, value) \
  static Type* type() { return BitsetType::New(BitsetType::k##type); }
  PROPER_BITSET_TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
#undef DEFINE_TYPE_CONSTRUCTOR

  static Type* SignedSmall() {
    return BitsetType::New(BitsetType::SignedSmall());
  }
  static Type* UnsignedSmall() {
    return BitsetType::New(BitsetType::UnsignedSmall());
  }

  static Type* Class(i::Handle<i::Map> map, Zone* zone) {
    return ClassType::New(map, zone);
  }
  static Type* Constant(i::Handle<i::Object> value, Zone* zone) {
    return ConstantType::New(value, zone);
  }
  static Type* Range(double min, double max, Zone* zone) {
    return RangeType::New(min, max, REPRESENTATION(BitsetType::kTagged |
                                                   BitsetType::kUntaggedNumber),
                          zone);
  }
  static Type* Context(Type* outer, Zone* zone) {
    return ContextType::New(outer, zone);
  }
  static Type* Array(Type* element, Zone* zone) {
    return ArrayType::New(element, zone);
  }
  static Type* Function(Type* result, Type* receiver, int arity, Zone* zone) {
    return FunctionType::New(result, receiver, arity, zone);
  }
  static Type* Function(Type* result, Zone* zone) {
    return Function(result, Any(), 0, zone);
  }
  static Type* Function(Type* result, Type* param0, Zone* zone) {
    Type* function = Function(result, Any(), 1, zone);
    function->AsFunction()->InitParameter(0, param0);
    return function;
  }
  static Type* Function(Type* result, Type* param0, Type* param1, Zone* zone) {
    Type* function = Function(result, Any(), 2, zone);
    function->AsFunction()->InitParameter(0, param0);
    function->AsFunction()->InitParameter(1, param1);
    return function;
  }
  static Type* Function(Type* result, Type* param0, Type* param1, Type* param2,
                        Zone* zone) {
    Type* function = Function(result, Any(), 3, zone);
    function->AsFunction()->InitParameter(0, param0);
    function->AsFunction()->InitParameter(1, param1);
    function->AsFunction()->InitParameter(2, param2);
    return function;
  }
  static Type* Function(Type* result, int arity, Type** params, Zone* zone) {
    Type* function = Function(result, Any(), arity, zone);
    for (int i = 0; i < arity; ++i) {
      function->AsFunction()->InitParameter(i, params[i]);
    }
    return function;
  }
  static Type* Tuple(Type* first, Type* second, Type* third, Zone* zone) {
    Type* tuple = TupleType::New(3, zone);
    tuple->AsTuple()->InitElement(0, first);
    tuple->AsTuple()->InitElement(1, second);
    tuple->AsTuple()->InitElement(2, third);
    return tuple;
  }

#define CONSTRUCT_SIMD_TYPE(NAME, Name, name, lane_count, lane_type) \
  static Type* Name(Isolate* isolate, Zone* zone);
  SIMD128_TYPES(CONSTRUCT_SIMD_TYPE)
#undef CONSTRUCT_SIMD_TYPE

  static Type* Union(Type* type1, Type* type2, Zone* reg);
  static Type* Intersect(Type* type1, Type* type2, Zone* reg);

  static Type* Of(double value, Zone* zone) {
    return BitsetType::New(BitsetType::ExpandInternals(BitsetType::Lub(value)));
  }
  static Type* Of(i::Object* value, Zone* zone) {
    return BitsetType::New(BitsetType::ExpandInternals(BitsetType::Lub(value)));
  }
  static Type* Of(i::Handle<i::Object> value, Zone* zone) {
    return Of(*value, zone);
  }

  // Extraction of components.
  static Type* Representation(Type* t, Zone* zone);
  static Type* Semantic(Type* t, Zone* zone);

  // Predicates.
  bool IsInhabited() { return BitsetType::IsInhabited(this->BitsetLub()); }

  bool Is(Type* that) { return this == that || this->SlowIs(that); }
  bool Maybe(Type* that);
  bool Equals(Type* that) { return this->Is(that) && that->Is(this); }

  // Equivalent to Constant(val)->Is(this), but avoiding allocation.
  bool Contains(i::Object* val);
  bool Contains(i::Handle<i::Object> val) { return this->Contains(*val); }

  // State-dependent versions of the above that consider subtyping between
  // a constant and its map class.
  static Type* NowOf(i::Object* value, Zone* zone);
  static Type* NowOf(i::Handle<i::Object> value, Zone* zone) {
    return NowOf(*value, zone);
  }
  bool NowIs(Type* that);
  bool NowContains(i::Object* val);
  bool NowContains(i::Handle<i::Object> val) { return this->NowContains(*val); }

  bool NowStable();

  // Inspection.
  bool IsRange() { return IsKind(TypeBase::kRange); }
  bool IsClass() { return IsKind(TypeBase::kClass); }
  bool IsConstant() { return IsKind(TypeBase::kConstant); }
  bool IsContext() { return IsKind(TypeBase::kContext); }
  bool IsArray() { return IsKind(TypeBase::kArray); }
  bool IsFunction() { return IsKind(TypeBase::kFunction); }
  bool IsTuple() { return IsKind(TypeBase::kTuple); }

  ClassType* AsClass() { return ClassType::cast(this); }
  ConstantType* AsConstant() { return ConstantType::cast(this); }
  RangeType* AsRange() { return RangeType::cast(this); }
  ContextType* AsContext() { return ContextType::cast(this); }
  ArrayType* AsArray() { return ArrayType::cast(this); }
  FunctionType* AsFunction() { return FunctionType::cast(this); }
  TupleType* AsTuple() { return TupleType::cast(this); }

  // Minimum and maximum of a numeric type.
  // These functions do not distinguish between -0 and +0.  If the type equals
  // kNaN, they return NaN; otherwise kNaN is ignored.  Only call these
  // functions on subtypes of Number.
  double Min();
  double Max();

  // Extracts a range from the type: if the type is a range or a union
  // containing a range, that range is returned; otherwise, NULL is returned.
  Type* GetRange();

  static bool IsInteger(i::Object* x);
  static bool IsInteger(double x) {
    return nearbyint(x) == x && !i::IsMinusZero(x);  // Allows for infinities.
  }

  int NumClasses();
  int NumConstants();

  template <class T>
  class Iterator {
   public:
    bool Done() const { return index_ < 0; }
    i::Handle<T> Current();
    void Advance();

   private:
    friend class Type;

    Iterator() : index_(-1) {}
    explicit Iterator(Type* type) : type_(type), index_(-1) { Advance(); }

    inline bool matches(Type* type);
    inline Type* get_type();

    Type* type_;
    int index_;
  };

  Iterator<i::Map> Classes() {
    if (this->IsBitset()) return Iterator<i::Map>();
    return Iterator<i::Map>(this);
  }
  Iterator<i::Object> Constants() {
    if (this->IsBitset()) return Iterator<i::Object>();
    return Iterator<i::Object>(this);
  }

  // Printing.

  enum PrintDimension { BOTH_DIMS, SEMANTIC_DIM, REPRESENTATION_DIM };

  void PrintTo(std::ostream& os, PrintDimension dim = BOTH_DIMS);  // NOLINT

#ifdef DEBUG
  void Print();
#endif

  // Helpers for testing.
  bool IsBitsetForTesting() { return IsBitset(); }
  bool IsUnionForTesting() { return IsUnion(); }
  bitset AsBitsetForTesting() { return AsBitset(); }
  UnionType* AsUnionForTesting() { return AsUnion(); }

 private:
  // Friends.
  template <class>
  friend class Iterator;
  friend BitsetType;
  friend UnionType;

  // Internal inspection.
  bool IsKind(TypeBase::Kind kind) { return TypeBase::IsKind(this, kind); }

  bool IsNone() { return this == None(); }
  bool IsAny() { return this == Any(); }
  bool IsBitset() { return BitsetType::IsBitset(this); }
  bool IsUnion() { return IsKind(TypeBase::kUnion); }

  bitset AsBitset() {
    DCHECK(this->IsBitset());
    return reinterpret_cast<BitsetType*>(this)->Bitset();
  }
  UnionType* AsUnion() { return UnionType::cast(this); }

  bitset Representation();

  // Auxiliary functions.
  bool SemanticMaybe(Type* that);

  bitset BitsetGlb() { return BitsetType::Glb(this); }
  bitset BitsetLub() { return BitsetType::Lub(this); }

  bool SlowIs(Type* that);
  bool SemanticIs(Type* that);

  static bool Overlap(RangeType* lhs, RangeType* rhs);
  static bool Contains(RangeType* lhs, RangeType* rhs);
  static bool Contains(RangeType* range, ConstantType* constant);
  static bool Contains(RangeType* range, i::Object* val);

  static int UpdateRange(Type* type, UnionType* result, int size, Zone* zone);

  static RangeType::Limits IntersectRangeAndBitset(Type* range, Type* bits,
                                                   Zone* zone);
  static RangeType::Limits ToLimits(bitset bits, Zone* zone);

  bool SimplyEquals(Type* that);

  static int AddToUnion(Type* type, UnionType* result, int size, Zone* zone);
  static int IntersectAux(Type* type, Type* other, UnionType* result, int size,
                          RangeType::Limits* limits, Zone* zone);
  static Type* NormalizeUnion(Type* unioned, int size, Zone* zone);
  static Type* NormalizeRangeAndBitset(Type* range, bitset* bits, Zone* zone);
};

// -----------------------------------------------------------------------------
// Type bounds. A simple struct to represent a pair of lower/upper types.

struct Bounds {
  Type* lower;
  Type* upper;

  Bounds()
      :  // Make sure accessing uninitialized bounds crashes big-time.
        lower(nullptr),
        upper(nullptr) {}
  explicit Bounds(Type* t) : lower(t), upper(t) {}
  Bounds(Type* l, Type* u) : lower(l), upper(u) { DCHECK(lower->Is(upper)); }

  // Unrestricted bounds.
  static Bounds Unbounded() { return Bounds(Type::None(), Type::Any()); }

  // Meet: both b1 and b2 are known to hold.
  static Bounds Both(Bounds b1, Bounds b2, Zone* zone) {
    Type* lower = Type::Union(b1.lower, b2.lower, zone);
    Type* upper = Type::Intersect(b1.upper, b2.upper, zone);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(upper)) lower = upper;
    return Bounds(lower, upper);
  }

  // Join: either b1 or b2 is known to hold.
  static Bounds Either(Bounds b1, Bounds b2, Zone* zone) {
    Type* lower = Type::Intersect(b1.lower, b2.lower, zone);
    Type* upper = Type::Union(b1.upper, b2.upper, zone);
    return Bounds(lower, upper);
  }

  static Bounds NarrowLower(Bounds b, Type* t, Zone* zone) {
    Type* lower = Type::Union(b.lower, t, zone);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(b.upper)) lower = b.upper;
    return Bounds(lower, b.upper);
  }
  static Bounds NarrowUpper(Bounds b, Type* t, Zone* zone) {
    Type* lower = b.lower;
    Type* upper = Type::Intersect(b.upper, t, zone);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(upper)) lower = upper;
    return Bounds(lower, upper);
  }

  bool Narrows(Bounds that) {
    return that.lower->Is(this->lower) && this->upper->Is(that.upper);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TYPES_H_
