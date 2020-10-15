// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/gc-info.h"

#include "include/cppgc/platform.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

TEST(GCInfoTableTest, InitialEmpty) {
  v8::base::PageAllocator page_allocator;
  GCInfoTable table(&page_allocator);
  EXPECT_EQ(GCInfoTable::kMinIndex, table.NumberOfGCInfosForTesting());
}

TEST(GCInfoTableTest, ResizeToMaxIndex) {
  v8::base::PageAllocator page_allocator;
  GCInfoTable table(&page_allocator);
  GCInfo info = {nullptr, nullptr, false};
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < GCInfoTable::kMaxIndex;
       i++) {
    GCInfoIndex index = table.RegisterNewGCInfo(info);
    EXPECT_EQ(i, index);
  }
}

TEST(GCInfoTableDeathTest, MoreThanMaxIndexInfos) {
  v8::base::PageAllocator page_allocator;
  GCInfoTable table(&page_allocator);
  GCInfo info = {nullptr, nullptr, false};
  // Create GCInfoTable::kMaxIndex entries.
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < GCInfoTable::kMaxIndex;
       i++) {
    table.RegisterNewGCInfo(info);
  }
  EXPECT_DEATH_IF_SUPPORTED(table.RegisterNewGCInfo(info), "");
}

TEST(GCInfoTableDeathTest, OldTableAreaIsReadOnly) {
  v8::base::PageAllocator page_allocator;
  GCInfoTable table(&page_allocator);
  GCInfo info = {nullptr, nullptr, false};
  // Use up all slots until limit.
  GCInfoIndex limit = table.LimitForTesting();
  // Bail out if initial limit is already the maximum because of large committed
  // pages. In this case, nothing can be comitted as read-only.
  if (limit == GCInfoTable::kMaxIndex) {
    return;
  }
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < limit; i++) {
    table.RegisterNewGCInfo(info);
  }
  EXPECT_EQ(limit, table.LimitForTesting());
  table.RegisterNewGCInfo(info);
  EXPECT_NE(limit, table.LimitForTesting());
  // Old area is now read-only.
  auto& first_slot = table.TableSlotForTesting(GCInfoTable::kMinIndex);
  EXPECT_DEATH_IF_SUPPORTED(first_slot.finalize = nullptr, "");
}

namespace {

class ThreadRegisteringGCInfoObjects final : public v8::base::Thread {
 public:
  ThreadRegisteringGCInfoObjects(GCInfoTable* table,
                                 GCInfoIndex num_registrations)
      : v8::base::Thread(Options("Thread registering GCInfo objects.")),
        table_(table),
        num_registrations_(num_registrations) {}

  void Run() final {
    GCInfo info = {nullptr, nullptr, false};
    for (GCInfoIndex i = 0; i < num_registrations_; i++) {
      table_->RegisterNewGCInfo(info);
    }
  }

 private:
  GCInfoTable* table_;
  GCInfoIndex num_registrations_;
};

}  // namespace

TEST(GCInfoTableTest, MultiThreadedResizeToMaxIndex) {
  constexpr size_t num_threads = 4;
  constexpr size_t main_thread_initialized = 2;
  constexpr size_t gc_infos_to_register =
      (GCInfoTable::kMaxIndex - 1) -
      (GCInfoTable::kMinIndex + main_thread_initialized);
  static_assert(gc_infos_to_register % num_threads == 0,
                "must sum up to kMaxIndex");
  constexpr size_t gc_infos_per_thread = gc_infos_to_register / num_threads;

  v8::base::PageAllocator page_allocator;
  GCInfoTable table(&page_allocator);
  GCInfo info = {nullptr, nullptr, false};
  for (size_t i = 0; i < main_thread_initialized; i++) {
    table.RegisterNewGCInfo(info);
  }

  v8::base::Thread* threads[num_threads];
  for (size_t i = 0; i < num_threads; i++) {
    threads[i] =
        new ThreadRegisteringGCInfoObjects(&table, gc_infos_per_thread);
  }
  for (size_t i = 0; i < num_threads; i++) {
    CHECK(threads[i]->Start());
  }
  for (size_t i = 0; i < num_threads; i++) {
    threads[i]->Join();
    delete threads[i];
  }
}

// Tests using the global table and GCInfoTrait.

namespace {

class GCInfoTraitTest : public testing::TestWithPlatform {};

class BasicType final {
 public:
  void Trace(Visitor*) const {}
};
class OtherBasicType final {
 public:
  void Trace(Visitor*) const {}
};

}  // namespace

TEST_F(GCInfoTraitTest, IndexInBounds) {
  const GCInfoIndex index = GCInfoTrait<BasicType>::Index();
  EXPECT_GT(GCInfoTable::kMaxIndex, index);
  EXPECT_LE(GCInfoTable::kMinIndex, index);
}

TEST_F(GCInfoTraitTest, TraitReturnsSameIndexForSameType) {
  const GCInfoIndex index1 = GCInfoTrait<BasicType>::Index();
  const GCInfoIndex index2 = GCInfoTrait<BasicType>::Index();
  EXPECT_EQ(index1, index2);
}

TEST_F(GCInfoTraitTest, TraitReturnsDifferentIndexForDifferentTypes) {
  const GCInfoIndex index1 = GCInfoTrait<BasicType>::Index();
  const GCInfoIndex index2 = GCInfoTrait<OtherBasicType>::Index();
  EXPECT_NE(index1, index2);
}

}  // namespace internal
}  // namespace cppgc
