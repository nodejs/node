// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "global-handles.h"
#include "snapshot.h"
#include "top.h"
#include "cctest.h"

using namespace v8::internal;

static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) env = v8::Context::New();
  v8::HandleScope scope;
  env->Enter();
}


TEST(MarkingStack) {
  int mem_size = 20 * kPointerSize;
  byte* mem = NewArray<byte>(20*kPointerSize);
  Address low = reinterpret_cast<Address>(mem);
  Address high = low + mem_size;
  MarkingStack s;
  s.Initialize(low, high);

  Address address = NULL;
  while (!s.is_full()) {
    s.Push(HeapObject::FromAddress(address));
    address += kPointerSize;
  }

  while (!s.is_empty()) {
    Address value = s.Pop()->address();
    address -= kPointerSize;
    CHECK_EQ(address, value);
  }

  CHECK_EQ(NULL, address);
  DeleteArray(mem);
}


TEST(Promotion) {
  // Ensure that we get a compacting collection so that objects are promoted
  // from new space.
  FLAG_gc_global = true;
  FLAG_always_compact = true;
  Heap::ConfigureHeap(2*256*KB, 4*MB);

  InitializeVM();

  v8::HandleScope sc;

  // Allocate a fixed array in the new space.
  int array_size =
      (Heap::MaxObjectSizeInPagedSpace() - FixedArray::kHeaderSize) /
      (kPointerSize * 4);
  Object* obj = Heap::AllocateFixedArray(array_size);
  CHECK(!obj->IsFailure());

  Handle<FixedArray> array(FixedArray::cast(obj));

  // Array should be in the new space.
  CHECK(Heap::InSpace(*array, NEW_SPACE));

  // Call the m-c collector, so array becomes an old object.
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  // Array now sits in the old space
  CHECK(Heap::InSpace(*array, OLD_POINTER_SPACE));
}


TEST(NoPromotion) {
  Heap::ConfigureHeap(2*256*KB, 4*MB);

  // Test the situation that some objects in new space are promoted to
  // the old space
  InitializeVM();

  v8::HandleScope sc;

  // Do a mark compact GC to shrink the heap.
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  // Allocate a big Fixed array in the new space.
  int size = (Heap::MaxObjectSizeInPagedSpace() - FixedArray::kHeaderSize) /
      kPointerSize;
  Object* obj = Heap::AllocateFixedArray(size);

  Handle<FixedArray> array(FixedArray::cast(obj));

  // Array still stays in the new space.
  CHECK(Heap::InSpace(*array, NEW_SPACE));

  // Allocate objects in the old space until out of memory.
  FixedArray* host = *array;
  while (true) {
    Object* obj = Heap::AllocateFixedArray(100, TENURED);
    if (obj->IsFailure()) break;

    host->set(0, obj);
    host = FixedArray::cast(obj);
  }

  // Call mark compact GC, and it should pass.
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  // array should not be promoted because the old space is full.
  CHECK(Heap::InSpace(*array, NEW_SPACE));
}


TEST(MarkCompactCollector) {
  InitializeVM();

  v8::HandleScope sc;
  // call mark-compact when heap is empty
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  // keep allocating garbage in new space until it fails
  const int ARRAY_SIZE = 100;
  Object* array;
  do {
    array = Heap::AllocateFixedArray(ARRAY_SIZE);
  } while (!array->IsFailure());
  CHECK(Heap::CollectGarbage(0, NEW_SPACE));

  array = Heap::AllocateFixedArray(ARRAY_SIZE);
  CHECK(!array->IsFailure());

  // keep allocating maps until it fails
  Object* mapp;
  do {
    mapp = Heap::AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  } while (!mapp->IsFailure());
  CHECK(Heap::CollectGarbage(0, MAP_SPACE));
  mapp = Heap::AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  CHECK(!mapp->IsFailure());

  // allocate a garbage
  String* func_name = String::cast(Heap::LookupAsciiSymbol("theFunction"));
  SharedFunctionInfo* function_share =
    SharedFunctionInfo::cast(Heap::AllocateSharedFunctionInfo(func_name));
  JSFunction* function =
    JSFunction::cast(Heap::AllocateFunction(*Top::function_map(),
                                            function_share,
                                            Heap::undefined_value()));
  Map* initial_map =
      Map::cast(Heap::AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize));
  function->set_initial_map(initial_map);
  Top::context()->global()->SetProperty(func_name, function, NONE);

  JSObject* obj = JSObject::cast(Heap::AllocateJSObject(function));
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  func_name = String::cast(Heap::LookupAsciiSymbol("theFunction"));
  CHECK(Top::context()->global()->HasLocalProperty(func_name));
  Object* func_value = Top::context()->global()->GetProperty(func_name);
  CHECK(func_value->IsJSFunction());
  function = JSFunction::cast(func_value);

  obj = JSObject::cast(Heap::AllocateJSObject(function));
  String* obj_name = String::cast(Heap::LookupAsciiSymbol("theObject"));
  Top::context()->global()->SetProperty(obj_name, obj, NONE);
  String* prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  obj->SetProperty(prop_name, Smi::FromInt(23), NONE);

  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  obj_name = String::cast(Heap::LookupAsciiSymbol("theObject"));
  CHECK(Top::context()->global()->HasLocalProperty(obj_name));
  CHECK(Top::context()->global()->GetProperty(obj_name)->IsJSObject());
  obj = JSObject::cast(Top::context()->global()->GetProperty(obj_name));
  prop_name = String::cast(Heap::LookupAsciiSymbol("theSlot"));
  CHECK(obj->GetProperty(prop_name) == Smi::FromInt(23));
}


static Handle<Map> CreateMap() {
  return Factory::NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
}


TEST(MapCompact) {
  FLAG_max_map_space_pages = 16;
  InitializeVM();

  {
    v8::HandleScope sc;
    // keep allocating maps while pointers are still encodable and thus
    // mark compact is permitted.
    Handle<JSObject> root = Factory::NewJSObjectFromMap(CreateMap());
    do {
      Handle<Map> map = CreateMap();
      map->set_prototype(*root);
      root = Factory::NewJSObjectFromMap(map);
    } while (Heap::map_space()->MapPointersEncodable());
  }
  // Now, as we don't have any handles to just allocated maps, we should
  // be able to trigger map compaction.
  // To give an additional chance to fail, try to force compaction which
  // should be impossible right now.
  Heap::CollectAllGarbage(true);
  // And now map pointers should be encodable again.
  CHECK(Heap::map_space()->MapPointersEncodable());
}


static int gc_starts = 0;
static int gc_ends = 0;

static void GCPrologueCallbackFunc() {
  CHECK(gc_starts == gc_ends);
  gc_starts++;
}


static void GCEpilogueCallbackFunc() {
  CHECK(gc_starts == gc_ends + 1);
  gc_ends++;
}


TEST(GCCallback) {
  InitializeVM();

  Heap::SetGlobalGCPrologueCallback(&GCPrologueCallbackFunc);
  Heap::SetGlobalGCEpilogueCallback(&GCEpilogueCallbackFunc);

  // Scavenge does not call GC callback functions.
  Heap::PerformScavenge();

  CHECK_EQ(0, gc_starts);
  CHECK_EQ(gc_ends, gc_starts);

  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));
  CHECK_EQ(1, gc_starts);
  CHECK_EQ(gc_ends, gc_starts);
}


static int NumberOfWeakCalls = 0;
static void WeakPointerCallback(v8::Persistent<v8::Value> handle, void* id) {
  NumberOfWeakCalls++;
}

TEST(ObjectGroups) {
  InitializeVM();

  NumberOfWeakCalls = 0;
  v8::HandleScope handle_scope;

  Handle<Object> g1s1 =
    GlobalHandles::Create(Heap::AllocateFixedArray(1));
  Handle<Object> g1s2 =
    GlobalHandles::Create(Heap::AllocateFixedArray(1));
  GlobalHandles::MakeWeak(g1s1.location(),
                          reinterpret_cast<void*>(1234),
                          &WeakPointerCallback);
  GlobalHandles::MakeWeak(g1s2.location(),
                          reinterpret_cast<void*>(1234),
                          &WeakPointerCallback);

  Handle<Object> g2s1 =
    GlobalHandles::Create(Heap::AllocateFixedArray(1));
  Handle<Object> g2s2 =
    GlobalHandles::Create(Heap::AllocateFixedArray(1));
  GlobalHandles::MakeWeak(g2s1.location(),
                          reinterpret_cast<void*>(1234),
                          &WeakPointerCallback);
  GlobalHandles::MakeWeak(g2s2.location(),
                          reinterpret_cast<void*>(1234),
                          &WeakPointerCallback);

  Handle<Object> root = GlobalHandles::Create(*g1s1);  // make a root.

  // Connect group 1 and 2, make a cycle.
  Handle<FixedArray>::cast(g1s2)->set(0, *g2s2);
  Handle<FixedArray>::cast(g2s1)->set(0, *g1s1);

  {
    Object** g1_objects[] = { g1s1.location(), g1s2.location() };
    Object** g2_objects[] = { g2s1.location(), g2s2.location() };
    GlobalHandles::AddGroup(g1_objects, 2);
    GlobalHandles::AddGroup(g2_objects, 2);
  }
  // Do a full GC
  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  // All object should be alive.
  CHECK_EQ(0, NumberOfWeakCalls);

  // Weaken the root.
  GlobalHandles::MakeWeak(root.location(),
                          reinterpret_cast<void*>(1234),
                          &WeakPointerCallback);

  // Groups are deleted, rebuild groups.
  {
    Object** g1_objects[] = { g1s1.location(), g1s2.location() };
    Object** g2_objects[] = { g2s1.location(), g2s2.location() };
    GlobalHandles::AddGroup(g1_objects, 2);
    GlobalHandles::AddGroup(g2_objects, 2);
  }

  CHECK(Heap::CollectGarbage(0, OLD_POINTER_SPACE));

  // All objects should be gone. 5 global handles in total.
  CHECK_EQ(5, NumberOfWeakCalls);
}
