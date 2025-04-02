// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "include/cppgc/allocation.h"
#include "include/cppgc/common.h"
#include "include/cppgc/cross-thread-persistent.h"
#include "include/cppgc/custom-space.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/name-provider.h"
#include "include/cppgc/persistent.h"
#include "include/v8-cppgc.h"
#include "include/v8-profiler.h"
#include "src/api/api-inl.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/object-allocator.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "src/profiler/heap-snapshot-generator.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"

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
    cppgc::internal::VisitorBase::TraceRawForTesting(
        visitor, const_cast<const CompactableGCed*>(object));
    visitor->RegisterMovableReference(
        const_cast<const CompactableGCed**>(&object));
  }
  CompactableGCed* object = nullptr;
};

}  // namespace v8::internal

namespace cppgc {
template <>
struct SpaceTrait<v8::internal::CompactableGCed> {
  using Space = CompactableCustomSpace;
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
    options.control = nullptr;
    options.global_object_name_resolver = nullptr;
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

static constexpr const char kExpectedCppRootsName[] = "C++ Persistent roots";
static constexpr const char kExpectedCppCrossThreadRootsName[] =
    "C++ CrossThreadPersistent roots";
static constexpr const char kExpectedCppStackRootsName[] =
    "C++ native stack roots";

template <typename T>
constexpr const char* GetExpectedName() {
  if (std::is_base_of<cppgc::NameProvider, T>::value ||
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
      *snapshot, {kExpectedCppRootsName, GetExpectedName<GCed>()}));
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
    options.control = nullptr;
    options.global_object_name_resolver = nullptr;
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

  void* original_pointer = gced->object;

  // This first snapshot should not trigger compaction of the cppgc heap because
  // the heap is still very small.
  const v8::HeapSnapshot* snapshot1 =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kNoHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot1));
  EXPECT_EQ(original_pointer, gced->object);

  // Manually run a GC with compaction. The GCed object should move.
  CppHeap::From(isolate()->heap()->cpp_heap())
      ->compactor()
      .EnableForNextGCForTesting();
  i::InvokeMajorGC(isolate(), i::GCFlag::kReduceMemoryFootprint);
  EXPECT_NE(original_pointer, gced->object);

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
      *snapshot, {kExpectedCppCrossThreadRootsName, GetExpectedName<GCed>()}));
}

TEST_F(UnifiedHeapSnapshotTest, RetainedByStackRoots) {
  auto* volatile gced = cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  const v8::HeapSnapshot* snapshot =
      TakeHeapSnapshot(cppgc::EmbedderStackState::kMayContainHeapPointers);
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot, {kExpectedCppStackRootsName, GetExpectedName<GCed>()}));
  EXPECT_STREQ(gced->GetHumanReadableName(), GetExpectedName<GCed>());
}

TEST_F(UnifiedHeapSnapshotTest, RetainingUnnamedTypeWithInternalDetails) {
  cppgc::Persistent<BaseWithoutName> base_without_name =
      cppgc::MakeGarbageCollected<BaseWithoutName>(allocation_handle());
  const v8::HeapSnapshot* snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot, {kExpectedCppRootsName, GetExpectedName<BaseWithoutName>()}));
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
  EXPECT_FALSE(ContainsRetainingPath(
      *snapshot, {kExpectedCppRootsName, cppgc::NameProvider::kHiddenName}));
  EXPECT_FALSE(ContainsRetainingPath(
      *snapshot, {kExpectedCppRootsName, GetExpectedName<BaseWithoutName>()}));
  EXPECT_EQ(GetCppSize(base_without_name.Get()), GetExtraNativeBytes(snapshot));
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
      *snapshot, {kExpectedCppRootsName, cppgc::NameProvider::kHiddenName,
                  GetExpectedName<GCed>()}));
  CheckSize(snapshot, cppgc::NameProvider::kHiddenName, 0);
  CheckSize(snapshot, GetExpectedName<GCed>(),
            GetCppSize(base_without_name->next.Get()));
  EXPECT_EQ(GetCppSize(base_without_name.Get()), GetExtraNativeBytes(snapshot));
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
      *snapshot, {kExpectedCppRootsName, cppgc::NameProvider::kHiddenName,
                  cppgc::NameProvider::kHiddenName, GetExpectedName<GCed>()}));
  CheckSize(snapshot, cppgc::NameProvider::kHiddenName, 0);
  CheckSize(snapshot, GetExpectedName<GCed>(), GetCppSize(third));
  EXPECT_EQ(GetCppSize(first) + GetCppSize(second),
            GetExtraNativeBytes(snapshot));
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
      *snapshot, {kExpectedCppRootsName, cppgc::NameProvider::kHiddenName,
                  cppgc::NameProvider::kHiddenName,
                  cppgc::NameProvider::kHiddenName, GetExpectedName<GCed>()}));
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
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedCppRootsName, cppgc::NameProvider::kHiddenName, "LeafJSObject"},
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
  const char* kExpectedName =
      snapshot_mode == v8::HeapProfiler::HeapSnapshotMode::kExposeInternals
          ? GetExpectedName<GCedWithJSRef>()
          : cppgc::NameProvider::kHiddenName;
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedCppRootsName, kExpectedName,
       // GCedWithJSRef is merged into MergedObject, replacing its name.
       "NextObject"}));
  const size_t js_size = Utils::OpenDirectHandle(*wrapper_object)->Size();
  if (snapshot_mode == v8::HeapProfiler::HeapSnapshotMode::kExposeInternals) {
    const size_t cpp_size = GetCppSize(gc_w_js_ref.Get());
    CheckSize(snapshot, kExpectedName, cpp_size + js_size);
  } else {
    CheckSize(snapshot, kExpectedName, js_size);
  }
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
    context->SetEmbedderData(kContextDataIndex, obj);
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
      {kExpectedCppRootsName, wrapped->GetHumanReadableName(),
       "system / NativeContext", "system / EmbedderDataArray",
       wrapped->GetHumanReadableName()},
      true));

  wrapped->set_detachedness(v8::EmbedderGraph::Node::Detachedness::kDetached);
  v8_isolate()->GetHeapProfiler()->DeleteAllHeapSnapshots();
  snapshot = TakeHeapSnapshot();
  EXPECT_TRUE(IsValidSnapshot(snapshot));
  EXPECT_TRUE(ContainsRetainingPath(
      *snapshot,
      {kExpectedCppRootsName, wrapped->GetHumanReadableName(),
       "system / NativeContext", "system / EmbedderDataArray",
       wrapped->GetHumanReadableName()},
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
  EXPECT_TRUE(ContainsRetainingPath(*snapshot,
                                    {kExpectedCppRootsName, "dynamic name 0"}));
  EXPECT_TRUE(ContainsRetainingPath(*snapshot,
                                    {kExpectedCppRootsName, "dynamic name 1"}));
  EXPECT_FALSE(
      ContainsRetainingPath(*snapshot, {kExpectedCppRootsName, "static name"}));
}

}  // namespace internal
}  // namespace v8
