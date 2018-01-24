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

#include "src/global-handles.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

TEST(Promotion) {
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

HEAP_TEST(NoPromotion) {
  // Page promotion allows pages to be moved to old space even in the case of
  // OOM scenarios.
  FLAG_page_promotion = false;

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  {
    v8::HandleScope sc(CcTest::isolate());
    Heap* heap = isolate->heap();

    heap::SealCurrentObjects(heap);

    int array_length = heap::FixedArrayLenFromSize(kMaxRegularHeapObjectSize);
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(array_length);

    heap->set_force_oom(true);
    // Array should be in the new space.
    CHECK(heap->InSpace(*array, NEW_SPACE));
    CcTest::CollectAllGarbage();
    CcTest::CollectAllGarbage();
    CHECK(heap->InSpace(*array, NEW_SPACE));
  }
}

HEAP_TEST(MarkCompactCollector) {
  FLAG_incremental_marking = false;
  FLAG_retain_maps_for_n_gc = 0;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<JSGlobalObject> global(isolate->context()->global_object());

  // call mark-compact when heap is empty
  CcTest::CollectGarbage(OLD_SPACE);

  // keep allocating garbage in new space until it fails
  const int arraysize = 100;
  AllocationResult allocation;
  do {
    allocation = heap->AllocateFixedArray(arraysize);
  } while (!allocation.IsRetry());
  CcTest::CollectGarbage(NEW_SPACE);
  heap->AllocateFixedArray(arraysize).ToObjectChecked();

  // keep allocating maps until it fails
  do {
    allocation = heap->AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  } while (!allocation.IsRetry());
  CcTest::CollectGarbage(MAP_SPACE);
  heap->AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize).ToObjectChecked();

  { HandleScope scope(isolate);
    // allocate a garbage
    Handle<String> func_name = factory->InternalizeUtf8String("theFunction");
    Handle<JSFunction> function = factory->NewFunctionForTest(func_name);
    JSReceiver::SetProperty(global, func_name, function, LanguageMode::kSloppy)
        .Check();

    factory->NewJSObject(function);
  }

  CcTest::CollectGarbage(OLD_SPACE);

  { HandleScope scope(isolate);
    Handle<String> func_name = factory->InternalizeUtf8String("theFunction");
    CHECK(Just(true) == JSReceiver::HasOwnProperty(global, func_name));
    Handle<Object> func_value =
        Object::GetProperty(global, func_name).ToHandleChecked();
    CHECK(func_value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
    Handle<JSObject> obj = factory->NewJSObject(function);

    Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
    JSReceiver::SetProperty(global, obj_name, obj, LanguageMode::kSloppy)
        .Check();
    Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
    JSReceiver::SetProperty(obj, prop_name, twenty_three, LanguageMode::kSloppy)
        .Check();
  }

  CcTest::CollectGarbage(OLD_SPACE);

  { HandleScope scope(isolate);
    Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
    CHECK(Just(true) == JSReceiver::HasOwnProperty(global, obj_name));
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


TEST(RegressJoinThreadsOnIsolateDeinit) {
  intptr_t size_limit = ShortLivingIsolate() * 2;
  for (int i = 0; i < 10; i++) {
    CHECK_GT(size_limit, ShortLivingIsolate());
  }
}

TEST(Regress5829) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope sc(CcTest::isolate());
  Heap* heap = isolate->heap();
  heap::SealCurrentObjects(heap);
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  marking->StartBlackAllocationForTesting();
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(10, TENURED);
  Address old_end = array->address() + array->Size();
  // Right trim the array without clearing the mark bits.
  array->set_length(9);
  heap->CreateFillerObjectAt(old_end - kPointerSize, kPointerSize,
                             ClearRecordedSlots::kNo);
  heap->old_space()->EmptyAllocationInfo();
  Page* page = Page::FromAddress(array->address());
  IncrementalMarking::MarkingState* marking_state = marking->marking_state();
  for (auto object_and_size :
       LiveObjectRange<kGreyObjects>(page, marking_state->bitmap(page))) {
    CHECK(!object_and_size.first->IsFiller());
  }
}

#endif  // __linux__ and !USE_SIMULATOR

}  // namespace heap
}  // namespace internal
}  // namespace v8
