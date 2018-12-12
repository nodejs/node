// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPES_H_
#define V8_COMPILER_TYPES_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/js-heap-broker.h"
#include "src/conversions.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

// SUMMARY
//
// A simple type system for compiler-internal use. It is based entirely on
// union types, and all subtyping hence amounts to set inclusion. Besides the
// obvious primitive types and some predefined unions, the type language also
// can express class types (a.k.a. specific maps) and singleton types (i.e.,
// concrete constants).
//
// The following equations and inequations hold:
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
//   OtherUndetectable < Object
//   DetectableReceiver = Receiver - OtherUndetectable
//
//   Constant(x) < T  iff instance_type(map(x)) < T
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
//   T1.Is(T2)     -- tests whether T1 is included in T2 (i.e., T1 <= T2)
//   T1.Maybe(T2)  -- tests whether T1 and T2 overlap (i.e., T1 /\ T2 =/= 0)
//
// Typically, the former is to be used to select representations (e.g., via
// T.Is(SignedSmall())), and the latter to check whether a specific case needs
// handling (e.g., via T.Maybe(Number())).
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
// bitsets. Bit 0 is reserved for tagging. Only structured types require
// allocation.

// -----------------------------------------------------------------------------
// Values for bitset types

// clang-format off

#define INTERNAL_BITSET_TYPE_LIST(V)                                      \
  V(OtherUnsigned31, 1u << 1)  \
  V(OtherUnsigned32, 1u << 2)  \
  V(OtherSigned32,   1u << 3)  \
  V(OtherNumber,     1u << 4)  \
  V(OtherString,     1u << 5)  \

#define PROPER_BITSET_TYPE_LIST(V) \
  V(None,                     0u)        \
  V(Negative31,               1u << 6)   \
  V(Null,                     1u << 7)   \
  V(Undefined,                1u << 8)   \
  V(Boolean,                  1u << 9)   \
  V(Unsigned30,               1u << 10)   \
  V(MinusZero,                1u << 11)  \
  V(NaN,                      1u << 12)  \
  V(Symbol,                   1u << 13)  \
  V(InternalizedString,       1u << 14)  \
  V(OtherCallable,            1u << 16)  \
  V(OtherObject,              1u << 17)  \
  V(OtherUndetectable,        1u << 18)  \
  V(CallableProxy,            1u << 19)  \
  V(OtherProxy,               1u << 20)  \
  V(Function,                 1u << 21)  \
  V(BoundFunction,            1u << 22)  \
  V(Hole,                     1u << 23)  \
  V(OtherInternal,            1u << 24)  \
  V(ExternalPointer,          1u << 25)  \
  V(Array,                    1u << 26)  \
  V(BigInt,                   1u << 27)  \
  \
  V(Signed31,                     kUnsigned30 | kNegative31) \
  V(Signed32,                     kSigned31 | kOtherUnsigned31 | \
                                  kOtherSigned32) \
  V(Signed32OrMinusZero,          kSigned32 | kMinusZero) \
  V(Signed32OrMinusZeroOrNaN,     kSigned32 | kMinusZero | kNaN) \
  V(Negative32,                   kNegative31 | kOtherSigned32) \
  V(Unsigned31,                   kUnsigned30 | kOtherUnsigned31) \
  V(Unsigned32,                   kUnsigned30 | kOtherUnsigned31 | \
                                  kOtherUnsigned32) \
  V(Unsigned32OrMinusZero,        kUnsigned32 | kMinusZero) \
  V(Unsigned32OrMinusZeroOrNaN,   kUnsigned32 | kMinusZero | kNaN) \
  V(Integral32,                   kSigned32 | kUnsigned32) \
  V(Integral32OrMinusZero,        kIntegral32 | kMinusZero) \
  V(Integral32OrMinusZeroOrNaN,   kIntegral32OrMinusZero | kNaN) \
  V(PlainNumber,                  kIntegral32 | kOtherNumber) \
  V(OrderedNumber,                kPlainNumber | kMinusZero) \
  V(MinusZeroOrNaN,               kMinusZero | kNaN) \
  V(Number,                       kOrderedNumber | kNaN) \
  V(Numeric,                      kNumber | kBigInt) \
  V(String,                       kInternalizedString | kOtherString) \
  V(UniqueName,                   kSymbol | kInternalizedString) \
  V(Name,                         kSymbol | kString) \
  V(InternalizedStringOrNull,     kInternalizedString | kNull) \
  V(BooleanOrNumber,              kBoolean | kNumber) \
  V(BooleanOrNullOrNumber,        kBooleanOrNumber | kNull) \
  V(BooleanOrNullOrUndefined,     kBoolean | kNull | kUndefined) \
  V(Oddball,                      kBooleanOrNullOrUndefined | kHole) \
  V(NullOrNumber,                 kNull | kNumber) \
  V(NullOrUndefined,              kNull | kUndefined) \
  V(Undetectable,                 kNullOrUndefined | kOtherUndetectable) \
  V(NumberOrHole,                 kNumber | kHole) \
  V(NumberOrOddball,              kNumber | kNullOrUndefined | kBoolean | \
                                  kHole) \
  V(NumericOrString,              kNumeric | kString) \
  V(NumberOrUndefined,            kNumber | kUndefined) \
  V(NumberOrUndefinedOrNullOrBoolean,  \
                                  kNumber | kNullOrUndefined | kBoolean) \
  V(PlainPrimitive,               kNumber | kString | kBoolean | \
                                  kNullOrUndefined) \
  V(NonBigIntPrimitive,           kSymbol | kPlainPrimitive) \
  V(Primitive,                    kBigInt | kNonBigIntPrimitive) \
  V(OtherUndetectableOrUndefined, kOtherUndetectable | kUndefined) \
  V(Proxy,                        kCallableProxy | kOtherProxy) \
  V(ArrayOrOtherObject,           kArray | kOtherObject) \
  V(ArrayOrProxy,                 kArray | kProxy) \
  V(DetectableCallable,           kFunction | kBoundFunction | \
                                  kOtherCallable | kCallableProxy) \
  V(Callable,                     kDetectableCallable | kOtherUndetectable) \
  V(NonCallable,                  kArray | kOtherObject | kOtherProxy) \
  V(NonCallableOrNull,            kNonCallable | kNull) \
  V(DetectableObject,             kArray | kFunction | kBoundFunction | \
                                  kOtherCallable | kOtherObject) \
  V(DetectableReceiver,           kDetectableObject | kProxy) \
  V(DetectableReceiverOrNull,     kDetectableReceiver | kNull) \
  V(Object,                       kDetectableObject | kOtherUndetectable) \
  V(Receiver,                     kObject | kProxy) \
  V(ReceiverOrUndefined,          kReceiver | kUndefined) \
  V(ReceiverOrNullOrUndefined,    kReceiver | kNull | kUndefined) \
  V(SymbolOrReceiver,             kSymbol | kReceiver) \
  V(StringOrReceiver,             kString | kReceiver) \
  V(Unique,                       kBoolean | kUniqueName | kNull | \
                                  kUndefined | kReceiver) \
  V(Internal,                     kHole | kExternalPointer | kOtherInternal) \
  V(NonInternal,                  kPrimitive | kReceiver) \
  V(NonBigInt,                    kNonBigIntPrimitive | kReceiver) \
  V(NonNumber,                    kBigInt | kUnique | kString | kInternal) \
  V(Any,                          0xfffffffeu)

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

#define BITSET_TYPE_LIST(V)    \
  INTERNAL_BITSET_TYPE_LIST(V) \
  PROPER_BITSET_TYPE_LIST(V)

class HeapConstantType;
class OtherNumberConstantType;
class TupleType;
class Type;
class UnionType;

// -----------------------------------------------------------------------------
// Bitset types (internal).

class V8_EXPORT_PRIVATE BitsetType {
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

  static bool IsNone(bitset bits) { return bits == kNone; }

  static bool Is(bitset bits1, bitset bits2) {
    return (bits1 | bits2) == bits2;
  }

  static double Min(bitset);
  static double Max(bitset);

  static bitset Glb(double min, double max);
  static bitset Lub(HeapObjectType const& type) {
    return Lub<HeapObjectType>(type);
  }
  static bitset Lub(MapRef const& map) { return Lub<MapRef>(map); }
  static bitset Lub(double value);
  static bitset Lub(double min, double max);
  static bitset ExpandInternals(bitset bits);

  static const char* Name(bitset);
  static void Print(std::ostream& os, bitset);  // NOLINT
#ifdef DEBUG
  static void Print(bitset);
#endif

  static bitset NumberBits(bitset bits);

 private:
  struct Boundary {
    bitset internal;
    bitset external;
    double min;
  };
  static const Boundary BoundariesArray[];
  static inline const Boundary* Boundaries();
  static inline size_t BoundariesSize();

  template <typename MapRefLike>
  static bitset Lub(MapRefLike const& map);
};

// -----------------------------------------------------------------------------
// Superclass for non-bitset types (internal).
class TypeBase {
 protected:
  friend class Type;

  enum Kind { kHeapConstant, kOtherNumberConstant, kTuple, kUnion, kRange };

  Kind kind() const { return kind_; }
  explicit TypeBase(Kind kind) : kind_(kind) {}

  static bool IsKind(Type type, Kind kind);

 private:
  Kind kind_;
};

// -----------------------------------------------------------------------------
// Range types.

class RangeType : public TypeBase {
 public:
  struct Limits {
    double min;
    double max;
    Limits(double min, double max) : min(min), max(max) {}
    explicit Limits(const RangeType* range)
        : min(range->Min()), max(range->Max()) {}
    bool IsEmpty();
    static Limits Empty() { return Limits(1, 0); }
    static Limits Intersect(Limits lhs, Limits rhs);
    static Limits Union(Limits lhs, Limits rhs);
  };

  double Min() const { return limits_.min; }
  double Max() const { return limits_.max; }

  static bool IsInteger(double x) {
    return nearbyint(x) == x && !IsMinusZero(x);  // Allows for infinities.
  }

 private:
  friend class Type;
  friend class BitsetType;
  friend class UnionType;

  static RangeType* New(double min, double max, Zone* zone) {
    return New(Limits(min, max), zone);
  }

  static RangeType* New(Limits lim, Zone* zone) {
    DCHECK(IsInteger(lim.min) && IsInteger(lim.max));
    DCHECK(lim.min <= lim.max);
    BitsetType::bitset bits = BitsetType::Lub(lim.min, lim.max);

    return new (zone->New(sizeof(RangeType))) RangeType(bits, lim);
  }

  RangeType(BitsetType::bitset bitset, Limits limits)
      : TypeBase(kRange), bitset_(bitset), limits_(limits) {}

  BitsetType::bitset Lub() const { return bitset_; }

  BitsetType::bitset bitset_;
  Limits limits_;
};

// -----------------------------------------------------------------------------
// The actual type.

class V8_EXPORT_PRIVATE Type {
 public:
  typedef BitsetType::bitset bitset;  // Internal

// Constructors.
#define DEFINE_TYPE_CONSTRUCTOR(type, value) \
  static Type type() { return NewBitset(BitsetType::k##type); }
  PROPER_BITSET_TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
#undef DEFINE_TYPE_CONSTRUCTOR

  Type() : payload_(0) {}

  static Type SignedSmall() { return NewBitset(BitsetType::SignedSmall()); }
  static Type UnsignedSmall() { return NewBitset(BitsetType::UnsignedSmall()); }

  static Type OtherNumberConstant(double value, Zone* zone);
  static Type HeapConstant(JSHeapBroker* broker, Handle<i::Object> value,
                           Zone* zone);
  static Type HeapConstant(const HeapObjectRef& value, Zone* zone);
  static Type Range(double min, double max, Zone* zone);
  static Type Range(RangeType::Limits lims, Zone* zone);
  static Type Tuple(Type first, Type second, Type third, Zone* zone);
  static Type Union(int length, Zone* zone);

  // NewConstant is a factory that returns Constant, Range or Number.
  static Type NewConstant(JSHeapBroker* broker, Handle<i::Object> value,
                          Zone* zone);
  static Type NewConstant(double value, Zone* zone);

  static Type Union(Type type1, Type type2, Zone* zone);
  static Type Intersect(Type type1, Type type2, Zone* zone);

  static Type For(HeapObjectType const& type) {
    return NewBitset(BitsetType::ExpandInternals(BitsetType::Lub(type)));
  }
  static Type For(MapRef const& type) {
    return NewBitset(BitsetType::ExpandInternals(BitsetType::Lub(type)));
  }

  // Predicates.
  bool IsNone() const { return payload_ == None().payload_; }
  bool IsInvalid() const { return payload_ == 0u; }

  bool Is(Type that) const {
    return payload_ == that.payload_ || this->SlowIs(that);
  }
  bool Maybe(Type that) const;
  bool Equals(Type that) const { return this->Is(that) && that.Is(*this); }

  // Inspection.
  bool IsBitset() const { return payload_ & 1; }
  bool IsRange() const { return IsKind(TypeBase::kRange); }
  bool IsHeapConstant() const { return IsKind(TypeBase::kHeapConstant); }
  bool IsOtherNumberConstant() const {
    return IsKind(TypeBase::kOtherNumberConstant);
  }
  bool IsTuple() const { return IsKind(TypeBase::kTuple); }

  const HeapConstantType* AsHeapConstant() const;
  const OtherNumberConstantType* AsOtherNumberConstant() const;
  const RangeType* AsRange() const;
  const TupleType* AsTuple() const;

  // Minimum and maximum of a numeric type.
  // These functions do not distinguish between -0 and +0.  NaN is ignored.
  // Only call them on subtypes of Number whose intersection with OrderedNumber
  // is not empty.
  double Min() const;
  double Max() const;

  // Extracts a range from the type: if the type is a range or a union
  // containing a range, that range is returned; otherwise, nullptr is returned.
  Type GetRange() const;

  int NumConstants() const;

  static Type Invalid() { return Type(); }

  bool operator==(Type other) const { return payload_ == other.payload_; }
  bool operator!=(Type other) const { return payload_ != other.payload_; }

  // Printing.

  void PrintTo(std::ostream& os) const;

#ifdef DEBUG
  void Print() const;
#endif

  // Helpers for testing.
  bool IsUnionForTesting() { return IsUnion(); }
  bitset AsBitsetForTesting() { return AsBitset(); }
  const UnionType* AsUnionForTesting() { return AsUnion(); }
  Type BitsetGlbForTesting() { return NewBitset(BitsetGlb()); }
  Type BitsetLubForTesting() { return NewBitset(BitsetLub()); }

 private:
  // Friends.
  template <class>
  friend class Iterator;
  friend BitsetType;
  friend UnionType;
  friend size_t hash_value(Type type);

  explicit Type(bitset bits) : payload_(bits | 1u) {}
  Type(TypeBase* type_base)  // NOLINT(runtime/explicit)
      : payload_(reinterpret_cast<uintptr_t>(type_base)) {}

  // Internal inspection.
  bool IsKind(TypeBase::Kind kind) const {
    if (IsBitset()) return false;
    const TypeBase* base = ToTypeBase();
    return base->kind() == kind;
  }

  const TypeBase* ToTypeBase() const {
    return reinterpret_cast<TypeBase*>(payload_);
  }
  static Type FromTypeBase(TypeBase* type) { return Type(type); }

  bool IsAny() const { return payload_ == Any().payload_; }
  bool IsUnion() const { return IsKind(TypeBase::kUnion); }

  bitset AsBitset() const {
    DCHECK(IsBitset());
    return static_cast<bitset>(payload_) ^ 1u;
  }

  const UnionType* AsUnion() const;

  bitset BitsetGlb() const;  // greatest lower bound that's a bitset
  bitset BitsetLub() const;  // least upper bound that's a bitset

  bool SlowIs(Type that) const;

  static Type NewBitset(bitset bits) { return Type(bits); }

  static bool Overlap(const RangeType* lhs, const RangeType* rhs);
  static bool Contains(const RangeType* lhs, const RangeType* rhs);

  static int UpdateRange(Type type, UnionType* result, int size, Zone* zone);

  static RangeType::Limits IntersectRangeAndBitset(Type range, Type bits,
                                                   Zone* zone);
  static RangeType::Limits ToLimits(bitset bits, Zone* zone);

  bool SimplyEquals(Type that) const;

  static int AddToUnion(Type type, UnionType* result, int size, Zone* zone);
  static int IntersectAux(Type type, Type other, UnionType* result, int size,
                          RangeType::Limits* limits, Zone* zone);
  static Type NormalizeUnion(UnionType* unioned, int size, Zone* zone);
  static Type NormalizeRangeAndBitset(Type range, bitset* bits, Zone* zone);

  // If LSB is set, the payload is a bitset; if LSB is clear, the payload is
  // a pointer to a subtype of the TypeBase class.
  uintptr_t payload_;
};

inline size_t hash_value(Type type) { return type.payload_; }
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, Type type);

// -----------------------------------------------------------------------------
// Constant types.

class OtherNumberConstantType : public TypeBase {
 public:
  double Value() const { return value_; }

  static bool IsOtherNumberConstant(double value);

 private:
  friend class Type;
  friend class BitsetType;

  static OtherNumberConstantType* New(double value, Zone* zone) {
    return new (zone->New(sizeof(OtherNumberConstantType)))
        OtherNumberConstantType(value);  // NOLINT
  }

  explicit OtherNumberConstantType(double value)
      : TypeBase(kOtherNumberConstant), value_(value) {
    CHECK(IsOtherNumberConstant(value));
  }

  BitsetType::bitset Lub() const { return BitsetType::kOtherNumber; }

  double value_;
};

class V8_EXPORT_PRIVATE HeapConstantType : public NON_EXPORTED_BASE(TypeBase) {
 public:
  Handle<HeapObject> Value() const;
  const HeapObjectRef& Ref() const { return heap_ref_; }

 private:
  friend class Type;
  friend class BitsetType;

  static HeapConstantType* New(const HeapObjectRef& heap_ref, Zone* zone) {
    DCHECK(!heap_ref.IsHeapNumber());
    DCHECK_IMPLIES(heap_ref.IsString(), heap_ref.IsInternalizedString());
    BitsetType::bitset bitset = BitsetType::Lub(heap_ref.GetHeapObjectType());
    return new (zone->New(sizeof(HeapConstantType)))
        HeapConstantType(bitset, heap_ref);
  }

  HeapConstantType(BitsetType::bitset bitset, const HeapObjectRef& heap_ref);

  BitsetType::bitset Lub() const { return bitset_; }

  BitsetType::bitset bitset_;
  HeapObjectRef heap_ref_;
};

// -----------------------------------------------------------------------------
// Superclass for types with variable number of type fields.
class StructuralType : public TypeBase {
 public:
  int LengthForTesting() const { return Length(); }

 protected:
  friend class Type;

  int Length() const { return length_; }

  Type Get(int i) const {
    DCHECK(0 <= i && i < this->Length());
    return elements_[i];
  }

  void Set(int i, Type type) {
    DCHECK(0 <= i && i < this->Length());
    elements_[i] = type;
  }

  void Shrink(int length) {
    DCHECK(2 <= length && length <= this->Length());
    length_ = length;
  }

  StructuralType(Kind kind, int length, Zone* zone)
      : TypeBase(kind), length_(length) {
    elements_ = reinterpret_cast<Type*>(zone->New(sizeof(Type) * length));
  }

 private:
  int length_;
  Type* elements_;
};

// -----------------------------------------------------------------------------
// Tuple types.

class TupleType : public StructuralType {
 public:
  int Arity() const { return this->Length(); }
  Type Element(int i) const { return this->Get(i); }

  void InitElement(int i, Type type) { this->Set(i, type); }

 private:
  friend class Type;

  TupleType(int length, Zone* zone) : StructuralType(kTuple, length, zone) {}

  static TupleType* New(int length, Zone* zone) {
    return new (zone->New(sizeof(TupleType))) TupleType(length, zone);
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

  static UnionType* New(int length, Zone* zone) {
    return new (zone->New(sizeof(UnionType))) UnionType(length, zone);
  }

  bool Wellformed() const;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPES_H_
