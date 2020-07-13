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
#include "src/heap/local-heap.h"
#include "src/heap/safepoint.h"
#include "src/objects/heap-number.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

static constexpr int kNumHandles = kHandleBlockSize * 2 + kHandleBlockSize / 2;

class PersistentHandlesThread final : public v8::base::Thread {
 public:
  PersistentHandlesThread(Heap* heap, std::vector<Handle<HeapNumber>> handles,
                          std::unique_ptr<PersistentHandles> ph, Address object,
                          base::Semaphore* sema_started,
                          base::Semaphore* sema_gc_finished)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        handles_(std::move(handles)),
        ph_(std::move(ph)),
        object_(object),
        sema_started_(sema_started),
        sema_gc_finished_(sema_gc_finished) {}

  void Run() override {
    LocalHeap local_heap(heap_, std::move(ph_));
    LocalHandleScope scope(&local_heap);

    for (int i = 0; i < kNumHandles; i++) {
      handles_.push_back(
          Handle<HeapNumber>::cast(local_heap.NewPersistentHandle(object_)));
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

  Heap* heap_;
  std::vector<Handle<HeapNumber>> handles_;
  std::unique_ptr<PersistentHandles> ph_;
  Address object_;
  base::Semaphore* sema_started_;
  base::Semaphore* sema_gc_finished_;
};

TEST(CreatePersistentHandles) {
  CcTest::InitializeVM();
  FLAG_local_heaps = true;
  Isolate* isolate = CcTest::i_isolate();

  Address object = kNullAddress;
  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<HeapNumber>> handles;

  HandleScope handle_scope(isolate);
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(42.0);

  object = number->ptr();

  for (int i = 0; i < kNumHandles; i++) {
    handles.push_back(Handle<HeapNumber>::cast(ph->NewHandle(object)));
  }

  base::Semaphore sema_started(0);
  base::Semaphore sema_gc_finished(0);

  // pass persistent handles to background thread
  std::unique_ptr<PersistentHandlesThread> thread(new PersistentHandlesThread(
      isolate->heap(), std::move(handles), std::move(ph), object, &sema_started,
      &sema_gc_finished));
  CHECK(thread->Start());

  sema_started.Wait();

  CcTest::CollectAllGarbage();
  sema_gc_finished.Signal();

  thread->Join();

  // get persistent handles back to main thread
  ph = std::move(thread->ph_);
  ph->NewHandle(number->ptr());
}

}  // namespace internal
}  // namespace v8
