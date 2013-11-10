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

// Testing auxiliaries (breaking the Type abstraction).
static bool IsBitset(Type* type) { return type->IsSmi(); }
static bool IsClass(Type* type) { return type->IsMap(); }
static bool IsConstant(Type* type) { return type->IsBox(); }
static bool IsUnion(Type* type) { return type->IsFixedArray(); }

static int AsBitset(Type* type) { return Smi::cast(type)->value(); }
static Map* AsClass(Type* type) { return Map::cast(type); }
static Object* AsConstant(Type* type) { return Box::cast(type)->value(); }
static FixedArray* AsUnion(Type* type) { return FixedArray::cast(type); }


static void CheckEqual(Handle<Type> type1, Handle<Type> type2) {
  CHECK_EQ(IsBitset(*type1), IsBitset(*type2));
  CHECK_EQ(IsClass(*type1), IsClass(*type2));
  CHECK_EQ(IsConstant(*type1), IsConstant(*type2));
  CHECK_EQ(IsUnion(*type1), IsUnion(*type2));
  CHECK_EQ(type1->NumClasses(), type2->NumClasses());
  CHECK_EQ(type1->NumConstants(), type2->NumConstants());
  if (IsBitset(*type1)) {
    CHECK_EQ(AsBitset(*type1), AsBitset(*type2));
  } else if (IsClass(*type1)) {
    CHECK_EQ(AsClass(*type1), AsClass(*type2));
  } else if (IsConstant(*type1)) {
    CHECK_EQ(AsConstant(*type1), AsConstant(*type2));
  } else if (IsUnion(*type1)) {
    CHECK_EQ(AsUnion(*type1)->length(), AsUnion(*type2)->length());
  }
  CHECK(type1->Is(type2));
  CHECK(type2->Is(type1));
}


static void CheckSub(Handle<Type> type1, Handle<Type> type2) {
  CHECK(type1->Is(type2));
  CHECK(!type2->Is(type1));
  if (IsBitset(*type1) && IsBitset(*type2)) {
    CHECK_NE(AsBitset(*type1), AsBitset(*type2));
  }
}


static void CheckUnordered(Handle<Type> type1, Handle<Type> type2) {
  CHECK(!type1->Is(type2));
  CHECK(!type2->Is(type1));
  if (IsBitset(*type1) && IsBitset(*type2)) {
    CHECK_NE(AsBitset(*type1), AsBitset(*type2));
  }
}


static void CheckOverlap(Handle<Type> type1, Handle<Type> type2) {
  CHECK(type1->Maybe(type2));
  CHECK(type2->Maybe(type1));
  if (IsBitset(*type1) && IsBitset(*type2)) {
    CHECK_NE(0, AsBitset(*type1) & AsBitset(*type2));
  }
}


static void CheckDisjoint(Handle<Type> type1, Handle<Type> type2) {
  CHECK(!type1->Is(type2));
  CHECK(!type2->Is(type1));
  CHECK(!type1->Maybe(type2));
  CHECK(!type2->Maybe(type1));
  if (IsBitset(*type1) && IsBitset(*type2)) {
    CHECK_EQ(0, AsBitset(*type1) & AsBitset(*type2));
  }
}


class HandlifiedTypes {
 public:
  explicit HandlifiedTypes(Isolate* isolate) :
      None(Type::None(), isolate),
      Any(Type::Any(), isolate),
      Oddball(Type::Oddball(), isolate),
      Boolean(Type::Boolean(), isolate),
      Null(Type::Null(), isolate),
      Undefined(Type::Undefined(), isolate),
      Number(Type::Number(), isolate),
      Smi(Type::Smi(), isolate),
      Signed32(Type::Signed32(), isolate),
      Double(Type::Double(), isolate),
      Name(Type::Name(), isolate),
      UniqueName(Type::UniqueName(), isolate),
      String(Type::String(), isolate),
      InternalizedString(Type::InternalizedString(), isolate),
      Symbol(Type::Symbol(), isolate),
      Receiver(Type::Receiver(), isolate),
      Object(Type::Object(), isolate),
      Array(Type::Array(), isolate),
      Function(Type::Function(), isolate),
      Proxy(Type::Proxy(), isolate),
      object_map(isolate->factory()->NewMap(JS_OBJECT_TYPE, 3 * kPointerSize)),
      array_map(isolate->factory()->NewMap(JS_ARRAY_TYPE, 4 * kPointerSize)),
      isolate_(isolate) {
    smi = handle(Smi::FromInt(666), isolate);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    ObjectClass = Class(object_map);
    ArrayClass = Class(array_map);
    SmiConstant = Constant(smi);
    Signed32Constant = Constant(signed32);
    ObjectConstant1 = Constant(object1);
    ObjectConstant2 = Constant(object2);
    ArrayConstant1 = Constant(array);
    ArrayConstant2 = Constant(array);
  }

  Handle<Type> None;
  Handle<Type> Any;
  Handle<Type> Oddball;
  Handle<Type> Boolean;
  Handle<Type> Null;
  Handle<Type> Undefined;
  Handle<Type> Number;
  Handle<Type> Smi;
  Handle<Type> Signed32;
  Handle<Type> Double;
  Handle<Type> Name;
  Handle<Type> UniqueName;
  Handle<Type> String;
  Handle<Type> InternalizedString;
  Handle<Type> Symbol;
  Handle<Type> Receiver;
  Handle<Type> Object;
  Handle<Type> Array;
  Handle<Type> Function;
  Handle<Type> Proxy;

  Handle<Type> ObjectClass;
  Handle<Type> ArrayClass;

  Handle<Type> SmiConstant;
  Handle<Type> Signed32Constant;
  Handle<Type> ObjectConstant1;
  Handle<Type> ObjectConstant2;
  Handle<Type> ArrayConstant1;
  Handle<Type> ArrayConstant2;

  Handle<Map> object_map;
  Handle<Map> array_map;

  Handle<i::Smi> smi;
  Handle<HeapNumber> signed32;
  Handle<JSObject> object1;
  Handle<JSObject> object2;
  Handle<JSArray> array;

  Handle<Type> Class(Handle<Map> map) {
    return handle(Type::Class(map), isolate_);
  }
  Handle<Type> Constant(Handle<i::Object> value) {
    return handle(Type::Constant(value, isolate_), isolate_);
  }
  Handle<Type> Union(Handle<Type> type1, Handle<Type> type2) {
    return handle(Type::Union(type1, type2), isolate_);
  }
  Handle<Type> Intersect(Handle<Type> type1, Handle<Type> type2) {
    return handle(Type::Intersect(type1, type2), isolate_);
  }

 private:
  Isolate* isolate_;
};


TEST(Bitset) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

  CHECK(IsBitset(*T.None));
  CHECK(IsBitset(*T.Any));
  CHECK(IsBitset(*T.String));
  CHECK(IsBitset(*T.Object));

  CHECK(IsBitset(Type::Union(T.String, T.Number)));
  CHECK(IsBitset(Type::Union(T.String, T.Receiver)));
  CHECK(IsBitset(Type::Optional(T.Object)));

  CHECK_EQ(0, AsBitset(*T.None));
  CHECK_EQ(AsBitset(*T.Number) | AsBitset(*T.String),
           AsBitset(Type::Union(T.String, T.Number)));
  CHECK_EQ(AsBitset(*T.Receiver),
           AsBitset(Type::Union(T.Receiver, T.Object)));
  CHECK_EQ(AsBitset(*T.String) | AsBitset(*T.Undefined),
           AsBitset(Type::Optional(T.String)));
}


TEST(Class) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

  CHECK(IsClass(*T.ObjectClass));
  CHECK(IsClass(*T.ArrayClass));

  CHECK(*T.object_map == AsClass(*T.ObjectClass));
  CHECK(*T.array_map == AsClass(*T.ArrayClass));
}


TEST(Constant) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

  CHECK(IsConstant(*T.SmiConstant));
  CHECK(IsConstant(*T.ObjectConstant1));
  CHECK(IsConstant(*T.ObjectConstant2));
  CHECK(IsConstant(*T.ArrayConstant1));
  CHECK(IsConstant(*T.ArrayConstant2));

  CHECK(*T.smi == AsConstant(*T.SmiConstant));
  CHECK(*T.object1 == AsConstant(*T.ObjectConstant1));
  CHECK(*T.object2 == AsConstant(*T.ObjectConstant2));
  CHECK(*T.object1 != AsConstant(*T.ObjectConstant2));
  CHECK(*T.array == AsConstant(*T.ArrayConstant1));
  CHECK(*T.array == AsConstant(*T.ArrayConstant2));
}


TEST(Is) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

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
  CheckSub(T.Smi, T.Number);
  CheckSub(T.Signed32, T.Number);
  CheckSub(T.Double, T.Number);
  CheckSub(T.Smi, T.Signed32);
  CheckUnordered(T.Smi, T.Double);
  CheckUnordered(T.Signed32, T.Double);

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

  CheckSub(T.SmiConstant, T.Smi);
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


TEST(Maybe) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

  CheckOverlap(T.Any, T.Any);
  CheckOverlap(T.Object, T.Object);

  CheckOverlap(T.Oddball, T.Any);
  CheckOverlap(T.Boolean, T.Oddball);
  CheckOverlap(T.Null, T.Oddball);
  CheckOverlap(T.Undefined, T.Oddball);
  CheckDisjoint(T.Boolean, T.Null);
  CheckDisjoint(T.Undefined, T.Null);
  CheckDisjoint(T.Boolean, T.Undefined);

  CheckOverlap(T.Number, T.Any);
  CheckOverlap(T.Smi, T.Number);
  CheckOverlap(T.Double, T.Number);
  CheckDisjoint(T.Signed32, T.Double);

  CheckOverlap(T.Name, T.Any);
  CheckOverlap(T.UniqueName, T.Any);
  CheckOverlap(T.UniqueName, T.Name);
  CheckOverlap(T.String, T.Name);
  CheckOverlap(T.InternalizedString, T.String);
  CheckOverlap(T.InternalizedString, T.UniqueName);
  CheckOverlap(T.InternalizedString, T.Name);
  CheckOverlap(T.Symbol, T.UniqueName);
  CheckOverlap(T.Symbol, T.Name);
  CheckOverlap(T.String, T.UniqueName);
  CheckDisjoint(T.String, T.Symbol);
  CheckDisjoint(T.InternalizedString, T.Symbol);

  CheckOverlap(T.Receiver, T.Any);
  CheckOverlap(T.Object, T.Any);
  CheckOverlap(T.Object, T.Receiver);
  CheckOverlap(T.Array, T.Object);
  CheckOverlap(T.Function, T.Object);
  CheckOverlap(T.Proxy, T.Receiver);
  CheckDisjoint(T.Object, T.Proxy);
  CheckDisjoint(T.Array, T.Function);

  CheckOverlap(T.ObjectClass, T.Any);
  CheckOverlap(T.ObjectConstant1, T.Any);

  CheckOverlap(T.ObjectClass, T.Object);
  CheckOverlap(T.ArrayClass, T.Object);
  CheckOverlap(T.ObjectClass, T.ObjectClass);
  CheckOverlap(T.ArrayClass, T.ArrayClass);
  CheckDisjoint(T.ObjectClass, T.ArrayClass);

  CheckOverlap(T.SmiConstant, T.Smi);
  CheckOverlap(T.SmiConstant, T.Signed32);
  CheckOverlap(T.SmiConstant, T.Number);
  CheckDisjoint(T.SmiConstant, T.Double);
  CheckOverlap(T.ObjectConstant1, T.Object);
  CheckOverlap(T.ObjectConstant2, T.Object);
  CheckOverlap(T.ArrayConstant1, T.Object);
  CheckOverlap(T.ArrayConstant1, T.Array);
  CheckOverlap(T.ArrayConstant1, T.ArrayConstant2);
  CheckOverlap(T.ObjectConstant1, T.ObjectConstant1);
  CheckDisjoint(T.ObjectConstant1, T.ObjectConstant2);
  CheckDisjoint(T.ObjectConstant1, T.ArrayConstant1);

  CheckDisjoint(T.ObjectConstant1, T.ObjectClass);
  CheckDisjoint(T.ObjectConstant2, T.ObjectClass);
  CheckDisjoint(T.ObjectConstant1, T.ArrayClass);
  CheckDisjoint(T.ObjectConstant2, T.ArrayClass);
  CheckDisjoint(T.ArrayConstant1, T.ObjectClass);
}


TEST(Union) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

  // Bitset-bitset
  CHECK(IsBitset(Type::Union(T.Object, T.Number)));
  CHECK(IsBitset(Type::Union(T.Object, T.Object)));
  CHECK(IsBitset(Type::Union(T.Any, T.None)));

  CheckEqual(T.Union(T.None, T.Number), T.Number);
  CheckEqual(T.Union(T.Object, T.Proxy), T.Receiver);
  CheckEqual(T.Union(T.Number, T.String), T.Union(T.String, T.Number));
  CheckSub(T.Union(T.Number, T.String), T.Any);

  // Class-class
  CHECK(IsClass(Type::Union(T.ObjectClass, T.ObjectClass)));
  CHECK(IsUnion(Type::Union(T.ObjectClass, T.ArrayClass)));

  CheckEqual(T.Union(T.ObjectClass, T.ObjectClass), T.ObjectClass);
  CheckSub(T.None, T.Union(T.ObjectClass, T.ArrayClass));
  CheckSub(T.Union(T.ObjectClass, T.ArrayClass), T.Any);
  CheckSub(T.ObjectClass, T.Union(T.ObjectClass, T.ArrayClass));
  CheckSub(T.ArrayClass, T.Union(T.ObjectClass, T.ArrayClass));
  CheckSub(T.Union(T.ObjectClass, T.ArrayClass), T.Object);
  CheckUnordered(T.Union(T.ObjectClass, T.ArrayClass), T.Array);
  CheckOverlap(T.Union(T.ObjectClass, T.ArrayClass), T.Array);
  CheckDisjoint(T.Union(T.ObjectClass, T.ArrayClass), T.Number);

  // Constant-constant
  CHECK(IsConstant(Type::Union(T.ObjectConstant1, T.ObjectConstant1)));
  CHECK(IsConstant(Type::Union(T.ArrayConstant1, T.ArrayConstant1)));
  CHECK(IsUnion(Type::Union(T.ObjectConstant1, T.ObjectConstant2)));

  CheckEqual(T.Union(T.ObjectConstant1, T.ObjectConstant1), T.ObjectConstant1);
  CheckEqual(T.Union(T.ArrayConstant1, T.ArrayConstant1), T.ArrayConstant1);
  CheckEqual(T.Union(T.ArrayConstant1, T.ArrayConstant1), T.ArrayConstant2);
  CheckSub(T.None, T.Union(T.ObjectConstant1, T.ObjectConstant2));
  CheckSub(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.Any);
  CheckSub(T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2));
  CheckSub(T.ObjectConstant2, T.Union(T.ObjectConstant1, T.ObjectConstant2));
  CheckSub(T.ArrayConstant2, T.Union(T.ArrayConstant1, T.ObjectConstant2));
  CheckSub(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.Object);
  CheckUnordered(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.ObjectClass);
  CheckUnordered(T.Union(T.ObjectConstant1, T.ArrayConstant1), T.Array);
  CheckOverlap(T.Union(T.ObjectConstant1, T.ArrayConstant1), T.Array);
  CheckOverlap(T.Union(T.ObjectConstant1, T.ArrayConstant1), T.ArrayConstant2);
  CheckDisjoint(T.Union(T.ObjectConstant1, T.ArrayConstant1), T.Number);
  CheckDisjoint(T.Union(T.ObjectConstant1, T.ArrayConstant1), T.ObjectClass);

  // Bitset-class
  CHECK(IsBitset(Type::Union(T.ObjectClass, T.Object)));
  CHECK(IsUnion(Type::Union(T.ObjectClass, T.Number)));

  CheckEqual(T.Union(T.ObjectClass, T.Object), T.Object);
  CheckSub(T.None, T.Union(T.ObjectClass, T.Number));
  CheckSub(T.Union(T.ObjectClass, T.Number), T.Any);
  CheckSub(T.Union(T.ObjectClass, T.Smi), T.Union(T.Object, T.Number));
  CheckSub(T.Union(T.ObjectClass, T.Array), T.Object);
  CheckUnordered(T.Union(T.ObjectClass, T.String), T.Array);
  CheckOverlap(T.Union(T.ObjectClass, T.String), T.Object);
  CheckDisjoint(T.Union(T.ObjectClass, T.String), T.Number);

  // Bitset-constant
  CHECK(IsBitset(Type::Union(T.SmiConstant, T.Number)));
  CHECK(IsBitset(Type::Union(T.ObjectConstant1, T.Object)));
  CHECK(IsUnion(Type::Union(T.ObjectConstant2, T.Number)));

  CheckEqual(T.Union(T.SmiConstant, T.Number), T.Number);
  CheckEqual(T.Union(T.ObjectConstant1, T.Object), T.Object);
  CheckSub(T.None, T.Union(T.ObjectConstant1, T.Number));
  CheckSub(T.Union(T.ObjectConstant1, T.Number), T.Any);
  CheckSub(T.Union(T.ObjectConstant1, T.Signed32), T.Union(T.Object, T.Number));
  CheckSub(T.Union(T.ObjectConstant1, T.Array), T.Object);
  CheckUnordered(T.Union(T.ObjectConstant1, T.String), T.Array);
  CheckOverlap(T.Union(T.ObjectConstant1, T.String), T.Object);
  CheckDisjoint(T.Union(T.ObjectConstant1, T.String), T.Number);
  CheckEqual(T.Union(T.Signed32, T.Signed32Constant), T.Signed32);

  // Class-constant
  CHECK(IsUnion(Type::Union(T.ObjectConstant1, T.ObjectClass)));
  CHECK(IsUnion(Type::Union(T.ArrayClass, T.ObjectConstant2)));

  CheckSub(T.None, T.Union(T.ObjectConstant1, T.ArrayClass));
  CheckSub(T.Union(T.ObjectConstant1, T.ArrayClass), T.Any);
  CheckSub(T.Union(T.ObjectConstant1, T.ArrayClass), T.Object);
  CheckSub(T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ArrayClass));
  CheckSub(T.ArrayClass, T.Union(T.ObjectConstant1, T.ArrayClass));
  CheckUnordered(T.ObjectClass, T.Union(T.ObjectConstant1, T.ArrayClass));
  CheckSub(
      T.Union(T.ObjectConstant1, T.ArrayClass), T.Union(T.Array, T.Object));
  CheckUnordered(T.Union(T.ObjectConstant1, T.ArrayClass), T.ArrayConstant1);
  CheckDisjoint(T.Union(T.ObjectConstant1, T.ArrayClass), T.ObjectConstant2);
  CheckDisjoint(T.Union(T.ObjectConstant1, T.ArrayClass), T.ObjectClass);

  // Bitset-union
  CHECK(IsBitset(
      Type::Union(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass))));
  CHECK(IsUnion(
      Type::Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.Number)));

  CheckEqual(
      T.Union(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass)),
      T.Object);
  CheckEqual(
      T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number),
      T.Union(T.ObjectConstant1, T.Union(T.Number, T.ArrayClass)));
  CheckSub(
      T.Double,
      T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number));
  CheckSub(
      T.ObjectConstant1,
      T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Double));
  CheckSub(
      T.None,
      T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Double));
  CheckSub(
      T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Double),
      T.Any);
  CheckSub(
      T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Double),
      T.Union(T.ObjectConstant1, T.Union(T.Number, T.ArrayClass)));

  // Class-union
  CHECK(IsUnion(
      Type::Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.ArrayClass)));
  CHECK(IsUnion(
      Type::Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.ObjectClass)));

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
  CHECK(IsUnion(Type::Union(
      T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2))));
  CHECK(IsUnion(Type::Union(
      T.Union(T.ArrayConstant1, T.ObjectClass), T.ObjectConstant1)));
  CHECK(IsUnion(Type::Union(
      T.Union(T.ArrayConstant1, T.ObjectConstant2), T.ObjectConstant1)));

  CheckEqual(
      T.Union(T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2)),
      T.Union(T.ObjectConstant2, T.ObjectConstant1));
  CheckEqual(
      T.Union(T.Union(T.ArrayConstant1, T.ObjectConstant2), T.ObjectConstant1),
      T.Union(T.ObjectConstant2, T.Union(T.ArrayConstant1, T.ObjectConstant1)));

  // Union-union
  CHECK(IsBitset(Type::Union(
      T.Union(T.Number, T.ArrayClass), T.Union(T.Signed32, T.Array))));
  CHECK(IsUnion(Type::Union(
      T.Union(T.Number, T.ArrayClass), T.Union(T.ObjectClass, T.ArrayClass))));

  CheckEqual(
      T.Union(
          T.Union(T.ObjectConstant2, T.ObjectConstant1),
          T.Union(T.ObjectConstant1, T.ObjectConstant2)),
      T.Union(T.ObjectConstant2, T.ObjectConstant1));
  CheckEqual(
      T.Union(
          T.Union(T.ObjectConstant2, T.ArrayConstant1),
          T.Union(T.ObjectConstant1, T.ArrayConstant2)),
      T.Union(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.ArrayConstant1));
  CheckEqual(
      T.Union(T.Union(T.Number, T.ArrayClass), T.Union(T.Smi, T.Array)),
      T.Union(T.Number, T.Array));
}


TEST(Intersect) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  HandlifiedTypes T(isolate);

  // Bitset-bitset
  CHECK(IsBitset(Type::Intersect(T.Object, T.Number)));
  CHECK(IsBitset(Type::Intersect(T.Object, T.Object)));
  CHECK(IsBitset(Type::Intersect(T.Any, T.None)));

  CheckEqual(T.Intersect(T.None, T.Number), T.None);
  CheckEqual(T.Intersect(T.Object, T.Proxy), T.None);
  CheckEqual(T.Intersect(T.Name, T.String), T.Intersect(T.String, T.Name));
  CheckEqual(T.Intersect(T.UniqueName, T.String), T.InternalizedString);

  // Class-class
  CHECK(IsClass(Type::Intersect(T.ObjectClass, T.ObjectClass)));
  CHECK(IsBitset(Type::Intersect(T.ObjectClass, T.ArrayClass)));

  CheckEqual(T.Intersect(T.ObjectClass, T.ObjectClass), T.ObjectClass);
  CheckEqual(T.Intersect(T.ObjectClass, T.ArrayClass), T.None);

  // Constant-constant
  CHECK(IsConstant(Type::Intersect(T.ObjectConstant1, T.ObjectConstant1)));
  CHECK(IsConstant(Type::Intersect(T.ArrayConstant1, T.ArrayConstant2)));
  CHECK(IsBitset(Type::Intersect(T.ObjectConstant1, T.ObjectConstant2)));

  CheckEqual(
      T.Intersect(T.ObjectConstant1, T.ObjectConstant1), T.ObjectConstant1);
  CheckEqual(
      T.Intersect(T.ArrayConstant1, T.ArrayConstant2), T.ArrayConstant1);
  CheckEqual(T.Intersect(T.ObjectConstant1, T.ObjectConstant2), T.None);

  // Bitset-class
  CHECK(IsClass(Type::Intersect(T.ObjectClass, T.Object)));
  CHECK(IsBitset(Type::Intersect(T.ObjectClass, T.Number)));

  CheckEqual(T.Intersect(T.ObjectClass, T.Object), T.ObjectClass);
  CheckEqual(T.Intersect(T.ObjectClass, T.Array), T.None);
  CheckEqual(T.Intersect(T.ObjectClass, T.Number), T.None);

  // Bitset-constant
  CHECK(IsBitset(Type::Intersect(T.Smi, T.Number)));
  CHECK(IsConstant(Type::Intersect(T.SmiConstant, T.Number)));
  CHECK(IsConstant(Type::Intersect(T.ObjectConstant1, T.Object)));

  CheckEqual(T.Intersect(T.Smi, T.Number), T.Smi);
  CheckEqual(T.Intersect(T.SmiConstant, T.Number), T.SmiConstant);
  CheckEqual(T.Intersect(T.ObjectConstant1, T.Object), T.ObjectConstant1);

  // Class-constant
  CHECK(IsBitset(Type::Intersect(T.ObjectConstant1, T.ObjectClass)));
  CHECK(IsBitset(Type::Intersect(T.ArrayClass, T.ObjectConstant2)));

  CheckEqual(T.Intersect(T.ObjectConstant1, T.ObjectClass), T.None);
  CheckEqual(T.Intersect(T.ArrayClass, T.ObjectConstant2), T.None);

  // Bitset-union
  CHECK(IsUnion(
      Type::Intersect(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass))));
  CHECK(IsBitset(
      Type::Intersect(T.Union(T.ArrayClass, T.ObjectConstant2), T.Number)));

  CheckEqual(
      T.Intersect(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass)),
      T.Union(T.ObjectConstant1, T.ObjectClass));
  CheckEqual(
      T.Intersect(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number),
      T.None);

  // Class-union
  CHECK(IsClass(
      Type::Intersect(T.Union(T.ArrayClass, T.ObjectConstant2), T.ArrayClass)));
  CHECK(IsClass(
      Type::Intersect(T.Union(T.Object, T.SmiConstant), T.ArrayClass)));
  CHECK(IsBitset(
      Type::Intersect(T.Union(T.ObjectClass, T.ArrayConstant1), T.ArrayClass)));

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
  CHECK(IsConstant(Type::Intersect(
      T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2))));
  CHECK(IsConstant(Type::Intersect(
      T.Union(T.Number, T.ObjectClass), T.SmiConstant)));
  CHECK(IsBitset(Type::Intersect(
      T.Union(T.ArrayConstant1, T.ObjectClass), T.ObjectConstant1)));

  CheckEqual(
      T.Intersect(
          T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2)),
      T.ObjectConstant1);
  CheckEqual(
      T.Intersect(T.SmiConstant, T.Union(T.Number, T.ObjectConstant2)),
      T.SmiConstant);
  CheckEqual(
      T.Intersect(T.Union(T.ArrayConstant1, T.ObjectClass), T.ObjectConstant1),
      T.None);

  // Union-union
  CHECK(IsUnion(Type::Intersect(
      T.Union(T.Number, T.ArrayClass), T.Union(T.Signed32, T.Array))));
  CHECK(IsBitset(Type::Intersect(
      T.Union(T.Number, T.ObjectClass), T.Union(T.Signed32, T.Array))));

  CheckEqual(
      T.Intersect(
          T.Union(T.Number, T.ArrayClass),
          T.Union(T.Smi, T.Array)),
      T.Union(T.Smi, T.ArrayClass));
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
          T.Union(T.Union(T.ObjectConstant2, T.ObjectConstant1), T.ArrayClass),
          T.Union(
              T.ObjectConstant1, T.Union(T.ArrayConstant1, T.ObjectConstant2))),
      T.Union(T.ObjectConstant2, T.ObjectConstant1));
  CheckEqual(
      T.Intersect(
          T.Union(T.ObjectConstant2, T.ArrayConstant1),
          T.Union(T.ObjectConstant1, T.ArrayConstant2)),
      T.ArrayConstant1);
}
