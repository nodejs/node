// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/api/api.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/safepoint.h"
#include "src/objects/heap-number.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

static constexpr int kNumHandles = kHandleBlockSize * 2 + kHandleBlockSize / 2;

namespace {

class PersistentHandlesThread final : public v8::base::Thread {
 public:
  PersistentHandlesThread(Heap* heap, std::vector<Handle<HeapNumber>> handles,
                          std::unique_ptr<PersistentHandles> ph,
                          HeapNumber number, base::Semaphore* sema_started,
                          base::Semaphore* sema_gc_finished)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        handles_(std::move(handles)),
        ph_(std::move(ph)),
        number_(number),
        sema_started_(sema_started),
        sema_gc_finished_(sema_gc_finished) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    for (int i = 0; i < kNumHandles; i++) {
      handles_.push_back(local_heap.NewPersistentHandle(number_));
    }

    sema_started_->Signal();

    {
      ParkedScope scope(&local_heap);
      sema_gc_finished_->Wait();
    }

    for (Handle<HeapNumber> handle : handles_) {
      CHECK_EQ(42.0, handle->value());
    }

    CHECK_EQ(handles_.size(), kNumHandles * 2);

    CHECK(!ph_);
    ph_ = local_heap.DetachPersistentHandles();
  }

  std::unique_ptr<PersistentHandles> DetachPersistentHandles() {
    CHECK(ph_);
    return std::move(ph_);
  }

 private:
  Heap* heap_;
  std::vector<Handle<HeapNumber>> handles_;
  std::unique_ptr<PersistentHandles> ph_;
  HeapNumber number_;
  base::Semaphore* sema_started_;
  base::Semaphore* sema_gc_finished_;
};

TEST(CreatePersistentHandles) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<HeapNumber>> handles;

  HandleScope handle_scope(isolate);
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(42.0);

  for (int i = 0; i < kNumHandles; i++) {
    handles.push_back(ph->NewHandle(number));
  }

  base::Semaphore sema_started(0);
  base::Semaphore sema_gc_finished(0);

  // pass persistent handles to background thread
  std::unique_ptr<PersistentHandlesThread> thread(new PersistentHandlesThread(
      isolate->heap(), std::move(handles), std::move(ph), *number,
      &sema_started, &sema_gc_finished));
  CHECK(thread->Start());

  sema_started.Wait();

  CcTest::CollectAllGarbage();
  sema_gc_finished.Signal();

  thread->Join();

  // get persistent handles back to main thread
  ph = thread->DetachPersistentHandles();
  ph->NewHandle(number);
}

TEST(DereferencePersistentHandle) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> phs = isolate->NewPersistentHandles();
  Handle<HeapNumber> ph;
  {
    HandleScope handle_scope(isolate);
    Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(42.0);
    ph = phs->NewHandle(number);
  }
  {
    LocalHeap local_heap(isolate->heap(), ThreadKind::kMain, std::move(phs));
    UnparkedScope scope(&local_heap);
    CHECK_EQ(42, ph->value());
    DisallowHandleDereference disallow_scope;
    CHECK_EQ(42, ph->value());
  }
}

TEST(NewPersistentHandleFailsWhenParked) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  LocalHeap local_heap(isolate->heap(), ThreadKind::kMain);
  // Fail here in debug mode: Persistent handles can't be created if local heap
  // is parked
  local_heap.NewPersistentHandle(Smi::FromInt(1));
}

TEST(NewPersistentHandleFailsWhenParkedExplicit) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  LocalHeap local_heap(isolate->heap(), ThreadKind::kMain,
                       isolate->NewPersistentHandles());
  // Fail here in debug mode: Persistent handles can't be created if local heap
  // is parked
  local_heap.NewPersistentHandle(Smi::FromInt(1));
}

}  // anonymous namespace

}  // namespace internal
}  // namespace v8
