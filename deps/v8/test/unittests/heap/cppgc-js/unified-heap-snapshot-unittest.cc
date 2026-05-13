// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstring>
#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/common.h"
#include "include/cppgc/cross-thread-persistent.h"
#include "include/cppgc/custom-space.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/member.h"
#include "include/cppgc/name-provider.h"
#include "include/cppgc/persistent.h"
#include "include/v8-cppgc.h"
#include "include/v8-profiler.h"
#include "src/api/api-inl.h"
#include "src/base/macros.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/object-allocator.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/heap-snapshot-generator.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/profiler/heap-snapshot-utils.h"

namespace cppgc {

class CompactableCustomSpace : public CustomSpace<CompactableCustomSpace> {
 public:
  static constexpr size_t kSpaceIndex = 0;
  static constexpr bool kSupportsCompaction = true;
};

}  // namespace cppgc

namespace v8::internal {

struct CompactableGCed : public cppgc::GarbageCollected<CompactableGCed>,
                         public cppgc::NameProvider {
 public:
  static constexpr const char kExpectedName[] = "CompactableGCed";
  void Trace(cppgc::Visitor* v) const {}
  const char* GetHumanReadableName() const final { return "CompactableGCed"; }
  size_t data = 0;
};

struct CompactableHolder : public cppgc::GarbageCollected<CompactableHolder> {
 public:
  explicit CompactableHolder(cppgc::AllocationHandle& allocation_handle) {
    object = cppgc::MakeGarbageCollected<CompactableGCed>(allocation_handle);
  }

  void Trace(cppgc::Visitor* visitor) const {
    visitor->Trace(object);
    visitor->RegisterMovableReference(object.GetSlotForTesting());
  }
  cppgc::subtle::UncompressedMember<CompactableGCed> object = nullptr;
};

struct EphemeronKey final : public cppgc::GarbageCollected<EphemeronKey> {
 public:
  void Trace(cppgc::Visitor* visitor) const {}
};

struct EphemeronValue final : public cppgc::GarbageCollected<EphemeronValue> {
 public:
  void Trace(cppgc::Visitor* visitor) const {}
};

struct EphemeronContainerBacking final
    : public cppgc::GarbageCollected<EphemeronContainerBacking> {
 public:
  static constexpr size_t kEphemeronCount = 3;
  using EphemeronPair = cppgc::EphemeronPair<EphemeronKey, EphemeronValue>;

  EphemeronContainerBacking()
      : ephemeron_pairs_{EphemeronPair{nullptr, nullptr},
                         EphemeronPair{nullptr, nullptr},
                         EphemeronPair{nullptr, nullptr}} {}

  void SetEphemeronPair(size_t index, EphemeronKey* key,
                        EphemeronValue* value) {
    CHECK_LT(index, kEphemeronCount);
    ephemeron_pairs_[index].key = key;
    ephemeron_pairs_[index].value = value;
  }

  void Trace(cppgc::Visitor* visitor) const {
    TraceStrongifiedEphemerons(visitor);
  }

  static void TraceStrong(cppgc::Visitor* visitor, const void* self) {
    static_cast<const EphemeronContainerBacking*>(self)->Trace(visitor);
  }

  static void TraceWeak(cppgc::Visitor* visitor, const void* self) {
    static_cast<const EphemeronContainerBacking*>(self)->TraceWeakEphemerons(
        visitor);
  }

  EphemeronKey* key(size_t index) const {
    return ephemeron_pairs_[index].key.Get();
  }
  EphemeronValue* value(size_t index) const {
    return ephemeron_pairs_[index].value.Get();
  }

  static void ClearCallback(const cppgc::LivenessBroker& broker,
                            const void* data) {
    auto* backing =
        static_cast<EphemeronContainerBacking*>(const_cast<void*>(data));
    for (auto& ephemeron_pair : backing->ephemeron_pairs_) {
      ephemeron_pair.ClearKeyAndValueIfKeyIsDead(broker);
    }
  }

 private:
  void TraceStrongifiedEphemerons(cppgc::Visitor* visitor) const {
    for (const auto& ephemeron_pair : ephemeron_pairs_) {
      visitor->TraceStrongly(ephemeron_pair.key);
      visitor->TraceEphemeron(ephemeron_pair.key, &ephemeron_pair.value);
    }
  }

  void TraceWeakEphemerons(cppgc::Visitor* visitor) const {
    for (const auto& ephemeron_pair : ephemeron_pairs_) {
      visitor->Trace(ephemeron_pair);
    }
  }

  std::array<EphemeronPair, kEphemeronCount> ephemeron_pairs_;
};

class EphemeronContainer : public cppgc::GarbageCollected<EphemeronContainer> {
 public:
  explicit EphemeronContainer(EphemeronContainerBacking* backing)
      : backing_(backing) {}

  void Trace(cppgc::Visitor* visitor) const {
    visitor->TraceWeakContainer(backing_.Get(),
                                EphemeronContainerBacking::ClearCallback,
                                backing_.Get());
  }

  EphemeronContainerBacking* backing() const { return backing_.Get(); }
  EphemeronKey* key(size_t index) const { return backing_->key(index); }
  EphemeronValue* value(size_t index) const { return backing_->value(index); }

 private:
  cppgc::UntracedMember<EphemeronContainerBacking> backing_;
};

}  // namespace v8::internal

namespace cppgc {
template <>
struct SpaceTrait<v8::internal::CompactableGCed> {
  using Space = CompactableCustomSpace;
};

template <>
struct TraceTrait<v8::internal::EphemeronContainerBacking>
    : public internal::TraceTraitBase<v8::internal::EphemeronContainerBacking> {
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, v8::internal::EphemeronContainerBacking::TraceStrong};
  }

  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return {self, v8::internal::EphemeronContainerBacking::TraceWeak};
  }
};
}  // namespace cppgc

namespace v8 {
namespace internal {

namespace {

template <typename TMixin>
class WithUnifiedHeapSnapshot : public TMixin {
 public:
  const v8::HeapSnapshot* TakeHeapSnapshot(
      cppgc::EmbedderStackState stack_state =
          cppgc::EmbedderStackState::kMayContainHeapPointers,
      v8::HeapProfiler::HeapSnapshotMode snapshot_mode =
          v8::HeapProfiler::HeapSnapshotMode::kExposeInternals) {
    v8::HeapProfiler* heap_profiler = TMixin::v8_isolate()->GetHeapProfiler();

    v8::HeapProfiler::HeapSnapshotOptions options;
    options.snapshot_mode = snapshot_mode;
    options.numerics_mode = v8::HeapProfiler::NumericsMode::kHideNumericValues;
    options.stack_state = stack_state;
    return heap_profiler->TakeHeapSnapshot(options);
  }

 protected:
  void TestMergedWrapperNode(v8::HeapProfiler::HeapSnapshotMode snapshot_mode);
};

using UnifiedHeapSnapshotTest = WithUnifiedHeapSnapshot<UnifiedHeapTest>;

bool IsValidSnapshot(const v8::HeapSnapshot* snapshot, int depth = 3) {
  const HeapSnapshot* heap_snapshot =
      reinterpret_cast<const HeapSnapshot*>(snapshot);
  std::unordered_set<const HeapEntry*> visited;
  for (const HeapGraphEdge& edge : heap_snapshot->edges()) {
    visited.insert(edge.to());
  }
  size_t unretained_entries_count = 0;
  for (const HeapEntry& entry : heap_snapshot->entries()) {
    if (visited.find(&entry) == visited.end() && entry.id() != 1) {
      entry.Print("entry with no retainer", "", depth, 0);
      ++unretained_entries_count;
    }
  }
  return unretained_entries_count == 0;
}

// Returns the IDs of all entries in the snapshot with the given name.
std::vector<SnapshotObjectId> GetIds(const v8::HeapSnapshot& snapshot,
                                     std::string name) {
  const HeapSnapshot& heap_snapshot =
      reinterpret_cast<const HeapSnapshot&>(snapshot);
  std::vector<SnapshotObjectId> result;
  for (const HeapEntry& entry : heap_snapshot.entries()) {
    if (entry.name() == name) {
      result.push_back(entry.id());
    }
  }
  return result;
}

bool ContainsRetainingPath(const v8::HeapSnapshot& snapshot,
                           const std::vector<std::string> retaining_path,
                           bool debug_retaining_path = false) {
  const HeapSnapshot& heap_snapshot =
      reinterpret_cast<const HeapSnapshot&>(snapshot);
  std::vector<HeapEntry*> haystack = {heap_snapshot.root()};
  for (size_t i = 0; i < retaining_path.size(); ++i) {
    const std::string& needle = retaining_path[i];
    std::vector<HeapEntry*> new_haystack;
    for (HeapEntry* parent : haystack) {
      for (int j = 0; j < parent->children_count(); j++) {
        HeapEntry* child = parent->child(j)->to();
        if (0 == strcmp(child->name(), needle.c_str())) {
          new_haystack.push_back(child);
        }
      }
    }
    if (new_haystack.empty()) {
      if (debug_retaining_path) {
        fprintf(stderr,
                "#\n# Could not find object with name '%s'\n#\n# Path:\n",
                needle.c_str());
        for (size_t j = 0; j < retaining_path.size(); ++j) {
          fprintf(stderr, "# - '%s'%s\n", retaining_path[j].c_str(),
                  i == j ? "\t<--- not found" : "");
        }
        fprintf(stderr, "#\n");
      }
      return false;
    }
    std::swap(haystack, new_haystack);
  }
  return true;
}

EphemeronContainer* CreateEphemeronContainer(
    std::vector<cppgc::Persistent<EphemeronKey>>& keys,
    cppgc::AllocationHandle& allocation_handle) {
  auto* backing =
      cppgc::MakeGarbageCollected<EphemeronContainerBacking>(allocation_handle);
  keys.reserve(EphemeronContainerBacking::kEphemeronCount);
  for (size_t i = 0; i < EphemeronContainerBacking::kEphemeronCount; ++i) {
    auto* key = cppgc::MakeGarbageCollected<EphemeronKey>(allocation_handle);
    auto* value =
        cppgc::MakeGarbageCollected<EphemeronValue>(allocation_handle);
    keys.emplace_back(key);
    backing->SetEphemeronPair(i, key, value);
  }
  return cppgc::MakeGarbageCollected<EphemeronContainer>(allocation_handle,
                                                         backing);
}

class BaseWithoutName : public cppgc::GarbageCollected<BaseWithoutName> {
 public:
  static constexpr const char kExpectedName[] =
      "v8::internal::(anonymous namespace)::BaseWithoutName";

  virtual void Trace(cppgc::Visitor* v) const {
    v->Trace(next);
    v->Trace(next2);
  }
  cppgc::Member<BaseWithoutName> next;
  cppgc::Member<BaseWithoutName> next2;
};
// static
constexpr const char BaseWithoutName::kExpectedName[];

class GCed final : public BaseWithoutName, public cppgc::NameProvider {
 public:
  static constexpr const char kExpectedName[] = "GCed";

  void Trace(cppgc::Visitor* v) const final { BaseWithoutName::Trace(v); }
  const char* GetHumanReadableName() const final { return "GCed"; }
};
// static
constexpr const char GCed::kExpectedName[];

static constexpr const char kExpectedGCRootsName[] = "(GC roots)";
static constexpr const char kExpectedCppRootsName[] = "C++ Persistent roots";
static constexpr const char kExpectedCppCrossThreadRootsName[] =
    "C++ CrossThreadPersistent roots";
static constexpr const char kExpectedCppStackRootsName[] =
    "C++ native stack roots";
static constexpr const char kExpectedCppStackTracedHandlesName[] =
    "C++ native stack traced handles";

template <typename T>
constexpr const char* GetExpectedName() {
  if (std::is_base_of_v<cppgc::NameProvider, T> ||
      cppgc::NameProvider::SupportsCppClassNamesAsObjectNames()) {
    return T::kExpectedName;
  } else {
    return cppgc::NameProvider::kHiddenName;
  }
}

size_t GetExtraNativeBytes(const v8::HeapSnapshot* snapshot) {
  return reinterpret_cast<const HeapSnapshot*>(snapshot)->extra_native_bytes();
}

template <typename Callback>
void ForEachEntryWithName(const v8::HeapSnapshot* snapshot, const char* name,
                          Callback callback) {
  const HeapSnapshot* heap_snapshot =
      reinterpret_cast<const HeapSnapshot*>(snapshot);
  for (const HeapEntry& entry : heap_snapshot->entries()) {
    if (strcmp(entry.name(), name) == 0) {
      callback(entry);
    }
  }
}

void CheckSize(const v8::HeapSnapshot* snapshot, const char* name,
               size_t size) {
  ForEachEntryWithName(snapshot, name, [size](const HeapEntry& entry) {
    EXPECT_EQ(size, entry.self_size());
  });
}

template <typename T>
size_t GetCppSize(T* object) {
  return cppgc::internal::HeapObjectHeader::FromObject(object).AllocatedSize();
}

}  // namespace

TEST_F(UnifiedHeapSnapshotTest, EmptySnapshot) {
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
}

TEST_F(UnifiedHeapSnapshotTest, RetainedByCppRoot) {
  cppgc::Persistent<GCed> gced =
      cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName, GetExpectedName<GCed>()}));
}

TEST_F(UnifiedHeapSnapshotTest, ConsistentId) {
  cppgc::Persistent<GCed> gced =
      cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  const v8::HeapSnapshot* snapshot1 = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot1));
  const v8::HeapSnapshot* snapshot2 = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot2));
  std::vector<SnapshotObjectId> ids1 =
      GetIds(*snapshot1, GetExpectedName<GCed>());
  std::vector<SnapshotObjectId> ids2 =
      GetIds(*snapshot2, GetExpectedName<GCed>());
  EXPECT_EQ(ids1.size(), size_t{1});
  EXPECT_EQ(ids2.size(), size_t{1});
  EXPECT_EQ(ids1[0], ids2[0]);
}

template <typename TMixin>
class WithCppHeapWithCustomSpace : public TMixin {
 public:
  static std::vector<std::unique_ptr<cppgc::CustomSpaceBase>>
  GetCustomSpaces() {
    std::vector<std::unique_ptr<cppgc::CustomSpaceBase>> custom_spaces;
    custom_spaces.emplace_back(
        std::make_unique<cppgc::CompactableCustomSpace>());
    return custom_spaces;
  }

  WithCppHeapWithCustomSpace() {
    IsolateWrapper::set_cpp_heap_for_next_isolate(v8::CppHeap::Create(
        V8::GetCurrentPlatform(), CppHeapCreateParams{GetCustomSpaces()}));
  }
};

class UnifiedHeapWithCustomSpaceSnapshotTest
    : public WithUnifiedHeap<                                //
          WithContextMixin<                                  //
              WithHeapInternals<                             //
                  WithInternalIsolateMixin<                  //
                      WithIsolateScopeMixin<                 //
                          WithIsolateMixin<                  //
                              WithCppHeapWithCustomSpace<    //
                                  WithDefaultPlatformMixin<  //
                                      ::testing::Test>>>>>>>> {
 public:
  const v8::HeapSnapshot* TakeHeapSnapshot(
      cppgc::EmbedderStackState stack_state =
          cppgc::EmbedderStackState::kMayContainHeapPointers,
      v8::HeapProfiler::HeapSnapshotMode snapshot_mode =
          v8::HeapProfiler::HeapSnapshotMode::kExposeInternals) {
    v8::HeapProfiler* heap_profiler = v8_isolate()->GetHeapProfiler();

    v8::HeapProfiler::HeapSnapshotOptions options;
    options.snapshot_mode = snapshot_mode;
    options.numerics_mode = v8::HeapProfiler::NumericsMode::kHideNumericValues;
    options.stack_state = stack_state;
    return heap_profiler->TakeHeapSnapshot(options);
  }
};

TEST_F(UnifiedHeapWithCustomSpaceSnapshotTest, ConsistentIdAfterCompaction) {
  // Ensure that only things held by Persistent handles will remain after GC.
  DisableConservativeStackScanningScopeForTesting no_css(isolate()->heap());

  // Allocate an object that will be thrown away by the GC, so that there's
  // somewhere for the compactor to move stuff to.
  cppgc::Persistent<CompactableGCed> trash =
      cppgc::MakeGarbageCollected<CompactableGCed>(allocation_handle());

  // Create the object which we'll actually test.
  cppgc::Persistent<CompactableHolder> gced =
      cppgc::MakeGarbageCollected<CompactableHolder>(allocation_handle(),
                                                     allocation_handle());

  // Release the persistent reference to the other object.
  trash.Release();

  void* original_pointer = gced->object.Get();

  // This first snapshot should not trigger compaction of the cppgc heap because
  // the heap is still very small.
  const v8::HeapSnapshot* snapshot1 =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot1));
  EXPECT_EQ(original_pointer, gced->object.Get());

  // Manually run a GC with compaction. The GCed object should move.
  CppHeap::From(isolate()->heap()->cpp_heap())
      ->compactor()
      .EnableForNextGCForTesting();
  i::InvokeMajorGC(isolate(), i::GCFlag::kReduceMemoryFootprint);
  EXPECT_NE(original_pointer, gced->object.Get());

  // In the second heap snapshot, the moved object should still have the same
  // ID.
  const v8::HeapSnapshot* snapshot2 =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot2));
  std::vector<SnapshotObjectId> ids1 =
      GetIds(*snapshot1, GetExpectedName<CompactableGCed>());
  std::vector<SnapshotObjectId> ids2 =
      GetIds(*snapshot2, GetExpectedName<CompactableGCed>());
  // Depending on build config, GetIds might have returned only the ID for the
  // CompactableGCed instance or it might have also returned the ID for the
  // CompactableHolder.
  EXPECT_TRUE(ids1.size() == 1 || ids1.size() == 2);
  std::sort(ids1.begin(), ids1.end());
  std::sort(ids2.begin(), ids2.end());
  EXPECT_EQ(ids1, ids2);
}

TEST_F(UnifiedHeapSnapshotTest, RetainedByCppCrossThreadRoot) {
  cppgc::subtle::CrossThreadPersistent<GCed> gced =
      cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot, {kExpectedGCRootsName, kExpectedCppCrossThreadRootsName,
                  GetExpectedName<GCed>()}));
}

TEST_F(UnifiedHeapSnapshotTest, RetainedByStackRoots) {
  auto* volatile gced = cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot, {kExpectedGCRootsName, kExpectedCppStackRootsName,
                  GetExpectedName<GCed>()}));
  EXPECT_STREQ(gced->GetHumanReadableName(), GetExpectedName<GCed>());
}

TEST_F(UnifiedHeapSnapshotTest, EphemeronContainerReachableFromStack) {
  // Keep keys alive in case we trigger GC during CreateEphemeronContainer.
  std::vector<cppgc::Persistent<EphemeronKey>> keys;
  EphemeronContainer* container =
      CreateEphemeronContainer(keys, allocation_handle());
  EphemeronContainerBacking* backing = container->backing();

  // Here we want to clear the vector of Persistents to demonstrate the keys are
  // kept alive just from the backing pointer on the stack.
  keys.clear();

  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers);
  ASSERT_TRUE(IsValidSnapshot(snapshot));

  HeapSnapshot* heap_snapshot =
      reinterpret_cast<HeapSnapshot*>(const_cast<v8::HeapSnapshot*>(snapshot));
  const HeapEntry* stack_roots_entry =
      GetEntryByName(heap_snapshot, kExpectedCppStackRootsName);
  ASSERT_NE(nullptr, stack_roots_entry);

  const HeapEntry* backing_entry =
      GetEntryFor(isolate(), heap_snapshot, backing);
  ASSERT_NE(nullptr, backing_entry);
  EXPECT_NE(nullptr, FindFirstEdgeTo(*stack_roots_entry, *backing_entry));
  // Because the backing is reachable from stack, the backing object has an edge
  // to each key in it.
  EXPECT_EQ(static_cast<int>(EphemeronContainerBacking::kEphemeronCount),
            backing_entry->children_count());

  for (size_t i = 0; i < EphemeronContainerBacking::kEphemeronCount; ++i) {
    const HeapEntry* key_entry =
        GetEntryFor(isolate(), heap_snapshot, container->key(i));
    ASSERT_NE(nullptr, key_entry);
    const HeapEntry* value_entry =
        GetEntryFor(isolate(), heap_snapshot, container->value(i));
    ASSERT_NE(nullptr, value_entry);

    // The ephemeron container backing is reachable from stack, so the container
    // has an edge to all keys.
    const HeapGraphEdge* backing_to_key =
        FindFirstEdgeTo(*backing_entry, *key_entry);
    ASSERT_NE(nullptr, backing_to_key);

    // Each key has an edge to its linked ephemeron value.
    const HeapGraphEdge* key_to_value =
        FindFirstEdgeTo(*key_entry, *value_entry);
    ASSERT_NE(nullptr, key_to_value);
    EXPECT_STREQ("part of key -> value pair in ephemeron table",
                 key_to_value->name());
  }
}

TEST_F(UnifiedHeapSnapshotTest, EphemeronContainerNotReachableFromStack) {
  std::vector<cppgc::Persistent<EphemeronKey>> keys;
  cppgc::Persistent<EphemeronContainer> container(
      CreateEphemeronContainer(keys, allocation_handle()));

  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);

  HeapSnapshot* heap_snapshot =
      reinterpret_cast<HeapSnapshot*>(const_cast<v8::HeapSnapshot*>(snapshot));
  EXPECT_EQ(nullptr, GetEntryByName(heap_snapshot, kExpectedCppStackRootsName));

  const HeapEntry* backing_entry =
      GetEntryFor(isolate(), heap_snapshot, container->backing());
  ASSERT_NE(nullptr, backing_entry);
  // Currently the ephemeron container backing doesn't have any edges when not
  // reachable from stack.
  EXPECT_EQ(0, backing_entry->children_count());

  for (size_t i = 0; i < EphemeronContainerBacking::kEphemeronCount; ++i) {
    const HeapEntry* key_entry =
        GetEntryFor(isolate(), heap_snapshot, container->key(i));
    ASSERT_NE(nullptr, key_entry);
    const HeapEntry* value_entry =
        GetEntryFor(isolate(), heap_snapshot, container->value(i));
    ASSERT_NE(nullptr, value_entry);

    // Each key has an edge to its linked ephemeron value.
    const HeapGraphEdge* key_to_value =
        FindFirstEdgeTo(*key_entry, *value_entry);
    ASSERT_NE(nullptr, key_to_value);
    EXPECT_STREQ("part of key -> value pair in ephemeron table",
                 key_to_value->name());
  }
}

TEST_F(UnifiedHeapSnapshotTest, RetainedByCppStackRootTracedReference) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  v8::TracedReference<v8::Object> traced(v8_isolate(),
                                         v8::Object::New(v8_isolate()));

  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppStackTracedHandlesName, "Object"}));
}

namespace {

class GCedWithTracedReference
    : public cppgc::GarbageCollected<GCedWithTracedReference> {
 public:
  void Trace(cppgc::Visitor* v) const { v->Trace(v8_object_); }

  v8::TracedReference<v8::Object> v8_object_;
};

}  // namespace

TEST_F(UnifiedHeapSnapshotTest, NoWeakTracedReference) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  auto* volatile gced =
      cppgc::MakeGarbageCollected<GCedWithTracedReference>(allocation_handle());
  gced->v8_object_.Reset(v8_isolate(), v8::Object::New(v8_isolate()));

  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_FALSE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppStackTracedHandlesName, "Object"}));
  EXPECT_FALSE(ContainsRetainingPath(
      *snapshot, {kExpectedGCRootsName, "(Traced handles)", "Object"}));
}

TEST_F(UnifiedHeapSnapshotTest, RetainingUnnamedTypeWithInternalDetails) {
  cppgc::Persistent<BaseWithoutName> base_without_name =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot, {kExpectedGCRootsName, kExpectedCppRootsName,
                  GetExpectedName<BaseWithoutName>()}));
  CheckSize(snapshot, GetExpectedName<BaseWithoutName>(),
            GetCppSize(base_without_name.Get()));
  EXPECT_EQ(0u, GetExtraNativeBytes(snapshot));
}

TEST_F(UnifiedHeapSnapshotTest, RetainingUnnamedTypeWithoutInternalDetails) {
  cppgc::Persistent<BaseWithoutName> base_without_name =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers,
                       v8::HeapProfiler::HeapSnapshotMode::kRegular);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot, {kExpectedGCRootsName, kExpectedCppRootsName,
                  GetExpectedName<BaseWithoutName>()}));
  CheckSize(snapshot, GetExpectedName<BaseWithoutName>(),
            GetCppSize(base_without_name.Get()));
  EXPECT_EQ(0u, GetExtraNativeBytes(snapshot));
}

TEST_F(UnifiedHeapSnapshotTest, RetainingNamedThroughUnnamed) {
  cppgc::Persistent<BaseWithoutName> base_without_name =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  base_without_name->next =
      cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers,
                       v8::HeapProfiler::HeapSnapshotMode::kRegular);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName,
       GetExpectedName<BaseWithoutName>(), GetExpectedName<GCed>()}));
  CheckSize(snapshot, GetExpectedName<BaseWithoutName>(),
            GetCppSize(base_without_name.Get()));
  CheckSize(snapshot, GetExpectedName<GCed>(),
            GetCppSize(base_without_name->next.Get()));
  EXPECT_EQ(0u, GetExtraNativeBytes(snapshot));
}

TEST_F(UnifiedHeapSnapshotTest, PendingCallStack) {
  // Test ensures that the algorithm handles references into the current call
  // stack.
  //
  // Graph:
  //   Persistent -> BaseWithoutName (2) <-> BaseWithoutName (1) -> GCed (3)
  //
  // Visitation order is (1)->(2)->(3) which is a corner case, as when following
  // back from (2)->(1) the object in (1) is already visited and will only later
  // be marked as visible.
  auto* first =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  auto* second =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  first->next = second;
  first->next->next = first;
  auto* third = cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  first->next2 = third;

  cppgc::Persistent<BaseWithoutName> holder(second);
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers,
                       v8::HeapProfiler::HeapSnapshotMode::kRegular);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName,
       GetExpectedName<BaseWithoutName>(), GetExpectedName<BaseWithoutName>(),
       GetExpectedName<GCed>()}));
  CheckSize(snapshot, GetExpectedName<BaseWithoutName>(), GetCppSize(first));
  CheckSize(snapshot, GetExpectedName<GCed>(), GetCppSize(third));
  EXPECT_EQ(0u, GetExtraNativeBytes(snapshot));
}

TEST_F(UnifiedHeapSnapshotTest, ReferenceToFinishedSCC) {
  // Test ensures that the algorithm handles reference into an already finished
  // SCC that is marked as hidden whereas the current SCC would resolve to
  // visible.
  //
  // Graph:
  //   Persistent -> BaseWithoutName (1)
  //   Persistent -> BaseWithoutName (2)
  //                        + <-> BaseWithoutName (3) -> BaseWithoutName (1)
  //                        + -> GCed (4)
  //
  // Visitation order (1)->(2)->(3)->(1) which is a corner case as (3) would set
  // a dependency on (1) which is hidden. Instead (3) should set a dependency on
  // (2) as (1) resolves to hidden whereas (2) resolves to visible. The test
  // ensures that resolved hidden dependencies are ignored.
  cppgc::Persistent<BaseWithoutName> hidden_holder(
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle()));
  auto* first =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  auto* second =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  first->next = second;
  second->next = *hidden_holder;
  second->next2 = first;
  first->next2 = cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  cppgc::Persistent<BaseWithoutName> holder(first);
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers,
                       v8::HeapProfiler::HeapSnapshotMode::kRegular);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName,
       GetExpectedName<BaseWithoutName>(), GetExpectedName<BaseWithoutName>(),
       GetExpectedName<BaseWithoutName>(), GetExpectedName<GCed>()}));
}

namespace {

class GCedWithJSRef : public cppgc::GarbageCollected<GCedWithJSRef> {
 public:
  static constexpr const char kExpectedName[] =
      "v8::internal::(anonymous namespace)::GCedWithJSRef";

  virtual void Trace(cppgc::Visitor* v) const { v->Trace(v8_object_); }

  void SetV8Object(v8::Isolate* isolate, v8::Local<v8::Object> object) {
    v8_object_.Reset(isolate, object);
  }

  TracedReference<v8::Object>& wrapper() { return v8_object_; }

  void set_detachedness(v8::EmbedderGraph::Node::Detachedness detachedness) {
    detachedness_ = detachedness;
  }
  v8::EmbedderGraph::Node::Detachedness detachedness() const {
    return detachedness_;
  }

 private:
  TracedReference<v8::Object> v8_object_;
  v8::EmbedderGraph::Node::Detachedness detachedness_ =
      v8::EmbedderGraph::Node::Detachedness ::kUnknown;
};

constexpr const char GCedWithJSRef::kExpectedName[];

class V8_NODISCARD JsTestingScope {
 public:
  explicit JsTestingScope(v8::Isolate* isolate)
      : isolate_(isolate),
        handle_scope_(isolate),
        context_(v8::Context::New(isolate)),
        context_scope_(context_) {}

  v8::Isolate* isolate() const { return isolate_; }
  v8::Local<v8::Context> context() const { return context_; }

 private:
  v8::Isolate* isolate_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

cppgc::Persistent<GCedWithJSRef> SetupWrapperWrappablePair(
    JsTestingScope& testing_scope, cppgc::AllocationHandle& allocation_handle,
    const char* name,
    v8::EmbedderGraph::Node::Detachedness detachedness =
        v8::EmbedderGraph::Node::Detachedness::kUnknown) {
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref =
      cppgc::MakeGarbageCollected<GCedWithJSRef>(allocation_handle);
  v8::Local<v8::Object> wrapper_object = WrapperHelper::CreateWrapper(
      testing_scope.context(), gc_w_js_ref.Get(), name);
  gc_w_js_ref->SetV8Object(testing_scope.isolate(), wrapper_object);
  gc_w_js_ref->set_detachedness(detachedness);
  return gc_w_js_ref;
}

}  // namespace

TEST_F(UnifiedHeapSnapshotTest, JSReferenceForcesVisibleObject) {
  // Test ensures that a C++->JS reference forces an object to be visible in the
  // snapshot.
  JsTestingScope testing_scope(v8_isolate());
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "LeafJSObject");
  // Reset the JS->C++ ref or otherwise the nodes would be merged.
  WrapperHelper::ResetWrappableConnection(
      v8_isolate(), gc_w_js_ref->wrapper().Get(v8_isolate()));
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers,
                       v8::HeapProfiler::HeapSnapshotMode::kRegular);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(
      ContainsRetainingPath(*snapshot,
                            {kExpectedGCRootsName, kExpectedCppRootsName,
                             GetExpectedName<GCedWithJSRef>(), "LeafJSObject"},
                            true));
}

template <typename TMixin>
void WithUnifiedHeapSnapshot<TMixin>::TestMergedWrapperNode(
    v8::HeapProfiler::HeapSnapshotMode snapshot_mode) {
  // Test ensures that the snapshot sets a wrapper node for C++->JS references
  // that have a valid back reference and that object nodes are merged. In
  // practice, the C++ node is merged into the existing JS node.
  JsTestingScope testing_scope(TMixin::v8_isolate());
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref = SetupWrapperWrappablePair(
      testing_scope, TMixin::allocation_handle(), "MergedObject");
  v8::Local<v8::Object> next_object = WrapperHelper::CreateWrapper(
      testing_scope.context(), nullptr, "NextObject");
  v8::Local<v8::Object> wrapper_object =
      gc_w_js_ref->wrapper().Get(TMixin::v8_isolate());
  // Chain another object to `wrapper_object`. Since `wrapper_object` should be
  // merged into `GCedWithJSRef`, the additional object must show up as direct
  // child from `GCedWithJSRef`.
  wrapper_object
      ->Set(testing_scope.context(),
            v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "link")
                .ToLocalChecked(),
            next_object)
      .ToChecked();
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot(
      cppgc::EmbedderStackState::kMayContainHeapPointers, snapshot_mode);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  const char* kExpectedName = GetExpectedName<GCedWithJSRef>();
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName, kExpectedName,
       // GCedWithJSRef is merged into MergedObject, replacing its name.
       "NextObject"}));
  const size_t js_size = Utils::OpenDirectHandle(*wrapper_object)->Size();
  const size_t cpp_size = GetCppSize(gc_w_js_ref.Get());
  CheckSize(snapshot, kExpectedName, cpp_size + js_size);
}

TEST_F(UnifiedHeapSnapshotTest, MergedWrapperNodeWithInternalDetails) {
  TestMergedWrapperNode(v8::HeapProfiler::HeapSnapshotMode::kExposeInternals);
}

TEST_F(UnifiedHeapSnapshotTest, MergedWrapperNodeWithoutInternalDetails) {
  TestMergedWrapperNode(v8::HeapProfiler::HeapSnapshotMode::kRegular);
}

namespace {

class DetachednessHandler {
 public:
  static size_t callback_count;

  static v8::EmbedderGraph::Node::Detachedness GetDetachedness(
      v8::Isolate* isolate, const v8::Local<v8::Value>& v8_value, uint16_t,
      void*) {
    callback_count++;
    return WrapperHelper::UnwrapAs<GCedWithJSRef>(isolate,
                                                  v8_value.As<v8::Object>())
        ->detachedness();
  }

  static void Reset() { callback_count = 0; }
};
// static
size_t DetachednessHandler::callback_count = 0;

constexpr uint8_t kExpectedDetachedValueForUnknown =
    static_cast<uint8_t>(v8::EmbedderGraph::Node::Detachedness::kUnknown);
constexpr uint8_t kExpectedDetachedValueForAttached =
    static_cast<uint8_t>(v8::EmbedderGraph::Node::Detachedness::kAttached);
constexpr uint8_t kExpectedDetachedValueForDetached =
    static_cast<uint8_t>(v8::EmbedderGraph::Node::Detachedness::kDetached);

}  // namespace

TEST_F(UnifiedHeapSnapshotTest, DetachedObjectsRetainedByJSReference) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  heap_profiler->SetGetDetachednessCallback(
      DetachednessHandler::GetDetachedness, nullptr);
  // Test ensures that objects that are retained by a JS reference are obtained
  // by the GetDetachedJSWrapperObjects() function
  JsTestingScope testing_scope(v8_isolate());
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "Obj",
      v8::EmbedderGraph::Node::Detachedness ::kDetached);
  // Ensure we are obtaining a Detached Wrapper
  CHECK_EQ(1, heap_profiler->GetDetachedJSWrapperObjects().size());

  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref_not_detached =
      SetupWrapperWrappablePair(
          testing_scope, allocation_handle(), "Obj",
          v8::EmbedderGraph::Node::Detachedness ::kAttached);
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref_unknown =
      SetupWrapperWrappablePair(
          testing_scope, allocation_handle(), "Obj",
          v8::EmbedderGraph::Node::Detachedness ::kUnknown);
  // Ensure we are only obtaining Wrappers that are Detached
  CHECK_EQ(1, heap_profiler->GetDetachedJSWrapperObjects().size());

  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref2 = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "Obj",
      v8::EmbedderGraph::Node::Detachedness ::kDetached);
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref3 = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "Obj",
      v8::EmbedderGraph::Node::Detachedness ::kDetached);
  // Ensure we are obtaining all Detached Wrappers
  CHECK_EQ(3, heap_profiler->GetDetachedJSWrapperObjects().size());
}

TEST_F(UnifiedHeapSnapshotTest, NoTriggerForStandAloneTracedReference) {
  // Test ensures that C++ objects with TracedReference have their V8 objects
  // not merged and queried for detachedness if the backreference is invalid.
  JsTestingScope testing_scope(v8_isolate());
  // Marking the object as attached. The check below queries for unknown, making
  // sure that the state is not propagated.
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "MergedObject",
      v8::EmbedderGraph::Node::Detachedness::kAttached);
  DetachednessHandler::Reset();
  v8_isolate()->GetHeapProfiler()->SetGetDetachednessCallback(
      DetachednessHandler::GetDetachedness, nullptr);
  WrapperHelper::ResetWrappableConnection(
      v8_isolate(), gc_w_js_ref->wrapper().Get(v8_isolate()));
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_EQ(0u, DetachednessHandler::callback_count);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(
      ContainsRetainingPath(*snapshot, {
                                           kExpectedGCRootsName,
                                           kExpectedCppRootsName,
                                           GetExpectedName<GCedWithJSRef>(),
                                       }));
  ForEachEntryWithName(
      snapshot, GetExpectedName<GCedWithJSRef>(), [](const HeapEntry& entry) {
        EXPECT_EQ(kExpectedDetachedValueForUnknown, entry.detachedness());
      });
}

TEST_F(UnifiedHeapSnapshotTest, TriggerDetachednessCallbackSettingAttached) {
  // Test ensures that objects with JS references that have a valid back
  // reference set do have their detachedness state queried and set (attached
  // version).
  JsTestingScope testing_scope(v8_isolate());
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "MergedObject",
      v8::EmbedderGraph::Node::Detachedness::kAttached);
  DetachednessHandler::Reset();
  v8_isolate()->GetHeapProfiler()->SetGetDetachednessCallback(
      DetachednessHandler::GetDetachedness, nullptr);
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_EQ(1u, DetachednessHandler::callback_count);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(
      ContainsRetainingPath(*snapshot, {
                                           kExpectedGCRootsName,
                                           kExpectedCppRootsName,
                                           GetExpectedName<GCedWithJSRef>(),
                                       }));
  ForEachEntryWithName(
      snapshot, GetExpectedName<GCedWithJSRef>(), [](const HeapEntry& entry) {
        EXPECT_EQ(kExpectedDetachedValueForAttached, entry.detachedness());
      });
}

TEST_F(UnifiedHeapSnapshotTest, TriggerDetachednessCallbackSettingDetached) {
  // Test ensures that objects with JS references that have a valid back
  // reference set do have their detachedness state queried and set (detached
  // version).
  JsTestingScope testing_scope(v8_isolate());
  cppgc::Persistent<GCedWithJSRef> gc_w_js_ref = SetupWrapperWrappablePair(
      testing_scope, allocation_handle(), "MergedObject",
      v8::EmbedderGraph::Node::Detachedness ::kDetached);
  DetachednessHandler::Reset();
  v8_isolate()->GetHeapProfiler()->SetGetDetachednessCallback(
      DetachednessHandler::GetDetachedness, nullptr);
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_EQ(1u, DetachednessHandler::callback_count);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(
      ContainsRetainingPath(*snapshot, {
                                           kExpectedGCRootsName,
                                           kExpectedCppRootsName,
                                           GetExpectedName<GCedWithJSRef>(),
                                       }));
  ForEachEntryWithName(
      snapshot, GetExpectedName<GCedWithJSRef>(), [](const HeapEntry& entry) {
        EXPECT_EQ(kExpectedDetachedValueForDetached, entry.detachedness());
      });
}

namespace {
class WrappedContext : public cppgc::GarbageCollected<WrappedContext>,
                       public cppgc::NameProvider {
 public:
  static constexpr const char kExpectedName[] = "cppgc WrappedContext";

  // Cycle:
  // Context -> EmbdderData -> WrappedContext JS object -> WrappedContext cppgc
  // object -> Context
  static cppgc::Persistent<WrappedContext> New(v8::Isolate* isolate) {
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Local<v8::Object> obj =
        WrapperHelper::CreateWrapper(context, nullptr, "js WrappedContext");
    context->SetEmbedderDataV2(kContextDataIndex, obj);
    cppgc::Persistent<WrappedContext> ref =
        cppgc::MakeGarbageCollected<WrappedContext>(
            isolate->GetCppHeap()->GetAllocationHandle(), isolate, obj,
            context);
    WrapperHelper::SetWrappableConnection(isolate, obj, ref.Get());
    return ref;
  }

  static v8::EmbedderGraph::Node::Detachedness GetDetachedness(
      v8::Isolate* isolate, const v8::Local<v8::Value>& v8_value, uint16_t,
      void*) {
    return WrapperHelper::UnwrapAs<WrappedContext>(isolate,
                                                   v8_value.As<v8::Object>())
        ->detachedness();
  }

  const char* GetHumanReadableName() const final { return kExpectedName; }

  virtual void Trace(cppgc::Visitor* v) const {
    v->Trace(object_);
    v->Trace(context_);
  }

  WrappedContext(v8::Isolate* isolate, v8::Local<v8::Object> object,
                 v8::Local<v8::Context> context) {
    object_.Reset(isolate, object);
    context_.Reset(isolate, context);
  }

  v8::Local<v8::Context> context(v8::Isolate* isolate) {
    return context_.Get(isolate);
  }

  void set_detachedness(v8::EmbedderGraph::Node::Detachedness detachedness) {
    detachedness_ = detachedness;
  }
  v8::EmbedderGraph::Node::Detachedness detachedness() const {
    return detachedness_;
  }

 private:
  static constexpr int kContextDataIndex = 0;
  // This is needed to merge the nodes in the heap snapshot.
  TracedReference<v8::Object> object_;
  TracedReference<v8::Context> context_;
  v8::EmbedderGraph::Node::Detachedness detachedness_ =
      v8::EmbedderGraph::Node::Detachedness::kUnknown;
};
}  // anonymous namespace

TEST_F(UnifiedHeapSnapshotTest, WrappedContext) {
  JsTestingScope testing_scope(v8_isolate());
  v8_isolate()->GetHeapProfiler()->SetGetDetachednessCallback(
      WrappedContext::GetDetachedness, nullptr);
  cppgc::Persistent<WrappedContext> wrapped = WrappedContext::New(v8_isolate());
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName,
       wrapped->GetHumanReadableName(), "system / NativeContext",
       "system / EmbedderDataArray", wrapped->GetHumanReadableName()},
      true));

  wrapped->set_detachedness(v8::EmbedderGraph::Node::Detachedness::kDetached);
  v8_isolate()->GetHeapProfiler()->DeleteAllHeapSnapshots();
  snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName,
       wrapped->GetHumanReadableName(), "system / NativeContext",
       "system / EmbedderDataArray", wrapped->GetHumanReadableName()},
      true));
  ForEachEntryWithName(
      snapshot, wrapped->GetHumanReadableName(), [](const HeapEntry& entry) {
        EXPECT_EQ(kExpectedDetachedValueForDetached, entry.detachedness());
      });
}

namespace {

class GCedWithDynamicName : public cppgc::GarbageCollected<GCedWithDynamicName>,
                            public cppgc::NameProvider {
 public:
  virtual void Trace(cppgc::Visitor* v) const {}

  void SetValue(int value) { value_ = value; }

  const char* GetHumanReadableName() const final {
    v8::HeapProfiler* heap_profiler =
        v8::Isolate::GetCurrent()->GetHeapProfiler();
    if (heap_profiler->IsTakingSnapshot()) {
      std::string name = "dynamic name " + std::to_string(value_);
      return heap_profiler->CopyNameForHeapSnapshot(name.c_str());
    }
    return "static name";
  }

 private:
  int value_ = 0;
};

}  // namespace

TEST_F(UnifiedHeapSnapshotTest, DynamicName) {
  cppgc::Persistent<GCedWithDynamicName> object_zero =
      cppgc::MakeGarbageCollected<GCedWithDynamicName>(allocation_handle());
  cppgc::Persistent<GCedWithDynamicName> object_one =
      cppgc::MakeGarbageCollected<GCedWithDynamicName>(allocation_handle());
  object_one->SetValue(1);
  std::string static_name =
      cppgc::internal::HeapObjectHeader::FromObject(object_one.Get())
          .GetName()
          .value;
  EXPECT_EQ(static_name, std::string("static name"));
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName, "dynamic name 0"}));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName, "dynamic name 1"}));
  EXPECT_FALSE(ContainsRetainingPath(
      *snapshot, {kExpectedGCRootsName, kExpectedCppRootsName, "static name"}));
}

namespace {

class ExternalData final : public cppgc::GarbageCollected<ExternalData> {
 public:
  static constexpr const char kExpectedName[] =
      "v8::internal::(anonymous namespace)::ExternalData";

  void Trace(cppgc::Visitor* v) const {}
};

class ExternalDataWithJSRef final
    : public cppgc::GarbageCollected<ExternalDataWithJSRef> {
 public:
  void Trace(cppgc::Visitor* v) const { v->Trace(wrapper_); }

  void SetWrapper(v8::Isolate* isolate, v8::Local<v8::Object> wrapper) {
    wrapper_.Reset(isolate, wrapper);
  }

 private:
  TracedReference<v8::Object> wrapper_;
};

class GCedWithCppHeapExternalJSRef final
    : public cppgc::GarbageCollected<GCedWithCppHeapExternalJSRef> {
 public:
  static constexpr const char kExpectedName[] =
      "v8::internal::(anonymous namespace)::GCedWithCppHeapExternalJSRef";

  void Trace(cppgc::Visitor* v) const { v->Trace(v8_cpp_heap_external_); }

  void SetCppHeapExternal(v8::Isolate* isolate,
                          v8::Local<v8::CppHeapExternal> object) {
    v8_cpp_heap_external_.Reset(isolate, object);
  }

 private:
  TracedReference<v8::CppHeapExternal> v8_cpp_heap_external_;
};

}  // namespace

TEST_F(UnifiedHeapSnapshotTest, CppHeapExternal) {
  auto* cpp_object =
      cppgc::MakeGarbageCollected<ExternalData>(allocation_handle());
  v8::Global<v8::CppHeapExternal> cpp_heap_external(
      v8_isolate(),
      v8::CppHeapExternal::New<ExternalData>(
          v8_isolate(), cpp_object, v8::CppHeapPointerTag::kDefaultTag));
  USE(cpp_heap_external);
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, "(Global handles)", "system / CppHeapExternal",
       GetExpectedName<ExternalData>()}));
}

TEST_F(UnifiedHeapSnapshotTest, JSApiWrapperObject) {
  auto* cpp_object =
      cppgc::MakeGarbageCollected<ExternalData>(allocation_handle());
  v8::Global<v8::Object> wrapper(
      v8_isolate(),
      WrapperHelper::CreateWrapper(v8_context(), cpp_object, "MyJSApiWrapper"));
  USE(wrapper);
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(
      ContainsRetainingPath(*snapshot,
                            {kExpectedGCRootsName, "(Global handles)",
                             "MyJSApiWrapper", GetExpectedName<ExternalData>()},
                            true));

  const HeapSnapshot* heap_snapshot =
      reinterpret_cast<const HeapSnapshot*>(snapshot);
  v8::SnapshotObjectId id =
      v8_isolate()->GetHeapProfiler()->GetObjectId(wrapper.Get(v8_isolate()));
  const HeapEntry* wrapper_entry = heap_snapshot->GetEntryById(id);
  ASSERT_NE(nullptr, wrapper_entry);
  EXPECT_STREQ("MyJSApiWrapper", wrapper_entry->name());

  const HeapGraphEdge* edge = GetNamedEdge(*wrapper_entry, "cppgc_object");
  ASSERT_TRUE(edge);
  EXPECT_STREQ(edge->to()->name(), GetExpectedName<ExternalData>());
}

TEST_F(UnifiedHeapSnapshotTest, MergeNodeDoesNotHaveCppgcObject) {
  cppgc::Persistent<ExternalDataWithJSRef> cpp_object =
      cppgc::MakeGarbageCollected<ExternalDataWithJSRef>(allocation_handle());
  v8::Global<v8::Object> wrapper(
      v8_isolate(), WrapperHelper::CreateWrapper(v8_context(), cpp_object.Get(),
                                                 "MergedJSApiWrapper"));
  cpp_object->SetWrapper(v8_isolate(), wrapper.Get(v8_isolate()));
  wrapper.Get(v8_isolate())
      ->Set(v8_context(),
            v8::String::NewFromUtf8(v8_isolate(), "link").ToLocalChecked(),
            v8::Object::New(v8_isolate()))
      .ToChecked();

  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));

  const HeapSnapshot* heap_snapshot =
      reinterpret_cast<const HeapSnapshot*>(snapshot);
  v8::SnapshotObjectId wrapper_id =
      v8_isolate()->GetHeapProfiler()->GetObjectId(wrapper.Get(v8_isolate()));
  const HeapEntry* wrapper_entry = heap_snapshot->GetEntryById(wrapper_id);
  ASSERT_NE(nullptr, wrapper_entry);
  EXPECT_FALSE(HasNamedEdge(*wrapper_entry, "cppgc_object"));
  EXPECT_TRUE(HasNamedEdge(*wrapper_entry, "link"));
}

TEST_F(UnifiedHeapSnapshotTest, CppHeapExternalTracedReference) {
  cppgc::Persistent<GCedWithCppHeapExternalJSRef> root =
      cppgc::MakeGarbageCollected<GCedWithCppHeapExternalJSRef>(
          allocation_handle());
  root->SetCppHeapExternal(
      v8_isolate(),
      v8::CppHeapExternal::New<ExternalData>(
          v8_isolate(),
          cppgc::MakeGarbageCollected<ExternalData>(allocation_handle()),
          v8::CppHeapPointerTag::kDefaultTag));
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedGCRootsName, kExpectedCppRootsName,
       GetExpectedName<GCedWithCppHeapExternalJSRef>(),
       "system / CppHeapExternal", GetExpectedName<ExternalData>()}));
}

}  // namespace internal
}  // namespace v8
