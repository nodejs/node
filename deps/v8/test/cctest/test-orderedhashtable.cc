// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <utility>
#include "src/v8.h"

#include "src/objects-inl.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_orderedhashtable {

static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
}

void CopyHashCode(Handle<JSReceiver> from, Handle<JSReceiver> to) {
  int hash = Smi::ToInt(from->GetHash());
  to->SetIdentityHash(hash);
}

void Verify(Isolate* isolate, Handle<HeapObject> obj) {
#if VERIFY_HEAP
  obj->ObjectVerify(isolate);
#endif
}

TEST(SmallOrderedHashSetInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashSet> set = factory->NewSmallOrderedHashSet();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());

  // Add a new key.
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  CHECK(!set->HasKey(isolate, key1));
  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));

  // Add existing key.
  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  CHECK(!set->HasKey(isolate, key2));
  set = SmallOrderedHashSet::Add(isolate, set, key2).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));

  set = SmallOrderedHashSet::Add(isolate, set, key2).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!set->HasKey(isolate, key3));
  set = SmallOrderedHashSet::Add(isolate, set, key3).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(3, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));

  set = SmallOrderedHashSet::Add(isolate, set, key3).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(3, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));

  Handle<Object> key4 = factory->NewHeapNumber(42.0);
  CHECK(!set->HasKey(isolate, key4));
  set = SmallOrderedHashSet::Add(isolate, set, key4).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(4, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));
  CHECK(set->HasKey(isolate, key4));

  set = SmallOrderedHashSet::Add(isolate, set, key4).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(4, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));
  CHECK(set->HasKey(isolate, key4));
}

TEST(SmallOrderedHashMapInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashMap> map = factory->NewSmallOrderedHashMap();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());

  // Add a new key.
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  Handle<Smi> value1(Smi::FromInt(1), isolate);
  CHECK(!map->HasKey(isolate, key1));
  map = SmallOrderedHashMap::Add(isolate, map, key1, value1).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));

  // Add existing key.
  map = SmallOrderedHashMap::Add(isolate, map, key1, value1).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  Handle<String> value = factory->NewStringFromAsciiChecked("foo");
  CHECK(!map->HasKey(isolate, key2));
  map = SmallOrderedHashMap::Add(isolate, map, key2, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));

  map = SmallOrderedHashMap::Add(isolate, map, key2, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!map->HasKey(isolate, key3));
  map = SmallOrderedHashMap::Add(isolate, map, key3, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(3, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));

  map = SmallOrderedHashMap::Add(isolate, map, key3, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(3, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));

  Handle<Object> key4 = factory->NewHeapNumber(42.0);
  CHECK(!map->HasKey(isolate, key4));
  map = SmallOrderedHashMap::Add(isolate, map, key4, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(4, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));
  CHECK(map->HasKey(isolate, key4));

  map = SmallOrderedHashMap::Add(isolate, map, key4, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(4, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));
  CHECK(map->HasKey(isolate, key4));
}

TEST(SmallOrderedHashSetDuplicateHashCode) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashSet> set = factory->NewSmallOrderedHashSet();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  set = SmallOrderedHashSet::Add(isolate, set, key2).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
}

TEST(SmallOrderedHashMapDuplicateHashCode) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashMap> map = factory->NewSmallOrderedHashMap();
  Handle<JSObject> value = factory->NewJSObjectWithNullProto();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  map = SmallOrderedHashMap::Add(isolate, map, key1, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  CHECK(!key1->SameValue(*key2));
  Object* hash1 = key1->GetHash();
  Object* hash2 = key2->GetHash();
  CHECK_EQ(hash1, hash2);

  map = SmallOrderedHashMap::Add(isolate, map, key2, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
}

TEST(SmallOrderedHashSetGrow) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashSet> set = factory->NewSmallOrderedHashSet();
  std::vector<Handle<Object>> keys;
  for (int i = 0; i < 254; i++) {
    Handle<Smi> key(Smi::FromInt(i), isolate);
    keys.push_back(key);
  }

  for (size_t i = 0; i < 4; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 4; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(4, set->NumberOfElements());
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);

  for (size_t i = 4; i < 8; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 8; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(8, set->NumberOfElements());
  CHECK_EQ(4, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);

  for (size_t i = 8; i < 16; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 16; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(16, set->NumberOfElements());
  CHECK_EQ(8, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);

  for (size_t i = 16; i < 32; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 32; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(32, set->NumberOfElements());
  CHECK_EQ(16, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);

  for (size_t i = 32; i < 64; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 64; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(64, set->NumberOfElements());
  CHECK_EQ(32, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);

  for (size_t i = 64; i < 128; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 128; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(128, set->NumberOfElements());
  CHECK_EQ(64, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);

  for (size_t i = 128; i < 254; i++) {
    set = SmallOrderedHashSet::Add(isolate, set, keys[i]).ToHandleChecked();
    Verify(isolate, set);
  }

  for (size_t i = 0; i < 254; i++) {
    CHECK(set->HasKey(isolate, keys[i]));
    Verify(isolate, set);
  }

  CHECK_EQ(254, set->NumberOfElements());
  CHECK_EQ(127, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  Verify(isolate, set);
}

TEST(SmallOrderedHashMapGrow) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashMap> map = factory->NewSmallOrderedHashMap();
  std::vector<Handle<Object>> keys;
  for (int i = 0; i < 254; i++) {
    Handle<Smi> key(Smi::FromInt(i), isolate);
    keys.push_back(key);
  }

  for (size_t i = 0; i < 4; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 4; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(4, map->NumberOfElements());
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);

  for (size_t i = 4; i < 8; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 8; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(8, map->NumberOfElements());
  CHECK_EQ(4, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);

  for (size_t i = 8; i < 16; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 16; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(16, map->NumberOfElements());
  CHECK_EQ(8, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);

  for (size_t i = 16; i < 32; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 32; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(32, map->NumberOfElements());
  CHECK_EQ(16, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);

  for (size_t i = 32; i < 64; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 64; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(64, map->NumberOfElements());
  CHECK_EQ(32, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);

  for (size_t i = 64; i < 128; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 128; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(128, map->NumberOfElements());
  CHECK_EQ(64, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);

  for (size_t i = 128; i < 254; i++) {
    map = SmallOrderedHashMap::Add(isolate, map, keys[i], keys[i])
              .ToHandleChecked();
    Verify(isolate, map);
  }

  for (size_t i = 0; i < 254; i++) {
    CHECK(map->HasKey(isolate, keys[i]));
    Verify(isolate, map);
  }

  CHECK_EQ(254, map->NumberOfElements());
  CHECK_EQ(127, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  Verify(isolate, map);
}

TEST(OrderedHashTableInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedHashMap> map = factory->NewOrderedHashMap();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());

  // Add a new key.
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  Handle<Smi> value1(Smi::FromInt(1), isolate);
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));
  map = OrderedHashMap::Add(isolate, map, key1, value1);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));

  // Add existing key.
  map = OrderedHashMap::Add(isolate, map, key1, value1);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  Handle<String> value = factory->NewStringFromAsciiChecked("bar");
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  map = OrderedHashMap::Add(isolate, map, key2, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));

  map = OrderedHashMap::Add(isolate, map, key2, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key3));
  map = OrderedHashMap::Add(isolate, map, key3, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(3, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));

  map = OrderedHashMap::Add(isolate, map, key3, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(3, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));

  Handle<Object> key4 = factory->NewHeapNumber(42.0);
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key4));
  map = OrderedHashMap::Add(isolate, map, key4, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(4, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key4));

  map = OrderedHashMap::Add(isolate, map, key4, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(4, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key4));
}

TEST(OrderedHashMapDuplicateHashCode) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedHashMap> map = factory->NewOrderedHashMap();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  Handle<JSObject> value = factory->NewJSObjectWithNullProto();
  map = OrderedHashMap::Add(isolate, map, key1, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  map = OrderedHashMap::Add(isolate, map, key2, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
}

TEST(OrderedHashMapDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<Smi> value1(Smi::FromInt(1), isolate);
  Handle<String> value = factory->NewStringFromAsciiChecked("bar");

  Handle<OrderedHashMap> map = factory->NewOrderedHashMap();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());

  // Delete from an empty hash table
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  CHECK(!OrderedHashMap::Delete(isolate, *map, *key1));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));

  map = OrderedHashMap::Add(isolate, map, key1, value1);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));

  // Delete single existing key
  CHECK(OrderedHashMap::Delete(isolate, *map, *key1));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));

  map = OrderedHashMap::Add(isolate, map, key1, value1);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  map = OrderedHashMap::Add(isolate, map, key2, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key3));
  map = OrderedHashMap::Add(isolate, map, key3, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(3, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));

  // Delete multiple existing keys
  CHECK(OrderedHashMap::Delete(isolate, *map, *key1));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK_EQ(2, map->NumberOfDeletedElements());
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));

  CHECK(OrderedHashMap::Delete(isolate, *map, *key2));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(3, map->NumberOfDeletedElements());
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key3));

  CHECK(OrderedHashMap::Delete(isolate, *map, *key3));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(4, map->NumberOfDeletedElements());
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key3));

  // Delete non existent key from non new hash table
  CHECK(!OrderedHashMap::Delete(isolate, *map, *key3));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(4, map->NumberOfDeletedElements());
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key3));

  // Delete non existent key from non empty hash table
  map = OrderedHashMap::Shrink(isolate, map);
  map = OrderedHashMap::Add(isolate, map, key1, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key3));
  CHECK(!OrderedHashMap::Delete(isolate, *map, *key2));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key3));
}

TEST(SmallOrderedHashMapDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<Smi> value1(Smi::FromInt(1), isolate);
  Handle<String> value = factory->NewStringFromAsciiChecked("bar");

  Handle<SmallOrderedHashMap> map = factory->NewSmallOrderedHashMap();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());

  // Delete from an empty hash table
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  CHECK(!SmallOrderedHashMap::Delete(isolate, *map, *key1));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(!map->HasKey(isolate, key1));

  map = SmallOrderedHashMap::Add(isolate, map, key1, value1).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));

  // Delete single existing key
  CHECK(SmallOrderedHashMap::Delete(isolate, *map, *key1));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(!map->HasKey(isolate, key1));

  map = SmallOrderedHashMap::Add(isolate, map, key1, value1).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  CHECK(!map->HasKey(isolate, key2));
  map = SmallOrderedHashMap::Add(isolate, map, key2, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!map->HasKey(isolate, key3));
  map = SmallOrderedHashMap::Add(isolate, map, key3, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(3, map->NumberOfElements());
  CHECK_EQ(1, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));

  // Delete multiple existing keys
  CHECK(SmallOrderedHashMap::Delete(isolate, *map, *key1));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(2, map->NumberOfElements());
  CHECK_EQ(2, map->NumberOfDeletedElements());
  CHECK(!map->HasKey(isolate, key1));
  CHECK(map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));

  CHECK(SmallOrderedHashMap::Delete(isolate, *map, *key2));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(3, map->NumberOfDeletedElements());
  CHECK(!map->HasKey(isolate, key1));
  CHECK(!map->HasKey(isolate, key2));
  CHECK(map->HasKey(isolate, key3));

  CHECK(SmallOrderedHashMap::Delete(isolate, *map, *key3));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(4, map->NumberOfDeletedElements());
  CHECK(!map->HasKey(isolate, key1));
  CHECK(!map->HasKey(isolate, key2));
  CHECK(!map->HasKey(isolate, key3));

  // Delete non existent key from non new hash table
  CHECK(!SmallOrderedHashMap::Delete(isolate, *map, *key3));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(0, map->NumberOfElements());
  CHECK_EQ(4, map->NumberOfDeletedElements());
  CHECK(!map->HasKey(isolate, key1));
  CHECK(!map->HasKey(isolate, key2));
  CHECK(!map->HasKey(isolate, key3));

  // Delete non existent key from non empty hash table
  map = SmallOrderedHashMap::Add(isolate, map, key1, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(!map->HasKey(isolate, key2));
  CHECK(!map->HasKey(isolate, key3));
  CHECK(!SmallOrderedHashMap::Delete(isolate, *map, *key2));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(!map->HasKey(isolate, key2));
  CHECK(!map->HasKey(isolate, key3));
}

TEST(OrderedHashMapDuplicateHashCodeDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedHashMap> map = factory->NewOrderedHashMap();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  Handle<JSObject> value = factory->NewJSObjectWithNullProto();
  map = OrderedHashMap::Add(isolate, map, key1, value);
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  // We shouldn't be able to delete the key!
  CHECK(!OrderedHashMap::Delete(isolate, *map, *key2));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(OrderedHashMap::HasKey(isolate, *map, *key1));
  CHECK(!OrderedHashMap::HasKey(isolate, *map, *key2));
}

TEST(SmallOrderedHashMapDuplicateHashCodeDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashMap> map = factory->NewSmallOrderedHashMap();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  Handle<JSObject> value = factory->NewJSObjectWithNullProto();
  map = SmallOrderedHashMap::Add(isolate, map, key1, value).ToHandleChecked();
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  // We shouldn't be able to delete the key!
  CHECK(!SmallOrderedHashMap::Delete(isolate, *map, *key2));
  Verify(isolate, map);
  CHECK_EQ(2, map->NumberOfBuckets());
  CHECK_EQ(1, map->NumberOfElements());
  CHECK_EQ(0, map->NumberOfDeletedElements());
  CHECK(map->HasKey(isolate, key1));
  CHECK(!map->HasKey(isolate, key2));
}

TEST(OrderedHashSetDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedHashSet> set = factory->NewOrderedHashSet();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());

  // Delete from an empty hash table
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  CHECK(!OrderedHashSet::Delete(isolate, *set, *key1));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key1));

  set = OrderedHashSet::Add(isolate, set, key1);
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));

  // Delete single existing key
  CHECK(OrderedHashSet::Delete(isolate, *set, *key1));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key1));

  set = OrderedHashSet::Add(isolate, set, key1);
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
  set = OrderedHashSet::Add(isolate, set, key2);
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key3));
  set = OrderedHashSet::Add(isolate, set, key3);
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(3, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key3));

  // Delete multiple existing keys
  CHECK(OrderedHashSet::Delete(isolate, *set, *key1));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK_EQ(2, set->NumberOfDeletedElements());
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key3));

  CHECK(OrderedHashSet::Delete(isolate, *set, *key2));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(3, set->NumberOfDeletedElements());
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key3));

  CHECK(OrderedHashSet::Delete(isolate, *set, *key3));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(4, set->NumberOfDeletedElements());
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key3));

  // Delete non existent key from non new hash table
  CHECK(!OrderedHashSet::Delete(isolate, *set, *key3));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(4, set->NumberOfDeletedElements());
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key3));

  // Delete non existent key from non empty hash table
  set = OrderedHashSet::Shrink(isolate, set);
  set = OrderedHashSet::Add(isolate, set, key1);
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key3));
  CHECK(!OrderedHashSet::Delete(isolate, *set, *key2));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key3));
}

TEST(SmallOrderedHashSetDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashSet> set = factory->NewSmallOrderedHashSet();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());

  // Delete from an empty hash table
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  CHECK(!SmallOrderedHashSet::Delete(isolate, *set, *key1));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(!set->HasKey(isolate, key1));

  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));

  // Delete single existing key
  CHECK(SmallOrderedHashSet::Delete(isolate, *set, *key1));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(!set->HasKey(isolate, key1));

  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));

  Handle<String> key2 = factory->NewStringFromAsciiChecked("foo");
  CHECK(!set->HasKey(isolate, key2));
  set = SmallOrderedHashSet::Add(isolate, set, key2).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key2));

  Handle<Symbol> key3 = factory->NewSymbol();
  CHECK(!set->HasKey(isolate, key3));
  set = SmallOrderedHashSet::Add(isolate, set, key3).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(3, set->NumberOfElements());
  CHECK_EQ(1, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));

  // Delete multiple existing keys
  CHECK(SmallOrderedHashSet::Delete(isolate, *set, *key1));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(2, set->NumberOfElements());
  CHECK_EQ(2, set->NumberOfDeletedElements());
  CHECK(!set->HasKey(isolate, key1));
  CHECK(set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));

  CHECK(SmallOrderedHashSet::Delete(isolate, *set, *key2));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(3, set->NumberOfDeletedElements());
  CHECK(!set->HasKey(isolate, key1));
  CHECK(!set->HasKey(isolate, key2));
  CHECK(set->HasKey(isolate, key3));

  CHECK(SmallOrderedHashSet::Delete(isolate, *set, *key3));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(4, set->NumberOfDeletedElements());
  CHECK(!set->HasKey(isolate, key1));
  CHECK(!set->HasKey(isolate, key2));
  CHECK(!set->HasKey(isolate, key3));

  // Delete non existent key from non new hash table
  CHECK(!SmallOrderedHashSet::Delete(isolate, *set, *key3));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(0, set->NumberOfElements());
  CHECK_EQ(4, set->NumberOfDeletedElements());
  CHECK(!set->HasKey(isolate, key1));
  CHECK(!set->HasKey(isolate, key2));
  CHECK(!set->HasKey(isolate, key3));

  // Delete non existent key from non empty hash table
  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(!set->HasKey(isolate, key2));
  CHECK(!set->HasKey(isolate, key3));
  CHECK(!SmallOrderedHashSet::Delete(isolate, *set, *key2));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(!set->HasKey(isolate, key2));
  CHECK(!set->HasKey(isolate, key3));
}

TEST(OrderedHashSetDuplicateHashCodeDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedHashSet> set = factory->NewOrderedHashSet();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  set = OrderedHashSet::Add(isolate, set, key1);
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  // We shouldn't be able to delete the key!
  CHECK(!OrderedHashSet::Delete(isolate, *set, *key2));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(OrderedHashSet::HasKey(isolate, *set, *key1));
  CHECK(!OrderedHashSet::HasKey(isolate, *set, *key2));
}

TEST(SmallOrderedHashSetDuplicateHashCodeDeletion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedHashSet> set = factory->NewSmallOrderedHashSet();
  Handle<JSObject> key1 = factory->NewJSObjectWithNullProto();
  set = SmallOrderedHashSet::Add(isolate, set, key1).ToHandleChecked();
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));

  Handle<JSObject> key2 = factory->NewJSObjectWithNullProto();
  CopyHashCode(key1, key2);

  // We shouldn't be able to delete the key!
  CHECK(!SmallOrderedHashSet::Delete(isolate, *set, *key2));
  Verify(isolate, set);
  CHECK_EQ(2, set->NumberOfBuckets());
  CHECK_EQ(1, set->NumberOfElements());
  CHECK_EQ(0, set->NumberOfDeletedElements());
  CHECK(set->HasKey(isolate, key1));
  CHECK(!set->HasKey(isolate, key2));
}

TEST(OrderedHashSetHandlerInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  HandleScope scope(isolate);

  Handle<HeapObject> set = OrderedHashSetHandler::Allocate(isolate, 4);
  Verify(isolate, set);

  // Add a new key.
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  CHECK(!OrderedHashSetHandler::HasKey(isolate, set, key1));
  set = OrderedHashSetHandler::Add(isolate, set, key1);
  Verify(isolate, set);
  CHECK(OrderedHashSetHandler::HasKey(isolate, set, key1));

  // Add existing key.
  set = OrderedHashSetHandler::Add(isolate, set, key1);
  Verify(isolate, set);
  CHECK(OrderedHashSetHandler::HasKey(isolate, set, key1));
  CHECK(SmallOrderedHashSet::Is(set));

  for (int i = 0; i < 1024; i++) {
    Handle<Smi> key_i(Smi::FromInt(i), isolate);
    set = OrderedHashSetHandler::Add(isolate, set, key_i);
    Verify(isolate, set);
    for (int j = 0; j <= i; j++) {
      Handle<Smi> key_j(Smi::FromInt(j), isolate);
      CHECK(OrderedHashSetHandler::HasKey(isolate, set, key_j));
    }
  }
  CHECK(OrderedHashSet::Is(set));
}

TEST(OrderedHashMapHandlerInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  HandleScope scope(isolate);

  Handle<HeapObject> map = OrderedHashMapHandler::Allocate(isolate, 4);
  Verify(isolate, map);

  // Add a new key.
  Handle<Smi> key1(Smi::FromInt(1), isolate);
  Handle<Smi> value1(Smi::FromInt(1), isolate);
  CHECK(!OrderedHashMapHandler::HasKey(isolate, map, key1));
  map = OrderedHashMapHandler::Add(isolate, map, key1, value1);
  Verify(isolate, map);
  CHECK(OrderedHashMapHandler::HasKey(isolate, map, key1));

  // Add existing key.
  map = OrderedHashMapHandler::Add(isolate, map, key1, value1);
  Verify(isolate, map);
  CHECK(OrderedHashMapHandler::HasKey(isolate, map, key1));
  CHECK(SmallOrderedHashMap::Is(map));
  for (int i = 0; i < 1024; i++) {
    Handle<Smi> key_i(Smi::FromInt(i), isolate);
    Handle<Smi> value_i(Smi::FromInt(i), isolate);
    map = OrderedHashMapHandler::Add(isolate, map, key_i, value_i);
    Verify(isolate, map);
    for (int j = 0; j <= i; j++) {
      Handle<Smi> key_j(Smi::FromInt(j), isolate);
      CHECK(OrderedHashMapHandler::HasKey(isolate, map, key_j));
    }
  }
  CHECK(OrderedHashMap::Is(map));
}

}  // namespace test_orderedhashtable
}  // namespace internal
}  // namespace v8
