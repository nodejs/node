// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_TYPES_H_
#define V8_AST_AST_TYPES_H_

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
//   OtherUndetectable < Object
//   DetectableReceiver = Receiver - OtherUndetectable
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

#define AST_MASK_BITSET_TYPE_LIST(V) \
  V(Representation, 0xffc00000u) \
  V(Semantic,       0x003ffffeu)

#define AST_REPRESENTATION(k) ((k) & AstBitsetType::kRepresentation)
#define AST_SEMANTIC(k)       ((k) & AstBitsetType::kSemantic)

// Bits 21-22 are available.
#define AST_REPRESENTATION_BITSET_TYPE_LIST(V)    \
  V(None,               0)                    \
  V(UntaggedBit,        1u << 23 | kSemantic) \
  V(UntaggedIntegral8,  1u << 24 | kSemantic) \
  V(UntaggedIntegral16, 1u << 25 | kSemantic) \
  V(UntaggedIntegral32, 1u << 26 | kSemantic) \
  V(UntaggedFloat32,    1u << 27 | kSemantic) \
  V(UntaggedFloat64,    1u << 28 | kSemantic) \
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

#define AST_INTERNAL_BITSET_TYPE_LIST(V)                                      \
  V(OtherUnsigned31, 1u << 1 | AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherUnsigned32, 1u << 2 | AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherSigned32,   1u << 3 | AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherNumber,     1u << 4 | AST_REPRESENTATION(kTagged | kUntaggedNumber))

#define AST_SEMANTIC_BITSET_TYPE_LIST(V)                                \
  V(Negative31,          1u << 5  |                                     \
                         AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Null,                1u << 6  | AST_REPRESENTATION(kTaggedPointer)) \
  V(Undefined,           1u << 7  | AST_REPRESENTATION(kTaggedPointer)) \
  V(Boolean,             1u << 8  | AST_REPRESENTATION(kTaggedPointer)) \
  V(Unsigned30,          1u << 9  |                                     \
                         AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(MinusZero,           1u << 10 |                                     \
                         AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(NaN,                 1u << 11 |                                     \
                         AST_REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Symbol,              1u << 12 | AST_REPRESENTATION(kTaggedPointer)) \
  V(InternalizedString,  1u << 13 | AST_REPRESENTATION(kTaggedPointer)) \
  V(OtherString,         1u << 14 | AST_REPRESENTATION(kTaggedPointer)) \
  V(OtherObject,         1u << 15 | AST_REPRESENTATION(kTaggedPointer)) \
  V(OtherUndetectable,   1u << 16 | AST_REPRESENTATION(kTaggedPointer)) \
  V(Proxy,               1u << 17 | AST_REPRESENTATION(kTaggedPointer)) \
  V(Function,            1u << 18 | AST_REPRESENTATION(kTaggedPointer)) \
  V(Hole,                1u << 19 | AST_REPRESENTATION(kTaggedPointer)) \
  V(OtherInternal,       1u << 20 |                                     \
                         AST_REPRESENTATION(kTagged | kUntagged))       \
  \
  V(Signed31,                   kUnsigned30 | kNegative31) \
  V(Signed32,                   kSigned31 | kOtherUnsigned31 |          \
                                kOtherSigned32)                         \
  V(Signed32OrMinusZero,        kSigned32 | kMinusZero) \
  V(Signed32OrMinusZeroOrNaN,   kSigned32 | kMinusZero | kNaN) \
  V(Negative32,                 kNegative31 | kOtherSigned32) \
  V(Unsigned31,                 kUnsigned30 | kOtherUnsigned31) \
  V(Unsigned32,                 kUnsigned30 | kOtherUnsigned31 | \
                                kOtherUnsigned32) \
  V(Unsigned32OrMinusZero,      kUnsigned32 | kMinusZero) \
  V(Unsigned32OrMinusZeroOrNaN, kUnsigned32 | kMinusZero | kNaN) \
  V(Integral32,                 kSigned32 | kUnsigned32) \
  V(PlainNumber,                kIntegral32 | kOtherNumber) \
  V(OrderedNumber,              kPlainNumber | kMinusZero) \
  V(MinusZeroOrNaN,             kMinusZero | kNaN) \
  V(Number,                     kOrderedNumber | kNaN) \
  V(String,                     kInternalizedString | kOtherString) \
  V(UniqueName,                 kSymbol | kInternalizedString) \
  V(Name,                       kSymbol | kString) \
  V(BooleanOrNumber,            kBoolean | kNumber) \
  V(BooleanOrNullOrNumber,      kBooleanOrNumber | kNull) \
  V(BooleanOrNullOrUndefined,   kBoolean | kNull | kUndefined) \
  V(NullOrNumber,               kNull | kNumber) \
  V(NullOrUndefined,            kNull | kUndefined) \
  V(Undetectable,               kNullOrUndefined | kOtherUndetectable) \
  V(NumberOrOddball,            kNumber | kNullOrUndefined | kBoolean | kHole) \
  V(NumberOrString,             kNumber | kString) \
  V(NumberOrUndefined,          kNumber | kUndefined) \
  V(PlainPrimitive,             kNumberOrString | kBoolean | kNullOrUndefined) \
  V(Primitive,                  kSymbol | kPlainPrimitive) \
  V(DetectableReceiver,         kFunction | kOtherObject | kProxy) \
  V(Object,                     kFunction | kOtherObject | kOtherUndetectable) \
  V(Receiver,                   kObject | kProxy) \
  V(StringOrReceiver,           kString | kReceiver) \
  V(Unique,                     kBoolean | kUniqueName | kNull | kUndefined | \
                                kReceiver) \
  V(Internal,                   kHole | kOtherInternal) \
  V(NonInternal,                kPrimitive | kReceiver) \
  V(NonNumber,                  kUnique | kString | kInternal) \
  V(Any,                        0xfffffffeu)

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

#define AST_PROPER_BITSET_TYPE_LIST(V)   \
  AST_REPRESENTATION_BITSET_TYPE_LIST(V) \
  AST_SEMANTIC_BITSET_TYPE_LIST(V)

#define AST_BITSET_TYPE_LIST(V)          \
  AST_MASK_BITSET_TYPE_LIST(V)           \
  AST_REPRESENTATION_BITSET_TYPE_LIST(V) \
  AST_INTERNAL_BITSET_TYPE_LIST(V)       \
  AST_SEMANTIC_BITSET_TYPE_LIST(V)

class AstType;

// -----------------------------------------------------------------------------
// Bitset types (internal).

class AstBitsetType {
 public:
  typedef uint32_t bitset;  // Internal

  enum : uint32_t {
#define DECLARE_TYPE(type, value) k##type = (value),
    AST_BITSET_TYPE_LIST(DECLARE_TYPE)
#undef DECLARE_TYPE
        kUnusedEOL = 0
  };

  static bitset SignedSmall();
  static bitset UnsignedSmall();

  bitset Bitset() {
    return static_cast<bitset>(reinterpret_cast<uintptr_t>(this) ^ 1u);
  }

  static bool IsInhabited(bitset bits) {
    return AST_SEMANTIC(bits) != kNone && AST_REPRESENTATION(bits) != kNone;
  }

  static bool SemanticIsInhabited(bitset bits) {
    return AST_SEMANTIC(bits) != kNone;
  }

  static bool Is(bitset bits1, bitset bits2) {
    return (bits1 | bits2) == bits2;
  }

  static double Min(bitset);
  static double Max(bitset);

  static bitset Glb(AstType* type);  // greatest lower bound that's a bitset
  static bitset Glb(double min, double max);
  static bitset Lub(AstType* type);  // least upper bound that's a bitset
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

  static bool IsBitset(AstType* type) {
    return reinterpret_cast<uintptr_t>(type) & 1;
  }

  static AstType* NewForTesting(bitset bits) { return New(bits); }

 private:
  friend class AstType;

  static AstType* New(bitset bits) {
    return reinterpret_cast<AstType*>(static_cast<uintptr_t>(bits | 1u));
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
class AstTypeBase {
 protected:
  friend class AstType;

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
  explicit AstTypeBase(Kind kind) : kind_(kind) {}

  static bool IsKind(AstType* type, Kind kind) {
    if (AstBitsetType::IsBitset(type)) return false;
    AstTypeBase* base = reinterpret_cast<AstTypeBase*>(type);
    return base->kind() == kind;
  }

  // The hacky conversion to/from AstType*.
  static AstType* AsType(AstTypeBase* type) {
    return reinterpret_cast<AstType*>(type);
  }
  static AstTypeBase* FromType(AstType* type) {
    return reinterpret_cast<AstTypeBase*>(type);
  }

 private:
  Kind kind_;
};

// -----------------------------------------------------------------------------
// Class types.

class AstClassType : public AstTypeBase {
 public:
  i::Handle<i::Map> Map() { return map_; }

 private:
  friend class AstType;
  friend class AstBitsetType;

  static AstType* New(i::Handle<i::Map> map, Zone* zone) {
    return AsType(new (zone->New(sizeof(AstClassType)))
                      AstClassType(AstBitsetType::Lub(*map), map));
  }

  static AstClassType* cast(AstType* type) {
    DCHECK(IsKind(type, kClass));
    return static_cast<AstClassType*>(FromType(type));
  }

  AstClassType(AstBitsetType::bitset bitset, i::Handle<i::Map> map)
      : AstTypeBase(kClass), bitset_(bitset), map_(map) {}

  AstBitsetType::bitset Lub() { return bitset_; }

  AstBitsetType::bitset bitset_;
  Handle<i::Map> map_;
};

// -----------------------------------------------------------------------------
// Constant types.

class AstConstantType : public AstTypeBase {
 public:
  i::Handle<i::Object> Value() { return object_; }

 private:
  friend class AstType;
  friend class AstBitsetType;

  static AstType* New(i::Handle<i::Object> value, Zone* zone) {
    AstBitsetType::bitset bitset = AstBitsetType::Lub(*value);
    return AsType(new (zone->New(sizeof(AstConstantType)))
                      AstConstantType(bitset, value));
  }

  static AstConstantType* cast(AstType* type) {
    DCHECK(IsKind(type, kConstant));
    return static_cast<AstConstantType*>(FromType(type));
  }

  AstConstantType(AstBitsetType::bitset bitset, i::Handle<i::Object> object)
      : AstTypeBase(kConstant), bitset_(bitset), object_(object) {}

  AstBitsetType::bitset Lub() { return bitset_; }

  AstBitsetType::bitset bitset_;
  Handle<i::Object> object_;
};
// TODO(neis): Also cache value if numerical.
// TODO(neis): Allow restricting the representation.

// -----------------------------------------------------------------------------
// Range types.

class AstRangeType : public AstTypeBase {
 public:
  struct Limits {
    double min;
    double max;
    Limits(double min, double max) : min(min), max(max) {}
    explicit Limits(AstRangeType* range)
        : min(range->Min()), max(range->Max()) {}
    bool IsEmpty();
    static Limits Empty() { return Limits(1, 0); }
    static Limits Intersect(Limits lhs, Limits rhs);
    static Limits Union(Limits lhs, Limits rhs);
  };

  double Min() { return limits_.min; }
  double Max() { return limits_.max; }

 private:
  friend class AstType;
  friend class AstBitsetType;
  friend class AstUnionType;

  static AstType* New(double min, double max,
                      AstBitsetType::bitset representation, Zone* zone) {
    return New(Limits(min, max), representation, zone);
  }

  static bool IsInteger(double x) {
    return nearbyint(x) == x && !i::IsMinusZero(x);  // Allows for infinities.
  }

  static AstType* New(Limits lim, AstBitsetType::bitset representation,
                      Zone* zone) {
    DCHECK(IsInteger(lim.min) && IsInteger(lim.max));
    DCHECK(lim.min <= lim.max);
    DCHECK(AST_REPRESENTATION(representation) == representation);
    AstBitsetType::bitset bits =
        AST_SEMANTIC(AstBitsetType::Lub(lim.min, lim.max)) | representation;

    return AsType(new (zone->New(sizeof(AstRangeType)))
                      AstRangeType(bits, lim));
  }

  static AstRangeType* cast(AstType* type) {
    DCHECK(IsKind(type, kRange));
    return static_cast<AstRangeType*>(FromType(type));
  }

  AstRangeType(AstBitsetType::bitset bitset, Limits limits)
      : AstTypeBase(kRange), bitset_(bitset), limits_(limits) {}

  AstBitsetType::bitset Lub() { return bitset_; }

  AstBitsetType::bitset bitset_;
  Limits limits_;
};

// -----------------------------------------------------------------------------
// Context types.

class AstContextType : public AstTypeBase {
 public:
  AstType* Outer() { return outer_; }

 private:
  friend class AstType;

  static AstType* New(AstType* outer, Zone* zone) {
    return AsType(new (zone->New(sizeof(AstContextType)))
                      AstContextType(outer));  // NOLINT
  }

  static AstContextType* cast(AstType* type) {
    DCHECK(IsKind(type, kContext));
    return static_cast<AstContextType*>(FromType(type));
  }

  explicit AstContextType(AstType* outer)
      : AstTypeBase(kContext), outer_(outer) {}

  AstType* outer_;
};

// -----------------------------------------------------------------------------
// Array types.

class AstArrayType : public AstTypeBase {
 public:
  AstType* Element() { return element_; }

 private:
  friend class AstType;

  explicit AstArrayType(AstType* element)
      : AstTypeBase(kArray), element_(element) {}

  static AstType* New(AstType* element, Zone* zone) {
    return AsType(new (zone->New(sizeof(AstArrayType))) AstArrayType(element));
  }

  static AstArrayType* cast(AstType* type) {
    DCHECK(IsKind(type, kArray));
    return static_cast<AstArrayType*>(FromType(type));
  }

  AstType* element_;
};

// -----------------------------------------------------------------------------
// Superclass for types with variable number of type fields.
class AstStructuralType : public AstTypeBase {
 public:
  int LengthForTesting() { return Length(); }

 protected:
  friend class AstType;

  int Length() { return length_; }

  AstType* Get(int i) {
    DCHECK(0 <= i && i < this->Length());
    return elements_[i];
  }

  void Set(int i, AstType* type) {
    DCHECK(0 <= i && i < this->Length());
    elements_[i] = type;
  }

  void Shrink(int length) {
    DCHECK(2 <= length && length <= this->Length());
    length_ = length;
  }

  AstStructuralType(Kind kind, int length, i::Zone* zone)
      : AstTypeBase(kind), length_(length) {
    elements_ =
        reinterpret_cast<AstType**>(zone->New(sizeof(AstType*) * length));
  }

 private:
  int length_;
  AstType** elements_;
};

// -----------------------------------------------------------------------------
// Function types.

class AstFunctionType : public AstStructuralType {
 public:
  int Arity() { return this->Length() - 2; }
  AstType* Result() { return this->Get(0); }
  AstType* Receiver() { return this->Get(1); }
  AstType* Parameter(int i) { return this->Get(2 + i); }

  void InitParameter(int i, AstType* type) { this->Set(2 + i, type); }

 private:
  friend class AstType;

  AstFunctionType(AstType* result, AstType* receiver, int arity, Zone* zone)
      : AstStructuralType(kFunction, 2 + arity, zone) {
    Set(0, result);
    Set(1, receiver);
  }

  static AstType* New(AstType* result, AstType* receiver, int arity,
                      Zone* zone) {
    return AsType(new (zone->New(sizeof(AstFunctionType)))
                      AstFunctionType(result, receiver, arity, zone));
  }

  static AstFunctionType* cast(AstType* type) {
    DCHECK(IsKind(type, kFunction));
    return static_cast<AstFunctionType*>(FromType(type));
  }
};

// -----------------------------------------------------------------------------
// Tuple types.

class AstTupleType : public AstStructuralType {
 public:
  int Arity() { return this->Length(); }
  AstType* Element(int i) { return this->Get(i); }

  void InitElement(int i, AstType* type) { this->Set(i, type); }

 private:
  friend class AstType;

  AstTupleType(int length, Zone* zone)
      : AstStructuralType(kTuple, length, zone) {}

  static AstType* New(int length, Zone* zone) {
    return AsType(new (zone->New(sizeof(AstTupleType)))
                      AstTupleType(length, zone));
  }

  static AstTupleType* cast(AstType* type) {
    DCHECK(IsKind(type, kTuple));
    return static_cast<AstTupleType*>(FromType(type));
  }
};

// -----------------------------------------------------------------------------
// Union types (internal).
// A union is a structured type with the following invariants:
// - its length is at least 2
// - at most one field is a bitset, and it must go into index 0
// - no field is a union
// - no field is a subtype of any other field
class AstUnionType : public AstStructuralType {
 private:
  friend AstType;
  friend AstBitsetType;

  AstUnionType(int length, Zone* zone)
      : AstStructuralType(kUnion, length, zone) {}

  static AstType* New(int length, Zone* zone) {
    return AsType(new (zone->New(sizeof(AstUnionType)))
                      AstUnionType(length, zone));
  }

  static AstUnionType* cast(AstType* type) {
    DCHECK(IsKind(type, kUnion));
    return static_cast<AstUnionType*>(FromType(type));
  }

  bool Wellformed();
};

class AstType {
 public:
  typedef AstBitsetType::bitset bitset;  // Internal

// Constructors.
#define DEFINE_TYPE_CONSTRUCTOR(type, value) \
  static AstType* type() { return AstBitsetType::New(AstBitsetType::k##type); }
  AST_PROPER_BITSET_TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
#undef DEFINE_TYPE_CONSTRUCTOR

  static AstType* SignedSmall() {
    return AstBitsetType::New(AstBitsetType::SignedSmall());
  }
  static AstType* UnsignedSmall() {
    return AstBitsetType::New(AstBitsetType::UnsignedSmall());
  }

  static AstType* Class(i::Handle<i::Map> map, Zone* zone) {
    return AstClassType::New(map, zone);
  }
  static AstType* Constant(i::Handle<i::Object> value, Zone* zone) {
    return AstConstantType::New(value, zone);
  }
  static AstType* Range(double min, double max, Zone* zone) {
    return AstRangeType::New(min, max,
                             AST_REPRESENTATION(AstBitsetType::kTagged |
                                                AstBitsetType::kUntaggedNumber),
                             zone);
  }
  static AstType* Context(AstType* outer, Zone* zone) {
    return AstContextType::New(outer, zone);
  }
  static AstType* Array(AstType* element, Zone* zone) {
    return AstArrayType::New(element, zone);
  }
  static AstType* Function(AstType* result, AstType* receiver, int arity,
                           Zone* zone) {
    return AstFunctionType::New(result, receiver, arity, zone);
  }
  static AstType* Function(AstType* result, Zone* zone) {
    return Function(result, Any(), 0, zone);
  }
  static AstType* Function(AstType* result, AstType* param0, Zone* zone) {
    AstType* function = Function(result, Any(), 1, zone);
    function->AsFunction()->InitParameter(0, param0);
    return function;
  }
  static AstType* Function(AstType* result, AstType* param0, AstType* param1,
                           Zone* zone) {
    AstType* function = Function(result, Any(), 2, zone);
    function->AsFunction()->InitParameter(0, param0);
    function->AsFunction()->InitParameter(1, param1);
    return function;
  }
  static AstType* Function(AstType* result, AstType* param0, AstType* param1,
                           AstType* param2, Zone* zone) {
    AstType* function = Function(result, Any(), 3, zone);
    function->AsFunction()->InitParameter(0, param0);
    function->AsFunction()->InitParameter(1, param1);
    function->AsFunction()->InitParameter(2, param2);
    return function;
  }
  static AstType* Function(AstType* result, int arity, AstType** params,
                           Zone* zone) {
    AstType* function = Function(result, Any(), arity, zone);
    for (int i = 0; i < arity; ++i) {
      function->AsFunction()->InitParameter(i, params[i]);
    }
    return function;
  }
  static AstType* Tuple(AstType* first, AstType* second, AstType* third,
                        Zone* zone) {
    AstType* tuple = AstTupleType::New(3, zone);
    tuple->AsTuple()->InitElement(0, first);
    tuple->AsTuple()->InitElement(1, second);
    tuple->AsTuple()->InitElement(2, third);
    return tuple;
  }

  static AstType* Union(AstType* type1, AstType* type2, Zone* zone);
  static AstType* Intersect(AstType* type1, AstType* type2, Zone* zone);

  static AstType* Of(double value, Zone* zone) {
    return AstBitsetType::New(
        AstBitsetType::ExpandInternals(AstBitsetType::Lub(value)));
  }
  static AstType* Of(i::Object* value, Zone* zone) {
    return AstBitsetType::New(
        AstBitsetType::ExpandInternals(AstBitsetType::Lub(value)));
  }
  static AstType* Of(i::Handle<i::Object> value, Zone* zone) {
    return Of(*value, zone);
  }

  static AstType* For(i::Map* map) {
    return AstBitsetType::New(
        AstBitsetType::ExpandInternals(AstBitsetType::Lub(map)));
  }
  static AstType* For(i::Handle<i::Map> map) { return For(*map); }

  // Extraction of components.
  static AstType* Representation(AstType* t, Zone* zone);
  static AstType* Semantic(AstType* t, Zone* zone);

  // Predicates.
  bool IsInhabited() { return AstBitsetType::IsInhabited(this->BitsetLub()); }

  bool Is(AstType* that) { return this == that || this->SlowIs(that); }
  bool Maybe(AstType* that);
  bool Equals(AstType* that) { return this->Is(that) && that->Is(this); }

  // Equivalent to Constant(val)->Is(this), but avoiding allocation.
  bool Contains(i::Object* val);
  bool Contains(i::Handle<i::Object> val) { return this->Contains(*val); }

  // State-dependent versions of the above that consider subtyping between
  // a constant and its map class.
  static AstType* NowOf(i::Object* value, Zone* zone);
  static AstType* NowOf(i::Handle<i::Object> value, Zone* zone) {
    return NowOf(*value, zone);
  }
  bool NowIs(AstType* that);
  bool NowContains(i::Object* val);
  bool NowContains(i::Handle<i::Object> val) { return this->NowContains(*val); }

  bool NowStable();

  // Inspection.
  bool IsRange() { return IsKind(AstTypeBase::kRange); }
  bool IsClass() { return IsKind(AstTypeBase::kClass); }
  bool IsConstant() { return IsKind(AstTypeBase::kConstant); }
  bool IsContext() { return IsKind(AstTypeBase::kContext); }
  bool IsArray() { return IsKind(AstTypeBase::kArray); }
  bool IsFunction() { return IsKind(AstTypeBase::kFunction); }
  bool IsTuple() { return IsKind(AstTypeBase::kTuple); }

  AstClassType* AsClass() { return AstClassType::cast(this); }
  AstConstantType* AsConstant() { return AstConstantType::cast(this); }
  AstRangeType* AsRange() { return AstRangeType::cast(this); }
  AstContextType* AsContext() { return AstContextType::cast(this); }
  AstArrayType* AsArray() { return AstArrayType::cast(this); }
  AstFunctionType* AsFunction() { return AstFunctionType::cast(this); }
  AstTupleType* AsTuple() { return AstTupleType::cast(this); }

  // Minimum and maximum of a numeric type.
  // These functions do not distinguish between -0 and +0.  If the type equals
  // kNaN, they return NaN; otherwise kNaN is ignored.  Only call these
  // functions on subtypes of Number.
  double Min();
  double Max();

  // Extracts a range from the type: if the type is a range or a union
  // containing a range, that range is returned; otherwise, NULL is returned.
  AstType* GetRange();

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
    friend class AstType;

    Iterator() : index_(-1) {}
    explicit Iterator(AstType* type) : type_(type), index_(-1) { Advance(); }

    inline bool matches(AstType* type);
    inline AstType* get_type();

    AstType* type_;
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
  AstUnionType* AsUnionForTesting() { return AsUnion(); }

 private:
  // Friends.
  template <class>
  friend class Iterator;
  friend AstBitsetType;
  friend AstUnionType;

  // Internal inspection.
  bool IsKind(AstTypeBase::Kind kind) {
    return AstTypeBase::IsKind(this, kind);
  }

  bool IsNone() { return this == None(); }
  bool IsAny() { return this == Any(); }
  bool IsBitset() { return AstBitsetType::IsBitset(this); }
  bool IsUnion() { return IsKind(AstTypeBase::kUnion); }

  bitset AsBitset() {
    DCHECK(this->IsBitset());
    return reinterpret_cast<AstBitsetType*>(this)->Bitset();
  }
  AstUnionType* AsUnion() { return AstUnionType::cast(this); }

  bitset Representation();

  // Auxiliary functions.
  bool SemanticMaybe(AstType* that);

  bitset BitsetGlb() { return AstBitsetType::Glb(this); }
  bitset BitsetLub() { return AstBitsetType::Lub(this); }

  bool SlowIs(AstType* that);
  bool SemanticIs(AstType* that);

  static bool Overlap(AstRangeType* lhs, AstRangeType* rhs);
  static bool Contains(AstRangeType* lhs, AstRangeType* rhs);
  static bool Contains(AstRangeType* range, AstConstantType* constant);
  static bool Contains(AstRangeType* range, i::Object* val);

  static int UpdateRange(AstType* type, AstUnionType* result, int size,
                         Zone* zone);

  static AstRangeType::Limits IntersectRangeAndBitset(AstType* range,
                                                      AstType* bits,
                                                      Zone* zone);
  static AstRangeType::Limits ToLimits(bitset bits, Zone* zone);

  bool SimplyEquals(AstType* that);

  static int AddToUnion(AstType* type, AstUnionType* result, int size,
                        Zone* zone);
  static int IntersectAux(AstType* type, AstType* other, AstUnionType* result,
                          int size, AstRangeType::Limits* limits, Zone* zone);
  static AstType* NormalizeUnion(AstType* unioned, int size, Zone* zone);
  static AstType* NormalizeRangeAndBitset(AstType* range, bitset* bits,
                                          Zone* zone);
};

// -----------------------------------------------------------------------------
// Type bounds. A simple struct to represent a pair of lower/upper types.

struct AstBounds {
  AstType* lower;
  AstType* upper;

  AstBounds()
      :  // Make sure accessing uninitialized bounds crashes big-time.
        lower(nullptr),
        upper(nullptr) {}
  explicit AstBounds(AstType* t) : lower(t), upper(t) {}
  AstBounds(AstType* l, AstType* u) : lower(l), upper(u) {
    DCHECK(lower->Is(upper));
  }

  // Unrestricted bounds.
  static AstBounds Unbounded() {
    return AstBounds(AstType::None(), AstType::Any());
  }

  // Meet: both b1 and b2 are known to hold.
  static AstBounds Both(AstBounds b1, AstBounds b2, Zone* zone) {
    AstType* lower = AstType::Union(b1.lower, b2.lower, zone);
    AstType* upper = AstType::Intersect(b1.upper, b2.upper, zone);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(upper)) lower = upper;
    return AstBounds(lower, upper);
  }

  // Join: either b1 or b2 is known to hold.
  static AstBounds Either(AstBounds b1, AstBounds b2, Zone* zone) {
    AstType* lower = AstType::Intersect(b1.lower, b2.lower, zone);
    AstType* upper = AstType::Union(b1.upper, b2.upper, zone);
    return AstBounds(lower, upper);
  }

  static AstBounds NarrowLower(AstBounds b, AstType* t, Zone* zone) {
    AstType* lower = AstType::Union(b.lower, t, zone);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(b.upper)) lower = b.upper;
    return AstBounds(lower, b.upper);
  }
  static AstBounds NarrowUpper(AstBounds b, AstType* t, Zone* zone) {
    AstType* lower = b.lower;
    AstType* upper = AstType::Intersect(b.upper, t, zone);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(upper)) lower = upper;
    return AstBounds(lower, upper);
  }

  bool Narrows(AstBounds that) {
    return that.lower->Is(this->lower) && this->upper->Is(that.upper);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_TYPES_H_
