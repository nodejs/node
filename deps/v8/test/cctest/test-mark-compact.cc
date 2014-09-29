// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <utility>

#include "src/v8.h"

#include "src/full-codegen.h"
#include "src/global-handles.h"
#include "src/snapshot.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


TEST(MarkingDeque) {
  CcTest::InitializeVM();
  int mem_size = 20 * kPointerSize;
  byte* mem = NewArray<byte>(20*kPointerSize);
  Address low = reinterpret_cast<Address>(mem);
  Address high = low + mem_size;
  MarkingDeque s;
  s.Initialize(low, high);

  Address original_address = reinterpret_cast<Address>(&s);
  Address current_address = original_address;
  while (!s.IsFull()) {
    s.PushBlack(HeapObject::FromAddress(current_address));
    current_address += kPointerSize;
  }

  while (!s.IsEmpty()) {
    Address value = s.Pop()->address();
    current_address -= kPointerSize;
    CHECK_EQ(current_address, value);
  }

  CHECK_EQ(original_address, current_address);
  DeleteArray(mem);
}


TEST(Promotion) {
  CcTest::InitializeVM();
  TestHeap* heap = CcTest::test_heap();
  heap->ConfigureHeap(1, 1, 1, 0);

  v8::HandleScope sc(CcTest::isolate());

  // Allocate a fixed array in the new space.
  int array_length =
      (Page::kMaxRegularHeapObjectSize - FixedArray::kHeaderSize) /
      (4 * kPointerSize);
  Object* obj = heap->AllocateFixedArray(array_length).ToObjectChecked();
  Handle<FixedArray> array(FixedArray::cast(obj));

  // Array should be in the new space.
  CHECK(heap->InSpace(*array, NEW_SPACE));

  // Call mark compact GC, so array becomes an old object.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Array now sits in the old space
  CHECK(heap->InSpace(*array, OLD_POINTER_SPACE));
}


TEST(NoPromotion) {
  CcTest::InitializeVM();
  TestHeap* heap = CcTest::test_heap();
  heap->ConfigureHeap(1, 1, 1, 0);

  v8::HandleScope sc(CcTest::isolate());

  // Allocate a big fixed array in the new space.
  int array_length =
      (Page::kMaxRegularHeapObjectSize - FixedArray::kHeaderSize) /
      (2 * kPointerSize);
  Object* obj = heap->AllocateFixedArray(array_length).ToObjectChecked();
  Handle<FixedArray> array(FixedArray::cast(obj));

  // Array should be in the new space.
  CHECK(heap->InSpace(*array, NEW_SPACE));

  // Simulate a full old space to make promotion fail.
  SimulateFullSpace(heap->old_pointer_space());

  // Call mark compact GC, and it should pass.
  heap->CollectGarbage(OLD_POINTER_SPACE);
}


TEST(MarkCompactCollector) {
  FLAG_incremental_marking = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  TestHeap* heap = CcTest::test_heap();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<GlobalObject> global(isolate->context()->global_object());

  // call mark-compact when heap is empty
  heap->CollectGarbage(OLD_POINTER_SPACE, "trigger 1");

  // keep allocating garbage in new space until it fails
  const int ARRAY_SIZE = 100;
  AllocationResult allocation;
  do {
    allocation = heap->AllocateFixedArray(ARRAY_SIZE);
  } while (!allocation.IsRetry());
  heap->CollectGarbage(NEW_SPACE, "trigger 2");
  heap->AllocateFixedArray(ARRAY_SIZE).ToObjectChecked();

  // keep allocating maps until it fails
  do {
    allocation = heap->AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  } while (!allocation.IsRetry());
  heap->CollectGarbage(MAP_SPACE, "trigger 3");
  heap->AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize).ToObjectChecked();

  { HandleScope scope(isolate);
    // allocate a garbage
    Handle<String> func_name = factory->InternalizeUtf8String("theFunction");
    Handle<JSFunction> function = factory->NewFunction(func_name);
    JSReceiver::SetProperty(global, func_name, function, SLOPPY).Check();

    factory->NewJSObject(function);
  }

  heap->CollectGarbage(OLD_POINTER_SPACE, "trigger 4");

  { HandleScope scope(isolate);
    Handle<String> func_name = factory->InternalizeUtf8String("theFunction");
    v8::Maybe<bool> maybe = JSReceiver::HasOwnProperty(global, func_name);
    CHECK(maybe.has_value);
    CHECK(maybe.value);
    Handle<Object> func_value =
        Object::GetProperty(global, func_name).ToHandleChecked();
    CHECK(func_value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
    Handle<JSObject> obj = factory->NewJSObject(function);

    Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
    JSReceiver::SetProperty(global, obj_name, obj, SLOPPY).Check();
    Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
    JSReceiver::SetProperty(obj, prop_name, twenty_three, SLOPPY).Check();
  }

  heap->CollectGarbage(OLD_POINTER_SPACE, "trigger 5");

  { HandleScope scope(isolate);
    Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
    v8::Maybe<bool> maybe = JSReceiver::HasOwnProperty(global, obj_name);
    CHECK(maybe.has_value);
    CHECK(maybe.value);
    Handle<Object> object =
        Object::GetProperty(global, obj_name).ToHandleChecked();
    CHECK(object->IsJSObject());
    Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    CHECK_EQ(*Object::GetProperty(object, prop_name).ToHandleChecked(),
             Smi::FromInt(23));
  }
}


// TODO(1600): compaction of map space is temporary removed from GC.
#if 0
static Handle<Map> CreateMap(Isolate* isolate) {
  return isolate->factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
}


TEST(MapCompact) {
  FLAG_max_map_space_pages = 16;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  {
    v8::HandleScope sc;
    // keep allocating maps while pointers are still encodable and thus
    // mark compact is permitted.
    Handle<JSObject> root = factory->NewJSObjectFromMap(CreateMap());
    do {
      Handle<Map> map = CreateMap();
      map->set_prototype(*root);
      root = factory->NewJSObjectFromMap(map);
    } while (CcTest::heap()->map_space()->MapPointersEncodable());
  }
  // Now, as we don't have any handles to just allocated maps, we should
  // be able to trigger map compaction.
  // To give an additional chance to fail, try to force compaction which
  // should be impossible right now.
  CcTest::heap()->CollectAllGarbage(Heap::kForceCompactionMask);
  // And now map pointers should be encodable again.
  CHECK(CcTest::heap()->map_space()->MapPointersEncodable());
}
#endif


static int NumberOfWeakCalls = 0;
static void WeakPointerCallback(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  std::pair<v8::Persistent<v8::Value>*, int>* p =
      reinterpret_cast<std::pair<v8::Persistent<v8::Value>*, int>*>(
          data.GetParameter());
  DCHECK_EQ(1234, p->second);
  NumberOfWeakCalls++;
  p->first->Reset();
}


TEST(ObjectGroups) {
  FLAG_incremental_marking = false;
  CcTest::InitializeVM();
  GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  TestHeap* heap = CcTest::test_heap();
  NumberOfWeakCalls = 0;
  v8::HandleScope handle_scope(CcTest::isolate());

  Handle<Object> g1s1 =
      global_handles->Create(heap->AllocateFixedArray(1).ToObjectChecked());
  Handle<Object> g1s2 =
      global_handles->Create(heap->AllocateFixedArray(1).ToObjectChecked());
  Handle<Object> g1c1 =
      global_handles->Create(heap->AllocateFixedArray(1).ToObjectChecked());
  std::pair<Handle<Object>*, int> g1s1_and_id(&g1s1, 1234);
  GlobalHandles::MakeWeak(g1s1.location(),
                          reinterpret_cast<void*>(&g1s1_and_id),
                          &WeakPointerCallback);
  std::pair<Handle<Object>*, int> g1s2_and_id(&g1s2, 1234);
  GlobalHandles::MakeWeak(g1s2.location(),
                          reinterpret_cast<void*>(&g1s2_and_id),
                          &WeakPointerCallback);
  std::pair<Handle<Object>*, int> g1c1_and_id(&g1c1, 1234);
  GlobalHandles::MakeWeak(g1c1.location(),
                          reinterpret_cast<void*>(&g1c1_and_id),
                          &WeakPointerCallback);

  Handle<Object> g2s1 =
      global_handles->Create(heap->AllocateFixedArray(1).ToObjectChecked());
  Handle<Object> g2s2 =
    global_handles->Create(heap->AllocateFixedArray(1).ToObjectChecked());
  Handle<Object> g2c1 =
    global_handles->Create(heap->AllocateFixedArray(1).ToObjectChecked());
  std::pair<Handle<Object>*, int> g2s1_and_id(&g2s1, 1234);
  GlobalHandles::MakeWeak(g2s1.location(),
                          reinterpret_cast<void*>(&g2s1_and_id),
                          &WeakPointerCallback);
  std::pair<Handle<Object>*, int> g2s2_and_id(&g2s2, 1234);
  GlobalHandles::MakeWeak(g2s2.location(),
                          reinterpret_cast<void*>(&g2s2_and_id),
                          &WeakPointerCallback);
  std::pair<Handle<Object>*, int> g2c1_and_id(&g2c1, 1234);
  GlobalHandles::MakeWeak(g2c1.location(),
                          reinterpret_cast<void*>(&g2c1_and_id),
                          &WeakPointerCallback);

  Handle<Object> root = global_handles->Create(*g1s1);  // make a root.

  // Connect group 1 and 2, make a cycle.
  Handle<FixedArray>::cast(g1s2)->set(0, *g2s2);
  Handle<FixedArray>::cast(g2s1)->set(0, *g1s1);

  {
    Object** g1_objects[] = { g1s1.location(), g1s2.location() };
    Object** g1_children[] = { g1c1.location() };
    Object** g2_objects[] = { g2s1.location(), g2s2.location() };
    Object** g2_children[] = { g2c1.location() };
    global_handles->AddObjectGroup(g1_objects, 2, NULL);
    global_handles->AddImplicitReferences(
        Handle<HeapObject>::cast(g1s1).location(), g1_children, 1);
    global_handles->AddObjectGroup(g2_objects, 2, NULL);
    global_handles->AddImplicitReferences(
        Handle<HeapObject>::cast(g2s1).location(), g2_children, 1);
  }
  // Do a full GC
  heap->CollectGarbage(OLD_POINTER_SPACE);

  // All object should be alive.
  CHECK_EQ(0, NumberOfWeakCalls);

  // Weaken the root.
  std::pair<Handle<Object>*, int> root_and_id(&root, 1234);
  GlobalHandles::MakeWeak(root.location(),
                          reinterpret_cast<void*>(&root_and_id),
                          &WeakPointerCallback);
  // But make children strong roots---all the objects (except for children)
  // should be collectable now.
  global_handles->ClearWeakness(g1c1.location());
  global_handles->ClearWeakness(g2c1.location());

  // Groups are deleted, rebuild groups.
  {
    Object** g1_objects[] = { g1s1.location(), g1s2.location() };
    Object** g1_children[] = { g1c1.location() };
    Object** g2_objects[] = { g2s1.location(), g2s2.location() };
    Object** g2_children[] = { g2c1.location() };
    global_handles->AddObjectGroup(g1_objects, 2, NULL);
    global_handles->AddImplicitReferences(
        Handle<HeapObject>::cast(g1s1).location(), g1_children, 1);
    global_handles->AddObjectGroup(g2_objects, 2, NULL);
    global_handles->AddImplicitReferences(
        Handle<HeapObject>::cast(g2s1).location(), g2_children, 1);
  }

  heap->CollectGarbage(OLD_POINTER_SPACE);

  // All objects should be gone. 5 global handles in total.
  CHECK_EQ(5, NumberOfWeakCalls);

  // And now make children weak again and collect them.
  GlobalHandles::MakeWeak(g1c1.location(),
                          reinterpret_cast<void*>(&g1c1_and_id),
                          &WeakPointerCallback);
  GlobalHandles::MakeWeak(g2c1.location(),
                          reinterpret_cast<void*>(&g2c1_and_id),
                          &WeakPointerCallback);

  heap->CollectGarbage(OLD_POINTER_SPACE);
  CHECK_EQ(7, NumberOfWeakCalls);
}


class TestRetainedObjectInfo : public v8::RetainedObjectInfo {
 public:
  TestRetainedObjectInfo() : has_been_disposed_(false) {}

  bool has_been_disposed() { return has_been_disposed_; }

  virtual void Dispose() {
    DCHECK(!has_been_disposed_);
    has_been_disposed_ = true;
  }

  virtual bool IsEquivalent(v8::RetainedObjectInfo* other) {
    return other == this;
  }

  virtual intptr_t GetHash() { return 0; }

  virtual const char* GetLabel() { return "whatever"; }

 private:
  bool has_been_disposed_;
};


TEST(EmptyObjectGroups) {
  CcTest::InitializeVM();
  GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  v8::HandleScope handle_scope(CcTest::isolate());

  Handle<Object> object = global_handles->Create(
      CcTest::test_heap()->AllocateFixedArray(1).ToObjectChecked());

  TestRetainedObjectInfo info;
  global_handles->AddObjectGroup(NULL, 0, &info);
  DCHECK(info.has_been_disposed());

  global_handles->AddImplicitReferences(
        Handle<HeapObject>::cast(object).location(), NULL, 0);
}


#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define V8_WITH_ASAN 1
#endif
#endif


// Here is a memory use test that uses /proc, and is therefore Linux-only.  We
// do not care how much memory the simulator uses, since it is only there for
// debugging purposes. Testing with ASAN doesn't make sense, either.
#if defined(__linux__) && !defined(USE_SIMULATOR) && !defined(V8_WITH_ASAN)


static uintptr_t ReadLong(char* buffer, intptr_t* position, int base) {
  char* end_address = buffer + *position;
  uintptr_t result = strtoul(buffer + *position, &end_address, base);
  CHECK(result != ULONG_MAX || errno != ERANGE);
  CHECK(end_address > buffer + *position);
  *position = end_address - buffer;
  return result;
}


// The memory use computed this way is not entirely accurate and depends on
// the way malloc allocates memory.  That's why the memory use may seem to
// increase even though the sum of the allocated object sizes decreases.  It
// also means that the memory use depends on the kernel and stdlib.
static intptr_t MemoryInUse() {
  intptr_t memory_use = 0;

  int fd = open("/proc/self/maps", O_RDONLY);
  if (fd < 0) return -1;

  const int kBufSize = 10000;
  char buffer[kBufSize];
  int length = read(fd, buffer, kBufSize);
  intptr_t line_start = 0;
  CHECK_LT(length, kBufSize);  // Make the buffer bigger.
  CHECK_GT(length, 0);  // We have to find some data in the file.
  while (line_start < length) {
    if (buffer[line_start] == '\n') {
      line_start++;
      continue;
    }
    intptr_t position = line_start;
    uintptr_t start = ReadLong(buffer, &position, 16);
    CHECK_EQ(buffer[position++], '-');
    uintptr_t end = ReadLong(buffer, &position, 16);
    CHECK_EQ(buffer[position++], ' ');
    CHECK(buffer[position] == '-' || buffer[position] == 'r');
    bool read_permission = (buffer[position++] == 'r');
    CHECK(buffer[position] == '-' || buffer[position] == 'w');
    bool write_permission = (buffer[position++] == 'w');
    CHECK(buffer[position] == '-' || buffer[position] == 'x');
    bool execute_permission = (buffer[position++] == 'x');
    CHECK(buffer[position] == '-' || buffer[position] == 'p');
    bool private_mapping = (buffer[position++] == 'p');
    CHECK_EQ(buffer[position++], ' ');
    uintptr_t offset = ReadLong(buffer, &position, 16);
    USE(offset);
    CHECK_EQ(buffer[position++], ' ');
    uintptr_t major = ReadLong(buffer, &position, 16);
    USE(major);
    CHECK_EQ(buffer[position++], ':');
    uintptr_t minor = ReadLong(buffer, &position, 16);
    USE(minor);
    CHECK_EQ(buffer[position++], ' ');
    uintptr_t inode = ReadLong(buffer, &position, 10);
    while (position < length && buffer[position] != '\n') position++;
    if ((read_permission || write_permission || execute_permission) &&
        private_mapping && inode == 0) {
      memory_use += (end - start);
    }

    line_start = position;
  }
  close(fd);
  return memory_use;
}


intptr_t ShortLivingIsolate() {
  v8::Isolate* isolate = v8::Isolate::New();
  { v8::Isolate::Scope isolate_scope(isolate);
    v8::Locker lock(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    CHECK(!context.IsEmpty());
  }
  isolate->Dispose();
  return MemoryInUse();
}


TEST(RegressJoinThreadsOnIsolateDeinit) {
  intptr_t size_limit = ShortLivingIsolate() * 2;
  for (int i = 0; i < 10; i++) {
    CHECK_GT(size_limit, ShortLivingIsolate());
  }
}

#endif  // __linux__ and !USE_SIMULATOR
