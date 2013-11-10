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


#define PRIMITIVE_TYPE_LIST(V)           \
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
  V(Internal,            1 << 16)

#define COMPOSED_TYPE_LIST(V)                                       \
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

#define TYPE_LIST(V)     \
  PRIMITIVE_TYPE_LIST(V) \
  COMPOSED_TYPE_LIST(V)



class Type : public Object {
 public:
  #define DEFINE_TYPE_CONSTRUCTOR(type, value)           \
    static Type* type() { return from_bitset(k##type); }
  TYPE_LIST(DEFINE_TYPE_CONSTRUCTOR)
  #undef DEFINE_TYPE_CONSTRUCTOR

  static Type* Class(Handle<Map> map) { return from_handle(map); }
  static Type* Constant(Handle<HeapObject> value) {
    return Constant(value, value->GetIsolate());
  }
  static Type* Constant(Handle<v8::internal::Object> value, Isolate* isolate) {
    return from_handle(isolate->factory()->NewBox(value));
  }

  static Type* Union(Handle<Type> type1, Handle<Type> type2);
  static Type* Intersect(Handle<Type> type1, Handle<Type> type2);
  static Type* Optional(Handle<Type> type);  // type \/ Undefined

  bool Is(Type* that) { return (this == that) ? true : SlowIs(that); }
  bool Is(Handle<Type> that) { return this->Is(*that); }
  bool Maybe(Type* that);
  bool Maybe(Handle<Type> that) { return this->Maybe(*that); }

  bool IsClass() { return is_class(); }
  bool IsConstant() { return is_constant(); }
  Handle<Map> AsClass() { return as_class(); }
  Handle<v8::internal::Object> AsConstant() { return as_constant(); }

  int NumClasses();
  int NumConstants();

  template<class T>
  class Iterator {
   public:
    bool Done() const { return index_ < 0; }
    Handle<T> Current();
    void Advance();

   private:
    friend class Type;

    Iterator() : index_(-1) {}
    explicit Iterator(Handle<Type> type) : type_(type), index_(-1) {
      Advance();
    }

    inline bool matches(Handle<Type> type);
    inline Handle<Type> get_type();

    Handle<Type> type_;
    int index_;
  };

  Iterator<Map> Classes() {
    if (this->is_bitset()) return Iterator<Map>();
    return Iterator<Map>(this->handle());
  }
  Iterator<v8::internal::Object> Constants() {
    if (this->is_bitset()) return Iterator<v8::internal::Object>();
    return Iterator<v8::internal::Object>(this->handle());
  }

  static Type* cast(v8::internal::Object* object) {
    Type* t = static_cast<Type*>(object);
    ASSERT(t->is_bitset() || t->is_class() ||
           t->is_constant() || t->is_union());
    return t;
  }

#ifdef OBJECT_PRINT
  void TypePrint();
  void TypePrint(FILE* out);
#endif

 private:
  // A union is a fixed array containing types. Invariants:
  // - its length is at least 2
  // - at most one field is a bitset, and it must go into index 0
  // - no field is a union
  typedef FixedArray Unioned;

  enum {
    #define DECLARE_TYPE(type, value) k##type = (value),
    TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE
    kUnusedEOL = 0
  };

  bool is_none() { return this == None(); }
  bool is_bitset() { return this->IsSmi(); }
  bool is_class() { return this->IsMap(); }
  bool is_constant() { return this->IsBox(); }
  bool is_union() { return this->IsFixedArray(); }

  bool SlowIs(Type* that);

  int as_bitset() { return Smi::cast(this)->value(); }
  Handle<Map> as_class() { return Handle<Map>::cast(handle()); }
  Handle<v8::internal::Object> as_constant() {
    Handle<Box> box = Handle<Box>::cast(handle());
    return v8::internal::handle(box->value(), box->GetIsolate());
  }
  Handle<Unioned> as_union() { return Handle<Unioned>::cast(handle()); }

  Handle<Type> handle() { return handle_via_isolate_of(this); }
  Handle<Type> handle_via_isolate_of(Type* type) {
    ASSERT(type->IsHeapObject());
    return v8::internal::handle(this, HeapObject::cast(type)->GetIsolate());
  }

  static Type* from_bitset(int bitset) {
    return static_cast<Type*>(Object::cast(Smi::FromInt(bitset)));
  }
  static Type* from_handle(Handle<HeapObject> handle) {
    return static_cast<Type*>(Object::cast(*handle));
  }

  static Handle<Type> union_get(Handle<Unioned> unioned, int i) {
    Type* type = static_cast<Type*>(unioned->get(i));
    ASSERT(!type->is_union());
    return type->handle_via_isolate_of(from_handle(unioned));
  }

  int LubBitset();  // least upper bound that's a bitset
  int GlbBitset();  // greatest lower bound that's a bitset
  bool InUnion(Handle<Unioned> unioned, int current_size);
  int ExtendUnion(Handle<Unioned> unioned, int current_size);
  int ExtendIntersection(
      Handle<Unioned> unioned, Handle<Type> type, int current_size);

  static const char* GetComposedName(int type) {
    switch (type) {
      #define PRINT_COMPOSED_TYPE(type, value)  \
      case k##type:                             \
        return # type;
      COMPOSED_TYPE_LIST(PRINT_COMPOSED_TYPE)
      #undef PRINT_COMPOSED_TYPE
    }
    return NULL;
  }

  static const char* GetPrimitiveName(int type) {
    switch (type) {
      #define PRINT_PRIMITIVE_TYPE(type, value)  \
      case k##type:                              \
        return # type;
      PRIMITIVE_TYPE_LIST(PRINT_PRIMITIVE_TYPE)
      #undef PRINT_PRIMITIVE_TYPE
      default:
        UNREACHABLE();
        return "InvalidType";
    }
  }
};


// A simple struct to represent a pair of lower/upper type bounds.
struct Bounds {
  Handle<Type> lower;
  Handle<Type> upper;

  Bounds() {}
  Bounds(Handle<Type> l, Handle<Type> u) : lower(l), upper(u) {
    ASSERT(lower->Is(upper));
  }
  Bounds(Type* l, Type* u, Isolate* isl) : lower(l, isl), upper(u, isl) {
    ASSERT(lower->Is(upper));
  }
  explicit Bounds(Handle<Type> t) : lower(t), upper(t) {
    ASSERT(lower->Is(upper));
  }
  Bounds(Type* t, Isolate* isl) : lower(t, isl), upper(t, isl) {
    ASSERT(lower->Is(upper));
  }

  // Unrestricted bounds.
  static Bounds Unbounded(Isolate* isl) {
    return Bounds(Type::None(), Type::Any(), isl);
  }

  // Meet: both b1 and b2 are known to hold.
  static Bounds Both(Bounds b1, Bounds b2, Isolate* isl) {
    Handle<Type> lower(Type::Union(b1.lower, b2.lower), isl);
    Handle<Type> upper(Type::Intersect(b1.upper, b2.upper), isl);
    // Lower bounds are considered approximate, correct as necessary.
    lower = handle(Type::Intersect(lower, upper), isl);
    return Bounds(lower, upper);
  }

  // Join: either b1 or b2 is known to hold.
  static Bounds Either(Bounds b1, Bounds b2, Isolate* isl) {
    return Bounds(
        handle(Type::Intersect(b1.lower, b2.lower), isl),
        handle(Type::Union(b1.upper, b2.upper), isl));
  }

  static Bounds NarrowLower(Bounds b, Handle<Type> t, Isolate* isl) {
    // Lower bounds are considered approximate, correct as necessary.
    t = handle(Type::Intersect(t, b.upper), isl);
    return Bounds(handle(Type::Union(b.lower, t), isl), b.upper);
  }
  static Bounds NarrowUpper(Bounds b, Handle<Type> t, Isolate* isl) {
    return Bounds(
        handle(Type::Intersect(b.lower, t), isl),
        handle(Type::Intersect(b.upper, t), isl));
  }
};

} }  // namespace v8::internal

#endif  // V8_TYPES_H_
