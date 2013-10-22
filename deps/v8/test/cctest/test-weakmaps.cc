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

#include "global-handles.h"
#include "snapshot.h"
#include "cctest.h"

using namespace v8::internal;


static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
}


static Handle<JSWeakMap> AllocateJSWeakMap(Isolate* isolate) {
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  Handle<Map> map = factory->NewMap(JS_WEAK_MAP_TYPE, JSWeakMap::kSize);
  Handle<JSObject> weakmap_obj = factory->NewJSObjectFromMap(map);
  Handle<JSWeakMap> weakmap(JSWeakMap::cast(*weakmap_obj));
  // Do not use handles for the hash table, it would make entries strong.
  Object* table_obj = ObjectHashTable::Allocate(heap, 1)->ToObjectChecked();
  ObjectHashTable* table = ObjectHashTable::cast(table_obj);
  weakmap->set_table(table);
  weakmap->set_next(Smi::FromInt(0));
  return weakmap;
}

static void PutIntoWeakMap(Handle<JSWeakMap> weakmap,
                           Handle<JSObject> key,
                           Handle<Object> value) {
  Handle<ObjectHashTable> table = PutIntoObjectHashTable(
      Handle<ObjectHashTable>(ObjectHashTable::cast(weakmap->table())),
      Handle<JSObject>(JSObject::cast(*key)),
      value);
  weakmap->set_table(*table);
}

static int NumberOfWeakCalls = 0;
static void WeakPointerCallback(v8::Isolate* isolate,
                                v8::Persistent<v8::Value>* handle,
                                void* id) {
  ASSERT(id == reinterpret_cast<void*>(1234));
  NumberOfWeakCalls++;
  handle->Dispose();
}


TEST(Weakness) {
  FLAG_incremental_marking = false;
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<JSWeakMap> weakmap = AllocateJSWeakMap(isolate);
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

  // Put entry into weak map.
  {
    HandleScope scope(isolate);
    PutIntoWeakMap(weakmap,
                   Handle<JSObject>(JSObject::cast(*key)),
                   Handle<Smi>(Smi::FromInt(23), isolate));
  }
  CHECK_EQ(1, ObjectHashTable::cast(weakmap->table())->NumberOfElements());

  // Force a full GC.
  heap->CollectAllGarbage(false);
  CHECK_EQ(0, NumberOfWeakCalls);
  CHECK_EQ(1, ObjectHashTable::cast(weakmap->table())->NumberOfElements());
  CHECK_EQ(
      0, ObjectHashTable::cast(weakmap->table())->NumberOfDeletedElements());

  // Make the global reference to the key weak.
  {
    HandleScope scope(isolate);
    global_handles->MakeWeak(key.location(),
                             reinterpret_cast<void*>(1234),
                             &WeakPointerCallback);
  }
  CHECK(global_handles->IsWeak(key.location()));

  // Force a full GC.
  // Perform two consecutive GCs because the first one will only clear
  // weak references whereas the second one will also clear weak maps.
  heap->CollectAllGarbage(false);
  CHECK_EQ(1, NumberOfWeakCalls);
  CHECK_EQ(1, ObjectHashTable::cast(weakmap->table())->NumberOfElements());
  CHECK_EQ(
      0, ObjectHashTable::cast(weakmap->table())->NumberOfDeletedElements());
  heap->CollectAllGarbage(false);
  CHECK_EQ(1, NumberOfWeakCalls);
  CHECK_EQ(0, ObjectHashTable::cast(weakmap->table())->NumberOfElements());
  CHECK_EQ(
      1, ObjectHashTable::cast(weakmap->table())->NumberOfDeletedElements());
}


TEST(Shrinking) {
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<JSWeakMap> weakmap = AllocateJSWeakMap(isolate);

  // Check initial capacity.
  CHECK_EQ(32, ObjectHashTable::cast(weakmap->table())->Capacity());

  // Fill up weak map to trigger capacity change.
  {
    HandleScope scope(isolate);
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    for (int i = 0; i < 32; i++) {
      Handle<JSObject> object = factory->NewJSObjectFromMap(map);
      PutIntoWeakMap(weakmap, object, Handle<Smi>(Smi::FromInt(i), isolate));
    }
  }

  // Check increased capacity.
  CHECK_EQ(128, ObjectHashTable::cast(weakmap->table())->Capacity());

  // Force a full GC.
  CHECK_EQ(32, ObjectHashTable::cast(weakmap->table())->NumberOfElements());
  CHECK_EQ(
      0, ObjectHashTable::cast(weakmap->table())->NumberOfDeletedElements());
  heap->CollectAllGarbage(false);
  CHECK_EQ(0, ObjectHashTable::cast(weakmap->table())->NumberOfElements());
  CHECK_EQ(
      32, ObjectHashTable::cast(weakmap->table())->NumberOfDeletedElements());

  // Check shrunk capacity.
  CHECK_EQ(32, ObjectHashTable::cast(weakmap->table())->Capacity());
}


// Test that weak map values on an evacuation candidate which are not reachable
// by other paths are correctly recorded in the slots buffer.
TEST(Regress2060a) {
  FLAG_always_compact = true;
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      factory->NewFunction(factory->function_string(), factory->null_value());
  Handle<JSObject> key = factory->NewJSObject(function);
  Handle<JSWeakMap> weakmap = AllocateJSWeakMap(isolate);

  // Start second old-space page so that values land on evacuation candidate.
  Page* first_page = heap->old_pointer_space()->anchor()->next_page();
  factory->NewFixedArray(900 * KB / kPointerSize, TENURED);

  // Fill up weak map with values on an evacuation candidate.
  {
    HandleScope scope(isolate);
    for (int i = 0; i < 32; i++) {
      Handle<JSObject> object = factory->NewJSObject(function, TENURED);
      CHECK(!heap->InNewSpace(object->address()));
      CHECK(!first_page->Contains(object->address()));
      PutIntoWeakMap(weakmap, key, object);
    }
  }

  // Force compacting garbage collection.
  CHECK(FLAG_always_compact);
  heap->CollectAllGarbage(Heap::kNoGCFlags);
}


// Test that weak map keys on an evacuation candidate which are reachable by
// other strong paths are correctly recorded in the slots buffer.
TEST(Regress2060b) {
  FLAG_always_compact = true;
#ifdef VERIFY_HEAP
  FLAG_verify_heap = true;
#endif

  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      factory->NewFunction(factory->function_string(), factory->null_value());

  // Start second old-space page so that keys land on evacuation candidate.
  Page* first_page = heap->old_pointer_space()->anchor()->next_page();
  factory->NewFixedArray(900 * KB / kPointerSize, TENURED);

  // Fill up weak map with keys on an evacuation candidate.
  Handle<JSObject> keys[32];
  for (int i = 0; i < 32; i++) {
    keys[i] = factory->NewJSObject(function, TENURED);
    CHECK(!heap->InNewSpace(keys[i]->address()));
    CHECK(!first_page->Contains(keys[i]->address()));
  }
  Handle<JSWeakMap> weakmap = AllocateJSWeakMap(isolate);
  for (int i = 0; i < 32; i++) {
    PutIntoWeakMap(weakmap,
                   keys[i],
                   Handle<Smi>(Smi::FromInt(i), isolate));
  }

  // Force compacting garbage collection. The subsequent collections are used
  // to verify that key references were actually updated.
  CHECK(FLAG_always_compact);
  heap->CollectAllGarbage(Heap::kNoGCFlags);
  heap->CollectAllGarbage(Heap::kNoGCFlags);
  heap->CollectAllGarbage(Heap::kNoGCFlags);
}
