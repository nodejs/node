// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/roots/roots-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace heap {

TEST(HeapIteratorNullPastEnd) {
  HeapIterator iterator(CcTest::heap());
  while (!iterator.next().is_null()) {
  }
  for (int i = 0; i < 20; i++) {
    CHECK(iterator.next().is_null());
  }
}

TEST(ReadOnlyHeapIteratorNullPastEnd) {
  ReadOnlyHeapIterator iterator(CcTest::heap()->read_only_heap());
  while (!iterator.Next().is_null()) {
  }
  for (int i = 0; i < 20; i++) {
    CHECK(iterator.Next().is_null());
  }
}

TEST(CombinedHeapIteratorNullPastEnd) {
  CombinedHeapIterator iterator(CcTest::heap());
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

TEST(ReadOnlyHeapIterator) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());
  const Object sample_object = CreateWritableObject();
  ReadOnlyHeapIterator iterator(CcTest::read_only_heap());

  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    CHECK(ReadOnlyHeap::Contains(obj));
    CHECK(!CcTest::heap()->Contains(obj));
    CHECK_NE(sample_object, obj);
  }
}

TEST(HeapIterator) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());
  const Object sample_object = CreateWritableObject();
  HeapIterator iterator(CcTest::heap());
  bool seen_sample_object = false;

  for (HeapObject obj = iterator.next(); !obj.is_null();
       obj = iterator.next()) {
    CHECK(!ReadOnlyHeap::Contains(obj));
    CHECK(CcTest::heap()->Contains(obj));
    if (sample_object == obj) seen_sample_object = true;
  }
  CHECK(seen_sample_object);
}

TEST(CombinedHeapIterator) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());
  const Object sample_object = CreateWritableObject();
  CombinedHeapIterator iterator(CcTest::heap());
  bool seen_sample_object = false;

  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    CHECK(IsValidHeapObject(CcTest::heap(), obj));
    if (sample_object == obj) seen_sample_object = true;
  }
  CHECK(seen_sample_object);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
