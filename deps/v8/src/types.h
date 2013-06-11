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
//   Number = Smi \/ Double
//   Name = String \/ Symbol
//   UniqueName = InternalizedString \/ Symbol
//   InternalizedString < String
//
//   Receiver = Object \/ Proxy
//   Array < Object
//   Function < Object
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
// Typically, the latter should be used to check whether a specific case needs
// handling (e.g., via T->Maybe(Number)).
//
// There is no functionality to discover whether a type is a leaf in the
// lattice. That is intentional. It should always be possible to refine the
// lattice (e.g., splitting up number types further) without invalidating any
// existing assumptions or tests.
//
// Internally, all 'primitive' types, and their unions, are represented as
// bitsets via smis. Class is a heap pointer to the respective map. Only
// Constant's, or unions containing Class'es or Constant's, require allocation.
//
// The type representation is heap-allocated, so cannot (currently) be used in
// a parallel compilation context.

class Type : public Object {
 public:
  static Type* None() { return from_bitset(kNone); }
  static Type* Any() { return from_bitset(kAny); }

  static Type* Oddball() { return from_bitset(kOddball); }
  static Type* Boolean() { return from_bitset(kBoolean); }
  static Type* Null() { return from_bitset(kNull); }
  static Type* Undefined() { return from_bitset(kUndefined); }

  static Type* Number() { return from_bitset(kNumber); }
  static Type* Smi() { return from_bitset(kSmi); }
  static Type* Double() { return from_bitset(kDouble); }

  static Type* Name() { return from_bitset(kName); }
  static Type* UniqueName() { return from_bitset(kUniqueName); }
  static Type* String() { return from_bitset(kString); }
  static Type* InternalizedString() { return from_bitset(kInternalizedString); }
  static Type* Symbol() { return from_bitset(kSymbol); }

  static Type* Receiver() { return from_bitset(kReceiver); }
  static Type* Object() { return from_bitset(kObject); }
  static Type* Array() { return from_bitset(kArray); }
  static Type* Function() { return from_bitset(kFunction); }
  static Type* Proxy() { return from_bitset(kProxy); }

  static Type* Class(Handle<Map> map) { return from_handle(map); }
  static Type* Constant(Handle<HeapObject> value) {
    return Constant(value, value->GetIsolate());
  }
  static Type* Constant(Handle<v8::internal::Object> value, Isolate* isolate) {
    return from_handle(isolate->factory()->NewBox(value));
  }

  static Type* Union(Handle<Type> type1, Handle<Type> type2);
  static Type* Optional(Handle<Type> type);  // type \/ Undefined

  bool Is(Handle<Type> that);
  bool Maybe(Handle<Type> that);

  // TODO(rossberg): method to iterate unions?

 private:
  // A union is a fixed array containing types. Invariants:
  // - its length is at least 2
  // - at most one field is a bitset, and it must go into index 0
  // - no field is a union
  typedef FixedArray Unioned;

  enum {
    kNull = 1 << 0,
    kUndefined = 1 << 1,
    kBoolean = 1 << 2,
    kSmi = 1 << 3,
    kDouble = 1 << 4,
    kSymbol = 1 << 5,
    kInternalizedString = 1 << 6,
    kOtherString = 1 << 7,
    kArray = 1 << 8,
    kFunction = 1 << 9,
    kOtherObject = 1 << 10,
    kProxy = 1 << 11,

    kOddball = kBoolean | kNull | kUndefined,
    kNumber = kSmi | kDouble,
    kString = kInternalizedString | kOtherString,
    kUniqueName = kSymbol | kInternalizedString,
    kName = kSymbol | kString,
    kObject = kArray | kFunction | kOtherObject,
    kReceiver = kObject | kProxy,
    kAny = kOddball | kNumber | kName | kReceiver,
    kNone = 0
  };

  bool is_bitset() { return this->IsSmi(); }
  bool is_class() { return this->IsMap(); }
  bool is_constant() { return this->IsBox(); }
  bool is_union() { return this->IsFixedArray(); }

  int as_bitset() { return Smi::cast(this)->value(); }
  Handle<Map> as_class() { return Handle<Map>::cast(handle()); }
  Handle<Box> as_constant() { return Handle<Box>::cast(handle()); }
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
};

} }  // namespace v8::internal

#endif  // V8_TYPES_H_
