// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/gc-info.h"

#include <type_traits>

#include "include/cppgc/platform.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/platform.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

constexpr GCInfo GetEmptyGCInfo() { return {nullptr, nullptr, nullptr, false}; }

class GCInfoTableTest : public ::testing::Test {
 public:
  GCInfoTableTest()
      : table_(std::make_unique<GCInfoTable>(page_allocator_, oom_handler_)) {}

  GCInfoIndex RegisterNewGCInfoForTesting(const GCInfo& info) {
    // Unused registered index will result in registering a new index.
    std::atomic<GCInfoIndex> registered_index{0};
    return table().RegisterNewGCInfo(registered_index, info);
  }

  GCInfoTable& table() { return *table_; }
  const GCInfoTable& table() const { return *table_; }

 private:
  v8::base::PageAllocator page_allocator_;
  FatalOutOfMemoryHandler oom_handler_;
  std::unique_ptr<GCInfoTable> table_;
};

using GCInfoTableDeathTest = GCInfoTableTest;

}  // namespace

TEST_F(GCInfoTableTest, InitialEmpty) {
  EXPECT_EQ(GCInfoTable::kMinIndex, table().NumberOfGCInfos());
}

TEST_F(GCInfoTableTest, ResizeToMaxIndex) {
  GCInfo info = GetEmptyGCInfo();
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < GCInfoTable::kMaxIndex;
       i++) {
    GCInfoIndex index = RegisterNewGCInfoForTesting(info);
    EXPECT_EQ(i, index);
  }
}

TEST_F(GCInfoTableDeathTest, MoreThanMaxIndexInfos) {
  GCInfo info = GetEmptyGCInfo();
  // Create GCInfoTable::kMaxIndex entries.
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < GCInfoTable::kMaxIndex;
       i++) {
    RegisterNewGCInfoForTesting(info);
  }
  EXPECT_DEATH_IF_SUPPORTED(RegisterNewGCInfoForTesting(info), "");
}

TEST_F(GCInfoTableDeathTest, OldTableAreaIsReadOnly) {
  GCInfo info = GetEmptyGCInfo();
  // Use up all slots until limit.
  GCInfoIndex limit = table().LimitForTesting();
  // Bail out if initial limit is already the maximum because of large committed
  // pages. In this case, nothing can be comitted as read-only.
  if (limit == GCInfoTable::kMaxIndex) {
    return;
  }
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < limit; i++) {
    RegisterNewGCInfoForTesting(info);
  }
  EXPECT_EQ(limit, table().LimitForTesting());
  RegisterNewGCInfoForTesting(info);
  EXPECT_NE(limit, table().LimitForTesting());
  // Old area is now read-only.
  auto& first_slot = table().TableSlotForTesting(GCInfoTable::kMinIndex);
  EXPECT_DEATH_IF_SUPPORTED(first_slot.finalize = nullptr, "");
}

namespace {

class ThreadRegisteringGCInfoObjects final : public v8::base::Thread {
 public:
  ThreadRegisteringGCInfoObjects(GCInfoTableTest* test,
                                 GCInfoIndex num_registrations)
      : v8::base::Thread(Options("Thread registering GCInfo objects.")),
        test_(test),
        num_registrations_(num_registrations) {}

  void Run() final {
    GCInfo info = GetEmptyGCInfo();
    for (GCInfoIndex i = 0; i < num_registrations_; i++) {
      test_->RegisterNewGCInfoForTesting(info);
    }
  }

 private:
  GCInfoTableTest* test_;
  GCInfoIndex num_registrations_;
};

}  // namespace

TEST_F(GCInfoTableTest, MultiThreadedResizeToMaxIndex) {
  constexpr size_t num_threads = 4;
  constexpr size_t main_thread_initialized = 2;
  constexpr size_t gc_infos_to_register =
      (GCInfoTable::kMaxIndex - 1) -
      (GCInfoTable::kMinIndex + main_thread_initialized);
  static_assert(gc_infos_to_register % num_threads == 0,
                "must sum up to kMaxIndex");
  constexpr size_t gc_infos_per_thread = gc_infos_to_register / num_threads;

  GCInfo info = GetEmptyGCInfo();
  for (size_t i = 0; i < main_thread_initialized; i++) {
    RegisterNewGCInfoForTesting(info);
  }

  v8::base::Thread* threads[num_threads];
  for (size_t i = 0; i < num_threads; i++) {
    threads[i] = new ThreadRegisteringGCInfoObjects(this, gc_infos_per_thread);
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

namespace {

struct Dummy {};

class BaseWithVirtualDestructor
    : public GarbageCollected<BaseWithVirtualDestructor> {
 public:
  virtual ~BaseWithVirtualDestructor() = default;
  void Trace(Visitor*) const {}

 private:
  std::unique_ptr<Dummy> non_trivially_destructible_;
};

class ChildOfBaseWithVirtualDestructor : public BaseWithVirtualDestructor {
 public:
  ~ChildOfBaseWithVirtualDestructor() override = default;
};

static_assert(std::has_virtual_destructor<BaseWithVirtualDestructor>::value,
              "Must have virtual destructor.");
static_assert(!std::is_trivially_destructible<BaseWithVirtualDestructor>::value,
              "Must not be trivially destructible");
#ifdef CPPGC_SUPPORTS_OBJECT_NAMES
static_assert(std::is_same<typename internal::GCInfoFolding<
                               ChildOfBaseWithVirtualDestructor,
                               ChildOfBaseWithVirtualDestructor::
                                   ParentMostGarbageCollectedType>::ResultType,
                           ChildOfBaseWithVirtualDestructor>::value,
              "No folding to preserve object names");
#else   // !CPPGC_SUPPORTS_OBJECT_NAMES
static_assert(std::is_same<typename internal::GCInfoFolding<
                               ChildOfBaseWithVirtualDestructor,
                               ChildOfBaseWithVirtualDestructor::
                                   ParentMostGarbageCollectedType>::ResultType,
                           BaseWithVirtualDestructor>::value,
              "Must fold into base as base has virtual destructor.");
#endif  // !CPPGC_SUPPORTS_OBJECT_NAMES

class TriviallyDestructibleBase
    : public GarbageCollected<TriviallyDestructibleBase> {
 public:
  virtual void Trace(Visitor*) const {}
};

class ChildOfTriviallyDestructibleBase : public TriviallyDestructibleBase {};

static_assert(!std::has_virtual_destructor<TriviallyDestructibleBase>::value,
              "Must not have virtual destructor.");
static_assert(std::is_trivially_destructible<TriviallyDestructibleBase>::value,
              "Must be trivially destructible");
#ifdef CPPGC_SUPPORTS_OBJECT_NAMES
static_assert(std::is_same<typename internal::GCInfoFolding<
                               ChildOfTriviallyDestructibleBase,
                               ChildOfTriviallyDestructibleBase::
                                   ParentMostGarbageCollectedType>::ResultType,
                           ChildOfTriviallyDestructibleBase>::value,
              "No folding to preserve object names");
#else   // !CPPGC_SUPPORTS_OBJECT_NAMES
static_assert(std::is_same<typename internal::GCInfoFolding<
                               ChildOfTriviallyDestructibleBase,
                               ChildOfTriviallyDestructibleBase::
                                   ParentMostGarbageCollectedType>::ResultType,
                           TriviallyDestructibleBase>::value,
              "Must fold into base as both are trivially destructible.");
#endif  // !CPPGC_SUPPORTS_OBJECT_NAMES

class TypeWithCustomFinalizationMethodAtBase
    : public GarbageCollected<TypeWithCustomFinalizationMethodAtBase> {
 public:
  void FinalizeGarbageCollectedObject() {}
  void Trace(Visitor*) const {}

 private:
  std::unique_ptr<Dummy> non_trivially_destructible_;
};

class ChildOfTypeWithCustomFinalizationMethodAtBase
    : public TypeWithCustomFinalizationMethodAtBase {};

static_assert(
    !std::has_virtual_destructor<TypeWithCustomFinalizationMethodAtBase>::value,
    "Must not have virtual destructor.");
static_assert(!std::is_trivially_destructible<
                  TypeWithCustomFinalizationMethodAtBase>::value,
              "Must not be trivially destructible");
#ifdef CPPGC_SUPPORTS_OBJECT_NAMES
static_assert(
    std::is_same<typename internal::GCInfoFolding<
                     ChildOfTypeWithCustomFinalizationMethodAtBase,
                     ChildOfTypeWithCustomFinalizationMethodAtBase::
                         ParentMostGarbageCollectedType>::ResultType,
                 ChildOfTypeWithCustomFinalizationMethodAtBase>::value,
    "No folding to preserve object names");
#else   // !CPPGC_SUPPORTS_OBJECT_NAMES
static_assert(std::is_same<typename internal::GCInfoFolding<
                               ChildOfTypeWithCustomFinalizationMethodAtBase,
                               ChildOfTypeWithCustomFinalizationMethodAtBase::
                                   ParentMostGarbageCollectedType>::ResultType,
                           TypeWithCustomFinalizationMethodAtBase>::value,
              "Must fold into base as base has custom finalizer dispatch.");
#endif  // !CPPGC_SUPPORTS_OBJECT_NAMES

}  // namespace

}  // namespace internal
}  // namespace cppgc
