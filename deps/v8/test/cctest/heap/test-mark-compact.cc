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

#include "include/v8-locker.h"
#include "src/handles/global-handles.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

TEST(Promotion) {
  if (v8_flags.single_generation) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  {
    v8::HandleScope sc(CcTest::isolate());
    Heap* heap = isolate->heap();

    heap::SealCurrentObjects(heap);

    int array_length = heap::FixedArrayLenFromSize(kMaxRegularHeapObjectSize);
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(array_length);

    // Array should be in the new space.
    CHECK(heap->InSpace(*array, NEW_SPACE));
    CcTest::CollectAllGarbage();
    CcTest::CollectAllGarbage();
    CHECK(heap->InSpace(*array, OLD_SPACE));
  }
}

// This is the same as Factory::NewMap, except it doesn't retry on
// allocation failure.
AllocationResult HeapTester::AllocateMapForTest(Isolate* isolate) {
  Heap* heap = isolate->heap();
  HeapObject obj;
  AllocationResult alloc = heap->AllocateRaw(Map::kSize, AllocationType::kMap);
  if (!alloc.To(&obj)) return alloc;
  obj.set_map_after_allocation(ReadOnlyRoots(heap).meta_map(),
                               SKIP_WRITE_BARRIER);
  return AllocationResult::FromObject(isolate->factory()->InitializeMap(
      Map::cast(obj), JS_OBJECT_TYPE, JSObject::kHeaderSize,
      TERMINAL_FAST_ELEMENTS_KIND, 0, heap));
}

// This is the same as Factory::NewFixedArray, except it doesn't retry
// on allocation failure.
AllocationResult HeapTester::AllocateFixedArrayForTest(
    Heap* heap, int length, AllocationType allocation) {
  DCHECK(length >= 0 && length <= FixedArray::kMaxLength);
  int size = FixedArray::SizeFor(length);
  HeapObject obj;
  {
    AllocationResult result = heap->AllocateRaw(size, allocation);
    if (!result.To(&obj)) return result;
  }
  obj.set_map_after_allocation(ReadOnlyRoots(heap).fixed_array_map(),
                               SKIP_WRITE_BARRIER);
  FixedArray array = FixedArray::cast(obj);
  array.set_length(length);
  MemsetTagged(array.data_start(), ReadOnlyRoots(heap).undefined_value(),
               length);
  return AllocationResult::FromObject(array);
}

HEAP_TEST(MarkCompactCollector) {
  v8_flags.incremental_marking = false;
  v8_flags.retain_maps_for_n_gc = 0;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<JSGlobalObject> global(isolate->context().global_object(), isolate);

  // call mark-compact when heap is empty
  CcTest::CollectGarbage(OLD_SPACE);

  AllocationResult allocation;
  if (!v8_flags.single_generation) {
    // keep allocating garbage in new space until it fails
    const int arraysize = 100;
    do {
      allocation =
          AllocateFixedArrayForTest(heap, arraysize, AllocationType::kYoung);
    } while (!allocation.IsFailure());
    CcTest::CollectGarbage(NEW_SPACE);
    AllocateFixedArrayForTest(heap, arraysize, AllocationType::kYoung)
        .ToObjectChecked();
  }

  // keep allocating maps until it fails
  do {
    allocation = AllocateMapForTest(isolate);
  } while (!allocation.IsFailure());
  CcTest::CollectGarbage(MAP_SPACE);
  AllocateMapForTest(isolate).ToObjectChecked();

  { HandleScope scope(isolate);
    // allocate a garbage
    Handle<String> func_name = factory->InternalizeUtf8String("theFunction");
    Handle<JSFunction> function = factory->NewFunctionForTesting(func_name);
    Object::SetProperty(isolate, global, func_name, function).Check();

    factory->NewJSObject(function);
  }

  CcTest::CollectGarbage(OLD_SPACE);

  { HandleScope scope(isolate);
    Handle<String> func_name = factory->InternalizeUtf8String("theFunction");
    CHECK(Just(true) == JSReceiver::HasOwnProperty(isolate, global, func_name));
    Handle<Object> func_value =
        Object::GetProperty(isolate, global, func_name).ToHandleChecked();
    CHECK(func_value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
    Handle<JSObject> obj = factory->NewJSObject(function);

    Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
    Object::SetProperty(isolate, global, obj_name, obj).Check();
    Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
    Object::SetProperty(isolate, obj, prop_name, twenty_three).Check();
  }

  CcTest::CollectGarbage(OLD_SPACE);

  { HandleScope scope(isolate);
    Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
    CHECK(Just(true) == JSReceiver::HasOwnProperty(isolate, global, obj_name));
    Handle<Object> object =
        Object::GetProperty(isolate, global, obj_name).ToHandleChecked();
    CHECK(object->IsJSObject());
    Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    CHECK_EQ(*Object::GetProperty(isolate, object, prop_name).ToHandleChecked(),
             Smi::FromInt(23));
  }
}

HEAP_TEST(DoNotEvacuatePinnedPages) {
  if (!v8_flags.compact || !v8_flags.single_generation) return;

  v8_flags.compact_on_every_full_gc = true;

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope sc(CcTest::isolate());
  Heap* heap = isolate->heap();

  heap::SealCurrentObjects(heap);

  auto handles = heap::CreatePadding(
      heap, static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()),
      AllocationType::kOld);

  Page* page = Page::FromHeapObject(*handles.front());

  CHECK(heap->InSpace(*handles.front(), OLD_SPACE));
  page->SetFlag(MemoryChunk::PINNED);

  CcTest::CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted(
      MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);

  // The pinned flag should prevent the page from moving.
  for (Handle<FixedArray> object : handles) {
    CHECK_EQ(page, Page::FromHeapObject(*object));
  }

  page->ClearFlag(MemoryChunk::PINNED);

  CcTest::CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted(
      MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);

  // `compact_on_every_full_gc` ensures that this page is an evacuation
  // candidate, so with the pin flag cleared compaction should now move it.
  for (Handle<FixedArray> object : handles) {
    CHECK_NE(page, Page::FromHeapObject(*object));
  }
}

HEAP_TEST(ObjectStartBitmap) {
#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope sc(CcTest::isolate());

  Heap* heap = isolate->heap();
  heap::SealCurrentObjects(heap);

  auto* factory = isolate->factory();

  Handle<HeapObject> h1 = factory->NewStringFromStaticChars("hello");
  Handle<HeapObject> h2 = factory->NewStringFromStaticChars("world");

  HeapObject obj1 = *h1;
  HeapObject obj2 = *h2;
  Page* page1 = Page::FromHeapObject(obj1);
  Page* page2 = Page::FromHeapObject(obj2);

  CHECK(page1->object_start_bitmap()->CheckBit(obj1.address()));
  CHECK(page2->object_start_bitmap()->CheckBit(obj2.address()));

  {
    // We need a safepoint for calling FindBasePtr.
    SafepointScope scope(heap);

    for (int k = 0; k < obj1.Size(); ++k) {
      Address obj1_inner_ptr = obj1.address() + k;
      CHECK_EQ(obj1.address(),
               page1->object_start_bitmap()->FindBasePtr(obj1_inner_ptr));
    }
    for (int k = 0; k < obj2.Size(); ++k) {
      Address obj2_inner_ptr = obj2.address() + k;
      CHECK_EQ(obj2.address(),
               page2->object_start_bitmap()->FindBasePtr(obj2_inner_ptr));
    }
  }

  // TODO(v8:12851): Patch the location of handle h2 with an inner pointer.
  // For now, garbage collection doesn't work with inner pointers in handles,
  // so we're sticking to a zero offset.
  const size_t offset = 0;
  h2.PatchValue(String::FromAddress(h2->address() + offset));

  CcTest::CollectAllGarbage();

  obj1 = *h1;
  obj2 = HeapObject::FromAddress(h2->address() - offset);
  page1 = Page::FromHeapObject(obj1);
  page2 = Page::FromHeapObject(obj2);

  CHECK(obj1.IsString());
  CHECK(obj2.IsString());

  // Bits set in the object_start_bitmap are not preserved when objects are
  // evacuated.
  CHECK(!page1->object_start_bitmap()->CheckBit(obj1.address()));
  CHECK(!page2->object_start_bitmap()->CheckBit(obj2.address()));

  {
    // We need a safepoint for calling FindBasePtr.
    SafepointScope scope(heap);

    // After FindBasePtr, the bits should be properly set again.
    for (int k = 0; k < obj1.Size(); ++k) {
      Address obj1_inner_ptr = obj1.address() + k;
      CHECK_EQ(obj1.address(),
               page1->object_start_bitmap()->FindBasePtr(obj1_inner_ptr));
    }
    CHECK(page1->object_start_bitmap()->CheckBit(obj1.address()));
    for (int k = obj2.Size() - 1; k >= 0; --k) {
      Address obj2_inner_ptr = obj2.address() + k;
      CHECK_EQ(obj2.address(),
               page2->object_start_bitmap()->FindBasePtr(obj2_inner_ptr));
    }
    CHECK(page2->object_start_bitmap()->CheckBit(obj2.address()));
  }
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
}

// TODO(1600): compaction of map space is temporary removed from GC.
#if 0
static Handle<Map> CreateMap(Isolate* isolate) {
  return isolate->factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
}

TEST(MapCompact) {
  v8_flags.max_map_space_pages = 16;
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
  CcTest::CollectAllGarbage(Heap::kForceCompactionMask);
  // And now map pointers should be encodable again.
  CHECK(CcTest::heap()->map_space()->MapPointersEncodable());
}
#endif

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

  const int kBufSize = 20000;
  char buffer[kBufSize];
  ssize_t length = read(fd, buffer, kBufSize);
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
    CHECK(buffer[position] == 's' || buffer[position] == 'p');
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
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  { v8::Isolate::Scope isolate_scope(isolate);
    v8::Locker lock(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    CHECK(!context.IsEmpty());
  }
  isolate->Dispose();
  return MemoryInUse();
}

UNINITIALIZED_TEST(RegressJoinThreadsOnIsolateDeinit) {
  // Memory is measured, do not allocate in background thread.
  v8_flags.stress_concurrent_allocation = false;
  intptr_t size_limit = ShortLivingIsolate() * 2;
  for (int i = 0; i < 10; i++) {
    CHECK_GT(size_limit, ShortLivingIsolate());
  }
}

TEST(Regress5829) {
  if (!v8_flags.incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope sc(CcTest::isolate());
  Heap* heap = isolate->heap();
  heap::SealCurrentObjects(heap);
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  CHECK(marking->black_allocation());
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(10, AllocationType::kOld);
  Address old_end = array->address() + array->Size();
  // Right trim the array without clearing the mark bits.
  array->set_length(9);
  heap->CreateFillerObjectAt(old_end - kTaggedSize, kTaggedSize);
  heap->old_space()->FreeLinearAllocationArea();
  Page* page = Page::FromAddress(array->address());
  MarkingState* marking_state = marking->marking_state();
  for (auto object_and_size :
       LiveObjectRange<kGreyObjects>(page, marking_state->bitmap(page))) {
    CHECK(!object_and_size.first.IsFreeSpaceOrFiller());
  }
}

#endif  // __linux__ and !USE_SIMULATOR

}  // namespace heap
}  // namespace internal
}  // namespace v8
