// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "api.h"
#include "debug.h"
#include "execution.h"
#include "factory.h"
#include "macro-assembler.h"
#include "objects.h"
#include "global-handles.h"
#include "cctest.h"

using namespace v8::internal;


TEST(ObjectHashTable) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());
  Handle<ObjectHashTable> table = factory->NewObjectHashTable(23);
  Handle<JSObject> a = factory->NewJSArray(7);
  Handle<JSObject> b = factory->NewJSArray(11);
  table = ObjectHashTable::Put(table, a, b);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_EQ(table->Lookup(*a), *b);
  CHECK_EQ(table->Lookup(*b), CcTest::heap()->the_hole_value());

  // Keys still have to be valid after objects were moved.
  CcTest::heap()->CollectGarbage(NEW_SPACE);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_EQ(table->Lookup(*a), *b);
  CHECK_EQ(table->Lookup(*b), CcTest::heap()->the_hole_value());

  // Keys that are overwritten should not change number of elements.
  table = ObjectHashTable::Put(table, a, factory->NewJSArray(13));
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_NE(table->Lookup(*a), *b);

  // Keys mapped to the hole should be removed permanently.
  table = ObjectHashTable::Put(table, a, factory->the_hole_value());
  CHECK_EQ(table->NumberOfElements(), 0);
  CHECK_EQ(table->NumberOfDeletedElements(), 1);
  CHECK_EQ(table->Lookup(*a), CcTest::heap()->the_hole_value());

  // Keys should map back to their respective values and also should get
  // an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    Handle<JSObject> value = factory->NewJSArray(11);
    table = ObjectHashTable::Put(table, key, value);
    CHECK_EQ(table->NumberOfElements(), i + 1);
    CHECK_NE(table->FindEntry(*key), ObjectHashTable::kNotFound);
    CHECK_EQ(table->Lookup(*key), *value);
    CHECK(key->GetIdentityHash()->IsSmi());
  }

  // Keys never added to the map which already have an identity hash
  // code should not be found.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    CHECK(JSReceiver::GetOrCreateIdentityHash(key)->IsSmi());
    CHECK_EQ(table->FindEntry(*key), ObjectHashTable::kNotFound);
    CHECK_EQ(table->Lookup(*key), CcTest::heap()->the_hole_value());
    CHECK(key->GetIdentityHash()->IsSmi());
  }

  // Keys that don't have an identity hash should not be found and also
  // should not get an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    CHECK_EQ(table->Lookup(*key), CcTest::heap()->the_hole_value());
    CHECK_EQ(key->GetIdentityHash(),
             CcTest::heap()->undefined_value());
  }
}


class ObjectHashTableTest: public ObjectHashTable {
 public:
  void insert(int entry, int key, int value) {
    set(EntryToIndex(entry), Smi::FromInt(key));
    set(EntryToIndex(entry) + 1, Smi::FromInt(value));
  }

  int lookup(int key) {
    return Smi::cast(Lookup(Smi::FromInt(key)))->value();
  }

  int capacity() {
    return Capacity();
  }
};


TEST(HashTableRehash) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());
  // Test almost filled table.
  {
    Handle<ObjectHashTable> table = factory->NewObjectHashTable(100);
    ObjectHashTableTest* t = reinterpret_cast<ObjectHashTableTest*>(*table);
    int capacity = t->capacity();
    for (int i = 0; i < capacity - 1; i++) {
      t->insert(i, i * i, i);
    }
    t->Rehash(Smi::FromInt(0));
    for (int i = 0; i < capacity - 1; i++) {
      CHECK_EQ(i, t->lookup(i * i));
    }
  }
  // Test half-filled table.
  {
    Handle<ObjectHashTable> table = factory->NewObjectHashTable(100);
    ObjectHashTableTest* t = reinterpret_cast<ObjectHashTableTest*>(*table);
    int capacity = t->capacity();
    for (int i = 0; i < capacity / 2; i++) {
      t->insert(i, i * i, i);
    }
    t->Rehash(Smi::FromInt(0));
    for (int i = 0; i < capacity / 2; i++) {
      CHECK_EQ(i, t->lookup(i * i));
    }
  }
}


#ifdef DEBUG
TEST(ObjectHashSetCausesGC) {
  i::FLAG_stress_compaction = false;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());
  Handle<ObjectHashSet> table = factory->NewObjectHashSet(1);
  Handle<JSObject> key = factory->NewJSArray(0);
  v8::Handle<v8::Object> key_obj = v8::Utils::ToLocal(key);

  // Force allocation of hash table backing store for hidden properties.
  key_obj->SetHiddenValue(v8_str("key 1"), v8_str("val 1"));
  key_obj->SetHiddenValue(v8_str("key 2"), v8_str("val 2"));
  key_obj->SetHiddenValue(v8_str("key 3"), v8_str("val 3"));

  // Simulate a full heap so that generating an identity hash code
  // in subsequent calls will request GC.
  SimulateFullSpace(CcTest::heap()->new_space());
  SimulateFullSpace(CcTest::heap()->old_pointer_space());

  // Calling Contains() should not cause GC ever.
  int gc_count = isolate->heap()->gc_count();
  CHECK(!table->Contains(*key));
  CHECK(gc_count == isolate->heap()->gc_count());

  // Calling Remove() will not cause GC in this case.
  table = ObjectHashSet::Remove(table, key);
  CHECK(gc_count == isolate->heap()->gc_count());

  // Calling Add() should cause GC.
  table = ObjectHashSet::Add(table, key);
  CHECK(gc_count < isolate->heap()->gc_count());
}
#endif


#ifdef DEBUG
TEST(ObjectHashTableCausesGC) {
  i::FLAG_stress_compaction = false;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());
  Handle<ObjectHashTable> table = factory->NewObjectHashTable(1);
  Handle<JSObject> key = factory->NewJSArray(0);
  v8::Handle<v8::Object> key_obj = v8::Utils::ToLocal(key);

  // Force allocation of hash table backing store for hidden properties.
  key_obj->SetHiddenValue(v8_str("key 1"), v8_str("val 1"));
  key_obj->SetHiddenValue(v8_str("key 2"), v8_str("val 2"));
  key_obj->SetHiddenValue(v8_str("key 3"), v8_str("val 3"));

  // Simulate a full heap so that generating an identity hash code
  // in subsequent calls will request GC.
  SimulateFullSpace(CcTest::heap()->new_space());
  SimulateFullSpace(CcTest::heap()->old_pointer_space());

  // Calling Lookup() should not cause GC ever.
  CHECK(table->Lookup(*key)->IsTheHole());

  // Calling Put() should request GC by returning a failure.
  int gc_count = isolate->heap()->gc_count();
  ObjectHashTable::Put(table, key, key);
  CHECK(gc_count < isolate->heap()->gc_count());
}
#endif
