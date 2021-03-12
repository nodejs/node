// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/api/api.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/local-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/heap/safepoint.h"
#include "src/objects/heap-number.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

namespace {

class LocalHandlesThread final : public v8::base::Thread {
 public:
  LocalHandlesThread(Heap* heap, Address object, base::Semaphore* sema_started,
                     base::Semaphore* sema_gc_finished)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        object_(object),
        sema_started_(sema_started),
        sema_gc_finished_(sema_gc_finished) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground);
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    static constexpr int kNumHandles =
        kHandleBlockSize * 2 + kHandleBlockSize / 2;

    std::vector<Handle<HeapNumber>> handles;
    handles.reserve(kNumHandles);

    for (int i = 0; i < kNumHandles; i++) {
      Handle<HeapNumber> number = handle(
          HeapNumber::cast(HeapObject::FromAddress(object_)), &local_heap);
      handles.push_back(number);
    }

    sema_started_->Signal();

    {
      ParkedScope scope(&local_heap);
      sema_gc_finished_->Wait();
    }

    for (Handle<HeapNumber> handle : handles) {
      CHECK_EQ(42.0, handle->value());
    }
  }

  Heap* heap_;
  Address object_;
  base::Semaphore* sema_started_;
  base::Semaphore* sema_gc_finished_;
};

TEST(CreateLocalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  Address object = kNullAddress;

  {
    HandleScope handle_scope(isolate);
    Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(42.0);
    object = (*number).address();
  }

  base::Semaphore sema_started(0);
  base::Semaphore sema_gc_finished(0);

  std::unique_ptr<LocalHandlesThread> thread(new LocalHandlesThread(
      isolate->heap(), object, &sema_started, &sema_gc_finished));
  CHECK(thread->Start());

  sema_started.Wait();

  CcTest::CollectAllGarbage();
  sema_gc_finished.Signal();

  thread->Join();
}

TEST(CreateLocalHandlesWithoutLocalHandleScope) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  handle(Smi::FromInt(17), isolate->main_thread_local_heap());
}

TEST(DereferenceLocalHandle) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  // Create a PersistentHandle to create the LocalHandle, and thus not have a
  // HandleScope present to override the LocalHandleScope.
  std::unique_ptr<PersistentHandles> phs = isolate->NewPersistentHandles();
  Handle<HeapNumber> ph;
  {
    HandleScope handle_scope(isolate);
    Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(42.0);
    ph = phs->NewHandle(number);
  }
  {
    LocalHeap local_heap(isolate->heap(), ThreadKind::kBackground,
                         std::move(phs));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);
    Handle<HeapNumber> local_number = handle(*ph, &local_heap);
    CHECK_EQ(42, local_number->value());
  }
}

TEST(DereferenceLocalHandleFailsWhenDisallowed) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  // Create a PersistentHandle to create the LocalHandle, and thus not have a
  // HandleScope present to override the LocalHandleScope.
  std::unique_ptr<PersistentHandles> phs = isolate->NewPersistentHandles();
  Handle<HeapNumber> ph;
  {
    HandleScope handle_scope(isolate);
    Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(42.0);
    ph = phs->NewHandle(number);
  }
  {
    LocalHeap local_heap(isolate->heap(), ThreadKind::kBackground,
                         std::move(phs));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);
    Handle<HeapNumber> local_number = handle(*ph, &local_heap);
    DisallowHandleDereference disallow_scope;
    CHECK_EQ(42, local_number->value());
  }
}

}  // anonymous namespace

}  // namespace internal
}  // namespace v8
