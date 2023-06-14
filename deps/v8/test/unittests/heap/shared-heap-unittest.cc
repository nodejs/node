// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/optional.h"
#include "src/base/platform/platform.h"
#include "src/heap/heap.h"
#include "src/heap/parked-scope.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if V8_CAN_CREATE_SHARED_HEAP_BOOL

namespace v8 {
namespace internal {

using SharedHeapTest = TestJSSharedMemoryWithIsolate;

class SharedHeapNoClientsTest : public TestJSSharedMemoryWithPlatform {
 public:
  SharedHeapNoClientsTest() {
    shared_space_isolate_wrapper.emplace(kNoCounters);
    shared_space_isolate_ = shared_space_isolate_wrapper->i_isolate();
  }

  ~SharedHeapNoClientsTest() override { shared_space_isolate_ = nullptr; }

  v8::Isolate* shared_space_isolate() {
    return reinterpret_cast<v8::Isolate*>(i_shared_space_isolate());
  }

  Isolate* i_shared_space_isolate() { return shared_space_isolate_; }

 private:
  Isolate* shared_space_isolate_;
  base::Optional<IsolateWrapper> shared_space_isolate_wrapper;
};

namespace {
const int kNumIterations = 2000;

template <typename Callback>
void SetupClientIsolateAndRunCallback(Callback callback) {
  IsolateWrapper isolate_wrapper(kNoCounters);
  v8::Isolate* client_isolate = isolate_wrapper.isolate();
  Isolate* i_client_isolate = reinterpret_cast<Isolate*>(client_isolate);

  callback(client_isolate, i_client_isolate);
}

class SharedOldSpaceAllocationThread final : public ParkingThread {
 public:
  SharedOldSpaceAllocationThread()
      : ParkingThread(base::Thread::Options("SharedOldSpaceAllocationThread")) {
  }

  void Run() override {
    SetupClientIsolateAndRunCallback(
        [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);

          for (int i = 0; i < kNumIterations; i++) {
            i_client_isolate->factory()->NewFixedArray(
                10, AllocationType::kSharedOld);
          }

          CollectGarbage(OLD_SPACE, client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }
};
}  // namespace

TEST_F(SharedHeapTest, ConcurrentAllocationInSharedOldSpace) {
  std::vector<std::unique_ptr<SharedOldSpaceAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedOldSpaceAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

namespace {
class SharedLargeOldSpaceAllocationThread final : public ParkingThread {
 public:
  SharedLargeOldSpaceAllocationThread()
      : ParkingThread(base::Thread::Options("SharedOldSpaceAllocationThread")) {
  }

  void Run() override {
    SetupClientIsolateAndRunCallback(
        [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);
          const int kNumIterations = 50;

          for (int i = 0; i < kNumIterations; i++) {
            HandleScope scope(i_client_isolate);
            Handle<FixedArray> fixed_array =
                i_client_isolate->factory()->NewFixedArray(
                    kMaxRegularHeapObjectSize / kTaggedSize,
                    AllocationType::kSharedOld);
            CHECK(MemoryChunk::FromHeapObject(*fixed_array)->IsLargePage());
          }

          CollectGarbage(OLD_SPACE, client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }
};
}  // namespace

TEST_F(SharedHeapTest, ConcurrentAllocationInSharedLargeOldSpace) {
  std::vector<std::unique_ptr<SharedLargeOldSpaceAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedLargeOldSpaceAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

namespace {
class SharedMapSpaceAllocationThread final : public ParkingThread {
 public:
  SharedMapSpaceAllocationThread()
      : ParkingThread(base::Thread::Options("SharedMapSpaceAllocationThread")) {
  }

  void Run() override {
    SetupClientIsolateAndRunCallback(
        [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);

          for (int i = 0; i < kNumIterations; i++) {
            i_client_isolate->factory()->NewMap(
                NATIVE_CONTEXT_TYPE, kVariableSizeSentinel,
                TERMINAL_FAST_ELEMENTS_KIND, 0, AllocationType::kSharedMap);
          }

          CollectGarbage(OLD_SPACE, client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }
};
}  // namespace

TEST_F(SharedHeapTest, ConcurrentAllocationInSharedMapSpace) {
  std::vector<std::unique_ptr<SharedMapSpaceAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedMapSpaceAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

TEST_F(SharedHeapNoClientsTest, SharedCollectionWithoutClients) {
  ::v8::internal::CollectGarbage(OLD_SPACE, shared_space_isolate());
}

void AllocateInSharedHeap(int iterations = 100) {
  SetupClientIsolateAndRunCallback([iterations](v8::Isolate* client_isolate,
                                                Isolate* i_client_isolate) {
    HandleScope outer_scope(i_client_isolate);
    std::vector<Handle<FixedArray>> arrays_in_handles;
    const int kKeptAliveInHandle = 1000;
    const int kKeptAliveInHeap = 100;
    Handle<FixedArray> arrays_in_heap =
        i_client_isolate->factory()->NewFixedArray(kKeptAliveInHeap,
                                                   AllocationType::kYoung);

    for (int i = 0; i < kNumIterations * iterations; i++) {
      HandleScope scope(i_client_isolate);
      Handle<FixedArray> array = i_client_isolate->factory()->NewFixedArray(
          100, AllocationType::kSharedOld);
      if (i < kKeptAliveInHandle) {
        // Keep some of those arrays alive across GCs through handles.
        arrays_in_handles.push_back(scope.CloseAndEscape(array));
      }

      if (i < kKeptAliveInHeap) {
        // Keep some of those arrays alive across GCs through client heap
        // references.
        arrays_in_heap->set(i, *array);
      }

      i_client_isolate->factory()->NewFixedArray(100, AllocationType::kYoung);
    }

    for (Handle<FixedArray> array : arrays_in_handles) {
      CHECK_EQ(array->length(), 100);
    }

    for (int i = 0; i < kKeptAliveInHeap; i++) {
      FixedArray array = FixedArray::cast(arrays_in_heap->get(i));
      CHECK_EQ(array.length(), 100);
    }
  });
}

TEST_F(SharedHeapTest, SharedCollectionWithOneClient) {
  v8_flags.max_old_space_size = 8;
  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  AllocateInSharedHeap();
}

namespace {
class SharedFixedArrayAllocationThread final : public ParkingThread {
 public:
  SharedFixedArrayAllocationThread()
      : ParkingThread(
            base::Thread::Options("SharedFixedArrayAllocationThread")) {}

  void Run() override { AllocateInSharedHeap(5); }
};
}  // namespace

TEST_F(SharedHeapTest, SharedCollectionWithMultipleClients) {
  v8_flags.max_old_space_size = 8;

  std::vector<std::unique_ptr<SharedFixedArrayAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedFixedArrayAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

namespace {

/**
 * The following two classes implement a recurring pattern for testing the
 * shared heap: two isolates (main and client), used respectively by the main
 * thread and a concurrent thread, that execute arbitrary fragments of code
 * (shown below in angular brackets) and synchronize in the following way
 * using parked semaphores:
 *
 *      main thread                    concurrent thread
 * ---------------------------------------------------------------
 *        <SETUP>
 *           |
 *      start thread ----------\
 *           |                  \---------> <SETUP>
 *           |                                 |
 *           |                  /-------- signal ready
 *     wait for ready <--------/               |
 *       <EXECUTE>                             |
 *     signal execute ---------\               |
 *           |                  \-----> wait for execute
 *           |                             <EXECUTE>
 *           |                  /------ signal complete
 *    wait for complete <------/               |
 *           |                                 |
 *      <COMPLETE>                        <COMPLETE>
 *           |                  /----------- exit
 *      join thread <----------/
 *      <TEARDOWN>
 *
 * Both threads allocate an arbitrary state object on their stack, which
 * may contain information that is shared between the executed fragments
 * of code.
 */

template <typename State>
class ConcurrentThread final : public ParkingThread {
 public:
  using ThreadType = ConcurrentThread<State>;
  using Callback = void(ThreadType*);

  explicit ConcurrentThread(ParkingSemaphore* sema_ready = nullptr,
                            ParkingSemaphore* sema_execute_start = nullptr,
                            ParkingSemaphore* sema_execute_complete = nullptr)
      : ParkingThread(Options("ConcurrentThread")),
        sema_ready_(sema_ready),
        sema_execute_start_(sema_execute_start),
        sema_execute_complete_(sema_execute_complete) {}

  void Run() override {
    IsolateWrapper isolate_wrapper(kNoCounters);
    i_client_isolate_ = isolate_wrapper.i_isolate();

    // Allocate the state on the stack, so that handles, direct handles or raw
    // pointers are stack-allocated.
    State state;
    state_ = &state;

    if (setup_callback_) setup_callback_(this);

    if (sema_ready_) sema_ready_->Signal();
    if (sema_execute_start_) {
      sema_execute_start_->ParkedWait(
          i_client_isolate_->main_thread_local_isolate());
    }

    if (execute_callback_) execute_callback_(this);

    if (sema_execute_complete_) sema_execute_complete_->Signal();

    if (complete_callback_) complete_callback_(this);

    i_client_isolate_ = nullptr;
    state_ = nullptr;
  }

  Isolate* i_client_isolate() const {
    DCHECK_NOT_NULL(i_client_isolate_);
    return i_client_isolate_;
  }

  v8::Isolate* client_isolate() const {
    return reinterpret_cast<v8::Isolate*>(i_client_isolate_);
  }

  State* state() {
    DCHECK_NOT_NULL(state_);
    return state_;
  }

  void with_setup(Callback* callback) { setup_callback_ = callback; }
  void with_execute(Callback* callback) { execute_callback_ = callback; }
  void with_complete(Callback* callback) { complete_callback_ = callback; }

 private:
  Isolate* i_client_isolate_ = nullptr;
  State* state_ = nullptr;
  ParkingSemaphore* sema_ready_ = nullptr;
  ParkingSemaphore* sema_execute_start_ = nullptr;
  ParkingSemaphore* sema_execute_complete_ = nullptr;
  Callback* setup_callback_ = nullptr;
  Callback* execute_callback_ = nullptr;
  Callback* complete_callback_ = nullptr;
};

template <typename State, typename ThreadState>
class SharedHeapTestBase : public TestJSSharedMemoryWithNativeContext {
 public:
  using TestType = SharedHeapTestBase<State, ThreadState>;
  using Callback = void(TestType*);
  using ThreadType = ConcurrentThread<ThreadState>;
  using ThreadCallback = typename ThreadType::Callback;

  SharedHeapTestBase()
      : thread_(std::make_unique<ThreadType>(&sema_ready_, &sema_execute_start_,
                                             &sema_execute_complete_)) {}

  void Interact() {
    // Allocate the state on the stack, so that handles, direct handles or raw
    // pointers are stack-allocated.
    State state;
    state_ = &state;

    if (setup_callback_) setup_callback_(this);
    CHECK(thread()->Start());
    sema_ready_.ParkedWait(i_isolate()->main_thread_local_isolate());
    if (execute_callback_) execute_callback_(this);
    sema_execute_start_.Signal();
    sema_execute_complete_.ParkedWait(i_isolate()->main_thread_local_isolate());
    if (complete_callback_) complete_callback_(this);
    thread()->ParkedJoin(i_isolate()->main_thread_local_isolate());
    if (teardown_callback_) teardown_callback_(this);
  }

  ConcurrentThread<State>* thread() const {
    DCHECK(thread_);
    return thread_.get();
  }

  State* state() {
    DCHECK_NOT_NULL(state_);
    return state_;
  }

  void with_setup(Callback* callback) { setup_callback_ = callback; }
  void with_execute(Callback* callback) { execute_callback_ = callback; }
  void with_complete(Callback* callback) { complete_callback_ = callback; }
  void with_teardown(Callback* callback) { teardown_callback_ = callback; }

 private:
  State* state_ = nullptr;
  std::unique_ptr<ConcurrentThread<State>> thread_;
  ParkingSemaphore sema_ready_{0};
  ParkingSemaphore sema_execute_start_{0};
  ParkingSemaphore sema_execute_complete_{0};
  Callback* setup_callback_ = nullptr;
  Callback* execute_callback_ = nullptr;
  Callback* complete_callback_ = nullptr;
  Callback* teardown_callback_ = nullptr;
};

}  // namespace

namespace {

// Testing the shared heap using ordinary (indirect) handles.

struct StateWithHandle {
  base::Optional<HandleScope> scope;
  Handle<FixedArray> handle;
  Global<v8::FixedArray> weak;
};

using SharedHeapTestStateWithHandle =
    SharedHeapTestBase<StateWithHandle, StateWithHandle>;

template <typename TestType, AllocationType allocation, AllocationSpace space>
void ToEachTheirOwnWithHandle(TestType* test) {
  using ThreadType = TestType::ThreadType;
  auto thread = test->thread();

  // Install all the callbacks.
  test->with_setup([](TestType* test) {
    auto isolate = test->i_isolate();
    auto state = test->state();
    // Install a handle scope.
    state->scope.emplace(isolate);
    // Allocate a fixed array, keep a handle and a weak reference.
    state->handle = isolate->factory()->NewFixedArray(10, allocation);
    auto l = Utils::Convert<FixedArray, v8::FixedArray>(state->handle);
    state->weak.Reset(test->v8_isolate(), l);
    state->weak.SetWeak();
  });

  thread->with_setup([](ThreadType* thread) {
    auto isolate = thread->i_client_isolate();
    auto state = thread->state();
    // Install a handle scope.
    state->scope.emplace(isolate);
    // Allocate a fixed array, keep a handle and a weak reference.
    state->handle = isolate->factory()->NewFixedArray(20, allocation);
    auto l = Utils::Convert<FixedArray, v8::FixedArray>(state->handle);
    state->weak.Reset(thread->client_isolate(), l);
    state->weak.SetWeak();
  });

  test->with_execute(
      [](TestType* test) { i::CollectGarbage(space, test->v8_isolate()); });

  thread->with_execute([](ThreadType* thread) {
    i::CollectGarbage(space, thread->client_isolate());
  });

  test->with_complete([](TestType* test) {
    // The handle should keep the fixed array from being reclaimed.
    EXPECT_FALSE(test->state()->weak.IsEmpty());
  });

  thread->with_complete([](ThreadType* thread) {
    // The handle should keep the fixed array from being reclaimed.
    EXPECT_FALSE(thread->state()->weak.IsEmpty());
    thread->state()->scope.reset();  // Deallocate the handle scope.
    i::CollectGarbage(space, thread->client_isolate());
  });

  test->with_teardown([](TestType* test) {
    test->state()->scope.reset();  // Deallocate the handle scope.
    i::CollectGarbage(space, test->v8_isolate());
  });

  // Perform the test.
  test->Interact();
}

}  // namespace

TEST_F(SharedHeapTestStateWithHandle, ToEachTheirOwnYoungYoung) {
  ToEachTheirOwnWithHandle<SharedHeapTestStateWithHandle,
                           AllocationType::kYoung, NEW_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithHandle, ToEachTheirOwnYoungOld) {
  ToEachTheirOwnWithHandle<SharedHeapTestStateWithHandle,
                           AllocationType::kYoung, OLD_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithHandle, ToEachTheirOwnOldYoung) {
  ToEachTheirOwnWithHandle<SharedHeapTestStateWithHandle, AllocationType::kOld,
                           NEW_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithHandle, ToEachTheirOwnOldOld) {
  ToEachTheirOwnWithHandle<SharedHeapTestStateWithHandle, AllocationType::kOld,
                           OLD_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithHandle, ToEachTheirOwnSharedYoung) {
  ToEachTheirOwnWithHandle<SharedHeapTestStateWithHandle,
                           AllocationType::kSharedOld, NEW_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithHandle, ToEachTheirOwnSharedOld) {
  ToEachTheirOwnWithHandle<SharedHeapTestStateWithHandle,
                           AllocationType::kSharedOld, OLD_SPACE>(this);
}

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING

namespace {

// Testing the shared heap using raw pointers.
// This works only with conservative stack scanning.

struct StateWithRawPointer {
  Address ptr;
  Global<v8::FixedArray> weak;
};

using SharedHeapTestStateWithRawPointer =
    SharedHeapTestBase<StateWithRawPointer, StateWithRawPointer>;

template <typename TestType, AllocationType allocation, AllocationSpace space>
void ToEachTheirOwnWithRawPointer(TestType* test) {
  using ThreadType = TestType::ThreadType;
  auto thread = test->thread();

  // Install all the callbacks.
  test->with_setup([](TestType* test) {
    auto isolate = test->i_isolate();
    auto state = test->state();
    {
      // Allocate a fixed array, keep a raw pointer and a weak reference.
      HandleScope scope(isolate);
      auto h = isolate->factory()->NewFixedArray(10, allocation);
      state->ptr = h->GetHeapObject().ptr();
      auto l = Utils::Convert<FixedArray, v8::FixedArray>(h);
      state->weak.Reset(test->v8_isolate(), l);
      state->weak.SetWeak();
    }
  });

  thread->with_setup([](ThreadType* thread) {
    auto isolate = thread->i_client_isolate();
    auto state = thread->state();
    {
      // Allocate a fixed array, keep a raw pointer and a weak reference.
      HandleScope scope(isolate);
      auto h = isolate->factory()->NewFixedArray(20, allocation);
      state->ptr = h->GetHeapObject().ptr();
      auto l = Utils::Convert<FixedArray, v8::FixedArray>(h);
      state->weak.Reset(thread->client_isolate(), l);
      state->weak.SetWeak();
    }
  });

  test->with_execute(
      [](TestType* test) { i::CollectGarbage(space, test->v8_isolate()); });

  thread->with_execute([](ThreadType* thread) {
    i::CollectGarbage(space, thread->client_isolate());
  });

  test->with_complete([](TestType* test) {
    // With conservative stack scanning, the raw pointer should keep the fixed
    // array from being reclaimed.
    EXPECT_FALSE(test->state()->weak.IsEmpty());
  });

  thread->with_complete([](ThreadType* thread) {
    // With conservative stack scanning, the raw pointer should keep the fixed
    // array from being reclaimed.
    EXPECT_FALSE(thread->state()->weak.IsEmpty());
    i::CollectGarbage(space, thread->client_isolate());
  });

  test->with_teardown(
      [](TestType* test) { i::CollectGarbage(space, test->v8_isolate()); });

  // Perform the test.
  test->Interact();
}

}  // namespace

TEST_F(SharedHeapTestStateWithRawPointer, ToEachTheirOwnYoungYoung) {
  ToEachTheirOwnWithRawPointer<SharedHeapTestStateWithRawPointer,
                               AllocationType::kYoung, NEW_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithRawPointer, ToEachTheirOwnYoungOld) {
  ToEachTheirOwnWithRawPointer<SharedHeapTestStateWithRawPointer,
                               AllocationType::kYoung, OLD_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithRawPointer, ToEachTheirOwnOldYoung) {
  ToEachTheirOwnWithRawPointer<SharedHeapTestStateWithRawPointer,
                               AllocationType::kOld, NEW_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithRawPointer, ToEachTheirOwnOldOld) {
  ToEachTheirOwnWithRawPointer<SharedHeapTestStateWithRawPointer,
                               AllocationType::kOld, OLD_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithRawPointer, ToEachTheirOwnSharedYoung) {
  ToEachTheirOwnWithRawPointer<SharedHeapTestStateWithRawPointer,
                               AllocationType::kSharedOld, NEW_SPACE>(this);
}

TEST_F(SharedHeapTestStateWithRawPointer, ToEachTheirOwnSharedOld) {
  ToEachTheirOwnWithRawPointer<SharedHeapTestStateWithRawPointer,
                               AllocationType::kSharedOld, OLD_SPACE>(this);
}

#endif  // V8_ENABLE_CONSERVATIVE_STACK_SCANNING

}  // namespace internal
}  // namespace v8

#endif  // V8_CAN_CREATE_SHARED_HEAP
