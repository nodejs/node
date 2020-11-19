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

#include <utility>

#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace test_weakmaps {

static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
}

static int NumberOfWeakCalls = 0;
static void WeakPointerCallback(const v8::WeakCallbackInfo<void>& data) {
  std::pair<v8::Persistent<v8::Value>*, int>* p =
      reinterpret_cast<std::pair<v8::Persistent<v8::Value>*, int>*>(
          data.GetParameter());
  CHECK_EQ(1234, p->second);
  NumberOfWeakCalls++;
  p->first->Reset();
}


TEST(Weakness) {
  FLAG_incremental_marking = false;
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();
  GlobalHandles* global_handles = isolate->global_handles();

  // Keep global reference to the key.
  Handle<Object> key;
  {
    HandleScope scope(isolate);
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    key = global_handles->Create(*object);
  }
  CHECK(!global_handles->IsWeak(key.location()));

  // Put two chained entries into weak map.
  {
    HandleScope scope(isolate);
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    Handle<Smi> smi(Smi::FromInt(23), isolate);
    int32_t hash = key->GetOrCreateHash(isolate).value();
    JSWeakCollection::Set(weakmap, key, object, hash);
    int32_t object_hash = object->GetOrCreateHash(isolate).value();
    JSWeakCollection::Set(weakmap, object, smi, object_hash);
  }
  CHECK_EQ(2, EphemeronHashTable::cast(weakmap->table()).NumberOfElements());

  // Force a full GC.
  CcTest::PreciseCollectAllGarbage();
  CHECK_EQ(0, NumberOfWeakCalls);
  CHECK_EQ(2, EphemeronHashTable::cast(weakmap->table()).NumberOfElements());
  CHECK_EQ(
      0, EphemeronHashTable::cast(weakmap->table()).NumberOfDeletedElements());

  // Make the global reference to the key weak.
  std::pair<Handle<Object>*, int> handle_and_id(&key, 1234);
  GlobalHandles::MakeWeak(
      key.location(), reinterpret_cast<void*>(&handle_and_id),
      &WeakPointerCallback, v8::WeakCallbackType::kParameter);
  CHECK(global_handles->IsWeak(key.location()));

  CcTest::PreciseCollectAllGarbage();
  CHECK_EQ(1, NumberOfWeakCalls);
  CHECK_EQ(0, EphemeronHashTable::cast(weakmap->table()).NumberOfElements());
  CHECK_EQ(
      2, EphemeronHashTable::cast(weakmap->table()).NumberOfDeletedElements());
}


TEST(Shrinking) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();

  // Check initial capacity.
  CHECK_EQ(32, EphemeronHashTable::cast(weakmap->table()).Capacity());

  // Fill up weak map to trigger capacity change.
  {
    HandleScope scope(isolate);
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    for (int i = 0; i < 32; i++) {
      Handle<JSObject> object = factory->NewJSObjectFromMap(map);
      Handle<Smi> smi(Smi::FromInt(i), isolate);
      int32_t object_hash = object->GetOrCreateHash(isolate).value();
      JSWeakCollection::Set(weakmap, object, smi, object_hash);
    }
  }

  // Check increased capacity.
  CHECK_EQ(128, EphemeronHashTable::cast(weakmap->table()).Capacity());

  // Force a full GC.
  CHECK_EQ(32, EphemeronHashTable::cast(weakmap->table()).NumberOfElements());
  CHECK_EQ(
      0, EphemeronHashTable::cast(weakmap->table()).NumberOfDeletedElements());
  CcTest::PreciseCollectAllGarbage();
  CHECK_EQ(0, EphemeronHashTable::cast(weakmap->table()).NumberOfElements());
  CHECK_EQ(
      32, EphemeronHashTable::cast(weakmap->table()).NumberOfDeletedElements());

  // Check shrunk capacity.
  CHECK_EQ(32, EphemeronHashTable::cast(weakmap->table()).Capacity());
}

namespace {
bool EphemeronHashTableContainsKey(EphemeronHashTable table, HeapObject key) {
  for (InternalIndex i : table.IterateEntries()) {
    if (table.KeyAt(i) == key) return true;
  }
  return false;
}
}  // namespace

TEST(WeakMapPromotionMarkCompact) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();

  CcTest::CollectAllGarbage();

  CHECK(FLAG_always_promote_young_mc
            ? !ObjectInYoungGeneration(weakmap->table())
            : ObjectInYoungGeneration(weakmap->table()));

  Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  Handle<JSObject> object = factory->NewJSObjectFromMap(map);
  Handle<Smi> smi(Smi::FromInt(1), isolate);
  int32_t object_hash = object->GetOrCreateHash(isolate).value();
  JSWeakCollection::Set(weakmap, object, smi, object_hash);

  CHECK(EphemeronHashTableContainsKey(
      EphemeronHashTable::cast(weakmap->table()), *object));
  CcTest::CollectAllGarbage();

  CHECK(FLAG_always_promote_young_mc ? !ObjectInYoungGeneration(*object)
                                     : ObjectInYoungGeneration(*object));
  CHECK(!ObjectInYoungGeneration(weakmap->table()));
  CHECK(EphemeronHashTableContainsKey(
      EphemeronHashTable::cast(weakmap->table()), *object));

  CcTest::CollectAllGarbage();
  CHECK(!ObjectInYoungGeneration(*object));
  CHECK(!ObjectInYoungGeneration(weakmap->table()));
  CHECK(EphemeronHashTableContainsKey(
      EphemeronHashTable::cast(weakmap->table()), *object));
}

TEST(WeakMapScavenge) {
  if (i::FLAG_single_generation) return;
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();

  heap::GcAndSweep(isolate->heap(), NEW_SPACE);
  CHECK(ObjectInYoungGeneration(weakmap->table()));

  Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  Handle<JSObject> object = factory->NewJSObjectFromMap(map);
  Handle<Smi> smi(Smi::FromInt(1), isolate);
  int32_t object_hash = object->GetOrCreateHash(isolate).value();
  JSWeakCollection::Set(weakmap, object, smi, object_hash);

  CHECK(EphemeronHashTableContainsKey(
      EphemeronHashTable::cast(weakmap->table()), *object));

  heap::GcAndSweep(isolate->heap(), NEW_SPACE);
  CHECK(ObjectInYoungGeneration(*object));
  CHECK(!ObjectInYoungGeneration(weakmap->table()));
  CHECK(EphemeronHashTableContainsKey(
      EphemeronHashTable::cast(weakmap->table()), *object));

  heap::GcAndSweep(isolate->heap(), NEW_SPACE);
  CHECK(!ObjectInYoungGeneration(*object));
  CHECK(!ObjectInYoungGeneration(weakmap->table()));
  CHECK(EphemeronHashTableContainsKey(
      EphemeronHashTable::cast(weakmap->table()), *object));
}

// Test that weak map values on an evacuation candidate which are not reachable
// by other paths are correctly recorded in the slots buffer.
TEST(Regress2060a) {
  if (i::FLAG_never_compact) return;
  FLAG_always_compact = true;
  FLAG_stress_concurrent_allocation = false;  // For SimulateFullSpace.
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      factory->NewFunctionForTest(factory->function_string());
  Handle<JSObject> key = factory->NewJSObject(function);
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();

  // Start second old-space page so that values land on evacuation candidate.
  Page* first_page = heap->old_space()->first_page();
  heap::SimulateFullSpace(heap->old_space());

  // Fill up weak map with values on an evacuation candidate.
  {
    HandleScope scope(isolate);
    for (int i = 0; i < 32; i++) {
      Handle<JSObject> object =
          factory->NewJSObject(function, AllocationType::kOld);
      CHECK(!Heap::InYoungGeneration(*object));
      CHECK(!first_page->Contains(object->address()));
      int32_t hash = key->GetOrCreateHash(isolate).value();
      JSWeakCollection::Set(weakmap, key, object, hash);
    }
  }

  // Force compacting garbage collection.
  CHECK(FLAG_always_compact);
  CcTest::CollectAllGarbage();
}


// Test that weak map keys on an evacuation candidate which are reachable by
// other strong paths are correctly recorded in the slots buffer.
TEST(Regress2060b) {
  if (i::FLAG_never_compact) return;
  FLAG_always_compact = true;
#ifdef VERIFY_HEAP
  FLAG_verify_heap = true;
#endif
  FLAG_stress_concurrent_allocation = false;  // For SimulateFullSpace.

  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      factory->NewFunctionForTest(factory->function_string());

  // Start second old-space page so that keys land on evacuation candidate.
  Page* first_page = heap->old_space()->first_page();
  heap::SimulateFullSpace(heap->old_space());

  // Fill up weak map with keys on an evacuation candidate.
  Handle<JSObject> keys[32];
  for (int i = 0; i < 32; i++) {
    keys[i] = factory->NewJSObject(function, AllocationType::kOld);
    CHECK(!Heap::InYoungGeneration(*keys[i]));
    CHECK(!first_page->Contains(keys[i]->address()));
  }
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();
  for (int i = 0; i < 32; i++) {
    Handle<Smi> smi(Smi::FromInt(i), isolate);
    int32_t hash = keys[i]->GetOrCreateHash(isolate).value();
    JSWeakCollection::Set(weakmap, keys[i], smi, hash);
  }

  // Force compacting garbage collection. The subsequent collections are used
  // to verify that key references were actually updated.
  CHECK(FLAG_always_compact);
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
}


TEST(Regress399527) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  {
    HandleScope scope(isolate);
    isolate->factory()->NewJSWeakMap();
    heap::SimulateIncrementalMarking(heap);
  }
  // The weak map is marked black here but leaving the handle scope will make
  // the object unreachable. Aborting incremental marking will clear all the
  // marking bits which makes the weak map garbage.
  CcTest::CollectAllGarbage();
}

TEST(WeakMapsWithChainedEntries) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  i::Isolate* i_isolate = CcTest::i_isolate();
  v8::HandleScope scope(isolate);

  const int initial_gc_count = i_isolate->heap()->gc_count();
  Handle<JSWeakMap> weakmap1 = i_isolate->factory()->NewJSWeakMap();
  Handle<JSWeakMap> weakmap2 = i_isolate->factory()->NewJSWeakMap();
  v8::Global<v8::Object> g1;
  v8::Global<v8::Object> g2;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> o1 = v8::Object::New(isolate);
    g1.Reset(isolate, o1);
    g1.SetWeak();
    v8::Local<v8::Object> o2 = v8::Object::New(isolate);
    g2.Reset(isolate, o2);
    g2.SetWeak();
    Handle<Object> i_o1 = v8::Utils::OpenHandle(*o1);
    Handle<Object> i_o2 = v8::Utils::OpenHandle(*o2);
    int32_t hash1 = i_o1->GetOrCreateHash(i_isolate).value();
    int32_t hash2 = i_o2->GetOrCreateHash(i_isolate).value();
    JSWeakCollection::Set(weakmap1, i_o1, i_o2, hash1);
    JSWeakCollection::Set(weakmap2, i_o2, i_o1, hash2);
  }
  CcTest::CollectGarbage(OLD_SPACE);
  CHECK(g1.IsEmpty());
  CHECK(g2.IsEmpty());
  CHECK_EQ(1, i_isolate->heap()->gc_count() - initial_gc_count);
}

}  // namespace test_weakmaps
}  // namespace internal
}  // namespace v8
