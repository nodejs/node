// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <utility>
#include "src/init/v8.h"

#include "src/objects/objects-inl.h"
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
  Object hash1 = key1->GetHash();
  Object hash2 = key2->GetHash();
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

TEST(OrderedNameDictionaryInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();
  dict = OrderedNameDictionary::Add(isolate, dict, key1, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());

  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key2));
  dict = OrderedNameDictionary::Add(isolate, dict, key2, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));
}

TEST(OrderedNameDictionaryFindEntry) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  PropertyDetails details = PropertyDetails::Empty();
  dict = OrderedNameDictionary::Add(isolate, dict, key1, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());

  int entry = dict->FindEntry(isolate, *key1);
  CHECK_EQ(entry, 0);
  CHECK_NE(entry, OrderedNameDictionary::kNotFound);

  Handle<Symbol> key2 = factory->NewSymbol();
  dict = OrderedNameDictionary::Add(isolate, dict, key2, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());

  entry = dict->FindEntry(isolate, *key1);
  CHECK_NE(entry, OrderedNameDictionary::kNotFound);
  CHECK_EQ(entry, 0);

  entry = dict->FindEntry(isolate, *key2);
  CHECK_NE(entry, OrderedNameDictionary::kNotFound);
  CHECK_EQ(entry, 1);
}

TEST(OrderedNameDictionaryValueAtAndValueAtPut) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();
  dict = OrderedNameDictionary::Add(isolate, dict, key1, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  int entry = dict->FindEntry(isolate, *key1);
  Handle<Object> found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *value);

  // Change the value
  Handle<String> other_value = isolate->factory()->InternalizeUtf8String("baz");
  dict->ValueAtPut(entry, *other_value);

  entry = dict->FindEntry(isolate, *key1);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key2));
  dict = OrderedNameDictionary::Add(isolate, dict, key2, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));

  entry = dict->FindEntry(isolate, *key1);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);

  entry = dict->FindEntry(isolate, *key2);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *value);

  // Change the value
  dict->ValueAtPut(entry, *other_value);

  entry = dict->FindEntry(isolate, *key1);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);
}

TEST(OrderedNameDictionaryDetailsAtAndDetailsAtPut) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();
  dict = OrderedNameDictionary::Add(isolate, dict, key1, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  int entry = dict->FindEntry(isolate, *key1);
  PropertyDetails found = dict->DetailsAt(entry);
  CHECK_EQ(PropertyDetails::Empty().AsSmi(), found.AsSmi());

  PropertyDetails other =
      PropertyDetails(kAccessor, READ_ONLY, PropertyCellType::kNoCell);
  dict->DetailsAtPut(entry, other);

  found = dict->DetailsAt(entry);
  CHECK_NE(PropertyDetails::Empty().AsSmi(), found.AsSmi());
  CHECK_EQ(other.AsSmi(), found.AsSmi());

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key2));
  dict = OrderedNameDictionary::Add(isolate, dict, key2, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));

  entry = dict->FindEntry(isolate, *key1);
  found = dict->DetailsAt(entry);
  CHECK_EQ(other.AsSmi(), found.AsSmi());
  CHECK_NE(PropertyDetails::Empty().AsSmi(), found.AsSmi());

  entry = dict->FindEntry(isolate, *key2);
  dict->DetailsAtPut(entry, other);

  found = dict->DetailsAt(entry);
  CHECK_NE(PropertyDetails::Empty().AsSmi(), found.AsSmi());
  CHECK_EQ(other.AsSmi(), found.AsSmi());
}

TEST(SmallOrderedNameDictionaryInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key1, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key2));
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key2, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));
}

TEST(SmallOrderedNameDictionaryInsertionMax) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  PropertyDetails details = PropertyDetails::Empty();

  char buf[10];
  for (int i = 0; i < SmallOrderedNameDictionary::kMaxCapacity; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    Handle<String> key = isolate->factory()->InternalizeUtf8String(buf);
    dict = SmallOrderedNameDictionary::Add(isolate, dict, key, value, details)
               .ToHandleChecked();
    Verify(isolate, dict);
  }

  CHECK_EQ(SmallOrderedNameDictionary::kMaxCapacity /
               SmallOrderedNameDictionary::kLoadFactor,
           dict->NumberOfBuckets());
  CHECK_EQ(SmallOrderedNameDictionary::kMaxCapacity, dict->NumberOfElements());

  // This should overflow and fail.
  CHECK(SmallOrderedNameDictionary::Add(isolate, dict, value, value, details)
            .is_null());
}

TEST(SmallOrderedNameDictionaryFindEntry) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();

  dict = SmallOrderedNameDictionary::Add(isolate, dict, key1, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  int entry = dict->FindEntry(isolate, *key1);
  CHECK_NE(entry, OrderedNameDictionary::kNotFound);

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key2));
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key2, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());

  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));
}

TEST(SmallOrderedNameDictionaryValueAtAndValueAtPut) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key1, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  int entry = dict->FindEntry(isolate, *key1);
  Handle<Object> found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *value);

  // Change the value
  Handle<String> other_value = isolate->factory()->InternalizeUtf8String("baz");
  dict->ValueAtPut(entry, *other_value);

  entry = dict->FindEntry(isolate, *key1);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key2));
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key2, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));

  entry = dict->FindEntry(isolate, *key1);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);

  entry = dict->FindEntry(isolate, *key2);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *value);

  // Change the value
  dict->ValueAtPut(entry, *other_value);

  entry = dict->FindEntry(isolate, *key1);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);
}

TEST(SmallOrderedNameDictionaryDetailsAtAndDetailsAtPut) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key1 = isolate->factory()->InternalizeUtf8String("foo");
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key1));
  PropertyDetails details = PropertyDetails::Empty();
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key1, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));

  int entry = dict->FindEntry(isolate, *key1);
  PropertyDetails found = dict->DetailsAt(entry);
  CHECK_EQ(PropertyDetails::Empty().AsSmi(), found.AsSmi());

  PropertyDetails other =
      PropertyDetails(kAccessor, READ_ONLY, PropertyCellType::kNoCell);
  dict->DetailsAtPut(entry, other);

  found = dict->DetailsAt(entry);
  CHECK_NE(PropertyDetails::Empty().AsSmi(), found.AsSmi());
  CHECK_EQ(other.AsSmi(), found.AsSmi());

  Handle<Symbol> key2 = factory->NewSymbol();
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key2));
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key2, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(2, dict->NumberOfElements());
  CHECK_EQ(0, dict->FindEntry(isolate, *key1));
  CHECK_EQ(1, dict->FindEntry(isolate, *key2));

  entry = dict->FindEntry(isolate, *key1);
  found = dict->DetailsAt(entry);
  CHECK_NE(PropertyDetails::Empty().AsSmi(), found.AsSmi());
  CHECK_EQ(other.AsSmi(), found.AsSmi());

  entry = dict->FindEntry(isolate, *key2);
  dict->DetailsAtPut(entry, other);

  found = dict->DetailsAt(entry);
  CHECK_NE(PropertyDetails::Empty().AsSmi(), found.AsSmi());
  CHECK_EQ(other.AsSmi(), found.AsSmi());
}

TEST(SmallOrderedNameDictionarySetAndMigrateHash) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  PropertyDetails details = PropertyDetails::Empty();

  CHECK_EQ(PropertyArray::kNoHashSentinel, dict->Hash());
  dict->SetHash(100);
  CHECK_EQ(100, dict->Hash());

  char buf[10];
  for (int i = 0; i < SmallOrderedNameDictionary::kMaxCapacity; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    Handle<String> key = isolate->factory()->InternalizeUtf8String(buf);
    dict = SmallOrderedNameDictionary::Add(isolate, dict, key, value, details)
               .ToHandleChecked();
    Verify(isolate, dict);
    CHECK_EQ(100, dict->Hash());
  }
}

TEST(OrderedNameDictionarySetAndMigrateHash) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  PropertyDetails details = PropertyDetails::Empty();

  CHECK_EQ(PropertyArray::kNoHashSentinel, dict->Hash());
  dict->SetHash(100);
  CHECK_EQ(100, dict->Hash());

  char buf[10];
  for (int i = 0; i <= 1024; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    Handle<String> key = isolate->factory()->InternalizeUtf8String(buf);
    dict = OrderedNameDictionary::Add(isolate, dict, key, value, details);
    Verify(isolate, dict);
    CHECK_EQ(100, dict->Hash());
  }
}

TEST(OrderedNameDictionaryHandlerInsertion) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  HandleScope scope(isolate);

  Handle<HeapObject> table = OrderedNameDictionaryHandler::Allocate(isolate, 4);
  CHECK(table->IsSmallOrderedNameDictionary());
  Verify(isolate, table);

  // Add a new key.
  Handle<String> value = isolate->factory()->InternalizeUtf8String("bar");
  Handle<String> key = isolate->factory()->InternalizeUtf8String("foo");
  PropertyDetails details = PropertyDetails::Empty();

  table =
      OrderedNameDictionaryHandler::Add(isolate, table, key, value, details);
  DCHECK(key->IsUniqueName());
  Verify(isolate, table);
  CHECK(table->IsSmallOrderedNameDictionary());
  CHECK_NE(OrderedNameDictionaryHandler::kNotFound,
           OrderedNameDictionaryHandler::FindEntry(isolate, *table, *key));

  char buf[10];
  for (int i = 0; i < 1024; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    key = isolate->factory()->InternalizeUtf8String(buf);
    table =
        OrderedNameDictionaryHandler::Add(isolate, table, key, value, details);
    DCHECK(key->IsUniqueName());
    Verify(isolate, table);

    for (int j = 0; j <= i; j++) {
      CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", j));
      Handle<Name> key_j = isolate->factory()->InternalizeUtf8String(buf);
      CHECK_NE(
          OrderedNameDictionaryHandler::kNotFound,
          OrderedNameDictionaryHandler::FindEntry(isolate, *table, *key_j));
    }

    for (int j = i + 1; j < 1024; j++) {
      CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", j));
      Handle<Name> key_j = isolate->factory()->InternalizeUtf8String(buf);
      CHECK_EQ(
          OrderedNameDictionaryHandler::kNotFound,
          OrderedNameDictionaryHandler::FindEntry(isolate, *table, *key_j));
    }
  }

  CHECK(table->IsOrderedNameDictionary());
}

TEST(OrderedNameDictionarySetEntry) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());
  CHECK_EQ(0, dict->NumberOfDeletedElements());

  Handle<String> key = factory->InternalizeUtf8String("foo");
  Handle<String> value = factory->InternalizeUtf8String("bar");
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key));
  PropertyDetails details = PropertyDetails::Empty();
  dict = OrderedNameDictionary::Add(isolate, dict, key, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());

  int entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(0, entry);
  Handle<Object> found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *value);

  // Change the value
  Handle<String> other_value = isolate->factory()->InternalizeUtf8String("baz");
  PropertyDetails other_details =
      PropertyDetails(kAccessor, READ_ONLY, PropertyCellType::kNoCell);
  dict->SetEntry(isolate, entry, *key, *other_value, other_details);

  entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(0, entry);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);
  found = handle(dict->KeyAt(entry), isolate);
  CHECK_EQ(*found, *key);
  PropertyDetails found_details = dict->DetailsAt(entry);
  CHECK_EQ(found_details.AsSmi(), other_details.AsSmi());
}

TEST(SmallOrderedNameDictionarySetEntry) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key = factory->InternalizeUtf8String("foo");
  Handle<String> value = factory->InternalizeUtf8String("bar");
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key));
  PropertyDetails details = PropertyDetails::Empty();
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->NumberOfDeletedElements());

  int entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(0, entry);
  Handle<Object> found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *value);

  // Change the value
  Handle<String> other_value = factory->InternalizeUtf8String("baz");
  PropertyDetails other_details =
      PropertyDetails(kAccessor, READ_ONLY, PropertyCellType::kNoCell);
  dict->SetEntry(isolate, entry, *key, *other_value, other_details);

  entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(0, entry);
  found = handle(dict->ValueAt(entry), isolate);
  CHECK_EQ(*found, *other_value);
  found = handle(dict->KeyAt(entry), isolate);
  CHECK_EQ(*found, *key);
  PropertyDetails found_details = dict->DetailsAt(entry);
  CHECK_EQ(found_details.AsSmi(), other_details.AsSmi());
}

TEST(OrderedNameDictionaryDeleteEntry) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<OrderedNameDictionary> dict = factory->NewOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key = factory->InternalizeUtf8String("foo");
  Handle<String> value = factory->InternalizeUtf8String("bar");
  CHECK_EQ(OrderedNameDictionary::kNotFound, dict->FindEntry(isolate, *key));
  PropertyDetails details = PropertyDetails::Empty();
  dict = OrderedNameDictionary::Add(isolate, dict, key, value, details);
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->NumberOfDeletedElements());

  int entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(0, entry);
  dict = OrderedNameDictionary::DeleteEntry(isolate, dict, entry);
  entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(OrderedNameDictionary::kNotFound, entry);
  CHECK_EQ(0, dict->NumberOfElements());

  char buf[10];
  // Make sure we grow at least once.
  CHECK_LT(OrderedNameDictionaryHandler::Capacity(*dict), 100);
  for (int i = 0; i < 100; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    key = factory->InternalizeUtf8String(buf);
    dict = OrderedNameDictionary::Add(isolate, dict, key, value, details);
    DCHECK(key->IsUniqueName());
    Verify(isolate, dict);
  }

  CHECK_EQ(100, dict->NumberOfElements());
  // Initial dictionary has grown.
  CHECK_EQ(0, dict->NumberOfDeletedElements());

  for (int i = 0; i < 100; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    key = factory->InternalizeUtf8String(buf);
    entry = dict->FindEntry(isolate, *key);

    dict = OrderedNameDictionary::DeleteEntry(isolate, dict, entry);
    Verify(isolate, dict);

    entry = dict->FindEntry(isolate, *key);
    CHECK_EQ(OrderedNameDictionary::kNotFound, entry);
  }
  CHECK_EQ(0, dict->NumberOfElements());
  // Dictionary shrunk again.
  CHECK_EQ(0, dict->NumberOfDeletedElements());
}

TEST(SmallOrderedNameDictionaryDeleteEntry) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<SmallOrderedNameDictionary> dict =
      factory->NewSmallOrderedNameDictionary();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(0, dict->NumberOfElements());

  Handle<String> key = factory->InternalizeUtf8String("foo");
  Handle<String> value = factory->InternalizeUtf8String("bar");
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound,
           dict->FindEntry(isolate, *key));
  PropertyDetails details = PropertyDetails::Empty();
  dict = SmallOrderedNameDictionary::Add(isolate, dict, key, value, details)
             .ToHandleChecked();
  Verify(isolate, dict);
  CHECK_EQ(2, dict->NumberOfBuckets());
  CHECK_EQ(1, dict->NumberOfElements());
  CHECK_EQ(0, dict->NumberOfDeletedElements());

  int entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(0, entry);
  dict = SmallOrderedNameDictionary::DeleteEntry(isolate, dict, entry);
  entry = dict->FindEntry(isolate, *key);
  CHECK_EQ(SmallOrderedNameDictionary::kNotFound, entry);

  char buf[10];
  // Make sure we grow at least once.
  CHECK_LT(dict->Capacity(), SmallOrderedNameDictionary::kMaxCapacity);

  for (int i = 0; i < SmallOrderedNameDictionary::kMaxCapacity; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    key = factory->InternalizeUtf8String(buf);
    dict = SmallOrderedNameDictionary::Add(isolate, dict, key, value, details)
               .ToHandleChecked();
    DCHECK(key->IsUniqueName());
    Verify(isolate, dict);
  }

  CHECK_EQ(SmallOrderedNameDictionary::kMaxCapacity, dict->NumberOfElements());
  // Dictionary has grown.
  CHECK_EQ(0, dict->NumberOfDeletedElements());

  for (int i = 0; i < SmallOrderedNameDictionary::kMaxCapacity; i++) {
    CHECK_LT(0, snprintf(buf, sizeof(buf), "foo%d", i));
    key = factory->InternalizeUtf8String(buf);

    entry = dict->FindEntry(isolate, *key);
    dict = SmallOrderedNameDictionary::DeleteEntry(isolate, dict, entry);
    Verify(isolate, dict);

    entry = dict->FindEntry(isolate, *key);
    CHECK_EQ(SmallOrderedNameDictionary::kNotFound, entry);
  }

  CHECK_EQ(0, dict->NumberOfElements());
  // Dictionary shrunk.
  CHECK_EQ(0, dict->NumberOfDeletedElements());
}

}  // namespace test_orderedhashtable
}  // namespace internal
}  // namespace v8
