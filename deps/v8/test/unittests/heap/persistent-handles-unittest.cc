// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/persistent-handles.h"

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using PersistentHandlesTest = TestWithIsolate;

TEST_F(PersistentHandlesTest, OrderOfBlocks) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  handle(ReadOnlyRoots(heap).empty_string(), isolate);
  HandleScopeData* data = isolate->handle_scope_data();

  Address* next;
  Address* limit;
  Handle<String> first_empty, last_empty;
  std::unique_ptr<PersistentHandles> ph;

  {
    PersistentHandlesScope persistent_scope(isolate);

    // fill block
    first_empty = handle(ReadOnlyRoots(heap).empty_string(), isolate);

    while (data->next < data->limit) {
      handle(ReadOnlyRoots(heap).empty_string(), isolate);
    }

    // add second block and two more handles on it
    handle(ReadOnlyRoots(heap).empty_string(), isolate);
    last_empty = handle(ReadOnlyRoots(heap).empty_string(), isolate);

    // remember next and limit in second block
    next = data->next;
    limit = data->limit;

    ph = persistent_scope.Detach();
  }

  CHECK_EQ(ph->block_next_, next);
  CHECK_EQ(ph->block_limit_, limit);

  CHECK_EQ(first_empty->length(), 0);
  CHECK_EQ(last_empty->length(), 0);
  CHECK_EQ(*first_empty, *last_empty);
}

namespace {
class CounterDummyVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    counter += end - start;
  }
  size_t counter = 0;
};

size_t count_handles(Isolate* isolate) {
  CounterDummyVisitor visitor;
  isolate->handle_scope_implementer()->Iterate(&visitor);
  return visitor.counter;
}

size_t count_handles(PersistentHandles* ph) {
  CounterDummyVisitor visitor;
  ph->Iterate(&visitor);
  return visitor.counter;
}
}  // namespace

TEST_F(PersistentHandlesTest, Iterate) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));
  HandleScopeData* data = isolate->handle_scope_data();

  size_t handles_in_empty_scope = count_handles(isolate);

  Handle<Object> init(ReadOnlyRoots(heap).empty_string(), isolate);
  Address* old_limit = data->limit;
  CHECK_EQ(count_handles(isolate), handles_in_empty_scope + 1);

  std::unique_ptr<PersistentHandles> ph;
  Handle<String> verify_handle;

  {
    PersistentHandlesScope persistent_scope(isolate);
    verify_handle = handle(ReadOnlyRoots(heap).empty_string(), isolate);
    CHECK_NE(old_limit, data->limit);
    CHECK_EQ(count_handles(isolate), handles_in_empty_scope + 2);
    ph = persistent_scope.Detach();
  }

#if DEBUG
  CHECK(ph->Contains(verify_handle.location()));
#else
  USE(verify_handle);
#endif

  ph->NewHandle(ReadOnlyRoots(heap).empty_string());
  CHECK_EQ(count_handles(ph.get()), 2);
  CHECK_EQ(count_handles(isolate), handles_in_empty_scope + 1);
}

}  // namespace internal
}  // namespace v8
