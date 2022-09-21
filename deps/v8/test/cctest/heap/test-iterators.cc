// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-object.h"
#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/read-only-spaces.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/roots/roots-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace heap {

TEST(HeapObjectIteratorNullPastEnd) {
  HeapObjectIterator iterator(CcTest::heap());
  while (!iterator.Next().is_null()) {
  }
  for (int i = 0; i < 20; i++) {
    CHECK(iterator.Next().is_null());
  }
}

TEST(ReadOnlyHeapObjectIteratorNullPastEnd) {
  ReadOnlyHeapObjectIterator iterator(CcTest::read_only_heap());
  while (!iterator.Next().is_null()) {
  }
  for (int i = 0; i < 20; i++) {
    CHECK(iterator.Next().is_null());
  }
}

TEST(CombinedHeapObjectIteratorNullPastEnd) {
  CombinedHeapObjectIterator iterator(CcTest::heap());
  while (!iterator.Next().is_null()) {
  }
  for (int i = 0; i < 20; i++) {
    CHECK(iterator.Next().is_null());
  }
}

namespace {
// An arbitrary object guaranteed to live on the non-read-only heap.
Object CreateWritableObject() {
  return *v8::Utils::OpenHandle(*v8::Object::New(CcTest::isolate()));
}
}  // namespace

TEST(ReadOnlyHeapObjectIterator) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());
  const Object sample_object = CreateWritableObject();
  ReadOnlyHeapObjectIterator iterator(CcTest::read_only_heap());

  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    CHECK(ReadOnlyHeap::Contains(obj));
    CHECK(!CcTest::heap()->Contains(obj));
    CHECK_NE(sample_object, obj);
  }
}

TEST(HeapObjectIterator) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());
  const Object sample_object = CreateWritableObject();
  HeapObjectIterator iterator(CcTest::heap());
  bool seen_sample_object = false;

  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    CHECK_IMPLIES(!v8_flags.enable_third_party_heap,
                  !ReadOnlyHeap::Contains(obj));
    CHECK(CcTest::heap()->Contains(obj));
    if (sample_object.SafeEquals(obj)) seen_sample_object = true;
  }
  CHECK(seen_sample_object);
}

TEST(CombinedHeapObjectIterator) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());
  const Object sample_object = CreateWritableObject();
  CombinedHeapObjectIterator iterator(CcTest::heap());
  bool seen_sample_object = false;

  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    CHECK(IsValidHeapObject(CcTest::heap(), obj));
    if (sample_object.SafeEquals(obj)) seen_sample_object = true;
  }
  CHECK(seen_sample_object);
}

TEST(PagedSpaceIterator) {
  Heap* const heap = CcTest::heap();
  PagedSpaceIterator iterator(heap);
  CHECK_EQ(iterator.Next(), reinterpret_cast<PagedSpace*>(heap->old_space()));
  CHECK_EQ(iterator.Next(), reinterpret_cast<PagedSpace*>(heap->code_space()));
  CHECK_EQ(iterator.Next(), reinterpret_cast<PagedSpace*>(heap->map_space()));
  for (int i = 0; i < 20; i++) {
    CHECK_NULL(iterator.Next());
  }
}

TEST(SpaceIterator) {
  auto* const read_only_space = CcTest::read_only_heap()->read_only_space();
  for (SpaceIterator it(CcTest::heap()); it.HasNext();) {
    // ReadOnlySpace is not actually a Space but is instead a BaseSpace, but
    // ensure it's not been inserted incorrectly.
    CHECK_NE(it.Next(), reinterpret_cast<BaseSpace*>(read_only_space));
  }
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
