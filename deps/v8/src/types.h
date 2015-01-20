// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPES_H_
#define V8_TYPES_H_

#include "src/conversions.h"
#include "src/factory.h"
#include "src/handles.h"
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
// maximum value.  Either value might be an infinity.
//
// Constant(v) is considered a subtype of Range(x..y) if v happens to be an
// integer between x and y.
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
//
// There are two type representations, using different allocation:
//
// - class Type (zone-allocated, for compiler and concurrent compilation)
// - class HeapType (heap-allocated, for persistent types)
//
// Both provide the same API, and the Convert method can be used to interconvert
// them. For zone types, no query method touches the heap, only constructors do.


// -----------------------------------------------------------------------------
// Values for bitset types

#define MASK_BITSET_TYPE_LIST(V) \
  V(Representation, 0xfff00000u) \
  V(Semantic,       0x000ffffeu)

#define REPRESENTATION(k) ((k) & BitsetType::kRepresentation)
#define SEMANTIC(k)       ((k) & BitsetType::kSemantic)

#define REPRESENTATION_BITSET_TYPE_LIST(V)    \
  V(None,               0)                    \
  V(UntaggedBit,        1u << 20 | kSemantic) \
  V(UntaggedSigned8,    1u << 21 | kSemantic) \
  V(UntaggedSigned16,   1u << 22 | kSemantic) \
  V(UntaggedSigned32,   1u << 23 | kSemantic) \
  V(UntaggedUnsigned8,  1u << 24 | kSemantic) \
  V(UntaggedUnsigned16, 1u << 25 | kSemantic) \
  V(UntaggedUnsigned32, 1u << 26 | kSemantic) \
  V(UntaggedFloat32,    1u << 27 | kSemantic) \
  V(UntaggedFloat64,    1u << 28 | kSemantic) \
  V(UntaggedPointer,    1u << 29 | kSemantic) \
  V(TaggedSigned,       1u << 30 | kSemantic) \
  V(TaggedPointer,      1u << 31 | kSemantic) \
  \
  V(UntaggedSigned,     kUntaggedSigned8 | kUntaggedSigned16 |              \
                        kUntaggedSigned32)                                  \
  V(UntaggedUnsigned,   kUntaggedUnsigned8 | kUntaggedUnsigned16 |          \
                        kUntaggedUnsigned32)                                \
  V(UntaggedIntegral8,  kUntaggedSigned8 | kUntaggedUnsigned8)              \
  V(UntaggedIntegral16, kUntaggedSigned16 | kUntaggedUnsigned16)            \
  V(UntaggedIntegral32, kUntaggedSigned32 | kUntaggedUnsigned32)            \
  V(UntaggedIntegral,   kUntaggedBit | kUntaggedSigned | kUntaggedUnsigned) \
  V(UntaggedFloat,      kUntaggedFloat32 | kUntaggedFloat64)                \
  V(UntaggedNumber,     kUntaggedIntegral | kUntaggedFloat)                 \
  V(Untagged,           kUntaggedNumber | kUntaggedPointer)                 \
  V(Tagged,             kTaggedSigned | kTaggedPointer)

#define INTERNAL_BITSET_TYPE_LIST(V)                                      \
  V(OtherUnsigned31, 1u << 1 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherUnsigned32, 1u << 2 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherSigned32,   1u << 3 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherNumber,     1u << 4 | REPRESENTATION(kTagged | kUntaggedNumber))

#define SEMANTIC_BITSET_TYPE_LIST(V) \
  V(NegativeSignedSmall, 1u << 5  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Null,                1u << 6  | REPRESENTATION(kTaggedPointer)) \
  V(Undefined,           1u << 7  | REPRESENTATION(kTaggedPointer)) \
  V(Boolean,             1u << 8  | REPRESENTATION(kTaggedPointer)) \
  V(UnsignedSmall,       1u << 9  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(MinusZero,           1u << 10 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(NaN,                 1u << 11 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Symbol,              1u << 12 | REPRESENTATION(kTaggedPointer)) \
  V(InternalizedString,  1u << 13 | REPRESENTATION(kTaggedPointer)) \
  V(OtherString,         1u << 14 | REPRESENTATION(kTaggedPointer)) \
  V(Undetectable,        1u << 15 | REPRESENTATION(kTaggedPointer)) \
  V(Array,               1u << 16 | REPRESENTATION(kTaggedPointer)) \
  V(OtherObject,         1u << 17 | REPRESENTATION(kTaggedPointer)) \
  V(Proxy,               1u << 18 | REPRESENTATION(kTaggedPointer)) \
  V(Internal,            1u << 19 | REPRESENTATION(kTagged | kUntagged)) \
  \
  V(SignedSmall,         kUnsignedSmall | kNegativeSignedSmall) \
  V(Signed32,            kSignedSmall | kOtherUnsigned31 | kOtherSigned32) \
  V(NegativeSigned32,    kNegativeSignedSmall | kOtherSigned32) \
  V(NonNegativeSigned32, kUnsignedSmall | kOtherUnsigned31) \
  V(Unsigned32,          kUnsignedSmall | kOtherUnsigned31 | kOtherUnsigned32) \
  V(Integral32,          kSigned32 | kUnsigned32) \
  V(PlainNumber,         kIntegral32 | kOtherNumber) \
  V(OrderedNumber,       kPlainNumber | kMinusZero) \
  V(Number,              kOrderedNumber | kNaN) \
  V(String,              kInternalizedString | kOtherString) \
  V(UniqueName,          kSymbol | kInternalizedString) \
  V(Name,                kSymbol | kString) \
  V(NumberOrString,      kNumber | kString) \
  V(PlainPrimitive,      kNumberOrString | kBoolean | kNull | kUndefined) \
  V(Primitive,           kSymbol | kPlainPrimitive) \
  V(DetectableObject,    kArray | kOtherObject) \
  V(DetectableReceiver,  kDetectableObject | kProxy) \
  V(Detectable,          kDetectableReceiver | kNumber | kName) \
  V(Object,              kDetectableObject | kUndetectable) \
  V(Receiver,            kObject | kProxy) \
  V(StringOrReceiver,    kString | kReceiver) \
  V(Unique,              kBoolean | kUniqueName | kNull | kUndefined | \
                         kReceiver) \
  V(NonNumber,           kUnique | kString | kInternal) \
  V(Any,                 0xfffffffeu)


/*
 * The following diagrams show how integers (in the mathematical sense) are
 * divided among the different atomic numerical types.
 *
 * If SmiValuesAre31Bits():
 *
 *   ON    OS32     OSS     US     OU31    OU32     ON
 * ______[_______[_______[_______[_______[_______[_______
 *     -2^31   -2^30     0      2^30    2^31    2^32
 *
 * Otherwise:
 *
 *   ON         OSS             US         OU32     ON
 * ______[_______________[_______________[_______[_______
 *     -2^31             0              2^31    2^32
 *
 *
 * E.g., OtherUnsigned32 (OU32) covers all integers from 2^31 to 2^32-1.
 *
 * NOTE: OtherSigned32 (OS32) and OU31 (OtherUnsigned31) are empty if Smis are
 *       32-bit wide.  They should thus never be used directly, only indirectly
 *       via e.g. Number.
 */

#define PROPER_BITSET_TYPE_LIST(V) \
  REPRESENTATION_BITSET_TYPE_LIST(V) \
  SEMANTIC_BITSET_TYPE_LIST(V)

#define BITSET_TYPE_LIST(V)          \
  MASK_BITSET_TYPE_LIST(V)           \
  REPRESENTATION_BITSET_TYPE_LIST(V) \
  INTERNAL_BITSET_TYPE_LIST(V)       \
  SEMANTIC_BITSET_TYPE_LIST(V)


// -----------------------------------------------------------------------------
// The abstract Type class, parameterized over the low-level representation.

// struct Config {
//   typedef TypeImpl<Config> Type;
//   typedef Base;
//   typedef Struct;
//   typedef Region;
//   template<class> struct Handle { typedef type; }  // No template typedefs...
//   template<class T> static Handle<T>::type null_handle();
//   template<class T> static Handle<T>::type handle(T* t);  // !is_bitset(t)
//   template<class T> static Handle<T>::type cast(Handle<Type>::type);
//   static bool is_bitset(Type*);
//   static bool is_class(Type*);
//   static bool is_struct(Type*, int tag);
//   static bitset as_bitset(Type*);
//   static i::Handle<i::Map> as_class(Type*);
//   static Handle<Struct>::type as_struct(Type*);
//   static Type* from_bitset(bitset);
//   static Handle<Type>::type from_bitset(bitset, Region*);
//   static Handle<Type>::type from_class(i::Handle<Map>, Region*);
//   static Handle<Type>::type from_struct(Handle<Struct>::type, int tag);
//   static Handle<Struct>::type struct_create(int tag, int length, Region*);
//   static void struct_shrink(Handle<Struct>::type, int length);
//   static int struct_tag(Handle<Struct>::type);
//   static int struct_length(Handle<Struct>::type);
//   static Handle<Type>::type struct_get(Handle<Struct>::type, int);
//   static void struct_set(Handle<Struct>::type, int, Handle<Type>::type);
//   template<class V>
//   static i::Handle<V> struct_get_value(Handle<Struct>::type, int);
//   template<class V>
//   static void struct_set_value(Handle<Struct>::type, int, i::Handle<V>);
// }
template<class Config>
class TypeImpl : public Config::Base {
 public:
  // Auxiliary types.

  typedef uint32_t bitset;  // Internal
  class BitsetType;         // Internal
  class StructuralType;     // Internal
  class UnionType;          // Internal

  class ClassType;
  class ConstantType;
  class RangeType;
  class ContextType;
  class ArrayType;
  class FunctionType;

  typedef typename Config::template Handle<TypeImpl>::type TypeHandle;
  typedef typename Config::template Handle<ClassType>::type ClassHandle;
  typedef typename Config::template Handle<ConstantType>::type ConstantHandle;
  typedef typename Config::template Handle<RangeType>::type RangeHandle;
  typedef typename Config::template Handle<ContextType>::type ContextHandle;
  typedef typename Config::template Handle<ArrayType>::type ArrayHandle;
  typedef typename Config::template Handle<FunctionType>::type FunctionHandle;
  typedef typename Config::template Handle<UnionType>::type UnionHandle;
  typedef typename Config::Region Region;

  // Constructors.

  #define DEFINE_TYPE_CONSTRUCTOR(type, value)                                \
    static TypeImpl* type() {                                                 \
      return BitsetType::New(BitsetType::k##type);                            \
    }                                                                         \
    static TypeHandle type(Region* region) {                                  \
      return BitsetType::New(BitsetType::k##type, region);                    \
    }
  PROPER_BITSET_TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
  #undef DEFINE_TYPE_CONSTRUCTOR

  static TypeHandle Class(i::Handle<i::Map> map, Region* region) {
    return ClassType::New(map, region);
  }
  static TypeHandle Constant(i::Handle<i::Object> value, Region* region) {
    return ConstantType::New(value, region);
  }
  static TypeHandle Range(
      i::Handle<i::Object> min, i::Handle<i::Object> max, Region* region) {
    return RangeType::New(min, max, region);
  }
  static TypeHandle Context(TypeHandle outer, Region* region) {
    return ContextType::New(outer, region);
  }
  static TypeHandle Array(TypeHandle element, Region* region) {
    return ArrayType::New(element, region);
  }
  static FunctionHandle Function(
      TypeHandle result, TypeHandle receiver, int arity, Region* region) {
    return FunctionType::New(result, receiver, arity, region);
  }
  static TypeHandle Function(TypeHandle result, Region* region) {
    return Function(result, Any(region), 0, region);
  }
  static TypeHandle Function(
      TypeHandle result, TypeHandle param0, Region* region) {
    FunctionHandle function = Function(result, Any(region), 1, region);
    function->InitParameter(0, param0);
    return function;
  }
  static TypeHandle Function(
      TypeHandle result, TypeHandle param0, TypeHandle param1, Region* region) {
    FunctionHandle function = Function(result, Any(region), 2, region);
    function->InitParameter(0, param0);
    function->InitParameter(1, param1);
    return function;
  }
  static TypeHandle Function(
      TypeHandle result, TypeHandle param0, TypeHandle param1,
      TypeHandle param2, Region* region) {
    FunctionHandle function = Function(result, Any(region), 3, region);
    function->InitParameter(0, param0);
    function->InitParameter(1, param1);
    function->InitParameter(2, param2);
    return function;
  }

  static TypeHandle Union(TypeHandle type1, TypeHandle type2, Region* reg);
  static TypeHandle Intersect(TypeHandle type1, TypeHandle type2, Region* reg);
  static TypeImpl* Union(TypeImpl* type1, TypeImpl* type2) {
    return BitsetType::New(type1->AsBitset() | type2->AsBitset());
  }
  static TypeImpl* Intersect(TypeImpl* type1, TypeImpl* type2) {
    return BitsetType::New(type1->AsBitset() & type2->AsBitset());
  }

  static TypeHandle Of(double value, Region* region) {
    return Config::from_bitset(BitsetType::Lub(value), region);
  }
  static TypeHandle Of(i::Object* value, Region* region) {
    return Config::from_bitset(BitsetType::Lub(value), region);
  }
  static TypeHandle Of(i::Handle<i::Object> value, Region* region) {
    return Of(*value, region);
  }

  // Predicates.

  bool IsInhabited() { return BitsetType::IsInhabited(this->BitsetLub()); }

  bool Is(TypeImpl* that) { return this == that || this->SlowIs(that); }
  template<class TypeHandle>
  bool Is(TypeHandle that) { return this->Is(*that); }

  bool Maybe(TypeImpl* that);
  template<class TypeHandle>
  bool Maybe(TypeHandle that) { return this->Maybe(*that); }

  bool Equals(TypeImpl* that) { return this->Is(that) && that->Is(this); }
  template<class TypeHandle>
  bool Equals(TypeHandle that) { return this->Equals(*that); }

  // Equivalent to Constant(val)->Is(this), but avoiding allocation.
  bool Contains(i::Object* val);
  bool Contains(i::Handle<i::Object> val) { return this->Contains(*val); }

  // State-dependent versions of the above that consider subtyping between
  // a constant and its map class.
  inline static TypeHandle NowOf(i::Object* value, Region* region);
  static TypeHandle NowOf(i::Handle<i::Object> value, Region* region) {
    return NowOf(*value, region);
  }
  bool NowIs(TypeImpl* that);
  template<class TypeHandle>
  bool NowIs(TypeHandle that)  { return this->NowIs(*that); }
  inline bool NowContains(i::Object* val);
  bool NowContains(i::Handle<i::Object> val) { return this->NowContains(*val); }

  bool NowStable();

  // Inspection.

  bool IsClass() {
    return Config::is_class(this)
        || Config::is_struct(this, StructuralType::kClassTag);
  }
  bool IsConstant() {
    return Config::is_struct(this, StructuralType::kConstantTag);
  }
  bool IsRange() {
    return Config::is_struct(this, StructuralType::kRangeTag);
  }
  bool IsContext() {
    return Config::is_struct(this, StructuralType::kContextTag);
  }
  bool IsArray() {
    return Config::is_struct(this, StructuralType::kArrayTag);
  }
  bool IsFunction() {
    return Config::is_struct(this, StructuralType::kFunctionTag);
  }

  ClassType* AsClass() { return ClassType::cast(this); }
  ConstantType* AsConstant() { return ConstantType::cast(this); }
  RangeType* AsRange() { return RangeType::cast(this); }
  ContextType* AsContext() { return ContextType::cast(this); }
  ArrayType* AsArray() { return ArrayType::cast(this); }
  FunctionType* AsFunction() { return FunctionType::cast(this); }

  // Minimum and maximum of a numeric type.
  // These functions do not distinguish between -0 and +0.  If the type equals
  // kNaN, they return NaN; otherwise kNaN is ignored.  Only call these
  // functions on subtypes of Number.
  double Min();
  double Max();

  // Extracts a range from the type. If the type is a range, it just
  // returns it; if it is a union, it returns the range component.
  // Note that it does not contain range for constants.
  RangeType* GetRange();

  int NumClasses();
  int NumConstants();

  template<class T> class Iterator;
  Iterator<i::Map> Classes() {
    if (this->IsBitset()) return Iterator<i::Map>();
    return Iterator<i::Map>(Config::handle(this));
  }
  Iterator<i::Object> Constants() {
    if (this->IsBitset()) return Iterator<i::Object>();
    return Iterator<i::Object>(Config::handle(this));
  }

  // Casting and conversion.

  static inline TypeImpl* cast(typename Config::Base* object);

  template<class OtherTypeImpl>
  static TypeHandle Convert(
      typename OtherTypeImpl::TypeHandle type, Region* region);

  // Printing.

  enum PrintDimension { BOTH_DIMS, SEMANTIC_DIM, REPRESENTATION_DIM };

  void PrintTo(std::ostream& os, PrintDimension dim = BOTH_DIMS);  // NOLINT

#ifdef DEBUG
  void Print();
#endif

 protected:
  // Friends.

  template<class> friend class Iterator;
  template<class> friend class TypeImpl;

  // Handle conversion.

  template<class T>
  static typename Config::template Handle<T>::type handle(T* type) {
    return Config::handle(type);
  }
  TypeImpl* unhandle() { return this; }

  // Internal inspection.

  bool IsNone() { return this == None(); }
  bool IsAny() { return this == Any(); }
  bool IsBitset() { return Config::is_bitset(this); }
  bool IsUnion() { return Config::is_struct(this, StructuralType::kUnionTag); }

  bitset AsBitset() {
    DCHECK(this->IsBitset());
    return static_cast<BitsetType*>(this)->Bitset();
  }
  UnionType* AsUnion() { return UnionType::cast(this); }

  // Auxiliary functions.

  bitset BitsetGlb() { return BitsetType::Glb(this); }
  bitset BitsetLub() { return BitsetType::Lub(this); }

  bool SlowIs(TypeImpl* that);

  static bool IsInteger(double x) {
    return nearbyint(x) == x && !i::IsMinusZero(x);  // Allows for infinities.
  }
  static bool IsInteger(i::Object* x) {
    return x->IsNumber() && IsInteger(x->Number());
  }

  struct Limits {
    i::Handle<i::Object> min;
    i::Handle<i::Object> max;
    Limits(i::Handle<i::Object> min, i::Handle<i::Object> max) :
      min(min), max(max) {}
    explicit Limits(RangeType* range) :
      min(range->Min()), max(range->Max()) {}
  };

  static Limits Intersect(Limits lhs, Limits rhs);
  static Limits Union(Limits lhs, Limits rhs);
  static bool Overlap(RangeType* lhs, RangeType* rhs);
  static bool Contains(RangeType* lhs, RangeType* rhs);
  static bool Contains(RangeType* range, i::Object* val);

  static int UpdateRange(
      RangeHandle type, UnionHandle result, int size, Region* region);

  bool SimplyEquals(TypeImpl* that);
  template<class TypeHandle>
  bool SimplyEquals(TypeHandle that) { return this->SimplyEquals(*that); }

  static int AddToUnion(
      TypeHandle type, UnionHandle result, int size, Region* region);
  static int IntersectAux(
      TypeHandle type, TypeHandle other,
      UnionHandle result, int size, Region* region);
  static TypeHandle NormalizeUnion(UnionHandle unioned, int size);
};


// -----------------------------------------------------------------------------
// Bitset types (internal).

template<class Config>
class TypeImpl<Config>::BitsetType : public TypeImpl<Config> {
 protected:
  friend class TypeImpl<Config>;

  enum {
    #define DECLARE_TYPE(type, value) k##type = (value),
    BITSET_TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE
    kUnusedEOL = 0
  };

  bitset Bitset() { return Config::as_bitset(this); }

  static TypeImpl* New(bitset bits) {
    DCHECK(bits == kNone || IsInhabited(bits));

    if (FLAG_enable_slow_asserts) {
      // Check that the bitset does not contain any holes in number ranges.
      bitset mask = kSemantic;
      if (!i::SmiValuesAre31Bits()) {
        mask &= ~(kOtherUnsigned31 | kOtherSigned32);
      }
      bitset number_bits = bits & kPlainNumber & mask;
      if (number_bits != 0) {
        bitset lub = Lub(Min(number_bits), Max(number_bits)) & mask;
        CHECK(lub == number_bits);
      }
    }

    return Config::from_bitset(bits);
  }
  static TypeHandle New(bitset bits, Region* region) {
    DCHECK(bits == kNone || IsInhabited(bits));
    return Config::from_bitset(bits, region);
  }
  // TODO(neis): Eventually allow again for types with empty semantics
  // part and modify intersection and possibly subtyping accordingly.

  static bool IsInhabited(bitset bits) {
    return bits & kSemantic;
  }

  static bool Is(bitset bits1, bitset bits2) {
    return (bits1 | bits2) == bits2;
  }

  static double Min(bitset);
  static double Max(bitset);

  static bitset Glb(TypeImpl* type);  // greatest lower bound that's a bitset
  static bitset Lub(TypeImpl* type);  // least upper bound that's a bitset
  static bitset Lub(i::Map* map);
  static bitset Lub(i::Object* value);
  static bitset Lub(double value);
  static bitset Lub(double min, double max);

  static const char* Name(bitset);
  static void Print(std::ostream& os, bitset);  // NOLINT
#ifdef DEBUG
  static void Print(bitset);
#endif

 private:
  struct BitsetMin{
    bitset bits;
    double min;
  };
  static const BitsetMin BitsetMins31[];
  static const BitsetMin BitsetMins32[];
  static const BitsetMin* BitsetMins() {
    return i::SmiValuesAre31Bits() ? BitsetMins31 : BitsetMins32;
  }
  static size_t BitsetMinsSize() {
    return i::SmiValuesAre31Bits() ? 7 : 5;
    /* arraysize(BitsetMins31) : arraysize(BitsetMins32); */
    // Using arraysize here doesn't compile on Windows.
  }
};


// -----------------------------------------------------------------------------
// Superclass for non-bitset types (internal).
// Contains a tag and a variable number of type or value fields.

template<class Config>
class TypeImpl<Config>::StructuralType : public TypeImpl<Config> {
 protected:
  template<class> friend class TypeImpl;
  friend struct ZoneTypeConfig;  // For tags.
  friend struct HeapTypeConfig;

  enum Tag {
    kClassTag,
    kConstantTag,
    kRangeTag,
    kContextTag,
    kArrayTag,
    kFunctionTag,
    kUnionTag
  };

  int Length() {
    return Config::struct_length(Config::as_struct(this));
  }
  TypeHandle Get(int i) {
    DCHECK(0 <= i && i < this->Length());
    return Config::struct_get(Config::as_struct(this), i);
  }
  void Set(int i, TypeHandle type) {
    DCHECK(0 <= i && i < this->Length());
    Config::struct_set(Config::as_struct(this), i, type);
  }
  void Shrink(int length) {
    DCHECK(2 <= length && length <= this->Length());
    Config::struct_shrink(Config::as_struct(this), length);
  }
  template<class V> i::Handle<V> GetValue(int i) {
    DCHECK(0 <= i && i < this->Length());
    return Config::template struct_get_value<V>(Config::as_struct(this), i);
  }
  template<class V> void SetValue(int i, i::Handle<V> x) {
    DCHECK(0 <= i && i < this->Length());
    Config::struct_set_value(Config::as_struct(this), i, x);
  }

  static TypeHandle New(Tag tag, int length, Region* region) {
    DCHECK(1 <= length);
    return Config::from_struct(Config::struct_create(tag, length, region));
  }
};


// -----------------------------------------------------------------------------
// Union types (internal).
// A union is a structured type with the following invariants:
// - its length is at least 2
// - at most one field is a bitset, and it must go into index 0
// - no field is a union
// - no field is a subtype of any other field
template<class Config>
class TypeImpl<Config>::UnionType : public StructuralType {
 public:
  static UnionHandle New(int length, Region* region) {
    return Config::template cast<UnionType>(
        StructuralType::New(StructuralType::kUnionTag, length, region));
  }

  static UnionType* cast(TypeImpl* type) {
    DCHECK(type->IsUnion());
    return static_cast<UnionType*>(type);
  }

  bool Wellformed();
};


// -----------------------------------------------------------------------------
// Class types.

template<class Config>
class TypeImpl<Config>::ClassType : public StructuralType {
 public:
  TypeHandle Bound(Region* region) {
    return Config::is_class(this) ?
        BitsetType::New(BitsetType::Lub(*Config::as_class(this)), region) :
        this->Get(0);
  }
  i::Handle<i::Map> Map() {
    return Config::is_class(this) ? Config::as_class(this) :
        this->template GetValue<i::Map>(1);
  }

  static ClassHandle New(i::Handle<i::Map> map, Region* region) {
    ClassHandle type =
        Config::template cast<ClassType>(Config::from_class(map, region));
    if (!type->IsClass()) {
      type = Config::template cast<ClassType>(
          StructuralType::New(StructuralType::kClassTag, 2, region));
      type->Set(0, BitsetType::New(BitsetType::Lub(*map), region));
      type->SetValue(1, map);
    }
    return type;
  }

  static ClassType* cast(TypeImpl* type) {
    DCHECK(type->IsClass());
    return static_cast<ClassType*>(type);
  }
};


// -----------------------------------------------------------------------------
// Constant types.

template<class Config>
class TypeImpl<Config>::ConstantType : public StructuralType {
 public:
  TypeHandle Bound() { return this->Get(0); }
  i::Handle<i::Object> Value() { return this->template GetValue<i::Object>(1); }

  static ConstantHandle New(i::Handle<i::Object> value, Region* region) {
    ConstantHandle type = Config::template cast<ConstantType>(
        StructuralType::New(StructuralType::kConstantTag, 2, region));
    type->Set(0, BitsetType::New(BitsetType::Lub(*value), region));
    type->SetValue(1, value);
    return type;
  }

  static ConstantType* cast(TypeImpl* type) {
    DCHECK(type->IsConstant());
    return static_cast<ConstantType*>(type);
  }
};
// TODO(neis): Also cache value if numerical.
// TODO(neis): Allow restricting the representation.


// -----------------------------------------------------------------------------
// Range types.

template<class Config>
class TypeImpl<Config>::RangeType : public StructuralType {
 public:
  int BitsetLub() { return this->Get(0)->AsBitset(); }
  i::Handle<i::Object> Min() { return this->template GetValue<i::Object>(1); }
  i::Handle<i::Object> Max() { return this->template GetValue<i::Object>(2); }

  static RangeHandle New(
      i::Handle<i::Object> min, i::Handle<i::Object> max, Region* region) {
    DCHECK(IsInteger(min->Number()) && IsInteger(max->Number()));
    DCHECK(min->Number() <= max->Number());
    RangeHandle type = Config::template cast<RangeType>(
        StructuralType::New(StructuralType::kRangeTag, 3, region));
    type->Set(0, BitsetType::New(
        BitsetType::Lub(min->Number(), max->Number()), region));
    type->SetValue(1, min);
    type->SetValue(2, max);
    return type;
  }

  static RangeHandle New(Limits lim, Region* region) {
    return New(lim.min, lim.max, region);
  }

  static RangeType* cast(TypeImpl* type) {
    DCHECK(type->IsRange());
    return static_cast<RangeType*>(type);
  }
};
// TODO(neis): Also cache min and max values.
// TODO(neis): Allow restricting the representation.


// -----------------------------------------------------------------------------
// Context types.

template<class Config>
class TypeImpl<Config>::ContextType : public StructuralType {
 public:
  TypeHandle Outer() { return this->Get(0); }

  static ContextHandle New(TypeHandle outer, Region* region) {
    ContextHandle type = Config::template cast<ContextType>(
        StructuralType::New(StructuralType::kContextTag, 1, region));
    type->Set(0, outer);
    return type;
  }

  static ContextType* cast(TypeImpl* type) {
    DCHECK(type->IsContext());
    return static_cast<ContextType*>(type);
  }
};


// -----------------------------------------------------------------------------
// Array types.

template<class Config>
class TypeImpl<Config>::ArrayType : public StructuralType {
 public:
  TypeHandle Element() { return this->Get(0); }

  static ArrayHandle New(TypeHandle element, Region* region) {
    ArrayHandle type = Config::template cast<ArrayType>(
        StructuralType::New(StructuralType::kArrayTag, 1, region));
    type->Set(0, element);
    return type;
  }

  static ArrayType* cast(TypeImpl* type) {
    DCHECK(type->IsArray());
    return static_cast<ArrayType*>(type);
  }
};


// -----------------------------------------------------------------------------
// Function types.

template<class Config>
class TypeImpl<Config>::FunctionType : public StructuralType {
 public:
  int Arity() { return this->Length() - 2; }
  TypeHandle Result() { return this->Get(0); }
  TypeHandle Receiver() { return this->Get(1); }
  TypeHandle Parameter(int i) { return this->Get(2 + i); }

  void InitParameter(int i, TypeHandle type) { this->Set(2 + i, type); }

  static FunctionHandle New(
      TypeHandle result, TypeHandle receiver, int arity, Region* region) {
    FunctionHandle type = Config::template cast<FunctionType>(
        StructuralType::New(StructuralType::kFunctionTag, 2 + arity, region));
    type->Set(0, result);
    type->Set(1, receiver);
    return type;
  }

  static FunctionType* cast(TypeImpl* type) {
    DCHECK(type->IsFunction());
    return static_cast<FunctionType*>(type);
  }
};


// -----------------------------------------------------------------------------
// Type iterators.

template<class Config> template<class T>
class TypeImpl<Config>::Iterator {
 public:
  bool Done() const { return index_ < 0; }
  i::Handle<T> Current();
  void Advance();

 private:
  template<class> friend class TypeImpl;

  Iterator() : index_(-1) {}
  explicit Iterator(TypeHandle type) : type_(type), index_(-1) {
    Advance();
  }

  inline bool matches(TypeHandle type);
  inline TypeHandle get_type();

  TypeHandle type_;
  int index_;
};


// -----------------------------------------------------------------------------
// Zone-allocated types; they are either (odd) integers to represent bitsets, or
// (even) pointers to structures for everything else.

struct ZoneTypeConfig {
  typedef TypeImpl<ZoneTypeConfig> Type;
  class Base {};
  typedef void* Struct;
  typedef i::Zone Region;
  template<class T> struct Handle { typedef T* type; };

  template<class T> static inline T* null_handle();
  template<class T> static inline T* handle(T* type);
  template<class T> static inline T* cast(Type* type);

  static inline bool is_bitset(Type* type);
  static inline bool is_class(Type* type);
  static inline bool is_struct(Type* type, int tag);

  static inline Type::bitset as_bitset(Type* type);
  static inline i::Handle<i::Map> as_class(Type* type);
  static inline Struct* as_struct(Type* type);

  static inline Type* from_bitset(Type::bitset);
  static inline Type* from_bitset(Type::bitset, Zone* zone);
  static inline Type* from_class(i::Handle<i::Map> map, Zone* zone);
  static inline Type* from_struct(Struct* structured);

  static inline Struct* struct_create(int tag, int length, Zone* zone);
  static inline void struct_shrink(Struct* structure, int length);
  static inline int struct_tag(Struct* structure);
  static inline int struct_length(Struct* structure);
  static inline Type* struct_get(Struct* structure, int i);
  static inline void struct_set(Struct* structure, int i, Type* type);
  template<class V>
  static inline i::Handle<V> struct_get_value(Struct* structure, int i);
  template<class V> static inline void struct_set_value(
      Struct* structure, int i, i::Handle<V> x);
};

typedef TypeImpl<ZoneTypeConfig> Type;


// -----------------------------------------------------------------------------
// Heap-allocated types; either smis for bitsets, maps for classes, boxes for
// constants, or fixed arrays for unions.

struct HeapTypeConfig {
  typedef TypeImpl<HeapTypeConfig> Type;
  typedef i::Object Base;
  typedef i::FixedArray Struct;
  typedef i::Isolate Region;
  template<class T> struct Handle { typedef i::Handle<T> type; };

  template<class T> static inline i::Handle<T> null_handle();
  template<class T> static inline i::Handle<T> handle(T* type);
  template<class T> static inline i::Handle<T> cast(i::Handle<Type> type);

  static inline bool is_bitset(Type* type);
  static inline bool is_class(Type* type);
  static inline bool is_struct(Type* type, int tag);

  static inline Type::bitset as_bitset(Type* type);
  static inline i::Handle<i::Map> as_class(Type* type);
  static inline i::Handle<Struct> as_struct(Type* type);

  static inline Type* from_bitset(Type::bitset);
  static inline i::Handle<Type> from_bitset(Type::bitset, Isolate* isolate);
  static inline i::Handle<Type> from_class(
      i::Handle<i::Map> map, Isolate* isolate);
  static inline i::Handle<Type> from_struct(i::Handle<Struct> structure);

  static inline i::Handle<Struct> struct_create(
      int tag, int length, Isolate* isolate);
  static inline void struct_shrink(i::Handle<Struct> structure, int length);
  static inline int struct_tag(i::Handle<Struct> structure);
  static inline int struct_length(i::Handle<Struct> structure);
  static inline i::Handle<Type> struct_get(i::Handle<Struct> structure, int i);
  static inline void struct_set(
      i::Handle<Struct> structure, int i, i::Handle<Type> type);
  template<class V>
  static inline i::Handle<V> struct_get_value(
      i::Handle<Struct> structure, int i);
  template<class V>
  static inline void struct_set_value(
      i::Handle<Struct> structure, int i, i::Handle<V> x);
};

typedef TypeImpl<HeapTypeConfig> HeapType;


// -----------------------------------------------------------------------------
// Type bounds. A simple struct to represent a pair of lower/upper types.

template<class Config>
struct BoundsImpl {
  typedef TypeImpl<Config> Type;
  typedef typename Type::TypeHandle TypeHandle;
  typedef typename Type::Region Region;

  TypeHandle lower;
  TypeHandle upper;

  BoundsImpl() :  // Make sure accessing uninitialized bounds crashes big-time.
    lower(Config::template null_handle<Type>()),
    upper(Config::template null_handle<Type>()) {}
  explicit BoundsImpl(TypeHandle t) : lower(t), upper(t) {}
  BoundsImpl(TypeHandle l, TypeHandle u) : lower(l), upper(u) {
    DCHECK(lower->Is(upper));
  }

  // Unrestricted bounds.
  static BoundsImpl Unbounded(Region* region) {
    return BoundsImpl(Type::None(region), Type::Any(region));
  }

  // Meet: both b1 and b2 are known to hold.
  static BoundsImpl Both(BoundsImpl b1, BoundsImpl b2, Region* region) {
    TypeHandle lower = Type::Union(b1.lower, b2.lower, region);
    TypeHandle upper = Type::Intersect(b1.upper, b2.upper, region);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(upper)) lower = upper;
    return BoundsImpl(lower, upper);
  }

  // Join: either b1 or b2 is known to hold.
  static BoundsImpl Either(BoundsImpl b1, BoundsImpl b2, Region* region) {
    TypeHandle lower = Type::Intersect(b1.lower, b2.lower, region);
    TypeHandle upper = Type::Union(b1.upper, b2.upper, region);
    return BoundsImpl(lower, upper);
  }

  static BoundsImpl NarrowLower(BoundsImpl b, TypeHandle t, Region* region) {
    TypeHandle lower = Type::Union(b.lower, t, region);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(b.upper)) lower = b.upper;
    return BoundsImpl(lower, b.upper);
  }
  static BoundsImpl NarrowUpper(BoundsImpl b, TypeHandle t, Region* region) {
    TypeHandle lower = b.lower;
    TypeHandle upper = Type::Intersect(b.upper, t, region);
    // Lower bounds are considered approximate, correct as necessary.
    if (!lower->Is(upper)) lower = upper;
    return BoundsImpl(lower, upper);
  }

  bool Narrows(BoundsImpl that) {
    return that.lower->Is(this->lower) && this->upper->Is(that.upper);
  }
};

typedef BoundsImpl<ZoneTypeConfig> Bounds;

} }  // namespace v8::internal

#endif  // V8_TYPES_H_
