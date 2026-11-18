// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/stacks.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm {

namespace {

// Decorates the platform page allocator and records which page reservations
// are alive and which have been freed.
class ReservationTrackingPageAllocator : public v8::PageAllocator {
 public:
  explicit ReservationTrackingPageAllocator(v8::PageAllocator* page_allocator)
      : page_allocator_(page_allocator) {}

  size_t AllocatePageSize() override {
    return page_allocator_->AllocatePageSize();
  }
  size_t CommitPageSize() override { return page_allocator_->CommitPageSize(); }
  void SetRandomMmapSeed(int64_t seed) override {
    page_allocator_->SetRandomMmapSeed(seed);
  }
  void* GetRandomMmapAddr() override {
    return page_allocator_->GetRandomMmapAddr();
  }

  void* AllocatePages(void* address, size_t size, size_t alignment,
                      Permission access) override {
    void* result =
        page_allocator_->AllocatePages(address, size, alignment, access);
    if (result != nullptr) {
      base::MutexGuard guard(&mutex_);
      live_[reinterpret_cast<Address>(result)] = size;
    }
    return result;
  }

  bool FreePages(void* address, size_t size) override {
    {
      base::MutexGuard guard(&mutex_);
      auto it = live_.find(reinterpret_cast<Address>(address));
      if (it != live_.end()) {
        // Freeing must cover exactly one whole reservation; this is what the
        // Windows implementation (VirtualFree with MEM_RELEASE) requires.
        CHECK_EQ(it->second, size);
        live_.erase(it);
        freed_.push_back(reinterpret_cast<Address>(address));
      }
    }
    return page_allocator_->FreePages(address, size);
  }

  bool ReleasePages(void* address, size_t size, size_t new_size) override {
    bool result = page_allocator_->ReleasePages(address, size, new_size);
    if (result) {
      base::MutexGuard guard(&mutex_);
      auto it = live_.find(reinterpret_cast<Address>(address));
      if (it != live_.end()) it->second = new_size;
    }
    return result;
  }
  bool SetPermissions(void* address, size_t size, Permission access) override {
    return page_allocator_->SetPermissions(address, size, access);
  }
  bool RecommitPages(void* address, size_t size, Permission access) override {
    return page_allocator_->RecommitPages(address, size, access);
  }
  bool DiscardSystemPages(void* address, size_t size) override {
    return page_allocator_->DiscardSystemPages(address, size);
  }
  bool DecommitPages(void* address, size_t size) override {
    return page_allocator_->DecommitPages(address, size);
  }
  bool SealPages(void* address, size_t size) override {
    return page_allocator_->SealPages(address, size);
  }
  bool ReserveForSharedMemoryMapping(void* address, size_t size) override {
    return page_allocator_->ReserveForSharedMemoryMapping(address, size);
  }
  std::unique_ptr<SharedMemory> AllocateSharedPages(
      size_t size, const void* original_address) override {
    return page_allocator_->AllocateSharedPages(size, original_address);
  }
  bool CanAllocateSharedPages() override {
    return page_allocator_->CanAllocateSharedPages();
  }

  bool IsLive(Address reservation) {
    base::MutexGuard guard(&mutex_);
    return live_.contains(reservation);
  }

  // Returns how often {reservation} was freed. A freed address range can be
  // handed out again by later allocations, so this can be more than one.
  size_t FreeCount(Address reservation) {
    base::MutexGuard guard(&mutex_);
    return std::count(freed_.begin(), freed_.end(), reservation);
  }

 private:
  v8::PageAllocator* const page_allocator_;
  base::Mutex mutex_;
  std::map<Address, size_t> live_;
  std::vector<Address> freed_;
};

#if V8_OS_WIN
// {StackMemory::Grow} installs the new segment as the current thread's stack
// bounds in the TEB. Restore the real bounds of the test thread afterwards.
class V8_NODISCARD ThreadStackBoundsScope {
 public:
  ThreadStackBoundsScope()
      : stack_base_(base::Stack::GetStackStart().value),
        stack_limit_(base::Stack::GetCommittedStackLimit().value) {}
  ~ThreadStackBoundsScope() {
    base::Stack::SetCurrentThreadStackBounds(stack_limit_, stack_base_);
  }

 private:
  uintptr_t stack_base_;
  uintptr_t stack_limit_;
};
#endif  // V8_OS_WIN

}  // namespace

// Regression tests for https://crbug.com/579560894: destroying a stack must
// return its whole reservation (including the guard pages) to the OS, not
// just decommit it.
class WasmStackMemoryReleaseTest : public TestWithPlatform {
 protected:
  void SetUp() override {
    old_page_allocator_ = SetPlatformPageAllocatorForTesting(&tracker_);
  }
  void TearDown() override {
    v8::PageAllocator* tracker = &tracker_;
    CHECK_EQ(tracker, SetPlatformPageAllocatorForTesting(old_page_allocator_));
  }

  // Returns the start of the reservation of the active segment of {stack}: one
  // guard page below its limit.
  Address ActiveSegmentReservation(const StackMemory& stack) {
    return stack.limit() - tracker_.AllocatePageSize();
  }

  ReservationTrackingPageAllocator tracker_{GetPlatformPageAllocator()};
  v8::PageAllocator* old_page_allocator_ = nullptr;
};

TEST_F(WasmStackMemoryReleaseTest, StackDestruction) {
  std::unique_ptr<StackMemory> stack = StackMemory::New();
  Address reservation = ActiveSegmentReservation(*stack);
  ASSERT_TRUE(tracker_.IsLive(reservation));

  stack.reset();
  EXPECT_EQ(1u, tracker_.FreeCount(reservation));
}

TEST_F(WasmStackMemoryReleaseTest, GrownStackDestruction) {
  // Start with the smallest possible segment so that the stack can grow.
  FlagScope<int> stack_size(&v8_flags.wasm_stack_switching_stack_size, 1);
  std::unique_ptr<StackMemory> stack = StackMemory::New();
  Address first_reservation = ActiveSegmentReservation(*stack);
  {
#if V8_OS_WIN
    ThreadStackBoundsScope thread_stack_bounds;
#endif
    ASSERT_TRUE(stack->Grow(stack->base(), KB));
  }
  Address second_reservation = ActiveSegmentReservation(*stack);
  ASSERT_NE(first_reservation, second_reservation);
  ASSERT_TRUE(tracker_.IsLive(first_reservation));
  ASSERT_TRUE(tracker_.IsLive(second_reservation));

  stack.reset();
  EXPECT_EQ(1u, tracker_.FreeCount(first_reservation));
  EXPECT_EQ(1u, tracker_.FreeCount(second_reservation));
}

TEST_F(WasmStackMemoryReleaseTest, PoolTrimming) {
  FlagScope<int> pool_capacity(&v8_flags.wasm_stack_pool_capacity_mb, 0);
  StackPool pool;
  std::unique_ptr<StackMemory> stack1 = StackMemory::New();
  std::unique_ptr<StackMemory> stack2 = StackMemory::New();
  Address reservation1 = ActiveSegmentReservation(*stack1);
  Address reservation2 = ActiveSegmentReservation(*stack2);
  pool.Add(std::move(stack1));
  pool.Add(std::move(stack2));

  // The pool is over capacity, so this destroys both pooled stacks and
  // allocates a fresh one (which may reuse one of the freed address ranges).
  std::unique_ptr<StackMemory> stack = pool.GetOrAllocate();
  EXPECT_EQ(1u, tracker_.FreeCount(reservation1));
  EXPECT_EQ(1u, tracker_.FreeCount(reservation2));
  EXPECT_EQ(0u, pool.Size());
}

TEST_F(WasmStackMemoryReleaseTest, ReleaseFinishedStacks) {
  StackPool pool;
  std::unique_ptr<StackMemory> stack1 = StackMemory::New();
  std::unique_ptr<StackMemory> stack2 = StackMemory::New();
  Address reservation1 = ActiveSegmentReservation(*stack1);
  Address reservation2 = ActiveSegmentReservation(*stack2);
  pool.Add(std::move(stack1));
  pool.Add(std::move(stack2));

  pool.ReleaseFinishedStacks();
  EXPECT_EQ(1u, tracker_.FreeCount(reservation1));
  EXPECT_EQ(1u, tracker_.FreeCount(reservation2));
}

TEST_F(WasmStackMemoryReleaseTest, PoolDestruction) {
  // This is what happens to the isolate's stack pool on isolate teardown.
  Address reservation1;
  Address reservation2;
  {
    StackPool pool;
    std::unique_ptr<StackMemory> stack1 = StackMemory::New();
    std::unique_ptr<StackMemory> stack2 = StackMemory::New();
    reservation1 = ActiveSegmentReservation(*stack1);
    reservation2 = ActiveSegmentReservation(*stack2);
    pool.Add(std::move(stack1));
    pool.Add(std::move(stack2));
  }
  EXPECT_EQ(1u, tracker_.FreeCount(reservation1));
  EXPECT_EQ(1u, tracker_.FreeCount(reservation2));
}

}  // namespace v8::internal::wasm
