// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPES_H_
#define V8_TYPES_H_

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
// PROPERTIES
//
// Various formal properties hold for constructors, operators, and predicates
// over types. For example, constructors are injective, subtyping is a complete
// partial order, union and intersection satisfy the usual algebraic properties.
//
// See test/cctest/test-types.cc for a comprehensive executable specification,
// especially with respect to the properties of the more exotic 'temporal'
// constructors and predicates (those prefixed 'Now').
//
// IMPLEMENTATION
//
// Internally, all 'primitive' types, and their unions, are represented as
// bitsets. Class is a heap pointer to the respective map. Only Constant's, or
// unions containing Class'es or Constant's, currently require allocation.
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
  V(Representation, static_cast<int>(0xffc00000)) \
  V(Semantic,       static_cast<int>(0x003fffff))

#define REPRESENTATION(k) ((k) & BitsetType::kRepresentation)
#define SEMANTIC(k)       ((k) & BitsetType::kSemantic)

#define REPRESENTATION_BITSET_TYPE_LIST(V) \
  V(None,             0)                   \
  V(UntaggedInt1,     1 << 22 | kSemantic) \
  V(UntaggedInt8,     1 << 23 | kSemantic) \
  V(UntaggedInt16,    1 << 24 | kSemantic) \
  V(UntaggedInt32,    1 << 25 | kSemantic) \
  V(UntaggedFloat32,  1 << 26 | kSemantic) \
  V(UntaggedFloat64,  1 << 27 | kSemantic) \
  V(UntaggedPtr,      1 << 28 | kSemantic) \
  V(TaggedInt,        1 << 29 | kSemantic) \
  /* MSB has to be sign-extended */        \
  V(TaggedPtr,        static_cast<int>(~0u << 30) | kSemantic) \
  \
  V(UntaggedInt,      kUntaggedInt1 | kUntaggedInt8 |      \
                      kUntaggedInt16 | kUntaggedInt32)     \
  V(UntaggedFloat,    kUntaggedFloat32 | kUntaggedFloat64) \
  V(UntaggedNumber,   kUntaggedInt | kUntaggedFloat)       \
  V(Untagged,         kUntaggedNumber | kUntaggedPtr)      \
  V(Tagged,           kTaggedInt | kTaggedPtr)

#define SEMANTIC_BITSET_TYPE_LIST(V) \
  V(Null,                1 << 0  | REPRESENTATION(kTaggedPtr)) \
  V(Undefined,           1 << 1  | REPRESENTATION(kTaggedPtr)) \
  V(Boolean,             1 << 2  | REPRESENTATION(kTaggedPtr)) \
  V(UnsignedSmall,       1 << 3  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherSignedSmall,    1 << 4  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherUnsigned31,     1 << 5  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherUnsigned32,     1 << 6  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherSigned32,       1 << 7  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(MinusZero,           1 << 8  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(NaN,                 1 << 9  | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(OtherNumber,         1 << 10 | REPRESENTATION(kTagged | kUntaggedNumber)) \
  V(Symbol,              1 << 11 | REPRESENTATION(kTaggedPtr)) \
  V(InternalizedString,  1 << 12 | REPRESENTATION(kTaggedPtr)) \
  V(OtherString,         1 << 13 | REPRESENTATION(kTaggedPtr)) \
  V(Undetectable,        1 << 14 | REPRESENTATION(kTaggedPtr)) \
  V(Array,               1 << 15 | REPRESENTATION(kTaggedPtr)) \
  V(Buffer,              1 << 16 | REPRESENTATION(kTaggedPtr)) \
  V(Function,            1 << 17 | REPRESENTATION(kTaggedPtr)) \
  V(RegExp,              1 << 18 | REPRESENTATION(kTaggedPtr)) \
  V(OtherObject,         1 << 19 | REPRESENTATION(kTaggedPtr)) \
  V(Proxy,               1 << 20 | REPRESENTATION(kTaggedPtr)) \
  V(Internal,            1 << 21 | REPRESENTATION(kTagged | kUntagged)) \
  \
  V(SignedSmall,         kUnsignedSmall | kOtherSignedSmall) \
  V(Signed32,            kSignedSmall | kOtherUnsigned31 | kOtherSigned32) \
  V(Unsigned32,          kUnsignedSmall | kOtherUnsigned31 | kOtherUnsigned32) \
  V(Integral32,          kSigned32 | kUnsigned32) \
  V(Number,              kIntegral32 | kMinusZero | kNaN | kOtherNumber) \
  V(String,              kInternalizedString | kOtherString) \
  V(UniqueName,          kSymbol | kInternalizedString) \
  V(Name,                kSymbol | kString) \
  V(NumberOrString,      kNumber | kString) \
  V(Primitive,           kNumber | kName | kBoolean | kNull | kUndefined) \
  V(DetectableObject,    kArray | kFunction | kRegExp | kOtherObject) \
  V(DetectableReceiver,  kDetectableObject | kProxy) \
  V(Detectable,          kDetectableReceiver | kNumber | kName) \
  V(Object,              kDetectableObject | kUndetectable) \
  V(Receiver,            kObject | kProxy) \
  V(NonNumber,           kBoolean | kName | kNull | kReceiver | \
                         kUndefined | kInternal) \
  V(Any,                 -1)

#define BITSET_TYPE_LIST(V) \
  MASK_BITSET_TYPE_LIST(V) \
  REPRESENTATION_BITSET_TYPE_LIST(V) \
  SEMANTIC_BITSET_TYPE_LIST(V)


// -----------------------------------------------------------------------------
// The abstract Type class, parameterized over the low-level representation.

// struct Config {
//   typedef TypeImpl<Config> Type;
//   typedef Base;
//   typedef Struct;
//   typedef Region;
//   template<class> struct Handle { typedef type; }  // No template typedefs...
//   template<class T> static Handle<T>::type handle(T* t);  // !is_bitset(t)
//   template<class T> static Handle<T>::type cast(Handle<Type>::type);
//   static bool is_bitset(Type*);
//   static bool is_class(Type*);
//   static bool is_struct(Type*, int tag);
//   static int as_bitset(Type*);
//   static i::Handle<i::Map> as_class(Type*);
//   static Handle<Struct>::type as_struct(Type*);
//   static Type* from_bitset(int bitset);
//   static Handle<Type>::type from_bitset(int bitset, Region*);
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

  class BitsetType;      // Internal
  class StructuralType;  // Internal
  class UnionType;       // Internal

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
    static TypeImpl* type() { return BitsetType::New(BitsetType::k##type); }  \
    static TypeHandle type(Region* region) {                                  \
      return BitsetType::New(BitsetType::k##type, region);                    \
    }
  BITSET_TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
  #undef DEFINE_TYPE_CONSTRUCTOR

  static TypeHandle Class(i::Handle<i::Map> map, Region* region) {
    return ClassType::New(map, region);
  }
  static TypeHandle Constant(i::Handle<i::Object> value, Region* region) {
    // TODO(neis): Return RangeType for numerical values.
    return ConstantType::New(value, region);
  }
  static TypeHandle Range(double min, double max, Region* region) {
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

  // Equivalent to Constant(value)->Is(this), but avoiding allocation.
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

  void PrintTo(OStream& os, PrintDimension dim = BOTH_DIMS);  // NOLINT

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

  int AsBitset() {
    DCHECK(this->IsBitset());
    return static_cast<BitsetType*>(this)->Bitset();
  }
  UnionType* AsUnion() { return UnionType::cast(this); }

  // Auxiliary functions.

  int BitsetGlb() { return BitsetType::Glb(this); }
  int BitsetLub() { return BitsetType::Lub(this); }
  int InherentBitsetLub() { return BitsetType::InherentLub(this); }

  bool SlowIs(TypeImpl* that);

  TypeHandle Rebound(int bitset, Region* region);
  int BoundBy(TypeImpl* that);
  int IndexInUnion(int bound, UnionHandle unioned, int current_size);
  static int ExtendUnion(
      UnionHandle unioned, int current_size, TypeHandle t,
      TypeHandle other, bool is_intersect, Region* region);
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

  int Bitset() { return Config::as_bitset(this); }

  static TypeImpl* New(int bitset) {
    return static_cast<BitsetType*>(Config::from_bitset(bitset));
  }
  static TypeHandle New(int bitset, Region* region) {
    return Config::from_bitset(bitset, region);
  }

  static bool IsInhabited(int bitset) {
    return (bitset & kRepresentation) && (bitset & kSemantic);
  }

  static bool Is(int bitset1, int bitset2) {
    return (bitset1 | bitset2) == bitset2;
  }

  static int Glb(TypeImpl* type);  // greatest lower bound that's a bitset
  static int Lub(TypeImpl* type);  // least upper bound that's a bitset
  static int Lub(i::Object* value);
  static int Lub(double value);
  static int Lub(int32_t value);
  static int Lub(uint32_t value);
  static int Lub(i::Map* map);
  static int Lub(double min, double max);
  static int InherentLub(TypeImpl* type);

  static const char* Name(int bitset);
  static void Print(OStream& os, int bitset);  // NOLINT
  using TypeImpl::PrintTo;
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
    return Config::is_class(this)
        ? BitsetType::New(BitsetType::Lub(*Config::as_class(this)), region)
        : this->Get(0);
  }
  i::Handle<i::Map> Map() {
    return Config::is_class(this)
        ? Config::as_class(this)
        : this->template GetValue<i::Map>(1);
  }

  static ClassHandle New(
      i::Handle<i::Map> map, TypeHandle bound, Region* region) {
    DCHECK(BitsetType::Is(bound->AsBitset(), BitsetType::Lub(*map)));
    ClassHandle type = Config::template cast<ClassType>(
        StructuralType::New(StructuralType::kClassTag, 2, region));
    type->Set(0, bound);
    type->SetValue(1, map);
    return type;
  }

  static ClassHandle New(i::Handle<i::Map> map, Region* region) {
    ClassHandle type =
        Config::template cast<ClassType>(Config::from_class(map, region));
    if (type->IsClass()) {
      return type;
    } else {
      TypeHandle bound = BitsetType::New(BitsetType::Lub(*map), region);
      return New(map, bound, region);
    }
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

  static ConstantHandle New(
      i::Handle<i::Object> value, TypeHandle bound, Region* region) {
    DCHECK(BitsetType::Is(bound->AsBitset(), BitsetType::Lub(*value)));
    ConstantHandle type = Config::template cast<ConstantType>(
        StructuralType::New(StructuralType::kConstantTag, 2, region));
    type->Set(0, bound);
    type->SetValue(1, value);
    return type;
  }

  static ConstantHandle New(i::Handle<i::Object> value, Region* region) {
    TypeHandle bound = BitsetType::New(BitsetType::Lub(*value), region);
    return New(value, bound, region);
  }

  static ConstantType* cast(TypeImpl* type) {
    DCHECK(type->IsConstant());
    return static_cast<ConstantType*>(type);
  }
};


// -----------------------------------------------------------------------------
// Range types.

template<class Config>
class TypeImpl<Config>::RangeType : public StructuralType {
 public:
  TypeHandle Bound() { return this->Get(0); }
  double Min() { return this->template GetValue<i::HeapNumber>(1)->value(); }
  double Max() { return this->template GetValue<i::HeapNumber>(2)->value(); }

  static RangeHandle New(
      double min, double max, TypeHandle bound, Region* region) {
    DCHECK(BitsetType::Is(bound->AsBitset(), BitsetType::Lub(min, max)));
    RangeHandle type = Config::template cast<RangeType>(
        StructuralType::New(StructuralType::kRangeTag, 3, region));
    type->Set(0, bound);
    Factory* factory = Config::isolate(region)->factory();
    Handle<HeapNumber> minV = factory->NewHeapNumber(min);
    Handle<HeapNumber> maxV = factory->NewHeapNumber(max);
    type->SetValue(1, minV);
    type->SetValue(2, maxV);
    return type;
  }

  static RangeHandle New(double min, double max, Region* region) {
    TypeHandle bound = BitsetType::New(BitsetType::Lub(min, max), region);
    return New(min, max, bound, region);
  }

  static RangeType* cast(TypeImpl* type) {
    DCHECK(type->IsRange());
    return static_cast<RangeType*>(type);
  }
};


// -----------------------------------------------------------------------------
// Context types.

template<class Config>
class TypeImpl<Config>::ContextType : public StructuralType {
 public:
  TypeHandle Bound() { return this->Get(0); }
  TypeHandle Outer() { return this->Get(1); }

  static ContextHandle New(TypeHandle outer, TypeHandle bound, Region* region) {
    DCHECK(BitsetType::Is(
        bound->AsBitset(), BitsetType::kInternal & BitsetType::kTaggedPtr));
    ContextHandle type = Config::template cast<ContextType>(
        StructuralType::New(StructuralType::kContextTag, 2, region));
    type->Set(0, bound);
    type->Set(1, outer);
    return type;
  }

  static ContextHandle New(TypeHandle outer, Region* region) {
    TypeHandle bound = BitsetType::New(
        BitsetType::kInternal & BitsetType::kTaggedPtr, region);
    return New(outer, bound, region);
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
  TypeHandle Bound() { return this->Get(0); }
  TypeHandle Element() { return this->Get(1); }

  static ArrayHandle New(TypeHandle element, TypeHandle bound, Region* region) {
    DCHECK(BitsetType::Is(bound->AsBitset(), BitsetType::kArray));
    ArrayHandle type = Config::template cast<ArrayType>(
        StructuralType::New(StructuralType::kArrayTag, 2, region));
    type->Set(0, bound);
    type->Set(1, element);
    return type;
  }

  static ArrayHandle New(TypeHandle element, Region* region) {
    TypeHandle bound = BitsetType::New(BitsetType::kArray, region);
    return New(element, bound, region);
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
  int Arity() { return this->Length() - 3; }
  TypeHandle Bound() { return this->Get(0); }
  TypeHandle Result() { return this->Get(1); }
  TypeHandle Receiver() { return this->Get(2); }
  TypeHandle Parameter(int i) { return this->Get(3 + i); }

  void InitParameter(int i, TypeHandle type) { this->Set(3 + i, type); }

  static FunctionHandle New(
      TypeHandle result, TypeHandle receiver, TypeHandle bound,
      int arity, Region* region) {
    DCHECK(BitsetType::Is(bound->AsBitset(), BitsetType::kFunction));
    FunctionHandle type = Config::template cast<FunctionType>(
        StructuralType::New(StructuralType::kFunctionTag, 3 + arity, region));
    type->Set(0, bound);
    type->Set(1, result);
    type->Set(2, receiver);
    return type;
  }

  static FunctionHandle New(
      TypeHandle result, TypeHandle receiver, int arity, Region* region) {
    TypeHandle bound = BitsetType::New(BitsetType::kFunction, region);
    return New(result, receiver, bound, arity, region);
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

  // TODO(neis): This will be removed again once we have struct_get_double().
  static inline i::Isolate* isolate(Region* region) {
    return region->isolate();
  }

  template<class T> static inline T* handle(T* type);
  template<class T> static inline T* cast(Type* type);

  static inline bool is_bitset(Type* type);
  static inline bool is_class(Type* type);
  static inline bool is_struct(Type* type, int tag);

  static inline int as_bitset(Type* type);
  static inline i::Handle<i::Map> as_class(Type* type);
  static inline Struct* as_struct(Type* type);

  static inline Type* from_bitset(int bitset);
  static inline Type* from_bitset(int bitset, Zone* zone);
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

  // TODO(neis): This will be removed again once we have struct_get_double().
  static inline i::Isolate* isolate(Region* region) {
    return region;
  }

  template<class T> static inline i::Handle<T> handle(T* type);
  template<class T> static inline i::Handle<T> cast(i::Handle<Type> type);

  static inline bool is_bitset(Type* type);
  static inline bool is_class(Type* type);
  static inline bool is_struct(Type* type, int tag);

  static inline int as_bitset(Type* type);
  static inline i::Handle<i::Map> as_class(Type* type);
  static inline i::Handle<Struct> as_struct(Type* type);

  static inline Type* from_bitset(int bitset);
  static inline i::Handle<Type> from_bitset(int bitset, Isolate* isolate);
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

  BoundsImpl() {}
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
    lower = Type::Intersect(lower, upper, region);
    return BoundsImpl(lower, upper);
  }

  // Join: either b1 or b2 is known to hold.
  static BoundsImpl Either(BoundsImpl b1, BoundsImpl b2, Region* region) {
    TypeHandle lower = Type::Intersect(b1.lower, b2.lower, region);
    TypeHandle upper = Type::Union(b1.upper, b2.upper, region);
    return BoundsImpl(lower, upper);
  }

  static BoundsImpl NarrowLower(BoundsImpl b, TypeHandle t, Region* region) {
    // Lower bounds are considered approximate, correct as necessary.
    t = Type::Intersect(t, b.upper, region);
    TypeHandle lower = Type::Union(b.lower, t, region);
    return BoundsImpl(lower, b.upper);
  }
  static BoundsImpl NarrowUpper(BoundsImpl b, TypeHandle t, Region* region) {
    TypeHandle lower = Type::Intersect(b.lower, t, region);
    TypeHandle upper = Type::Intersect(b.upper, t, region);
    return BoundsImpl(lower, upper);
  }

  bool Narrows(BoundsImpl that) {
    return that.lower->Is(this->lower) && this->upper->Is(that.upper);
  }
};

typedef BoundsImpl<ZoneTypeConfig> Bounds;

} }  // namespace v8::internal

#endif  // V8_TYPES_H_
