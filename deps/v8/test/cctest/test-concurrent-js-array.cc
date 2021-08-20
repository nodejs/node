// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"
#include "src/base/platform/semaphore.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/objects/js-array-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

static constexpr int kNumArrays = 1024;

namespace {

class BackgroundThread final : public v8::base::Thread {
 public:
  BackgroundThread(Heap* heap, std::vector<Handle<JSArray>> handles,
                   std::unique_ptr<PersistentHandles> ph,
                   base::Semaphore* sema_started)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        handles_(std::move(handles)),
        ph_(std::move(ph)),
        sema_started_(sema_started) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);
    Isolate* isolate = heap_->isolate();

    for (int i = 0; i < kNumArrays; i++) {
      handles_[i] = local_heap.NewPersistentHandle(handles_[i]);
    }

    sema_started_->Signal();

    // Iterate in the opposite directions as the main thread to make a race at
    // some point more likely.
    static constexpr int kIndex = 1;
    for (int i = 0; i < kNumArrays; i++) {
      Handle<JSArray> x = handles_[i];
      Handle<FixedArrayBase> elements =
          local_heap.NewPersistentHandle(x->elements(isolate, kRelaxedLoad));
      ElementsKind elements_kind = x->map(isolate).elements_kind();

      // Mirroring the conditions in JSArrayRef::GetOwnCowElement.
      if (!IsSmiOrObjectElementsKind(elements_kind)) continue;
      if (elements->map() != ReadOnlyRoots(isolate).fixed_cow_array_map()) {
        continue;
      }

      base::Optional<Object> result =
          ConcurrentLookupIterator::TryGetOwnCowElement(
              isolate, FixedArray::cast(*elements), elements_kind,
              Smi::ToInt(x->length(isolate, kRelaxedLoad)), kIndex);

      if (result.has_value()) {
        // On any success, the elements at index 1 must be the original value
        // Smi(1).
        CHECK(result.value().IsSmi());
        CHECK_EQ(Smi::ToInt(result.value()), 1);
      }
    }
  }

 private:
  Heap* heap_;
  std::vector<Handle<JSArray>> handles_;
  std::unique_ptr<PersistentHandles> ph_;
  base::Semaphore* sema_started_;
};

TEST(ArrayWithCowElements) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<JSArray>> handles;
  std::vector<Handle<JSArray>> persistent_handles;

  HandleScope handle_scope(isolate);

  // Create kNumArrays arrays with COW backing stores.
  CompileRun(
      "function f() { return [0,1,2,3,4]; }\n"
      "const xs = [];\n"
      "let i = 0;\n");

  for (int i = 0; i < kNumArrays; i++) {
    Handle<JSArray> x = Handle<JSArray>::cast(Utils::OpenHandle(
        *CompileRunChecked(CcTest::isolate(), "xs[i++] = f();")));
    CHECK_EQ(x->elements().map(), ReadOnlyRoots(isolate).fixed_cow_array_map());
    handles.push_back(x);
    persistent_handles.push_back(ph->NewHandle(x));
  }

  base::Semaphore sema_started(0);

  // Pass persistent handles to background thread.
  std::unique_ptr<BackgroundThread> thread(new BackgroundThread(
      isolate->heap(), persistent_handles, std::move(ph), &sema_started));
  CHECK(thread->Start());

  sema_started.Wait();

  // On the main thread, mutate the arrays, converting to a non-COW backing
  // store.
  static const char* const kMutators[] = {
      "xs[--i].length--;", "xs[--i].length++;",  "xs[--i].length = 0;",
      "xs[--i][1] = 42;",  "delete xs[--i][1];", "xs[--i].push(42);",
      "xs[--i].pop();",    "xs[--i][1] = 1.5;",  "xs[--i][1] = {};",
  };
  static const int kNumMutators = arraysize(kMutators);

  for (int i = kNumArrays - 1; i >= 0; i--) {
    CompileRunChecked(CcTest::isolate(), kMutators[i % kNumMutators]);
    CHECK_NE(handles[i]->elements().map(),
             ReadOnlyRoots(isolate).fixed_cow_array_map());
  }

  thread->Join();
}

}  // anonymous namespace
}  // namespace internal
}  // namespace v8
