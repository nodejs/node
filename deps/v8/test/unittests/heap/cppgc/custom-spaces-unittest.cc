// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "include/cppgc/custom-space.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/raw-heap.h"
#include "test/unittests/heap/cppgc/tests.h"

namespace cppgc {

class CustomSpace1 : public CustomSpace<CustomSpace1> {
 public:
  static constexpr size_t kSpaceIndex = 0;
};

class CustomSpace2 : public CustomSpace<CustomSpace2> {
 public:
  static constexpr size_t kSpaceIndex = 1;
};

namespace internal {

namespace {

size_t g_destructor_callcount;

class TestWithHeapWithCustomSpaces : public testing::TestWithPlatform {
 protected:
  TestWithHeapWithCustomSpaces() {
    Heap::HeapOptions options;
    options.custom_spaces.emplace_back(std::make_unique<CustomSpace1>());
    options.custom_spaces.emplace_back(std::make_unique<CustomSpace2>());
    heap_ = Heap::Create(platform_, std::move(options));
    g_destructor_callcount = 0;
  }

  void PreciseGC() {
    heap_->ForceGarbageCollectionSlow(
        ::testing::UnitTest::GetInstance()->current_test_info()->name(),
        "Testing", cppgc::Heap::StackState::kNoHeapPointers);
  }

  cppgc::Heap* GetHeap() const { return heap_.get(); }

 private:
  std::unique_ptr<cppgc::Heap> heap_;
};

class RegularGCed final : public GarbageCollected<RegularGCed> {
 public:
  void Trace(Visitor*) const {}
};

class CustomGCed1 final : public GarbageCollected<CustomGCed1> {
 public:
  ~CustomGCed1() { g_destructor_callcount++; }
  void Trace(Visitor*) const {}
};
class CustomGCed2 final : public GarbageCollected<CustomGCed2> {
 public:
  ~CustomGCed2() { g_destructor_callcount++; }
  void Trace(Visitor*) const {}
};

class CustomGCedBase : public GarbageCollected<CustomGCedBase> {
 public:
  void Trace(Visitor*) const {}
};
class CustomGCedFinal1 final : public CustomGCedBase {
 public:
  ~CustomGCedFinal1() { g_destructor_callcount++; }
};
class CustomGCedFinal2 final : public CustomGCedBase {
 public:
  ~CustomGCedFinal2() { g_destructor_callcount++; }
};

}  // namespace

}  // namespace internal

template <>
struct SpaceTrait<internal::CustomGCed1> {
  using Space = CustomSpace1;
};

template <>
struct SpaceTrait<internal::CustomGCed2> {
  using Space = CustomSpace2;
};

template <typename T>
struct SpaceTrait<
    T, std::enable_if_t<std::is_base_of<internal::CustomGCedBase, T>::value>> {
  using Space = CustomSpace1;
};

namespace internal {

TEST_F(TestWithHeapWithCustomSpaces, AllocateOnCustomSpaces) {
  auto* regular =
      MakeGarbageCollected<RegularGCed>(GetHeap()->GetAllocationHandle());
  auto* custom1 =
      MakeGarbageCollected<CustomGCed1>(GetHeap()->GetAllocationHandle());
  auto* custom2 =
      MakeGarbageCollected<CustomGCed2>(GetHeap()->GetAllocationHandle());
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces,
            NormalPage::FromPayload(custom1)->space()->index());
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces + 1,
            NormalPage::FromPayload(custom2)->space()->index());
  EXPECT_EQ(static_cast<size_t>(RawHeap::RegularSpaceType::kNormal1),
            NormalPage::FromPayload(regular)->space()->index());
}

TEST_F(TestWithHeapWithCustomSpaces, DifferentSpacesUsesDifferentPages) {
  auto* regular =
      MakeGarbageCollected<RegularGCed>(GetHeap()->GetAllocationHandle());
  auto* custom1 =
      MakeGarbageCollected<CustomGCed1>(GetHeap()->GetAllocationHandle());
  auto* custom2 =
      MakeGarbageCollected<CustomGCed2>(GetHeap()->GetAllocationHandle());
  EXPECT_NE(NormalPage::FromPayload(regular), NormalPage::FromPayload(custom1));
  EXPECT_NE(NormalPage::FromPayload(regular), NormalPage::FromPayload(custom2));
  EXPECT_NE(NormalPage::FromPayload(custom1), NormalPage::FromPayload(custom2));
}

TEST_F(TestWithHeapWithCustomSpaces,
       AllocateOnCustomSpacesSpecifiedThroughBase) {
  auto* regular =
      MakeGarbageCollected<RegularGCed>(GetHeap()->GetAllocationHandle());
  auto* custom1 =
      MakeGarbageCollected<CustomGCedFinal1>(GetHeap()->GetAllocationHandle());
  auto* custom2 =
      MakeGarbageCollected<CustomGCedFinal2>(GetHeap()->GetAllocationHandle());
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces,
            NormalPage::FromPayload(custom1)->space()->index());
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces,
            NormalPage::FromPayload(custom2)->space()->index());
  EXPECT_EQ(static_cast<size_t>(RawHeap::RegularSpaceType::kNormal1),
            NormalPage::FromPayload(regular)->space()->index());
}

TEST_F(TestWithHeapWithCustomSpaces, SweepCustomSpace) {
  MakeGarbageCollected<CustomGCedFinal1>(GetHeap()->GetAllocationHandle());
  MakeGarbageCollected<CustomGCedFinal2>(GetHeap()->GetAllocationHandle());
  MakeGarbageCollected<CustomGCed1>(GetHeap()->GetAllocationHandle());
  MakeGarbageCollected<CustomGCed2>(GetHeap()->GetAllocationHandle());
  EXPECT_EQ(0u, g_destructor_callcount);
  PreciseGC();
  EXPECT_EQ(4u, g_destructor_callcount);
}

}  // namespace internal

// Test custom space compactability.

class CompactableCustomSpace : public CustomSpace<CompactableCustomSpace> {
 public:
  static constexpr size_t kSpaceIndex = 0;
  static constexpr bool kSupportsCompaction = true;
};

class NotCompactableCustomSpace
    : public CustomSpace<NotCompactableCustomSpace> {
 public:
  static constexpr size_t kSpaceIndex = 1;
  static constexpr bool kSupportsCompaction = false;
};

class DefaultCompactableCustomSpace
    : public CustomSpace<DefaultCompactableCustomSpace> {
 public:
  static constexpr size_t kSpaceIndex = 2;
  // By default space are not compactable.
};

namespace internal {
namespace {

class TestWithHeapWithCompactableCustomSpaces
    : public testing::TestWithPlatform {
 protected:
  TestWithHeapWithCompactableCustomSpaces() {
    Heap::HeapOptions options;
    options.custom_spaces.emplace_back(
        std::make_unique<CompactableCustomSpace>());
    options.custom_spaces.emplace_back(
        std::make_unique<NotCompactableCustomSpace>());
    options.custom_spaces.emplace_back(
        std::make_unique<DefaultCompactableCustomSpace>());
    heap_ = Heap::Create(platform_, std::move(options));
    g_destructor_callcount = 0;
  }

  void PreciseGC() {
    heap_->ForceGarbageCollectionSlow("TestWithHeapWithCompactableCustomSpaces",
                                      "Testing",
                                      cppgc::Heap::StackState::kNoHeapPointers);
  }

  cppgc::Heap* GetHeap() const { return heap_.get(); }

 private:
  std::unique_ptr<cppgc::Heap> heap_;
};

class CompactableGCed final : public GarbageCollected<CompactableGCed> {
 public:
  void Trace(Visitor*) const {}
};
class NotCompactableGCed final : public GarbageCollected<NotCompactableGCed> {
 public:
  void Trace(Visitor*) const {}
};
class DefaultCompactableGCed final
    : public GarbageCollected<DefaultCompactableGCed> {
 public:
  void Trace(Visitor*) const {}
};

}  // namespace
}  // namespace internal

template <>
struct SpaceTrait<internal::CompactableGCed> {
  using Space = CompactableCustomSpace;
};
template <>
struct SpaceTrait<internal::NotCompactableGCed> {
  using Space = NotCompactableCustomSpace;
};
template <>
struct SpaceTrait<internal::DefaultCompactableGCed> {
  using Space = DefaultCompactableCustomSpace;
};

namespace internal {

TEST_F(TestWithHeapWithCompactableCustomSpaces,
       AllocateOnCompactableCustomSpaces) {
  auto* compactable =
      MakeGarbageCollected<CompactableGCed>(GetHeap()->GetAllocationHandle());
  auto* not_compactable = MakeGarbageCollected<NotCompactableGCed>(
      GetHeap()->GetAllocationHandle());
  auto* default_compactable = MakeGarbageCollected<DefaultCompactableGCed>(
      GetHeap()->GetAllocationHandle());
  EXPECT_TRUE(NormalPage::FromPayload(compactable)->space()->is_compactable());
  EXPECT_FALSE(
      NormalPage::FromPayload(not_compactable)->space()->is_compactable());
  EXPECT_FALSE(
      NormalPage::FromPayload(default_compactable)->space()->is_compactable());
}

}  // namespace internal

}  // namespace cppgc
