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

#include <stdlib.h>

#include "v8.h"

#include "factory.h"
#include "global-handles.h"
#include "unique.h"
#include "cctest.h"

using namespace v8::internal;

template <class T, class U>
void CheckHashCodeEqual(Unique<T> a, Unique<U> b) {
  int64_t hasha = static_cast<int64_t>(a.Hashcode());
  int64_t hashb = static_cast<int64_t>(b.Hashcode());
  CHECK_NE(static_cast<int64_t>(0), hasha);
  CHECK_NE(static_cast<int64_t>(0), hashb);
  CHECK_EQ(hasha, hashb);
}


template <class T, class U>
void CheckHashCodeNotEqual(Unique<T> a, Unique<U> b) {
  int64_t hasha = static_cast<int64_t>(a.Hashcode());
  int64_t hashb = static_cast<int64_t>(b.Hashcode());
  CHECK_NE(static_cast<int64_t>(0), hasha);
  CHECK_NE(static_cast<int64_t>(0), hashb);
  CHECK_NE(hasha, hashb);
}


TEST(UniqueCreate) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Handle<String> A = factory->InternalizeUtf8String("A");
  Unique<String> HA(A);

  CHECK(*HA.handle() == *A);
  CHECK_EQ(*A, *HA.handle());

  Unique<String> HA2(A);

  CheckHashCodeEqual(HA, HA2);
  CHECK(HA == HA2);
  CHECK_EQ(*HA.handle(), *HA2.handle());

  CHECK(HA2 == HA);
  CHECK_EQ(*HA2.handle(), *HA.handle());

  Handle<String> B = factory->InternalizeUtf8String("B");
  Unique<String> HB(B);

  CheckHashCodeNotEqual(HA, HB);
  CHECK(HA != HB);
  CHECK_NE(*HA.handle(), *HB.handle());

  CHECK(HB != HA);
  CHECK_NE(*HB.handle(), *HA.handle());

  // TODO(titzer): check that Unique properly survives a GC.
}


TEST(UniqueSubsume) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Handle<String> A = factory->InternalizeUtf8String("A");
  Unique<String> HA(A);

  CHECK(*HA.handle() == *A);
  CHECK_EQ(*A, *HA.handle());

  Unique<Object> HO = HA;  // Here comes the subsumption, boys.

  CheckHashCodeEqual(HA, HO);
  CHECK(HA == HO);
  CHECK_EQ(*HA.handle(), *HO.handle());

  CHECK(HO == HA);
  CHECK_EQ(*HO.handle(), *HA.handle());
}


TEST(UniqueSet_Add) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));

  Zone zone(isolate);

  UniqueSet<String>* set = new(&zone) UniqueSet<String>();

  CHECK_EQ(0, set->size());
  set->Add(A, &zone);
  CHECK_EQ(1, set->size());
  set->Add(A, &zone);
  CHECK_EQ(1, set->size());
  set->Add(B, &zone);
  CHECK_EQ(2, set->size());
  set->Add(C, &zone);
  CHECK_EQ(3, set->size());
  set->Add(C, &zone);
  CHECK_EQ(3, set->size());
  set->Add(B, &zone);
  CHECK_EQ(3, set->size());
  set->Add(A, &zone);
  CHECK_EQ(3, set->size());
}


template <class T>
static void CHECK_SETS(
    UniqueSet<T>* set1, UniqueSet<T>* set2, bool expected) {
  CHECK(set1->Equals(set1));
  CHECK(set2->Equals(set2));
  CHECK(expected == set1->Equals(set2));
  CHECK(expected == set2->Equals(set1));
}


TEST(UniqueSet_Equals) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));

  Zone zone(isolate);

  UniqueSet<String>* set1 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* set2 = new(&zone) UniqueSet<String>();

  CHECK_SETS(set1, set2, true);

  set1->Add(A, &zone);

  CHECK_SETS(set1, set2, false);

  set2->Add(A, &zone);

  CHECK_SETS(set1, set2, true);

  set1->Add(B, &zone);

  CHECK_SETS(set1, set2, false);

  set2->Add(C, &zone);

  CHECK_SETS(set1, set2, false);

  set1->Add(C, &zone);

  CHECK_SETS(set1, set2, false);

  set2->Add(B, &zone);

  CHECK_SETS(set1, set2, true);
}


TEST(UniqueSet_IsSubset1) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));

  Zone zone(isolate);

  UniqueSet<String>* set1 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* set2 = new(&zone) UniqueSet<String>();

  CHECK(set1->IsSubset(set2));
  CHECK(set2->IsSubset(set1));

  set1->Add(A, &zone);

  CHECK(!set1->IsSubset(set2));
  CHECK(set2->IsSubset(set1));

  set2->Add(B, &zone);

  CHECK(!set1->IsSubset(set2));
  CHECK(!set2->IsSubset(set1));

  set2->Add(A, &zone);

  CHECK(set1->IsSubset(set2));
  CHECK(!set2->IsSubset(set1));

  set1->Add(B, &zone);

  CHECK(set1->IsSubset(set2));
  CHECK(set2->IsSubset(set1));
}


TEST(UniqueSet_IsSubset2) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));
  Unique<String> D(factory->InternalizeUtf8String("D"));
  Unique<String> E(factory->InternalizeUtf8String("E"));
  Unique<String> F(factory->InternalizeUtf8String("F"));
  Unique<String> G(factory->InternalizeUtf8String("G"));

  Zone zone(isolate);

  UniqueSet<String>* set1 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* set2 = new(&zone) UniqueSet<String>();

  set1->Add(A, &zone);
  set1->Add(C, &zone);
  set1->Add(E, &zone);

  set2->Add(A, &zone);
  set2->Add(B, &zone);
  set2->Add(C, &zone);
  set2->Add(D, &zone);
  set2->Add(E, &zone);
  set2->Add(F, &zone);

  CHECK(set1->IsSubset(set2));
  CHECK(!set2->IsSubset(set1));

  set1->Add(G, &zone);

  CHECK(!set1->IsSubset(set2));
  CHECK(!set2->IsSubset(set1));
}


template <class T>
static UniqueSet<T>* MakeSet(Zone* zone, int which, Unique<T>* elements) {
  UniqueSet<T>* set = new(zone) UniqueSet<T>();
  for (int i = 0; i < 32; i++) {
    if ((which & (1 << i)) != 0) set->Add(elements[i], zone);
  }
  return set;
}


TEST(UniqueSet_IsSubsetExhaustive) {
  const int kSetSize = 6;

  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Zone zone(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));
  Unique<String> D(factory->InternalizeUtf8String("D"));
  Unique<String> E(factory->InternalizeUtf8String("E"));
  Unique<String> F(factory->InternalizeUtf8String("F"));
  Unique<String> G(factory->InternalizeUtf8String("G"));

  Unique<String> elements[] = {
    A, B, C, D, E, F, G
  };

  // Exhaustively test all sets with <= 6 elements.
  for (int i = 0; i < (1 << kSetSize); i++) {
    for (int j = 0; j < (1 << kSetSize); j++) {
      UniqueSet<String>* set1 = MakeSet(&zone, i, elements);
      UniqueSet<String>* set2 = MakeSet(&zone, j, elements);

      CHECK(((i & j) == i) == set1->IsSubset(set2));
    }
  }
}


TEST(UniqueSet_Intersect1) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));

  Zone zone(isolate);

  UniqueSet<String>* set1 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* set2 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* result;

  CHECK(set1->IsSubset(set2));
  CHECK(set2->IsSubset(set1));

  set1->Add(A, &zone);

  result = set1->Intersect(set2, &zone);

  CHECK_EQ(0, result->size());
  CHECK(set2->Equals(result));

  set2->Add(A, &zone);

  result = set1->Intersect(set2, &zone);

  CHECK_EQ(1, result->size());
  CHECK(set1->Equals(result));
  CHECK(set2->Equals(result));

  set2->Add(B, &zone);
  set2->Add(C, &zone);

  result = set1->Intersect(set2, &zone);

  CHECK_EQ(1, result->size());
  CHECK(set1->Equals(result));
}


TEST(UniqueSet_IntersectExhaustive) {
  const int kSetSize = 6;

  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Zone zone(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));
  Unique<String> D(factory->InternalizeUtf8String("D"));
  Unique<String> E(factory->InternalizeUtf8String("E"));
  Unique<String> F(factory->InternalizeUtf8String("F"));
  Unique<String> G(factory->InternalizeUtf8String("G"));

  Unique<String> elements[] = {
    A, B, C, D, E, F, G
  };

  // Exhaustively test all sets with <= 6 elements.
  for (int i = 0; i < (1 << kSetSize); i++) {
    for (int j = 0; j < (1 << kSetSize); j++) {
      UniqueSet<String>* set1 = MakeSet(&zone, i, elements);
      UniqueSet<String>* set2 = MakeSet(&zone, j, elements);

      UniqueSet<String>* result = set1->Intersect(set2, &zone);
      UniqueSet<String>* expected = MakeSet(&zone, i & j, elements);

      CHECK(result->Equals(expected));
      CHECK(expected->Equals(result));
    }
  }
}


TEST(UniqueSet_Union1) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));

  Zone zone(isolate);

  UniqueSet<String>* set1 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* set2 = new(&zone) UniqueSet<String>();
  UniqueSet<String>* result;

  CHECK(set1->IsSubset(set2));
  CHECK(set2->IsSubset(set1));

  set1->Add(A, &zone);

  result = set1->Union(set2, &zone);

  CHECK_EQ(1, result->size());
  CHECK(set1->Equals(result));

  set2->Add(A, &zone);

  result = set1->Union(set2, &zone);

  CHECK_EQ(1, result->size());
  CHECK(set1->Equals(result));
  CHECK(set2->Equals(result));

  set2->Add(B, &zone);
  set2->Add(C, &zone);

  result = set1->Union(set2, &zone);

  CHECK_EQ(3, result->size());
  CHECK(set2->Equals(result));
}


TEST(UniqueSet_UnionExhaustive) {
  const int kSetSize = 6;

  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Zone zone(isolate);

  Unique<String> A(factory->InternalizeUtf8String("A"));
  Unique<String> B(factory->InternalizeUtf8String("B"));
  Unique<String> C(factory->InternalizeUtf8String("C"));
  Unique<String> D(factory->InternalizeUtf8String("D"));
  Unique<String> E(factory->InternalizeUtf8String("E"));
  Unique<String> F(factory->InternalizeUtf8String("F"));
  Unique<String> G(factory->InternalizeUtf8String("G"));

  Unique<String> elements[] = {
    A, B, C, D, E, F, G
  };

  // Exhaustively test all sets with <= 6 elements.
  for (int i = 0; i < (1 << kSetSize); i++) {
    for (int j = 0; j < (1 << kSetSize); j++) {
      UniqueSet<String>* set1 = MakeSet(&zone, i, elements);
      UniqueSet<String>* set2 = MakeSet(&zone, j, elements);

      UniqueSet<String>* result = set1->Union(set2, &zone);
      UniqueSet<String>* expected = MakeSet(&zone, i | j, elements);

      CHECK(result->Equals(expected));
      CHECK(expected->Equals(result));
    }
  }
}

