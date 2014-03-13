// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_TYPES_H_
#define V8_TYPES_H_

#include "v8.h"

#include "objects.h"

namespace v8 {
namespace internal {


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
//   Oddball = Boolean \/ Null \/ Undefined
//   Number = Signed32 \/ Unsigned32 \/ Double
//   Smi <= Signed32
//   Name = String \/ Symbol
//   UniqueName = InternalizedString \/ Symbol
//   InternalizedString < String
//
//   Allocated = Receiver \/ Number \/ Name
//   Detectable = Allocated - Undetectable
//   Undetectable < Object
//   Receiver = Object \/ Proxy
//   Array < Object
//   Function < Object
//   RegExp < Object
//
//   Class(map) < T   iff instance_type(map) < T
//   Constant(x) < T  iff instance_type(map(x)) < T
//
// Note that Constant(x) < Class(map(x)) does _not_ hold, since x's map can
// change! (Its instance type cannot, however.)
// TODO(rossberg): the latter is not currently true for proxies, because of fix,
// but will hold once we implement direct proxies.
//
// There are two main functions for testing types:
//
//   T1->Is(T2)     -- tests whether T1 is included in T2 (i.e., T1 <= T2)
//   T1->Maybe(T2)  -- tests whether T1 and T2 overlap (i.e., T1 /\ T2 =/= 0)
//
// Typically, the former is to be used to select representations (e.g., via
// T->Is(Integer31())), and the to check whether a specific case needs handling
// (e.g., via T->Maybe(Number())).
//
// There is no functionality to discover whether a type is a leaf in the
// lattice. That is intentional. It should always be possible to refine the
// lattice (e.g., splitting up number types further) without invalidating any
// existing assumptions or tests.
//
// Consequently, do not use pointer equality for type tests, always use Is!
//
// Internally, all 'primitive' types, and their unions, are represented as
// bitsets via smis. Class is a heap pointer to the respective map. Only
// Constant's, or unions containing Class'es or Constant's, require allocation.
// Note that the bitset representation is closed under both Union and Intersect.
//
// The type representation is heap-allocated, so cannot (currently) be used in
// a concurrent compilation context.


#define BITSET_TYPE_LIST(V)              \
  V(None,                0)              \
  V(Null,                1 << 0)         \
  V(Undefined,           1 << 1)         \
  V(Boolean,             1 << 2)         \
  V(Smi,                 1 << 3)         \
  V(OtherSigned32,       1 << 4)         \
  V(Unsigned32,          1 << 5)         \
  V(Double,              1 << 6)         \
  V(Symbol,              1 << 7)         \
  V(InternalizedString,  1 << 8)         \
  V(OtherString,         1 << 9)         \
  V(Undetectable,        1 << 10)        \
  V(Array,               1 << 11)        \
  V(Function,            1 << 12)        \
  V(RegExp,              1 << 13)        \
  V(OtherObject,         1 << 14)        \
  V(Proxy,               1 << 15)        \
  V(Internal,            1 << 16)        \
  \
  V(Oddball,         kBoolean | kNull | kUndefined)                 \
  V(Signed32,        kSmi | kOtherSigned32)                         \
  V(Number,          kSigned32 | kUnsigned32 | kDouble)             \
  V(String,          kInternalizedString | kOtherString)            \
  V(UniqueName,      kSymbol | kInternalizedString)                 \
  V(Name,            kSymbol | kString)                             \
  V(NumberOrString,  kNumber | kString)                             \
  V(Object,          kUndetectable | kArray | kFunction |           \
                     kRegExp | kOtherObject)                        \
  V(Receiver,        kObject | kProxy)                              \
  V(Allocated,       kDouble | kName | kReceiver)                   \
  V(Any,             kOddball | kNumber | kAllocated | kInternal)   \
  V(NonNumber,       kAny - kNumber)                                \
  V(Detectable,      kAllocated - kUndetectable)


// struct Config {
//   typedef Base;
//   typedef Unioned;
//   typedef Region;
//   template<class> struct Handle { typedef type; }  // No template typedefs...
//   static Handle<Type>::type handle(Type* type);    // !is_bitset(type)
//   static bool is_bitset(Type*);
//   static bool is_class(Type*);
//   static bool is_constant(Type*);
//   static bool is_union(Type*);
//   static int as_bitset(Type*);
//   static i::Handle<i::Map> as_class(Type*);
//   static i::Handle<i::Object> as_constant(Type*);
//   static Handle<Unioned>::type as_union(Type*);
//   static Type* from_bitset(int bitset);
//   static Handle<Type>::type from_bitset(int bitset, Region*);
//   static Handle<Type>::type from_class(i::Handle<i::Map>, Region*)
//   static Handle<Type>::type from_constant(i::Handle<i::Object>, Region*);
//   static Handle<Type>::type from_union(Handle<Unioned>::type);
//   static Handle<Unioned>::type union_create(int size, Region*);
//   static void union_shrink(Handle<Unioned>::type, int size);
//   static Handle<Type>::type union_get(Handle<Unioned>::type, int);
//   static void union_set(Handle<Unioned>::type, int, Handle<Type>::type);
//   static int union_length(Handle<Unioned>::type);
// }
template<class Config>
class TypeImpl : public Config::Base {
 public:
  typedef typename Config::template Handle<TypeImpl>::type TypeHandle;
  typedef typename Config::Region Region;

  #define DEFINE_TYPE_CONSTRUCTOR(type, value)                        \
    static TypeImpl* type() { return Config::from_bitset(k##type); }  \
    static TypeHandle type(Region* region) {                          \
      return Config::from_bitset(k##type, region);                    \
    }
  BITSET_TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
  #undef DEFINE_TYPE_CONSTRUCTOR

  static TypeHandle Class(i::Handle<i::Map> map, Region* region) {
    return Config::from_class(map, region);
  }
  static TypeHandle Constant(i::Handle<i::Object> value, Region* region) {
    return Config::from_constant(value, region);
  }

  static TypeHandle Union(TypeHandle type1, TypeHandle type2, Region* reg);
  static TypeHandle Intersect(TypeHandle type1, TypeHandle type2, Region* reg);

  static TypeHandle Of(i::Handle<i::Object> value, Region* region) {
    return Config::from_bitset(LubBitset(*value), region);
  }

  bool Is(TypeImpl* that) { return this == that || this->SlowIs(that); }
  template<class TypeHandle>
  bool Is(TypeHandle that) { return this->Is(*that); }
  bool Maybe(TypeImpl* that);
  template<class TypeHandle>
  bool Maybe(TypeHandle that) { return this->Maybe(*that); }

  // State-dependent versions of Of and Is that consider subtyping between
  // a constant and its map class.
  static TypeHandle OfCurrently(i::Handle<i::Object> value, Region* region);
  bool IsCurrently(TypeImpl* that);
  template<class TypeHandle>
  bool IsCurrently(TypeHandle that)  { return this->IsCurrently(*that); }

  bool IsClass() { return Config::is_class(this); }
  bool IsConstant() { return Config::is_constant(this); }
  i::Handle<i::Map> AsClass() { return Config::as_class(this); }
  i::Handle<i::Object> AsConstant() { return Config::as_constant(this); }

  int NumClasses();
  int NumConstants();

  template<class T>
  class Iterator {
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

  Iterator<i::Map> Classes() {
    if (this->IsBitset()) return Iterator<i::Map>();
    return Iterator<i::Map>(Config::handle(this));
  }
  Iterator<i::Object> Constants() {
    if (this->IsBitset()) return Iterator<i::Object>();
    return Iterator<i::Object>(Config::handle(this));
  }

  static TypeImpl* cast(typename Config::Base* object) {
    TypeImpl* t = static_cast<TypeImpl*>(object);
    ASSERT(t->IsBitset() || t->IsClass() || t->IsConstant() || t->IsUnion());
    return t;
  }

  template<class OtherTypeImpl>
  static TypeHandle Convert(
      typename OtherTypeImpl::TypeHandle type, Region* region);

#ifdef OBJECT_PRINT
  void TypePrint();
  void TypePrint(FILE* out);
#endif

 private:
  template<class> friend class Iterator;
  template<class> friend class TypeImpl;

  // A union is a fixed array containing types. Invariants:
  // - its length is at least 2
  // - at most one field is a bitset, and it must go into index 0
  // - no field is a union
  typedef typename Config::Unioned Unioned;
  typedef typename Config::template Handle<Unioned>::type UnionedHandle;

  enum {
    #define DECLARE_TYPE(type, value) k##type = (value),
    BITSET_TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE
    kUnusedEOL = 0
  };

  bool IsNone() { return this == None(); }
  bool IsAny() { return this == Any(); }
  bool IsBitset() { return Config::is_bitset(this); }
  bool IsUnion() { return Config::is_union(this); }
  int AsBitset() { return Config::as_bitset(this); }
  UnionedHandle AsUnion() { return Config::as_union(this); }

  static int UnionLength(UnionedHandle unioned) {
    return Config::union_length(unioned);
  }
  static TypeHandle UnionGet(UnionedHandle unioned, int i) {
    return Config::union_get(unioned, i);
  }

  bool SlowIs(TypeImpl* that);

  int LubBitset();  // least upper bound that's a bitset
  int GlbBitset();  // greatest lower bound that's a bitset

  static int LubBitset(i::Object* value);
  static int LubBitset(i::Map* map);

  bool InUnion(UnionedHandle unioned, int current_size);
  static int ExtendUnion(
      UnionedHandle unioned, TypeHandle t, int current_size);
  static int ExtendIntersection(
      UnionedHandle unioned, TypeHandle t, TypeHandle other, int current_size);

#ifdef OBJECT_PRINT
  static const char* bitset_name(int bitset);
#endif
};


// Zone-allocated types are either (odd) integers to represent bitsets, or
// (even) pointers to zone lists for everything else. The first slot of every
// list is an explicit tag value to distinguish representation.
struct ZoneTypeConfig {
 private:
  typedef i::ZoneList<void*> Tagged;

  enum Tag {
    kClassTag,
    kConstantTag,
    kUnionTag
  };

  static Tagged* tagged_create(Tag tag, int size, Zone* zone) {
    Tagged* tagged = new(zone) Tagged(size + 1, zone);
    tagged->Add(reinterpret_cast<void*>(tag), zone);
    tagged->AddBlock(NULL, size, zone);
    return tagged;
  }
  static void tagged_shrink(Tagged* tagged, int size) {
    tagged->Rewind(size + 1);
  }
  static Tag tagged_tag(Tagged* tagged) {
    return static_cast<Tag>(reinterpret_cast<intptr_t>(tagged->at(0)));
  }
  template<class T>
  static T tagged_get(Tagged* tagged, int i) {
    return reinterpret_cast<T>(tagged->at(i + 1));
  }
  template<class T>
  static void tagged_set(Tagged* tagged, int i, T value) {
    tagged->at(i + 1) = reinterpret_cast<T>(value);
  }
  static int tagged_length(Tagged* tagged) {
    return tagged->length() - 1;
  }

 public:
  typedef TypeImpl<ZoneTypeConfig> Type;
  class Base {};
  typedef i::ZoneList<Type*> Unioned;
  typedef i::Zone Region;
  template<class T> struct Handle { typedef T* type; };

  static Type* handle(Type* type) { return type; }

  static bool is(Type* type, Tag tag) {
    return is_tagged(type) && tagged_tag(as_tagged(type)) == tag;
  }

  static bool is_bitset(Type* type) {
    return reinterpret_cast<intptr_t>(type) & 1;
  }
  static bool is_tagged(Type* type) { return !is_bitset(type); }
  static bool is_class(Type* type) { return is(type, kClassTag); }
  static bool is_constant(Type* type) { return is(type, kConstantTag); }
  static bool is_union(Type* type) { return is(type, kUnionTag); }
  static bool tagged_is_union(Tagged* tagged) {
    return is(from_tagged(tagged), kUnionTag);
  }

  static int as_bitset(Type* type) {
    ASSERT(is_bitset(type));
    return static_cast<int>(reinterpret_cast<intptr_t>(type) >> 1);
  }
  static Tagged* as_tagged(Type* type) {
    ASSERT(is_tagged(type));
    return reinterpret_cast<Tagged*>(type);
  }
  static i::Handle<i::Map> as_class(Type* type) {
    ASSERT(is_class(type));
    return i::Handle<i::Map>(tagged_get<i::Map**>(as_tagged(type), 0));
  }
  static i::Handle<i::Object> as_constant(Type* type) {
    ASSERT(is_constant(type));
    return i::Handle<i::Object>(tagged_get<i::Object**>(as_tagged(type), 0));
  }
  static Unioned* as_union(Type* type) {
    ASSERT(is_union(type));
    return tagged_as_union(as_tagged(type));
  }
  static Unioned* tagged_as_union(Tagged* tagged) {
    ASSERT(tagged_is_union(tagged));
    return reinterpret_cast<Unioned*>(tagged);
  }

  static Type* from_bitset(int bitset) {
    return reinterpret_cast<Type*>((bitset << 1) | 1);
  }
  static Type* from_bitset(int bitset, Zone* Zone) {
    return from_bitset(bitset);
  }
  static Type* from_tagged(Tagged* tagged) {
    return reinterpret_cast<Type*>(tagged);
  }
  static Type* from_class(i::Handle<i::Map> map, Zone* zone) {
    Tagged* tagged = tagged_create(kClassTag, 1, zone);
    tagged_set(tagged, 0, map.location());
    return from_tagged(tagged);
  }
  static Type* from_constant(i::Handle<i::Object> value, Zone* zone) {
    Tagged* tagged = tagged_create(kConstantTag, 1, zone);
    tagged_set(tagged, 0, value.location());
    return from_tagged(tagged);
  }
  static Type* from_union(Unioned* unioned) {
    return from_tagged(tagged_from_union(unioned));
  }
  static Tagged* tagged_from_union(Unioned* unioned) {
    return reinterpret_cast<Tagged*>(unioned);
  }

  static Unioned* union_create(int size, Zone* zone) {
    return tagged_as_union(tagged_create(kUnionTag, size, zone));
  }
  static void union_shrink(Unioned* unioned, int size) {
    tagged_shrink(tagged_from_union(unioned), size);
  }
  static Type* union_get(Unioned* unioned, int i) {
    Type* type = tagged_get<Type*>(tagged_from_union(unioned), i);
    ASSERT(!is_union(type));
    return type;
  }
  static void union_set(Unioned* unioned, int i, Type* type) {
    ASSERT(!is_union(type));
    tagged_set(tagged_from_union(unioned), i, type);
  }
  static int union_length(Unioned* unioned) {
    return tagged_length(tagged_from_union(unioned));
  }
};


// Heap-allocated types are either smis for bitsets, maps for classes, boxes for
// constants, or fixed arrays for unions.
struct HeapTypeConfig {
  typedef TypeImpl<HeapTypeConfig> Type;
  typedef i::Object Base;
  typedef i::FixedArray Unioned;
  typedef i::Isolate Region;
  template<class T> struct Handle { typedef i::Handle<T> type; };

  static i::Handle<Type> handle(Type* type) {
    return i::handle(type, i::HeapObject::cast(type)->GetIsolate());
  }

  static bool is_bitset(Type* type) { return type->IsSmi(); }
  static bool is_class(Type* type) { return type->IsMap(); }
  static bool is_constant(Type* type) { return type->IsBox(); }
  static bool is_union(Type* type) { return type->IsFixedArray(); }

  static int as_bitset(Type* type) {
    return Smi::cast(type)->value();
  }
  static i::Handle<i::Map> as_class(Type* type) {
    return i::handle(i::Map::cast(type));
  }
  static i::Handle<i::Object> as_constant(Type* type) {
    i::Box* box = i::Box::cast(type);
    return i::handle(box->value(), box->GetIsolate());
  }
  static i::Handle<Unioned> as_union(Type* type) {
    return i::handle(i::FixedArray::cast(type));
  }

  static Type* from_bitset(int bitset) {
    return Type::cast(i::Smi::FromInt(bitset));
  }
  static i::Handle<Type> from_bitset(int bitset, Isolate* isolate) {
    return i::handle(from_bitset(bitset), isolate);
  }
  static i::Handle<Type> from_class(i::Handle<i::Map> map, Isolate* isolate) {
    return i::Handle<Type>::cast(i::Handle<Object>::cast(map));
  }
  static i::Handle<Type> from_constant(
      i::Handle<i::Object> value, Isolate* isolate) {
    i::Handle<Box> box = isolate->factory()->NewBox(value);
    return i::Handle<Type>::cast(i::Handle<Object>::cast(box));
  }
  static i::Handle<Type> from_union(i::Handle<Unioned> unioned) {
    return i::Handle<Type>::cast(i::Handle<Object>::cast(unioned));
  }

  static i::Handle<Unioned> union_create(int size, Isolate* isolate) {
    return isolate->factory()->NewFixedArray(size);
  }
  static void union_shrink(i::Handle<Unioned> unioned, int size) {
    unioned->Shrink(size);
  }
  static i::Handle<Type> union_get(i::Handle<Unioned> unioned, int i) {
    Type* type = static_cast<Type*>(unioned->get(i));
    ASSERT(!is_union(type));
    return i::handle(type, unioned->GetIsolate());
  }
  static void union_set(
      i::Handle<Unioned> unioned, int i, i::Handle<Type> type) {
    ASSERT(!is_union(*type));
    unioned->set(i, *type);
  }
  static int union_length(i::Handle<Unioned> unioned) {
    return unioned->length();
  }
};

typedef TypeImpl<ZoneTypeConfig> Type;
typedef TypeImpl<HeapTypeConfig> HeapType;


// A simple struct to represent a pair of lower/upper type bounds.
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
    ASSERT(lower->Is(upper));
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
};

typedef BoundsImpl<ZoneTypeConfig> Bounds;


} }  // namespace v8::internal

#endif  // V8_TYPES_H_
