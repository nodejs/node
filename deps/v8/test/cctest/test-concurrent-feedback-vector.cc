// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <unordered_set>

#include "src/api/api.h"
#include "src/base/platform/semaphore.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

namespace {

// kCycles is large enough to ensure we see every state we are interested in.
const int kCycles = 1000;
static std::atomic<bool> all_states_seen{false};

class FeedbackVectorExplorationThread final : public v8::base::Thread {
 public:
  FeedbackVectorExplorationThread(Heap* heap, base::Semaphore* sema_started,
                                  base::Semaphore* vector_ready,
                                  base::Semaphore* vector_consumed,
                                  std::unique_ptr<PersistentHandles> ph,
                                  Handle<FeedbackVector> feedback_vector)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        feedback_vector_(feedback_vector),
        ph_(std::move(ph)),
        sema_started_(sema_started),
        vector_ready_(vector_ready),
        vector_consumed_(vector_consumed) {}

  using InlineCacheSet = std::unordered_set<InlineCacheState, std::hash<int>>;
  bool AllRequiredStatesSeen(const InlineCacheSet& found) {
    auto end = found.end();
    return (found.find(UNINITIALIZED) != end &&
            found.find(MONOMORPHIC) != end && found.find(POLYMORPHIC) != end &&
            found.find(MEGAMORPHIC) != end);
  }

  void Run() override {
    Isolate* isolate = heap_->isolate();
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope scope(&local_heap);

    // Get the feedback vector
    NexusConfig nexus_config =
        NexusConfig::FromBackgroundThread(isolate, &local_heap);
    FeedbackSlot slot(0);

    // FeedbackVectorExplorationThread signals that it's beginning it's loop.
    sema_started_->Signal();

    InlineCacheSet found_states;
    for (int i = 0; i < kCycles; i++) {
      FeedbackNexus nexus(feedback_vector_, slot, nexus_config);
      auto state = nexus.ic_state();
      if (state == MONOMORPHIC || state == POLYMORPHIC) {
        MapHandles maps;
        nexus.ExtractMaps(&maps);
        for (unsigned int i = 0; i < maps.size(); i++) {
          CHECK(maps[i]->IsMap());
        }
      }

      if (found_states.find(state) == found_states.end()) {
        found_states.insert(state);
        if (AllRequiredStatesSeen(found_states)) {
          // We are finished.
          break;
        }
      }
    }

    if (!AllRequiredStatesSeen(found_states)) {
      // Repeat the exercise with an explicit handshaking protocol. This ensures
      // at least coverage of the necessary code paths even though it is
      // avoiding actual concurrency. I found that in test runs, there is always
      // one or two bots that have a thread interleaving that doesn't allow all
      // states to be seen. This is for that situation.
      vector_ready_->Wait();
      fprintf(stderr, "Worker beginning to check for uninitialized\n");
      {
        FeedbackNexus nexus(feedback_vector_, slot, nexus_config);
        auto state = nexus.ic_state();
        CHECK_EQ(state, UNINITIALIZED);
      }
      vector_consumed_->Signal();
      vector_ready_->Wait();
      fprintf(stderr, "Worker beginning to check for monomorphic\n");
      {
        FeedbackNexus nexus(feedback_vector_, slot, nexus_config);
        auto state = nexus.ic_state();
        CHECK_EQ(state, MONOMORPHIC);
        MapHandles maps;
        nexus.ExtractMaps(&maps);
        CHECK(maps[0]->IsMap());
      }
      vector_consumed_->Signal();
      vector_ready_->Wait();
      fprintf(stderr, "Worker beginning to check for polymorphic\n");
      {
        FeedbackNexus nexus(feedback_vector_, slot, nexus_config);
        auto state = nexus.ic_state();
        CHECK_EQ(state, POLYMORPHIC);
        MapHandles maps;
        nexus.ExtractMaps(&maps);
        for (unsigned int i = 0; i < maps.size(); i++) {
          CHECK(maps[i]->IsMap());
        }
      }
      vector_consumed_->Signal();
      vector_ready_->Wait();
      fprintf(stderr, "Worker beginning to check for megamorphic\n");
      {
        FeedbackNexus nexus(feedback_vector_, slot, nexus_config);
        auto state = nexus.ic_state();
        CHECK_EQ(state, MEGAMORPHIC);
      }
    }

    all_states_seen.store(true, std::memory_order_release);
    vector_consumed_->Signal();

    CHECK(!ph_);
    ph_ = local_heap.DetachPersistentHandles();
  }

  Heap* heap_;
  Handle<FeedbackVector> feedback_vector_;
  std::unique_ptr<PersistentHandles> ph_;
  base::Semaphore* sema_started_;

  // These two semaphores control the explicit handshaking mode in case we
  // didn't see all states within kCycles loops.
  base::Semaphore* vector_ready_;
  base::Semaphore* vector_consumed_;
};

static void CheckedWait(base::Semaphore& semaphore) {
  while (!all_states_seen.load(std::memory_order_acquire)) {
    if (semaphore.WaitFor(base::TimeDelta::FromMilliseconds(1))) break;
  }
}

// Verify that a LoadIC can be cycled through different states and safely
// read on a background thread.
TEST(CheckLoadICStates) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  FLAG_lazy_feedback_allocation = false;
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  HandleScope handle_scope(isolate);

  Handle<HeapObject> o1 = Handle<HeapObject>::cast(
      Utils::OpenHandle(*CompileRun("o1 = { bar: {} };")));
  Handle<HeapObject> o2 = Handle<HeapObject>::cast(
      Utils::OpenHandle(*CompileRun("o2 = { baz: 3, bar: 3 };")));
  Handle<HeapObject> o3 = Handle<HeapObject>::cast(
      Utils::OpenHandle(*CompileRun("o3 = { blu: 3, baz: 3, bar: 3 };")));
  Handle<HeapObject> o4 = Handle<HeapObject>::cast(Utils::OpenHandle(
      *CompileRun("o4 = { ble: 3, blu: 3, baz: 3, bar: 3 };")));
  auto result = CompileRun(
      "function foo(o) {"
      "  let a = o.bar;"
      "  return a;"
      "}"
      "foo(o1);"
      "foo;");
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*result));
  Handle<FeedbackVector> vector(function->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(vector, slot);
  CHECK(IsLoadICKind(nexus.kind()));
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  nexus.ConfigureUninitialized();

  // Now the basic environment is set up. Start the worker thread.
  base::Semaphore sema_started(0);
  base::Semaphore vector_ready(0);
  base::Semaphore vector_consumed(0);
  Handle<FeedbackVector> persistent_vector =
      Handle<FeedbackVector>::cast(ph->NewHandle(vector));
  std::unique_ptr<FeedbackVectorExplorationThread> thread(
      new FeedbackVectorExplorationThread(isolate->heap(), &sema_started,
                                          &vector_ready, &vector_consumed,
                                          std::move(ph), persistent_vector));
  CHECK(thread->Start());
  sema_started.Wait();

  // Cycle the IC through all states repeatedly.

  // {dummy_handler} is just an arbitrary value to associate with a map in order
  // to fill in the feedback vector slots in a minimally acceptable way.
  MaybeObjectHandle dummy_handler(Smi::FromInt(10), isolate);
  for (int i = 0; i < kCycles; i++) {
    if (all_states_seen.load(std::memory_order_acquire)) break;

    CHECK_EQ(UNINITIALIZED, nexus.ic_state());
    if (i == (kCycles - 1)) {
      // If we haven't seen all states by the last attempt, enter an explicit
      // handshaking mode.
      vector_ready.Signal();
      CheckedWait(vector_consumed);
      fprintf(stderr, "Main thread configuring monomorphic\n");
    }
    nexus.ConfigureMonomorphic(Handle<Name>(), Handle<Map>(o1->map(), isolate),
                               dummy_handler);
    CHECK_EQ(MONOMORPHIC, nexus.ic_state());

    if (i == (kCycles - 1)) {
      vector_ready.Signal();
      CheckedWait(vector_consumed);
      fprintf(stderr, "Main thread configuring polymorphic\n");
    }

    // Go polymorphic.
    std::vector<MapAndHandler> map_and_handlers;
    map_and_handlers.push_back(
        MapAndHandler(Handle<Map>(o1->map(), isolate), dummy_handler));
    map_and_handlers.push_back(
        MapAndHandler(Handle<Map>(o2->map(), isolate), dummy_handler));
    map_and_handlers.push_back(
        MapAndHandler(Handle<Map>(o3->map(), isolate), dummy_handler));
    map_and_handlers.push_back(
        MapAndHandler(Handle<Map>(o4->map(), isolate), dummy_handler));
    nexus.ConfigurePolymorphic(Handle<Name>(), map_and_handlers);
    CHECK_EQ(POLYMORPHIC, nexus.ic_state());

    if (i == (kCycles - 1)) {
      vector_ready.Signal();
      CheckedWait(vector_consumed);
      fprintf(stderr, "Main thread configuring megamorphic\n");
    }

    // Go Megamorphic
    nexus.ConfigureMegamorphic();
    CHECK_EQ(MEGAMORPHIC, nexus.ic_state());

    if (i == (kCycles - 1)) {
      vector_ready.Signal();
      CheckedWait(vector_consumed);
      fprintf(stderr, "Main thread finishing\n");
    }

    nexus.ConfigureUninitialized();
  }

  CHECK(all_states_seen.load(std::memory_order_acquire));
  thread->Join();
}

}  // anonymous namespace

}  // namespace internal
}  // namespace v8
