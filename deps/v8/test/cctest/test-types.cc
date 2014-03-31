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

#include "cctest.h"
#include "types.h"

using namespace v8::internal;

template<class Type, class TypeHandle, class Region>
class Types {
 public:
  Types(Region* region, Isolate* isolate) :
      Representation(Type::Representation(region)),
      Semantic(Type::Semantic(region)),
      None(Type::None(region)),
      Any(Type::Any(region)),
      Oddball(Type::Oddball(region)),
      Boolean(Type::Boolean(region)),
      Null(Type::Null(region)),
      Undefined(Type::Undefined(region)),
      Number(Type::Number(region)),
      SignedSmall(Type::SignedSmall(region)),
      Signed32(Type::Signed32(region)),
      Float(Type::Float(region)),
      Name(Type::Name(region)),
      UniqueName(Type::UniqueName(region)),
      String(Type::String(region)),
      InternalizedString(Type::InternalizedString(region)),
      Symbol(Type::Symbol(region)),
      Receiver(Type::Receiver(region)),
      Object(Type::Object(region)),
      Array(Type::Array(region)),
      Function(Type::Function(region)),
      Proxy(Type::Proxy(region)),
      object_map(isolate->factory()->NewMap(JS_OBJECT_TYPE, 3 * kPointerSize)),
      array_map(isolate->factory()->NewMap(JS_ARRAY_TYPE, 4 * kPointerSize)),
      region_(region) {
    smi = handle(Smi::FromInt(666), isolate);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    ObjectClass = Type::Class(object_map, region);
    ArrayClass = Type::Class(array_map, region);
    SmiConstant = Type::Constant(smi, region);
    Signed32Constant = Type::Constant(signed32, region);
    ObjectConstant1 = Type::Constant(object1, region);
    ObjectConstant2 = Type::Constant(object2, region);
    ArrayConstant1 = Type::Constant(array, region);
    ArrayConstant2 = Type::Constant(array, region);
  }

  TypeHandle Representation;
  TypeHandle Semantic;
  TypeHandle None;
  TypeHandle Any;
  TypeHandle Oddball;
  TypeHandle Boolean;
  TypeHandle Null;
  TypeHandle Undefined;
  TypeHandle Number;
  TypeHandle SignedSmall;
  TypeHandle Signed32;
  TypeHandle Float;
  TypeHandle Name;
  TypeHandle UniqueName;
  TypeHandle String;
  TypeHandle InternalizedString;
  TypeHandle Symbol;
  TypeHandle Receiver;
  TypeHandle Object;
  TypeHandle Array;
  TypeHandle Function;
  TypeHandle Proxy;

  TypeHandle ObjectClass;
  TypeHandle ArrayClass;

  TypeHandle SmiConstant;
  TypeHandle Signed32Constant;
  TypeHandle ObjectConstant1;
  TypeHandle ObjectConstant2;
  TypeHandle ArrayConstant1;
  TypeHandle ArrayConstant2;

  Handle<i::Map> object_map;
  Handle<i::Map> array_map;

  Handle<i::Smi> smi;
  Handle<i::HeapNumber> signed32;
  Handle<i::JSObject> object1;
  Handle<i::JSObject> object2;
  Handle<i::JSArray> array;

  TypeHandle Union(TypeHandle t1, TypeHandle t2) {
    return Type::Union(t1, t2, region_);
  }
  TypeHandle Intersect(TypeHandle t1, TypeHandle t2) {
    return Type::Intersect(t1, t2, region_);
  }

  template<class Type2, class TypeHandle2>
  TypeHandle Convert(TypeHandle2 t) {
    return Type::template Convert<Type2>(t, region_);
  }

  TypeHandle Fuzz(int depth = 5) {
    switch (rand() % (depth == 0 ? 3 : 20)) {
      case 0: {  // bitset
        int n = 0
        #define COUNT_BITSET_TYPES(type, value) + 1
        BITSET_TYPE_LIST(COUNT_BITSET_TYPES)
        #undef COUNT_BITSET_TYPES
        ;
        int i = rand() % n;
        #define PICK_BITSET_TYPE(type, value) \
          if (i-- == 0) return Type::type(region_);
        BITSET_TYPE_LIST(PICK_BITSET_TYPE)
        #undef PICK_BITSET_TYPE
        UNREACHABLE();
      }
      case 1:  // class
        switch (rand() % 2) {
          case 0: return ObjectClass;
          case 1: return ArrayClass;
        }
        UNREACHABLE();
      case 2:  // constant
        switch (rand() % 6) {
          case 0: return SmiConstant;
          case 1: return Signed32Constant;
          case 2: return ObjectConstant1;
          case 3: return ObjectConstant2;
          case 4: return ArrayConstant1;
          case 5: return ArrayConstant2;
        }
        UNREACHABLE();
      default: {  // union
        int n = rand() % 10;
        TypeHandle type = None;
        for (int i = 0; i < n; ++i) {
          type = Type::Union(type, Fuzz(depth - 1), region_);
        }
        return type;
      }
    }
    UNREACHABLE();
  }

 private:
  Region* region_;
};


// Testing auxiliaries (breaking the Type abstraction).
struct ZoneRep {
  static bool IsTagged(Type* t, int tag) {
    return !IsBitset(t)
        && reinterpret_cast<intptr_t>(AsTagged(t)->at(0)) == tag;
  }
  static bool IsBitset(Type* t) { return reinterpret_cast<intptr_t>(t) & 1; }
  static bool IsClass(Type* t) { return IsTagged(t, 0); }
  static bool IsConstant(Type* t) { return IsTagged(t, 1); }
  static bool IsUnion(Type* t) { return IsTagged(t, 2); }

  static ZoneList<void*>* AsTagged(Type* t) {
    return reinterpret_cast<ZoneList<void*>*>(t);
  }
  static int AsBitset(Type* t) {
    return static_cast<int>(reinterpret_cast<intptr_t>(t) >> 1);
  }
  static Map* AsClass(Type* t) {
    return *reinterpret_cast<Map**>(AsTagged(t)->at(2));
  }
  static Object* AsConstant(Type* t) {
    return *reinterpret_cast<Object**>(AsTagged(t)->at(2));
  }
  static ZoneList<Type*>* AsUnion(Type* t) {
    return reinterpret_cast<ZoneList<Type*>*>(AsTagged(t));
  }

  static Zone* ToRegion(Zone* zone, Isolate* isolate) { return zone; }
};


struct HeapRep {
  static bool IsBitset(Handle<HeapType> t) { return t->IsSmi(); }
  static bool IsClass(Handle<HeapType> t) { return t->IsMap(); }
  static bool IsConstant(Handle<HeapType> t) { return t->IsBox(); }
  static bool IsUnion(Handle<HeapType> t) { return t->IsFixedArray(); }

  static int AsBitset(Handle<HeapType> t) { return Smi::cast(*t)->value(); }
  static Map* AsClass(Handle<HeapType> t) { return Map::cast(*t); }
  static Object* AsConstant(Handle<HeapType> t) {
    return Box::cast(*t)->value();
  }
  static FixedArray* AsUnion(Handle<HeapType> t) {
    return FixedArray::cast(*t);
  }

  static Isolate* ToRegion(Zone* zone, Isolate* isolate) { return isolate; }
};


template<class Type, class TypeHandle, class Region, class Rep>
struct Tests : Rep {
  Isolate* isolate;
  HandleScope scope;
  Zone zone;
  Types<Type, TypeHandle, Region> T;

  Tests() :
      isolate(CcTest::i_isolate()),
      scope(isolate),
      zone(isolate),
      T(Rep::ToRegion(&zone, isolate), isolate) {
  }

  void CheckEqual(TypeHandle type1, TypeHandle type2) {
    CHECK_EQ(Rep::IsBitset(type1), Rep::IsBitset(type2));
    CHECK_EQ(Rep::IsClass(type1), Rep::IsClass(type2));
    CHECK_EQ(Rep::IsConstant(type1), Rep::IsConstant(type2));
    CHECK_EQ(Rep::IsUnion(type1), Rep::IsUnion(type2));
    CHECK_EQ(type1->NumClasses(), type2->NumClasses());
    CHECK_EQ(type1->NumConstants(), type2->NumConstants());
    if (Rep::IsBitset(type1)) {
      CHECK_EQ(Rep::AsBitset(type1), Rep::AsBitset(type2));
    } else if (Rep::IsClass(type1)) {
      CHECK_EQ(Rep::AsClass(type1), Rep::AsClass(type2));
    } else if (Rep::IsConstant(type1)) {
      CHECK_EQ(Rep::AsConstant(type1), Rep::AsConstant(type2));
    } else if (Rep::IsUnion(type1)) {
      CHECK_EQ(Rep::AsUnion(type1)->length(), Rep::AsUnion(type2)->length());
    }
    CHECK(type1->Is(type2));
    CHECK(type2->Is(type1));
  }

  void CheckSub(TypeHandle type1, TypeHandle type2) {
    CHECK(type1->Is(type2));
    CHECK(!type2->Is(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_NE(Rep::AsBitset(type1), Rep::AsBitset(type2));
    }
  }

  void CheckUnordered(TypeHandle type1, TypeHandle type2) {
    CHECK(!type1->Is(type2));
    CHECK(!type2->Is(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_NE(Rep::AsBitset(type1), Rep::AsBitset(type2));
    }
  }

  void CheckOverlap(TypeHandle type1, TypeHandle type2, TypeHandle mask) {
    CHECK(type1->Maybe(type2));
    CHECK(type2->Maybe(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_NE(0,
          Rep::AsBitset(type1) & Rep::AsBitset(type2) & Rep::AsBitset(mask));
    }
  }

  void CheckDisjoint(TypeHandle type1, TypeHandle type2, TypeHandle mask) {
    CHECK(!type1->Is(type2));
    CHECK(!type2->Is(type1));
    CHECK(!type1->Maybe(type2));
    CHECK(!type2->Maybe(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_EQ(0,
          Rep::AsBitset(type1) & Rep::AsBitset(type2) & Rep::AsBitset(mask));
    }
  }

  void Bitset() {
    CHECK(this->IsBitset(T.None));
    CHECK(this->IsBitset(T.Any));
    CHECK(this->IsBitset(T.String));
    CHECK(this->IsBitset(T.Object));

    CHECK(this->IsBitset(T.Union(T.String, T.Number)));
    CHECK(this->IsBitset(T.Union(T.String, T.Receiver)));

    CHECK_EQ(0, this->AsBitset(T.None));
    CHECK_EQ(
        this->AsBitset(T.Number) | this->AsBitset(T.String),
        this->AsBitset(T.Union(T.String, T.Number)));
    CHECK_EQ(
        this->AsBitset(T.Receiver),
        this->AsBitset(T.Union(T.Receiver, T.Object)));
  }

  void Class() {
    CHECK(this->IsClass(T.ObjectClass));
    CHECK(this->IsClass(T.ArrayClass));

    CHECK(*T.object_map == this->AsClass(T.ObjectClass));
    CHECK(*T.array_map == this->AsClass(T.ArrayClass));
  }

  void Constant() {
    CHECK(this->IsConstant(T.SmiConstant));
    CHECK(this->IsConstant(T.ObjectConstant1));
    CHECK(this->IsConstant(T.ObjectConstant2));
    CHECK(this->IsConstant(T.ArrayConstant1));
    CHECK(this->IsConstant(T.ArrayConstant2));

    CHECK(*T.smi == this->AsConstant(T.SmiConstant));
    CHECK(*T.object1 == this->AsConstant(T.ObjectConstant1));
    CHECK(*T.object2 == this->AsConstant(T.ObjectConstant2));
    CHECK(*T.object1 != this->AsConstant(T.ObjectConstant2));
    CHECK(*T.array == this->AsConstant(T.ArrayConstant1));
    CHECK(*T.array == this->AsConstant(T.ArrayConstant2));
  }

  void Is() {
    // Reflexivity
    CHECK(T.None->Is(T.None));
    CHECK(T.Any->Is(T.Any));
    CHECK(T.Object->Is(T.Object));

    CHECK(T.ObjectClass->Is(T.ObjectClass));
    CHECK(T.ObjectConstant1->Is(T.ObjectConstant1));
    CHECK(T.ArrayConstant1->Is(T.ArrayConstant2));

    // Symmetry and Transitivity
    CheckSub(T.None, T.Number);
    CheckSub(T.None, T.Any);

    CheckSub(T.Oddball, T.Any);
    CheckSub(T.Boolean, T.Oddball);
    CheckSub(T.Null, T.Oddball);
    CheckSub(T.Undefined, T.Oddball);
    CheckUnordered(T.Boolean, T.Null);
    CheckUnordered(T.Undefined, T.Null);
    CheckUnordered(T.Boolean, T.Undefined);

    CheckSub(T.Number, T.Any);
    CheckSub(T.SignedSmall, T.Number);
    CheckSub(T.Signed32, T.Number);
    CheckSub(T.Float, T.Number);
    CheckSub(T.SignedSmall, T.Signed32);
    CheckUnordered(T.SignedSmall, T.Float);
    CheckUnordered(T.Signed32, T.Float);

    CheckSub(T.Name, T.Any);
    CheckSub(T.UniqueName, T.Any);
    CheckSub(T.UniqueName, T.Name);
    CheckSub(T.String, T.Name);
    CheckSub(T.InternalizedString, T.String);
    CheckSub(T.InternalizedString, T.UniqueName);
    CheckSub(T.InternalizedString, T.Name);
    CheckSub(T.Symbol, T.UniqueName);
    CheckSub(T.Symbol, T.Name);
    CheckUnordered(T.String, T.UniqueName);
    CheckUnordered(T.String, T.Symbol);
    CheckUnordered(T.InternalizedString, T.Symbol);

    CheckSub(T.Receiver, T.Any);
    CheckSub(T.Object, T.Any);
    CheckSub(T.Object, T.Receiver);
    CheckSub(T.Array, T.Object);
    CheckSub(T.Function, T.Object);
    CheckSub(T.Proxy, T.Receiver);
    CheckUnordered(T.Object, T.Proxy);
    CheckUnordered(T.Array, T.Function);

    // Structured subtyping
    CheckSub(T.None, T.ObjectClass);
    CheckSub(T.None, T.ObjectConstant1);
    CheckSub(T.ObjectClass, T.Any);
    CheckSub(T.ObjectConstant1, T.Any);

    CheckSub(T.ObjectClass, T.Object);
    CheckSub(T.ArrayClass, T.Object);
    CheckUnordered(T.ObjectClass, T.ArrayClass);

    CheckSub(T.SmiConstant, T.SignedSmall);
    CheckSub(T.SmiConstant, T.Signed32);
    CheckSub(T.SmiConstant, T.Number);
    CheckSub(T.ObjectConstant1, T.Object);
    CheckSub(T.ObjectConstant2, T.Object);
    CheckSub(T.ArrayConstant1, T.Object);
    CheckSub(T.ArrayConstant1, T.Array);
    CheckUnordered(T.ObjectConstant1, T.ObjectConstant2);
    CheckUnordered(T.ObjectConstant1, T.ArrayConstant1);

    CheckUnordered(T.ObjectConstant1, T.ObjectClass);
    CheckUnordered(T.ObjectConstant2, T.ObjectClass);
    CheckUnordered(T.ObjectConstant1, T.ArrayClass);
    CheckUnordered(T.ObjectConstant2, T.ArrayClass);
    CheckUnordered(T.ArrayConstant1, T.ObjectClass);
  }

  void Maybe() {
    CheckOverlap(T.Any, T.Any, T.Semantic);
    CheckOverlap(T.Object, T.Object, T.Semantic);

    CheckOverlap(T.Oddball, T.Any, T.Semantic);
    CheckOverlap(T.Boolean, T.Oddball, T.Semantic);
    CheckOverlap(T.Null, T.Oddball, T.Semantic);
    CheckOverlap(T.Undefined, T.Oddball, T.Semantic);
    CheckDisjoint(T.Boolean, T.Null, T.Semantic);
    CheckDisjoint(T.Undefined, T.Null, T.Semantic);
    CheckDisjoint(T.Boolean, T.Undefined, T.Semantic);

    CheckOverlap(T.Number, T.Any, T.Semantic);
    CheckOverlap(T.SignedSmall, T.Number, T.Semantic);
    CheckOverlap(T.Float, T.Number, T.Semantic);
    CheckDisjoint(T.Signed32, T.Float, T.Semantic);

    CheckOverlap(T.Name, T.Any, T.Semantic);
    CheckOverlap(T.UniqueName, T.Any, T.Semantic);
    CheckOverlap(T.UniqueName, T.Name, T.Semantic);
    CheckOverlap(T.String, T.Name, T.Semantic);
    CheckOverlap(T.InternalizedString, T.String, T.Semantic);
    CheckOverlap(T.InternalizedString, T.UniqueName, T.Semantic);
    CheckOverlap(T.InternalizedString, T.Name, T.Semantic);
    CheckOverlap(T.Symbol, T.UniqueName, T.Semantic);
    CheckOverlap(T.Symbol, T.Name, T.Semantic);
    CheckOverlap(T.String, T.UniqueName, T.Semantic);
    CheckDisjoint(T.String, T.Symbol, T.Semantic);
    CheckDisjoint(T.InternalizedString, T.Symbol, T.Semantic);

    CheckOverlap(T.Receiver, T.Any, T.Semantic);
    CheckOverlap(T.Object, T.Any, T.Semantic);
    CheckOverlap(T.Object, T.Receiver, T.Semantic);
    CheckOverlap(T.Array, T.Object, T.Semantic);
    CheckOverlap(T.Function, T.Object, T.Semantic);
    CheckOverlap(T.Proxy, T.Receiver, T.Semantic);
    CheckDisjoint(T.Object, T.Proxy, T.Semantic);
    CheckDisjoint(T.Array, T.Function, T.Semantic);

    CheckOverlap(T.ObjectClass, T.Any, T.Semantic);
    CheckOverlap(T.ObjectConstant1, T.Any, T.Semantic);

    CheckOverlap(T.ObjectClass, T.Object, T.Semantic);
    CheckOverlap(T.ArrayClass, T.Object, T.Semantic);
    CheckOverlap(T.ObjectClass, T.ObjectClass, T.Semantic);
    CheckOverlap(T.ArrayClass, T.ArrayClass, T.Semantic);
    CheckDisjoint(T.ObjectClass, T.ArrayClass, T.Semantic);

    CheckOverlap(T.SmiConstant, T.SignedSmall, T.Semantic);
    CheckOverlap(T.SmiConstant, T.Signed32, T.Semantic);
    CheckOverlap(T.SmiConstant, T.Number, T.Semantic);
    CheckDisjoint(T.SmiConstant, T.Float, T.Semantic);
    CheckOverlap(T.ObjectConstant1, T.Object, T.Semantic);
    CheckOverlap(T.ObjectConstant2, T.Object, T.Semantic);
    CheckOverlap(T.ArrayConstant1, T.Object, T.Semantic);
    CheckOverlap(T.ArrayConstant1, T.Array, T.Semantic);
    CheckOverlap(T.ArrayConstant1, T.ArrayConstant2, T.Semantic);
    CheckOverlap(T.ObjectConstant1, T.ObjectConstant1, T.Semantic);
    CheckDisjoint(T.ObjectConstant1, T.ObjectConstant2, T.Semantic);
    CheckDisjoint(T.ObjectConstant1, T.ArrayConstant1, T.Semantic);

    CheckDisjoint(T.ObjectConstant1, T.ObjectClass, T.Semantic);
    CheckDisjoint(T.ObjectConstant2, T.ObjectClass, T.Semantic);
    CheckDisjoint(T.ObjectConstant1, T.ArrayClass, T.Semantic);
    CheckDisjoint(T.ObjectConstant2, T.ArrayClass, T.Semantic);
    CheckDisjoint(T.ArrayConstant1, T.ObjectClass, T.Semantic);
  }

  void Union() {
    // Bitset-bitset
    CHECK(this->IsBitset(T.Union(T.Object, T.Number)));
    CHECK(this->IsBitset(T.Union(T.Object, T.Object)));
    CHECK(this->IsBitset(T.Union(T.Any, T.None)));

    CheckEqual(T.Union(T.None, T.Number), T.Number);
    CheckEqual(T.Union(T.Object, T.Proxy), T.Receiver);
    CheckEqual(T.Union(T.Number, T.String), T.Union(T.String, T.Number));
    CheckSub(T.Union(T.Number, T.String), T.Any);

    // Class-class
    CHECK(this->IsClass(T.Union(T.ObjectClass, T.ObjectClass)));
    CHECK(this->IsUnion(T.Union(T.ObjectClass, T.ArrayClass)));

    CheckEqual(T.Union(T.ObjectClass, T.ObjectClass), T.ObjectClass);
    CheckSub(T.None, T.Union(T.ObjectClass, T.ArrayClass));
    CheckSub(T.Union(T.ObjectClass, T.ArrayClass), T.Any);
    CheckSub(T.ObjectClass, T.Union(T.ObjectClass, T.ArrayClass));
    CheckSub(T.ArrayClass, T.Union(T.ObjectClass, T.ArrayClass));
    CheckSub(T.Union(T.ObjectClass, T.ArrayClass), T.Object);
    CheckUnordered(T.Union(T.ObjectClass, T.ArrayClass), T.Array);
    CheckOverlap(T.Union(T.ObjectClass, T.ArrayClass), T.Array, T.Semantic);
    CheckDisjoint(T.Union(T.ObjectClass, T.ArrayClass), T.Number, T.Semantic);

    // Constant-constant
    CHECK(this->IsConstant(T.Union(T.ObjectConstant1, T.ObjectConstant1)));
    CHECK(this->IsConstant(T.Union(T.ArrayConstant1, T.ArrayConstant1)));
    CHECK(this->IsUnion(T.Union(T.ObjectConstant1, T.ObjectConstant2)));

    CheckEqual(
        T.Union(T.ObjectConstant1, T.ObjectConstant1),
        T.ObjectConstant1);
    CheckEqual(T.Union(T.ArrayConstant1, T.ArrayConstant1), T.ArrayConstant1);
    CheckEqual(T.Union(T.ArrayConstant1, T.ArrayConstant1), T.ArrayConstant2);
    CheckSub(T.None, T.Union(T.ObjectConstant1, T.ObjectConstant2));
    CheckSub(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.Any);
    CheckSub(T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2));
    CheckSub(T.ObjectConstant2, T.Union(T.ObjectConstant1, T.ObjectConstant2));
    CheckSub(T.ArrayConstant2, T.Union(T.ArrayConstant1, T.ObjectConstant2));
    CheckSub(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.Object);
    CheckUnordered(
        T.Union(T.ObjectConstant1, T.ObjectConstant2), T.ObjectClass);
    CheckUnordered(T.Union(T.ObjectConstant1, T.ArrayConstant1), T.Array);
    CheckOverlap(
        T.Union(T.ObjectConstant1, T.ArrayConstant1), T.Array, T.Semantic);
    CheckOverlap(
        T.Union(T.ObjectConstant1, T.ArrayConstant1), T.ArrayConstant2,
        T.Semantic);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayConstant1), T.Number, T.Semantic);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayConstant1), T.ObjectClass,
        T.Semantic);

    // Bitset-class
    CHECK(this->IsBitset(T.Union(T.ObjectClass, T.Object)));
    CHECK(this->IsUnion(T.Union(T.ObjectClass, T.Number)));

    CheckEqual(T.Union(T.ObjectClass, T.Object), T.Object);
    CheckSub(T.None, T.Union(T.ObjectClass, T.Number));
    CheckSub(T.Union(T.ObjectClass, T.Number), T.Any);
    CheckSub(
        T.Union(T.ObjectClass, T.SignedSmall), T.Union(T.Object, T.Number));
    CheckSub(T.Union(T.ObjectClass, T.Array), T.Object);
    CheckUnordered(T.Union(T.ObjectClass, T.String), T.Array);
    CheckOverlap(T.Union(T.ObjectClass, T.String), T.Object, T.Semantic);
    CheckDisjoint(T.Union(T.ObjectClass, T.String), T.Number, T.Semantic);

    // Bitset-constant
    CHECK(this->IsBitset(T.Union(T.SmiConstant, T.Number)));
    CHECK(this->IsBitset(T.Union(T.ObjectConstant1, T.Object)));
    CHECK(this->IsUnion(T.Union(T.ObjectConstant2, T.Number)));

    CheckEqual(T.Union(T.SmiConstant, T.Number), T.Number);
    CheckEqual(T.Union(T.ObjectConstant1, T.Object), T.Object);
    CheckSub(T.None, T.Union(T.ObjectConstant1, T.Number));
    CheckSub(T.Union(T.ObjectConstant1, T.Number), T.Any);
    CheckSub(
        T.Union(T.ObjectConstant1, T.Signed32), T.Union(T.Object, T.Number));
    CheckSub(T.Union(T.ObjectConstant1, T.Array), T.Object);
    CheckUnordered(T.Union(T.ObjectConstant1, T.String), T.Array);
    CheckOverlap(T.Union(T.ObjectConstant1, T.String), T.Object, T.Semantic);
    CheckDisjoint(T.Union(T.ObjectConstant1, T.String), T.Number, T.Semantic);
    CheckEqual(T.Union(T.Signed32, T.Signed32Constant), T.Signed32);

    // Class-constant
    CHECK(this->IsUnion(T.Union(T.ObjectConstant1, T.ObjectClass)));
    CHECK(this->IsUnion(T.Union(T.ArrayClass, T.ObjectConstant2)));

    CheckSub(T.None, T.Union(T.ObjectConstant1, T.ArrayClass));
    CheckSub(T.Union(T.ObjectConstant1, T.ArrayClass), T.Any);
    CheckSub(T.Union(T.ObjectConstant1, T.ArrayClass), T.Object);
    CheckSub(T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ArrayClass));
    CheckSub(T.ArrayClass, T.Union(T.ObjectConstant1, T.ArrayClass));
    CheckUnordered(T.ObjectClass, T.Union(T.ObjectConstant1, T.ArrayClass));
    CheckSub(
        T.Union(T.ObjectConstant1, T.ArrayClass), T.Union(T.Array, T.Object));
    CheckUnordered(T.Union(T.ObjectConstant1, T.ArrayClass), T.ArrayConstant1);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayClass), T.ObjectConstant2,
        T.Semantic);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayClass), T.ObjectClass, T.Semantic);

    // Bitset-union
    CHECK(this->IsBitset(
        T.Union(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass))));
    CHECK(this->IsUnion(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.Number)));

    CheckEqual(
        T.Union(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Object);
    CheckEqual(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number),
        T.Union(T.ObjectConstant1, T.Union(T.Number, T.ArrayClass)));
    CheckSub(
        T.Float,
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number));
    CheckSub(
        T.ObjectConstant1,
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Float));
    CheckSub(
        T.None,
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Float));
    CheckSub(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Float),
        T.Any);
    CheckSub(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Float),
        T.Union(T.ObjectConstant1, T.Union(T.Number, T.ArrayClass)));

    // Class-union
    CHECK(this->IsUnion(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.ArrayClass)));
    CHECK(this->IsUnion(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.ObjectClass)));

    CheckEqual(
        T.Union(T.ObjectClass, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Union(T.ObjectClass, T.ObjectConstant1));
    CheckSub(
        T.None,
        T.Union(T.ObjectClass, T.Union(T.ObjectConstant1, T.ObjectClass)));
    CheckSub(
        T.Union(T.ObjectClass, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Any);
    CheckSub(
        T.Union(T.ObjectClass, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Object);
    CheckEqual(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.ArrayClass),
        T.Union(T.ArrayClass, T.ObjectConstant2));

    // Constant-union
    CHECK(this->IsUnion(T.Union(
        T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2))));
    CHECK(this->IsUnion(T.Union(
        T.Union(T.ArrayConstant1, T.ObjectClass), T.ObjectConstant1)));
    CHECK(this->IsUnion(T.Union(
        T.Union(T.ArrayConstant1, T.ObjectConstant2), T.ObjectConstant1)));

    CheckEqual(
        T.Union(
            T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Union(
            T.Union(T.ArrayConstant1, T.ObjectConstant2), T.ObjectConstant1),
        T.Union(
            T.ObjectConstant2, T.Union(T.ArrayConstant1, T.ObjectConstant1)));

    // Union-union
    CHECK(this->IsBitset(T.Union(
        T.Union(T.Number, T.ArrayClass),
        T.Union(T.Signed32, T.Array))));
    CHECK(this->IsUnion(T.Union(
        T.Union(T.Number, T.ArrayClass),
        T.Union(T.ObjectClass, T.ArrayClass))));

    CheckEqual(
        T.Union(
            T.Union(T.ObjectConstant2, T.ObjectConstant1),
            T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Union(
            T.Union(T.ObjectConstant2, T.ArrayConstant1),
            T.Union(T.ObjectConstant1, T.ArrayConstant2)),
        T.Union(
            T.Union(T.ObjectConstant1, T.ObjectConstant2),
            T.ArrayConstant1));
    CheckEqual(
        T.Union(
            T.Union(T.Number, T.ArrayClass),
            T.Union(T.SignedSmall, T.Array)),
        T.Union(T.Number, T.Array));
  }

  void Intersect() {
    // Bitset-bitset
    CHECK(this->IsBitset(T.Intersect(T.Object, T.Number)));
    CHECK(this->IsBitset(T.Intersect(T.Object, T.Object)));
    CHECK(this->IsBitset(T.Intersect(T.Any, T.None)));

    CheckEqual(T.Intersect(T.None, T.Number), T.None);
    CheckSub(T.Intersect(T.Object, T.Proxy), T.Representation);
    CheckEqual(T.Intersect(T.Name, T.String), T.Intersect(T.String, T.Name));
    CheckEqual(T.Intersect(T.UniqueName, T.String), T.InternalizedString);

    // Class-class
    CHECK(this->IsClass(T.Intersect(T.ObjectClass, T.ObjectClass)));
    CHECK(this->IsBitset(T.Intersect(T.ObjectClass, T.ArrayClass)));

    CheckEqual(T.Intersect(T.ObjectClass, T.ObjectClass), T.ObjectClass);
    CheckEqual(T.Intersect(T.ObjectClass, T.ArrayClass), T.None);

    // Constant-constant
    CHECK(this->IsConstant(T.Intersect(T.ObjectConstant1, T.ObjectConstant1)));
    CHECK(this->IsConstant(T.Intersect(T.ArrayConstant1, T.ArrayConstant2)));
    CHECK(this->IsBitset(T.Intersect(T.ObjectConstant1, T.ObjectConstant2)));

    CheckEqual(
        T.Intersect(T.ObjectConstant1, T.ObjectConstant1), T.ObjectConstant1);
    CheckEqual(
        T.Intersect(T.ArrayConstant1, T.ArrayConstant2), T.ArrayConstant1);
    CheckEqual(T.Intersect(T.ObjectConstant1, T.ObjectConstant2), T.None);

    // Bitset-class
    CHECK(this->IsClass(T.Intersect(T.ObjectClass, T.Object)));
    CHECK(this->IsBitset(T.Intersect(T.ObjectClass, T.Number)));

    CheckEqual(T.Intersect(T.ObjectClass, T.Object), T.ObjectClass);
    CheckSub(T.Intersect(T.ObjectClass, T.Array), T.Representation);
    CheckSub(T.Intersect(T.ObjectClass, T.Number), T.Representation);

    // Bitset-constant
    CHECK(this->IsBitset(T.Intersect(T.SignedSmall, T.Number)));
    CHECK(this->IsConstant(T.Intersect(T.SmiConstant, T.Number)));
    CHECK(this->IsConstant(T.Intersect(T.ObjectConstant1, T.Object)));

    CheckEqual(T.Intersect(T.SignedSmall, T.Number), T.SignedSmall);
    CheckEqual(T.Intersect(T.SmiConstant, T.Number), T.SmiConstant);
    CheckEqual(T.Intersect(T.ObjectConstant1, T.Object), T.ObjectConstant1);

    // Class-constant
    CHECK(this->IsBitset(T.Intersect(T.ObjectConstant1, T.ObjectClass)));
    CHECK(this->IsBitset(T.Intersect(T.ArrayClass, T.ObjectConstant2)));

    CheckEqual(T.Intersect(T.ObjectConstant1, T.ObjectClass), T.None);
    CheckEqual(T.Intersect(T.ArrayClass, T.ObjectConstant2), T.None);

    // Bitset-union
    CHECK(this->IsUnion(
        T.Intersect(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass))));
    CHECK(this->IsBitset(
        T.Intersect(T.Union(T.ArrayClass, T.ObjectConstant2), T.Number)));

    CheckEqual(
        T.Intersect(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Union(T.ObjectConstant1, T.ObjectClass));
    CheckEqual(
        T.Intersect(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number),
        T.None);

    // Class-union
    CHECK(this->IsClass(
        T.Intersect(T.Union(T.ArrayClass, T.ObjectConstant2), T.ArrayClass)));
    CHECK(this->IsClass(
        T.Intersect(T.Union(T.Object, T.SmiConstant), T.ArrayClass)));
    CHECK(this->IsBitset(
        T.Intersect(T.Union(T.ObjectClass, T.ArrayConstant1), T.ArrayClass)));

    CheckEqual(
        T.Intersect(T.ArrayClass, T.Union(T.ObjectConstant2, T.ArrayClass)),
        T.ArrayClass);
    CheckEqual(
        T.Intersect(T.ArrayClass, T.Union(T.Object, T.SmiConstant)),
        T.ArrayClass);
    CheckEqual(
        T.Intersect(T.Union(T.ObjectClass, T.ArrayConstant1), T.ArrayClass),
        T.None);

    // Constant-union
    CHECK(this->IsConstant(T.Intersect(
        T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2))));
    CHECK(this->IsConstant(T.Intersect(
        T.Union(T.Number, T.ObjectClass), T.SmiConstant)));
    CHECK(this->IsBitset(T.Intersect(
        T.Union(T.ArrayConstant1, T.ObjectClass), T.ObjectConstant1)));

    CheckEqual(
        T.Intersect(
            T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.ObjectConstant1);
    CheckEqual(
        T.Intersect(T.SmiConstant, T.Union(T.Number, T.ObjectConstant2)),
        T.SmiConstant);
    CheckEqual(
        T.Intersect(
            T.Union(T.ArrayConstant1, T.ObjectClass), T.ObjectConstant1),
        T.None);

    // Union-union
    CHECK(this->IsUnion(T.Intersect(
        T.Union(T.Number, T.ArrayClass), T.Union(T.Signed32, T.Array))));
    CHECK(this->IsBitset(T.Intersect(
        T.Union(T.Number, T.ObjectClass), T.Union(T.Signed32, T.Array))));

    CheckEqual(
        T.Intersect(
            T.Union(T.Number, T.ArrayClass),
            T.Union(T.SignedSmall, T.Array)),
        T.Union(T.SignedSmall, T.ArrayClass));
    CheckEqual(
        T.Intersect(
            T.Union(T.Number, T.ObjectClass),
            T.Union(T.Signed32, T.Array)),
        T.Signed32);
    CheckEqual(
        T.Intersect(
            T.Union(T.ObjectConstant2, T.ObjectConstant1),
            T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Intersect(
            T.Union(
                T.Union(T.ObjectConstant2, T.ObjectConstant1), T.ArrayClass),
            T.Union(
                T.ObjectConstant1,
                T.Union(T.ArrayConstant1, T.ObjectConstant2))),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Intersect(
            T.Union(T.ObjectConstant2, T.ArrayConstant1),
            T.Union(T.ObjectConstant1, T.ArrayConstant2)),
        T.ArrayConstant1);
  }

  template<class Type2, class TypeHandle2, class Region2, class Rep2>
  void Convert() {
    Types<Type2, TypeHandle2, Region2> T2(
        Rep2::ToRegion(&zone, isolate), isolate);
    for (int i = 0; i < 100; ++i) {
      TypeHandle type = T.Fuzz();
      CheckEqual(type,
                 T.template Convert<Type2>(T2.template Convert<Type>(type)));
    }
  }
};

typedef Tests<Type, Type*, Zone, ZoneRep> ZoneTests;
typedef Tests<HeapType, Handle<HeapType>, Isolate, HeapRep> HeapTests;


TEST(Bitset) {
  CcTest::InitializeVM();
  ZoneTests().Bitset();
  HeapTests().Bitset();
}


TEST(Class) {
  CcTest::InitializeVM();
  ZoneTests().Class();
  HeapTests().Class();
}


TEST(Constant) {
  CcTest::InitializeVM();
  ZoneTests().Constant();
  HeapTests().Constant();
}


TEST(Is) {
  CcTest::InitializeVM();
  ZoneTests().Is();
  HeapTests().Is();
}


TEST(Maybe) {
  CcTest::InitializeVM();
  ZoneTests().Maybe();
  HeapTests().Maybe();
}


TEST(Union) {
  CcTest::InitializeVM();
  ZoneTests().Union();
  HeapTests().Union();
}


TEST(Intersect) {
  CcTest::InitializeVM();
  ZoneTests().Intersect();
  HeapTests().Intersect();
}


TEST(Convert) {
  CcTest::InitializeVM();
  ZoneTests().Convert<HeapType, Handle<HeapType>, Isolate, HeapRep>();
  HeapTests().Convert<Type, Type*, Zone, ZoneRep>();
}
