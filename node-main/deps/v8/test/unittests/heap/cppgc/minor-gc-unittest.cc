// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(CPPGC_YOUNG_GENERATION)

#include <initializer_list>
#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/explicit-management.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/internal/caged-heap-local-data.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

bool IsHeapObjectYoung(void* obj) {
  return HeapObjectHeader::FromObject(obj).IsYoung();
}

bool IsHeapObjectOld(void* obj) { return !IsHeapObjectYoung(obj); }

class SimpleGCedBase : public GarbageCollected<SimpleGCedBase> {
 public:
  static size_t destructed_objects;

  virtual ~SimpleGCedBase() { ++destructed_objects; }

  virtual void Trace(Visitor* v) const { v->Trace(next); }

  Member<SimpleGCedBase> next;
};

size_t SimpleGCedBase::destructed_objects;

template <size_t Size>
class SimpleGCed : public SimpleGCedBase {
  char array[Size];
};

using Small = SimpleGCed<64>;
using Large = SimpleGCed<kLargeObjectSizeThreshold * 2>;

template <typename Type>
struct OtherType;
template <>
struct OtherType<Small> {
  using Type = Large;
};
template <>
struct OtherType<Large> {
  using Type = Small;
};

void ExpectPageYoung(BasePage& page) {
  EXPECT_TRUE(page.contains_young_objects());
  auto& age_table = CagedHeapLocalData::Get().age_table;
  EXPECT_EQ(AgeTable::Age::kYoung,
            age_table.GetAgeForRange(
                CagedHeap::OffsetFromAddress(page.PayloadStart()),
                CagedHeap::OffsetFromAddress(page.PayloadEnd())));
}

void ExpectPageMixed(BasePage& page) {
  EXPECT_TRUE(page.contains_young_objects());
  auto& age_table = CagedHeapLocalData::Get().age_table;
  EXPECT_EQ(AgeTable::Age::kMixed,
            age_table.GetAgeForRange(
                CagedHeap::OffsetFromAddress(page.PayloadStart()),
                CagedHeap::OffsetFromAddress(page.PayloadEnd())));
}

void ExpectPageOld(BasePage& page) {
  EXPECT_FALSE(page.contains_young_objects());
  auto& age_table = CagedHeapLocalData::Get().age_table;
  EXPECT_EQ(AgeTable::Age::kOld,
            age_table.GetAgeForRange(
                CagedHeap::OffsetFromAddress(page.PayloadStart()),
                CagedHeap::OffsetFromAddress(page.PayloadEnd())));
}

class RememberedSetExtractor : HeapVisitor<RememberedSetExtractor> {
  friend class HeapVisitor<RememberedSetExtractor>;

 public:
  static std::set<void*> Extract(cppgc::Heap* heap) {
    RememberedSetExtractor extractor;
    extractor.Traverse(Heap::From(heap)->raw_heap());
    return std::move(extractor.slots_);
  }

 private:
  void VisitPage(BasePage& page) {
    auto* slot_set = page.slot_set();
    if (!slot_set) return;

    const uintptr_t page_start = reinterpret_cast<uintptr_t>(&page);
    const size_t buckets_size = SlotSet::BucketsForSize(page.AllocatedSize());

    slot_set->Iterate(
        page_start, 0, buckets_size,
        [this](SlotSet::Address slot) {
          slots_.insert(reinterpret_cast<void*>(slot));
          return heap::base::KEEP_SLOT;
        },
        SlotSet::EmptyBucketMode::FREE_EMPTY_BUCKETS);
  }

  bool VisitNormalPage(NormalPage& page) {
    VisitPage(page);
    return true;
  }

  bool VisitLargePage(LargePage& page) {
    VisitPage(page);
    return true;
  }

  std::set<void*> slots_;
};

}  // namespace

class MinorGCTest : public testing::TestWithHeap {
 public:
  MinorGCTest() : testing::TestWithHeap() {
    // Enable young generation flag and run GC. After the first run the heap
    // will enable minor GC.
    Heap::From(GetHeap())->EnableGenerationalGC();
    CollectMajor();

    SimpleGCedBase::destructed_objects = 0;
  }

  ~MinorGCTest() override { Heap::From(GetHeap())->Terminate(); }

  static size_t DestructedObjects() {
    return SimpleGCedBase::destructed_objects;
  }

  void CollectMinor() {
    Heap::From(GetHeap())->CollectGarbage(GCConfig::MinorPreciseAtomicConfig());
  }

  void CollectMinorWithStack() {
    Heap::From(GetHeap())->CollectGarbage(
        GCConfig::MinorConservativeAtomicConfig());
  }

  void CollectMajor() {
    Heap::From(GetHeap())->CollectGarbage(GCConfig::PreciseAtomicConfig());
  }

  void CollectMajorWithStack() {
    Heap::From(GetHeap())->CollectGarbage(GCConfig::ConservativeAtomicConfig());
  }

  const auto& RememberedSourceObjects() const {
    return Heap::From(GetHeap())->remembered_set().remembered_source_objects_;
  }

  const auto& RememberedInConstructionObjects() const {
    return Heap::From(GetHeap())
        ->remembered_set()
        .remembered_in_construction_objects_.previous;
  }
};

template <typename SmallOrLarge>
class MinorGCTestForType : public MinorGCTest {
 public:
  using Type = SmallOrLarge;
};

using ObjectTypes = ::testing::Types<Small, Large>;
TYPED_TEST_SUITE(MinorGCTestForType, ObjectTypes);

namespace {

enum class GCType {
  kMinor,
  kMajor,
};

enum class StackType {
  kWithout,
  kWith,
};

template <GCType gc_type, StackType stack_type, typename... Args>
void RunGCAndExpectObjectsPromoted(MinorGCTest& test, Args*... args) {
  EXPECT_TRUE((IsHeapObjectYoung(args) && ...));
  if constexpr (gc_type == GCType::kMajor) {
    if constexpr (stack_type == StackType::kWithout) {
      test.CollectMajor();
    } else {
      test.CollectMajorWithStack();
    }
  } else {
    if constexpr (stack_type == StackType::kWithout) {
      test.CollectMinor();
    } else {
      test.CollectMinorWithStack();
    }
  }
  EXPECT_TRUE((IsHeapObjectOld(args) && ...));
}

struct ExpectRememberedSlotsAdded final {
  ExpectRememberedSlotsAdded(
      const MinorGCTest& test,
      std::initializer_list<void*> slots_expected_to_be_remembered)
      : test_(test),
        slots_expected_to_be_remembered_(slots_expected_to_be_remembered),
        initial_slots_(RememberedSetExtractor::Extract(test.GetHeap())) {
    // Check that the remembered set doesn't contain specified slots.
    EXPECT_FALSE(std::includes(initial_slots_.begin(), initial_slots_.end(),
                               slots_expected_to_be_remembered_.begin(),
                               slots_expected_to_be_remembered_.end()));
  }

  ~ExpectRememberedSlotsAdded() {
    const auto current_slots = RememberedSetExtractor::Extract(test_.GetHeap());
    EXPECT_EQ(initial_slots_.size() + slots_expected_to_be_remembered_.size(),
              current_slots.size());
    EXPECT_TRUE(std::includes(current_slots.begin(), current_slots.end(),
                              slots_expected_to_be_remembered_.begin(),
                              slots_expected_to_be_remembered_.end()));
  }

 private:
  const MinorGCTest& test_;
  std::set<void*> slots_expected_to_be_remembered_;
  std::set<void*> initial_slots_;
};

struct ExpectRememberedSlotsRemoved final {
  ExpectRememberedSlotsRemoved(
      const MinorGCTest& test,
      std::initializer_list<void*> slots_expected_to_be_removed)
      : test_(test),
        slots_expected_to_be_removed_(slots_expected_to_be_removed),
        initial_slots_(RememberedSetExtractor::Extract(test.GetHeap())) {
    DCHECK_GE(initial_slots_.size(), slots_expected_to_be_removed_.size());
    // Check that the remembered set does contain specified slots to be removed.
    EXPECT_TRUE(std::includes(initial_slots_.begin(), initial_slots_.end(),
                              slots_expected_to_be_removed_.begin(),
                              slots_expected_to_be_removed_.end()));
  }

  ~ExpectRememberedSlotsRemoved() {
    const auto current_slots = RememberedSetExtractor::Extract(test_.GetHeap());
    EXPECT_EQ(initial_slots_.size() - slots_expected_to_be_removed_.size(),
              current_slots.size());
    EXPECT_FALSE(std::includes(current_slots.begin(), current_slots.end(),
                               slots_expected_to_be_removed_.begin(),
                               slots_expected_to_be_removed_.end()));
  }

 private:
  const MinorGCTest& test_;
  std::set<void*> slots_expected_to_be_removed_;
  std::set<void*> initial_slots_;
};

struct ExpectNoRememberedSlotsAdded final {
  explicit ExpectNoRememberedSlotsAdded(const MinorGCTest& test)
      : test_(test),
        initial_remembered_slots_(
            RememberedSetExtractor::Extract(test.GetHeap())) {}

  ~ExpectNoRememberedSlotsAdded() {
    EXPECT_EQ(initial_remembered_slots_,
              RememberedSetExtractor::Extract(test_.GetHeap()));
  }

 private:
  const MinorGCTest& test_;
  std::set<void*> initial_remembered_slots_;
};

}  // namespace

TYPED_TEST(MinorGCTestForType, MinorCollection) {
  using Type = typename TestFixture::Type;

  MakeGarbageCollected<Type>(this->GetAllocationHandle());
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  MinorGCTest::CollectMinor();
  EXPECT_EQ(1u, TestFixture::DestructedObjects());

  {
    subtle::NoGarbageCollectionScope no_gc_scope(*Heap::From(this->GetHeap()));

    Type* prev = nullptr;
    for (size_t i = 0; i < 64; ++i) {
      auto* ptr = MakeGarbageCollected<Type>(this->GetAllocationHandle());
      ptr->next = prev;
      prev = ptr;
    }
  }

  MinorGCTest::CollectMinor();
  EXPECT_EQ(65u, TestFixture::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, StickyBits) {
  using Type = typename TestFixture::Type;

  Persistent<Type> p1 = MakeGarbageCollected<Type>(this->GetAllocationHandle());
  TestFixture::CollectMinor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(p1.Get()).IsYoung());
  TestFixture::CollectMajor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(p1.Get()).IsYoung());
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, OldObjectIsNotVisited) {
  using Type = typename TestFixture::Type;

  Persistent<Type> p = MakeGarbageCollected<Type>(this->GetAllocationHandle());
  TestFixture::CollectMinor();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  EXPECT_FALSE(HeapObjectHeader::FromObject(p.Get()).IsYoung());

  // Check that the old deleted object won't be visited during minor GC.
  Type* raw = p.Release();
  TestFixture::CollectMinor();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  EXPECT_FALSE(HeapObjectHeader::FromObject(raw).IsYoung());
  EXPECT_FALSE(HeapObjectHeader::FromObject(raw).IsFree());

  // Check that the old deleted object will be revisited in major GC.
  TestFixture::CollectMajor();
  EXPECT_EQ(1u, TestFixture::DestructedObjects());
}

template <typename Type1, typename Type2>
void InterGenerationalPointerTest(MinorGCTest* test, cppgc::Heap* heap) {
  Persistent<Type1> old =
      MakeGarbageCollected<Type1>(heap->GetAllocationHandle());
  test->CollectMinor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(old.Get()).IsYoung());

  Type2* young = nullptr;

  {
    subtle::NoGarbageCollectionScope no_gc_scope(*Heap::From(heap));

    // Allocate young objects.
    for (size_t i = 0; i < 64; ++i) {
      auto* ptr = MakeGarbageCollected<Type2>(heap->GetAllocationHandle());
      ptr->next = young;
      young = ptr;
      EXPECT_TRUE(HeapObjectHeader::FromObject(young).IsYoung());
      const uintptr_t offset = CagedHeap::OffsetFromAddress(young);
      // Age may be young or unknown.
      EXPECT_NE(AgeTable::Age::kOld,
                CagedHeapLocalData::Get().age_table.GetAge(offset));
    }
  }

  auto remembered_set_size_before_barrier =
      RememberedSetExtractor::Extract(test->GetHeap()).size();

  // Issue generational barrier.
  old->next = young;

  auto remembered_set_size_after_barrier =
      RememberedSetExtractor::Extract(test->GetHeap()).size();

  EXPECT_EQ(remembered_set_size_before_barrier + 1u,
            remembered_set_size_after_barrier);

  // Check that the remembered set is visited.
  test->CollectMinor();

  EXPECT_EQ(0u, MinorGCTest::DestructedObjects());
  EXPECT_TRUE(RememberedSetExtractor::Extract(test->GetHeap()).empty());

  for (size_t i = 0; i < 64; ++i) {
    EXPECT_FALSE(HeapObjectHeader::FromObject(young).IsFree());
    EXPECT_FALSE(HeapObjectHeader::FromObject(young).IsYoung());
    young = static_cast<Type2*>(young->next.Get());
  }

  old.Release();
  test->CollectMajor();
  EXPECT_EQ(65u, MinorGCTest::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerForSamePageTypes) {
  using Type = typename TestFixture::Type;
  InterGenerationalPointerTest<Type, Type>(this, this->GetHeap());
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerForDifferentPageTypes) {
  using Type = typename TestFixture::Type;
  InterGenerationalPointerTest<Type, typename OtherType<Type>::Type>(
      this, this->GetHeap());
}

TYPED_TEST(MinorGCTestForType, OmitGenerationalBarrierForOnStackObject) {
  using Type = typename TestFixture::Type;

  class StackAllocated {
    CPPGC_STACK_ALLOCATED();

   public:
    Type* ptr = nullptr;
  } stack_object;

  // Try issuing generational barrier for on-stack object.
  stack_object.ptr = MakeGarbageCollected<Type>(this->GetAllocationHandle());
  subtle::HeapConsistency::WriteBarrierParams params;
  EXPECT_EQ(subtle::HeapConsistency::WriteBarrierType::kNone,
            subtle::HeapConsistency::GetWriteBarrierType(
                reinterpret_cast<void*>(&stack_object.ptr), stack_object.ptr,
                params));
}

TYPED_TEST(MinorGCTestForType, OmitGenerationalBarrierForSentinels) {
  using Type = typename TestFixture::Type;

  Persistent<Type> old =
      MakeGarbageCollected<Type>(this->GetAllocationHandle());

  TestFixture::CollectMinor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(old.Get()).IsYoung());

  {
    ExpectNoRememberedSlotsAdded _(*this);
    // Try issuing generational barrier for nullptr.
    old->next = static_cast<Type*>(nullptr);
  }
  {
    ExpectNoRememberedSlotsAdded _(*this);
    // Try issuing generational barrier for sentinel.
    old->next = kSentinelPointer;
  }
}

template <typename From, typename To>
void TestRememberedSetInvalidation(MinorGCTest& test) {
  Persistent<From> old = MakeGarbageCollected<From>(test.GetAllocationHandle());

  test.CollectMinor();

  auto* young = MakeGarbageCollected<To>(test.GetAllocationHandle());

  {
    ExpectRememberedSlotsAdded _(test, {old->next.GetSlotForTesting()});
    // Issue the generational barrier.
    old->next = young;
  }

  {
    ExpectRememberedSlotsRemoved _(test, {old->next.GetSlotForTesting()});
    // Release the persistent and free the old object.
    auto* old_raw = old.Release();
    subtle::FreeUnreferencedObject(test.GetHeapHandle(), *old_raw);
  }

  // Visiting remembered slots must not fail.
  test.CollectMinor();
}

TYPED_TEST(MinorGCTestForType, RememberedSetInvalidationOnPromptlyFree) {
  using Type1 = typename TestFixture::Type;
  using Type2 = typename OtherType<Type1>::Type;
  TestRememberedSetInvalidation<Type1, Type1>(*this);
  TestRememberedSetInvalidation<Type1, Type2>(*this);
}

TEST_F(MinorGCTest, RememberedSetInvalidationOnShrink) {
  using Member = Member<Small>;

  static constexpr size_t kTrailingMembers = 64;
  static constexpr size_t kBytesToAllocate = kTrailingMembers * sizeof(Member);

  static constexpr size_t kFirstMemberToInvalidate = kTrailingMembers / 2;
  static constexpr size_t kLastMemberToInvalidate = kTrailingMembers;

  // Create an object with additional kBytesToAllocate bytes.
  Persistent<Small> old = MakeGarbageCollected<Small>(
      this->GetAllocationHandle(), AdditionalBytes(kBytesToAllocate));

  auto get_member = [&old](size_t i) -> Member& {
    return *reinterpret_cast<Member*>(reinterpret_cast<uint8_t*>(old.Get()) +
                                      sizeof(Small) + i * sizeof(Member));
  };

  CollectMinor();

  auto* young = MakeGarbageCollected<Small>(GetAllocationHandle());

  const size_t remembered_set_size_before_barrier =
      RememberedSetExtractor::Extract(GetHeap()).size();

  // Issue the generational barriers.
  for (size_t i = kFirstMemberToInvalidate; i < kLastMemberToInvalidate; ++i) {
    // Construct the member.
    new (&get_member(i)) Member;
    // Issue the barrier.
    get_member(i) = young;
  }

  const auto remembered_set_size_after_barrier =
      RememberedSetExtractor::Extract(GetHeap()).size();

  // Check that barriers hit (kLastMemberToInvalidate -
  // kFirstMemberToInvalidate) times.
  EXPECT_EQ(remembered_set_size_before_barrier +
                (kLastMemberToInvalidate - kFirstMemberToInvalidate),
            remembered_set_size_after_barrier);

  // Shrink the buffer for old object.
  subtle::Resize(*old, AdditionalBytes(kBytesToAllocate / 2));

  const auto remembered_set_after_shrink =
      RememberedSetExtractor::Extract(GetHeap()).size();

  // Check that the reference was invalidated.
  EXPECT_EQ(remembered_set_size_before_barrier, remembered_set_after_shrink);

  // Visiting remembered slots must not fail.
  CollectMinor();
}

namespace {

template <typename Value>
struct InlinedObject {
  CPPGC_DISALLOW_NEW();
  struct Inner {
    CPPGC_DISALLOW_NEW();
    Inner() = default;
    explicit Inner(AllocationHandle& handle)
        : ref(MakeGarbageCollected<Value>(handle)) {}

    void Trace(Visitor* v) const { v->Trace(ref); }

    double d = -1.;
    Member<Value> ref;
  };

  InlinedObject() = default;
  explicit InlinedObject(AllocationHandle& handle)
      : ref(MakeGarbageCollected<Value>(handle)), inner(handle) {}

  void Trace(cppgc::Visitor* v) const {
    v->Trace(ref);
    v->Trace(inner);
  }

  int a_ = -1;
  Member<Value> ref;
  Inner inner;
};

template <typename Value>
class GCedWithInlinedArray
    : public GarbageCollected<GCedWithInlinedArray<Value>> {
 public:
  static constexpr size_t kNumObjects = 16;

  GCedWithInlinedArray(HeapHandle& heap_handle, AllocationHandle& alloc_handle)
      : heap_handle_(heap_handle), alloc_handle_(alloc_handle) {}

  using WriteBarrierParams = subtle::HeapConsistency::WriteBarrierParams;
  using HeapConsistency = subtle::HeapConsistency;

  void SetInPlaceRange(size_t from, size_t to) {
    DCHECK_GT(to, from);
    DCHECK_GT(kNumObjects, from);

    for (; from != to; ++from)
      new (&objects[from]) InlinedObject<Value>(alloc_handle_);

    GenerationalBarrierForSourceObject(&objects[from]);
  }

  void Trace(cppgc::Visitor* v) const {
    for (const auto& object : objects) v->Trace(object);
  }

  InlinedObject<Value> objects[kNumObjects];

 private:
  void GenerationalBarrierForSourceObject(void* object) {
    DCHECK(object);
    WriteBarrierParams params;
    const auto barrier_type = HeapConsistency::GetWriteBarrierType(
        object, params, [this]() -> HeapHandle& { return heap_handle_; });
    EXPECT_EQ(HeapConsistency::WriteBarrierType::kGenerational, barrier_type);
    HeapConsistency::GenerationalBarrierForSourceObject(params, object);
  }

  HeapHandle& heap_handle_;
  AllocationHandle& alloc_handle_;
};

}  // namespace

TYPED_TEST(MinorGCTestForType, GenerationalBarrierDeferredTracing) {
  using Type = typename TestFixture::Type;

  Persistent<GCedWithInlinedArray<Type>> array =
      MakeGarbageCollected<GCedWithInlinedArray<Type>>(
          this->GetAllocationHandle(), this->GetHeapHandle(),
          this->GetAllocationHandle());

  this->CollectMinor();

  EXPECT_TRUE(IsHeapObjectOld(array.Get()));

  const auto& remembered_objects = this->RememberedSourceObjects();
  {
    ExpectNoRememberedSlotsAdded _(*this);
    EXPECT_EQ(0u, remembered_objects.count(
                      &HeapObjectHeader::FromObject(array->objects)));

    array->SetInPlaceRange(2, 4);

    EXPECT_EQ(1u, remembered_objects.count(
                      &HeapObjectHeader::FromObject(array->objects)));
  }

  RunGCAndExpectObjectsPromoted<GCType::kMinor, StackType::kWithout>(
      *this, array->objects[2].ref.Get(), array->objects[2].inner.ref.Get(),
      array->objects[3].ref.Get(), array->objects[3].inner.ref.Get());

  EXPECT_EQ(0u, remembered_objects.size());
}

namespace {
class GCedWithCustomWeakCallback final
    : public GarbageCollected<GCedWithCustomWeakCallback> {
 public:
  static size_t custom_callback_called;

  void CustomWeakCallbackMethod(const LivenessBroker& broker) {
    custom_callback_called++;
  }

  void Trace(cppgc::Visitor* visitor) const {
    visitor->RegisterWeakCallbackMethod<
        GCedWithCustomWeakCallback,
        &GCedWithCustomWeakCallback::CustomWeakCallbackMethod>(this);
  }
};
size_t GCedWithCustomWeakCallback::custom_callback_called = 0;
}  // namespace

TEST_F(MinorGCTest, ReexecuteCustomCallback) {
  // Create an object with additional kBytesToAllocate bytes.
  Persistent<GCedWithCustomWeakCallback> old =
      MakeGarbageCollected<GCedWithCustomWeakCallback>(GetAllocationHandle());

  CollectMinor();
  EXPECT_EQ(1u, GCedWithCustomWeakCallback::custom_callback_called);

  CollectMinor();
  EXPECT_EQ(2u, GCedWithCustomWeakCallback::custom_callback_called);

  CollectMinor();
  EXPECT_EQ(3u, GCedWithCustomWeakCallback::custom_callback_called);

  CollectMajor();
  // The callback must be called only once.
  EXPECT_EQ(4u, GCedWithCustomWeakCallback::custom_callback_called);
}

TEST_F(MinorGCTest, AgeTableIsReset) {
  using Type1 = SimpleGCed<16>;
  using Type2 = SimpleGCed<64>;
  using Type3 = SimpleGCed<kLargeObjectSizeThreshold * 2>;

  Persistent<Type1> p1 = MakeGarbageCollected<Type1>(GetAllocationHandle());
  Persistent<Type2> p2 = MakeGarbageCollected<Type2>(GetAllocationHandle());
  Persistent<Type3> p3 = MakeGarbageCollected<Type3>(GetAllocationHandle());

  auto* page1 = BasePage::FromPayload(p1.Get());
  auto* page2 = BasePage::FromPayload(p2.Get());
  auto* page3 = BasePage::FromPayload(p3.Get());

  ASSERT_FALSE(page1->is_large());
  ASSERT_FALSE(page2->is_large());
  ASSERT_TRUE(page3->is_large());

  ASSERT_NE(page1, page2);
  ASSERT_NE(page1, page3);
  ASSERT_NE(page2, page3);

  // First, expect all the pages to be young.
  ExpectPageYoung(*page1);
  ExpectPageYoung(*page2);
  ExpectPageYoung(*page3);

  CollectMinor();

  // Expect pages to be promoted after the minor GC.
  ExpectPageOld(*page1);
  ExpectPageOld(*page2);
  ExpectPageOld(*page3);

  // Allocate another objects on the normal pages and a new large page.
  p1 = MakeGarbageCollected<Type1>(GetAllocationHandle());
  p2 = MakeGarbageCollected<Type2>(GetAllocationHandle());
  p3 = MakeGarbageCollected<Type3>(GetAllocationHandle());

  // Expect now the normal pages to be mixed.
  ExpectPageMixed(*page1);
  ExpectPageMixed(*page2);
  // The large page must remain old.
  ExpectPageOld(*page3);

  CollectMajor();

  // After major GC all the pages must also become old.
  ExpectPageOld(*page1);
  ExpectPageOld(*page2);
  ExpectPageOld(*BasePage::FromPayload(p3.Get()));
}

namespace {

template <GCType type>
struct GCOnConstruction {
  explicit GCOnConstruction(MinorGCTest& test, size_t depth) {
    if constexpr (type == GCType::kMajor) {
      test.CollectMajorWithStack();
    } else {
      test.CollectMinorWithStack();
    }
    EXPECT_EQ(depth, test.RememberedInConstructionObjects().size());
  }
};

template <GCType type>
struct InConstructionWithYoungRef
    : GarbageCollected<InConstructionWithYoungRef<type>> {
  using ValueType = SimpleGCed<64>;

  explicit InConstructionWithYoungRef(MinorGCTest& test)
      : call_gc(test, 1u),
        m(MakeGarbageCollected<ValueType>(test.GetAllocationHandle())) {}

  void Trace(Visitor* v) const { v->Trace(m); }

  GCOnConstruction<type> call_gc;
  Member<ValueType> m;
};

}  // namespace

TEST_F(MinorGCTest, RevisitInConstructionObjectsMinorMinorWithStack) {
  static constexpr auto kFirstGCType = GCType::kMinor;

  auto* gced = MakeGarbageCollected<InConstructionWithYoungRef<kFirstGCType>>(
      GetAllocationHandle(), *this);

  RunGCAndExpectObjectsPromoted<GCType::kMinor, StackType::kWith>(
      *this, gced->m.Get());

  EXPECT_EQ(0u, RememberedInConstructionObjects().size());
}

TEST_F(MinorGCTest, RevisitInConstructionObjectsMinorMinorWithoutStack) {
  static constexpr auto kFirstGCType = GCType::kMinor;

  Persistent<InConstructionWithYoungRef<kFirstGCType>> gced =
      MakeGarbageCollected<InConstructionWithYoungRef<kFirstGCType>>(
          GetAllocationHandle(), *this);

  RunGCAndExpectObjectsPromoted<GCType::kMinor, StackType::kWithout>(
      *this, gced->m.Get());

  EXPECT_EQ(0u, RememberedInConstructionObjects().size());
}

TEST_F(MinorGCTest, RevisitInConstructionObjectsMajorMinorWithStack) {
  static constexpr auto kFirstGCType = GCType::kMajor;

  auto* gced = MakeGarbageCollected<InConstructionWithYoungRef<kFirstGCType>>(
      GetAllocationHandle(), *this);

  RunGCAndExpectObjectsPromoted<GCType::kMinor, StackType::kWith>(
      *this, gced->m.Get());

  EXPECT_EQ(0u, RememberedInConstructionObjects().size());
}

TEST_F(MinorGCTest, RevisitInConstructionObjectsMajorMinorWithoutStack) {
  static constexpr auto kFirstGCType = GCType::kMajor;

  Persistent<InConstructionWithYoungRef<kFirstGCType>> gced =
      MakeGarbageCollected<InConstructionWithYoungRef<kFirstGCType>>(
          GetAllocationHandle(), *this);

  RunGCAndExpectObjectsPromoted<GCType::kMinor, StackType::kWithout>(
      *this, gced->m.Get());

  EXPECT_EQ(0u, RememberedInConstructionObjects().size());
}

TEST_F(MinorGCTest, PreviousInConstructionObjectsAreDroppedAfterFullGC) {
  MakeGarbageCollected<InConstructionWithYoungRef<GCType::kMinor>>(
      GetAllocationHandle(), *this);

  EXPECT_EQ(1u, RememberedInConstructionObjects().size());

  CollectMajor();

  EXPECT_EQ(0u, RememberedInConstructionObjects().size());
}

namespace {

template <GCType type>
struct NestedInConstructionWithYoungRef
    : GarbageCollected<NestedInConstructionWithYoungRef<type>> {
  using ValueType = SimpleGCed<64>;

  NestedInConstructionWithYoungRef(MinorGCTest& test, size_t depth)
      : NestedInConstructionWithYoungRef(test, 1, depth) {}

  NestedInConstructionWithYoungRef(MinorGCTest& test, size_t current_depth,
                                   size_t max_depth)
      : current_depth(current_depth),
        max_depth(max_depth),
        next(current_depth != max_depth
                 ? MakeGarbageCollected<NestedInConstructionWithYoungRef<type>>(
                       test.GetAllocationHandle(), test, current_depth + 1,
                       max_depth)
                 : nullptr),
        call_gc(test, current_depth),
        m(MakeGarbageCollected<ValueType>(test.GetAllocationHandle())) {}

  void Trace(Visitor* v) const {
    v->Trace(next);
    v->Trace(m);
  }

  size_t current_depth = 0;
  size_t max_depth = 0;

  Member<NestedInConstructionWithYoungRef<type>> next;
  GCOnConstruction<type> call_gc;
  Member<ValueType> m;
};

}  // namespace

TEST_F(MinorGCTest, RevisitNestedInConstructionObjects) {
  static constexpr auto kFirstGCType = GCType::kMinor;

  Persistent<NestedInConstructionWithYoungRef<kFirstGCType>> gced =
      MakeGarbageCollected<NestedInConstructionWithYoungRef<kFirstGCType>>(
          GetAllocationHandle(), *this, 10);

  CollectMinor();

  for (auto* p = gced.Get(); p; p = p->next.Get()) {
    EXPECT_TRUE(IsHeapObjectOld(p));
    EXPECT_TRUE(IsHeapObjectOld(p->m));
  }

  EXPECT_EQ(0u, RememberedInConstructionObjects().size());
}

}  // namespace internal
}  // namespace cppgc

#endif
