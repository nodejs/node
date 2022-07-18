// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/embedder-tracing.h"

#include "include/v8-function.h"
#include "include/v8-template.h"
#include "src/handles/global-handles.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using LocalEmbedderHeapTracerWithIsolate = TestWithHeapInternals;

namespace heap {

using testing::StrictMock;
using testing::_;
using testing::Return;
using v8::EmbedderHeapTracer;
using v8::internal::LocalEmbedderHeapTracer;

namespace {

LocalEmbedderHeapTracer::WrapperInfo CreateWrapperInfo() {
  return LocalEmbedderHeapTracer::WrapperInfo(nullptr, nullptr);
}

}  // namespace

class MockEmbedderHeapTracer : public EmbedderHeapTracer {
 public:
  MOCK_METHOD(void, TracePrologue, (EmbedderHeapTracer::TraceFlags),
              (override));
  MOCK_METHOD(void, TraceEpilogue, (EmbedderHeapTracer::TraceSummary*),
              (override));
  MOCK_METHOD(void, EnterFinalPause, (EmbedderHeapTracer::EmbedderStackState),
              (override));
  MOCK_METHOD(bool, IsTracingDone, (), (override));
  MOCK_METHOD(void, RegisterV8References,
              ((const std::vector<std::pair<void*, void*> >&)), (override));
  MOCK_METHOD(bool, AdvanceTracing, (double deadline_in_ms), (override));
};

TEST(LocalEmbedderHeapTracer, InUse) {
  MockEmbedderHeapTracer mock_remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&mock_remote_tracer);
  EXPECT_TRUE(local_tracer.InUse());
}

TEST(LocalEmbedderHeapTracer, NoRemoteTracer) {
  LocalEmbedderHeapTracer local_tracer(nullptr);
  // We should be able to call all functions without a remote tracer being
  // attached.
  EXPECT_FALSE(local_tracer.InUse());
  local_tracer.TracePrologue(EmbedderHeapTracer::TraceFlags::kNoFlags);
  local_tracer.EnterFinalPause();
  bool done = local_tracer.Trace(std::numeric_limits<double>::infinity());
  EXPECT_TRUE(done);
  local_tracer.TraceEpilogue();
}

TEST(LocalEmbedderHeapTracer, TracePrologueForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, TracePrologue(_));
  local_tracer.TracePrologue(EmbedderHeapTracer::TraceFlags::kNoFlags);
}

TEST(LocalEmbedderHeapTracer, TracePrologueForwardsMemoryReducingFlag) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer,
              TracePrologue(EmbedderHeapTracer::TraceFlags::kReduceMemory));
  local_tracer.TracePrologue(EmbedderHeapTracer::TraceFlags::kReduceMemory);
}

TEST(LocalEmbedderHeapTracer, TraceEpilogueForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, TraceEpilogue(_));
  local_tracer.TraceEpilogue();
}

TEST(LocalEmbedderHeapTracer, EnterFinalPauseForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, EnterFinalPause(_));
  local_tracer.EnterFinalPause();
}

TEST(LocalEmbedderHeapTracer, IsRemoteTracingDoneForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, IsTracingDone());
  local_tracer.IsRemoteTracingDone();
}

TEST(LocalEmbedderHeapTracer, EnterFinalPauseDefaultStackStateUnkown) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  // The default stack state is expected to be unkown.
  EXPECT_CALL(
      remote_tracer,
      EnterFinalPause(
          EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers));
  local_tracer.EnterFinalPause();
}

TEST_F(LocalEmbedderHeapTracerWithIsolate,
       EnterFinalPauseStackStateIsForwarded) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  EmbedderStackStateScope scope =
      EmbedderStackStateScope::ExplicitScopeForTesting(
          &local_tracer,
          EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers);
  EXPECT_CALL(
      remote_tracer,
      EnterFinalPause(EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers));
  local_tracer.EnterFinalPause();
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, TemporaryEmbedderStackState) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  // Default is unknown, see above.
  {
    EmbedderStackStateScope scope =
        EmbedderStackStateScope::ExplicitScopeForTesting(
            &local_tracer,
            EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers);
    EXPECT_CALL(remote_tracer,
                EnterFinalPause(
                    EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers));
    local_tracer.EnterFinalPause();
  }
}

TEST_F(LocalEmbedderHeapTracerWithIsolate,
       TemporaryEmbedderStackStateRestores) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  // Default is unknown, see above.
  {
    EmbedderStackStateScope scope =
        EmbedderStackStateScope::ExplicitScopeForTesting(
            &local_tracer,
            EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers);
    {
      EmbedderStackStateScope nested_scope =
          EmbedderStackStateScope::ExplicitScopeForTesting(
              &local_tracer,
              EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers);
      EXPECT_CALL(
          remote_tracer,
          EnterFinalPause(
              EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers));
      local_tracer.EnterFinalPause();
    }
    EXPECT_CALL(remote_tracer,
                EnterFinalPause(
                    EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers));
    local_tracer.EnterFinalPause();
  }
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, TraceEpilogueStackStateResets) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  EmbedderStackStateScope scope =
      EmbedderStackStateScope::ExplicitScopeForTesting(
          &local_tracer,
          EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers);
  EXPECT_CALL(
      remote_tracer,
      EnterFinalPause(EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers));
  local_tracer.EnterFinalPause();
  EXPECT_CALL(remote_tracer, TraceEpilogue(_));
  local_tracer.TraceEpilogue();
  EXPECT_CALL(
      remote_tracer,
      EnterFinalPause(
          EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers));
  local_tracer.EnterFinalPause();
}

TEST(LocalEmbedderHeapTracer, IsRemoteTracingDoneIncludesRemote) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, IsTracingDone());
  local_tracer.IsRemoteTracingDone();
}

TEST(LocalEmbedderHeapTracer, RegisterV8ReferencesWithRemoteTracer) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  {
    LocalEmbedderHeapTracer::ProcessingScope scope(&local_tracer);
    scope.AddWrapperInfoForTesting(CreateWrapperInfo());
    EXPECT_CALL(remote_tracer, RegisterV8References(_));
  }
  EXPECT_CALL(remote_tracer, IsTracingDone()).WillOnce(Return(false));
  EXPECT_FALSE(local_tracer.IsRemoteTracingDone());
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, SetRemoteTracerSetsIsolate) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_EQ(isolate(), reinterpret_cast<Isolate*>(remote_tracer.isolate()));
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, DestructorClearsIsolate) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  {
    LocalEmbedderHeapTracer local_tracer(isolate());
    local_tracer.SetRemoteTracer(&remote_tracer);
    EXPECT_EQ(isolate(), reinterpret_cast<Isolate*>(remote_tracer.isolate()));
  }
  EXPECT_EQ(nullptr, remote_tracer.isolate());
}

namespace {

v8::Local<v8::Object> ConstructTraceableJSApiObject(
    v8::Local<v8::Context> context, void* first_field, void* second_field) {
  v8::EscapableHandleScope scope(context->GetIsolate());
  v8::Local<v8::FunctionTemplate> function_t =
      v8::FunctionTemplate::New(context->GetIsolate());
  v8::Local<v8::ObjectTemplate> instance_t = function_t->InstanceTemplate();
  instance_t->SetInternalFieldCount(2);
  v8::Local<v8::Function> function =
      function_t->GetFunction(context).ToLocalChecked();
  v8::Local<v8::Object> instance =
      function->NewInstance(context).ToLocalChecked();
  instance->SetAlignedPointerInInternalField(0, first_field);
  instance->SetAlignedPointerInInternalField(1, second_field);
  EXPECT_FALSE(instance.IsEmpty());
  i::Handle<i::JSReceiver> js_obj = v8::Utils::OpenHandle(*instance);
  EXPECT_EQ(i::JS_API_OBJECT_TYPE, js_obj->map().instance_type());
  return scope.Escape(instance);
}

enum class TracePrologueBehavior { kNoop, kCallV8WriteBarrier };

class TestEmbedderHeapTracer final : public v8::EmbedderHeapTracer {
 public:
  TestEmbedderHeapTracer() = default;
  TestEmbedderHeapTracer(TracePrologueBehavior prologue_behavior,
                         v8::Global<v8::Array> array)
      : prologue_behavior_(prologue_behavior), array_(std::move(array)) {}

  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) final {
    registered_from_v8_.insert(registered_from_v8_.end(),
                               embedder_fields.begin(), embedder_fields.end());
  }

  void AddReferenceForTracing(v8::TracedReference<v8::Value>* ref) {
    to_register_with_v8_references_.push_back(ref);
  }

  bool AdvanceTracing(double deadline_in_ms) final {
    for (auto ref : to_register_with_v8_references_) {
      RegisterEmbedderReference(ref->As<v8::Data>());
    }
    to_register_with_v8_references_.clear();
    return true;
  }

  bool IsTracingDone() final { return to_register_with_v8_references_.empty(); }

  void TracePrologue(EmbedderHeapTracer::TraceFlags) final {
    if (prologue_behavior_ == TracePrologueBehavior::kCallV8WriteBarrier) {
      auto local = array_.Get(isolate());
      local
          ->Set(local->GetCreationContext().ToLocalChecked(), 0,
                v8::Object::New(isolate()))
          .Check();
    }
  }

  void TraceEpilogue(TraceSummary*) final {}
  void EnterFinalPause(EmbedderStackState) final {}

  bool IsRegisteredFromV8(void* first_field) const {
    for (auto pair : registered_from_v8_) {
      if (pair.first == first_field) return true;
    }
    return false;
  }

  void DoNotConsiderAsRootForScavenge(v8::TracedReference<v8::Value>* handle) {
    handle->SetWrapperClassId(17);
    non_root_handles_.push_back(handle);
  }

  bool IsRootForNonTracingGC(
      const v8::TracedReference<v8::Value>& handle) final {
    return handle.WrapperClassId() != 17;
  }

  void ResetHandleInNonTracingGC(
      const v8::TracedReference<v8::Value>& handle) final {
    for (auto* non_root_handle : non_root_handles_) {
      if (*non_root_handle == handle) {
        non_root_handle->Reset();
      }
    }
  }

 private:
  std::vector<std::pair<void*, void*>> registered_from_v8_;
  std::vector<v8::TracedReference<v8::Value>*> to_register_with_v8_references_;
  TracePrologueBehavior prologue_behavior_ = TracePrologueBehavior::kNoop;
  v8::Global<v8::Array> array_;
  std::vector<v8::TracedReference<v8::Value>*> non_root_handles_;
};

class V8_NODISCARD TemporaryEmbedderHeapTracerScope final {
 public:
  TemporaryEmbedderHeapTracerScope(v8::Isolate* isolate,
                                   v8::EmbedderHeapTracer* tracer)
      : isolate_(isolate) {
    isolate_->SetEmbedderHeapTracer(tracer);
  }

  ~TemporaryEmbedderHeapTracerScope() {
    isolate_->SetEmbedderHeapTracer(nullptr);
  }

 private:
  v8::Isolate* const isolate_;
};

}  // namespace

using EmbedderTracingTest = TestWithHeapInternalsAndContext;

TEST_F(EmbedderTracingTest, V8RegisterEmbedderReference) {
  // Tests that wrappers are properly registered with the embedder heap
  // tracer.
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  void* first_and_second_field = reinterpret_cast<void*>(0x2);
  v8::Local<v8::Object> api_object = ConstructTraceableJSApiObject(
      context, first_and_second_field, first_and_second_field);
  ASSERT_FALSE(api_object.IsEmpty());
  i_isolate()->heap()->CollectGarbage(i::OLD_SPACE,
                                      GarbageCollectionReason::kTesting);
  EXPECT_TRUE(tracer.IsRegisteredFromV8(first_and_second_field));
}

TEST_F(EmbedderTracingTest, EmbedderRegisteringV8Reference) {
  // Tests that references that are registered by the embedder heap tracer are
  // considered live by V8.
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  auto handle = std::make_unique<v8::TracedReference<v8::Value>>();
  {
    v8::HandleScope inner_scope(v8_isolate());
    v8::Local<v8::Value> o =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    handle->Reset(v8_isolate(), o);
  }
  tracer.AddReferenceForTracing(handle.get());
  i_isolate()->heap()->CollectGarbage(i::OLD_SPACE,
                                      GarbageCollectionReason::kTesting);
  EXPECT_FALSE(handle->IsEmpty());
}

TEST_F(EmbedderTracingTest, TracingInEphemerons) {
  // Tests that wrappers that are part of ephemerons are traced.
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> key =
      v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
  void* first_and_second_field = reinterpret_cast<void*>(0x8);
  Handle<JSWeakMap> weak_map = i_isolate()->factory()->NewJSWeakMap();
  {
    v8::HandleScope inner_scope(v8_isolate());
    v8::Local<v8::Object> api_object = ConstructTraceableJSApiObject(
        context, first_and_second_field, first_and_second_field);
    EXPECT_FALSE(api_object.IsEmpty());
    Handle<JSObject> js_key =
        handle(JSObject::cast(*v8::Utils::OpenHandle(*key)), i_isolate());
    Handle<JSReceiver> js_api_object = v8::Utils::OpenHandle(*api_object);
    int32_t hash = js_key->GetOrCreateHash(i_isolate()).value();
    JSWeakCollection::Set(weak_map, js_key, js_api_object, hash);
  }
  i_isolate()->heap()->CollectGarbage(i::OLD_SPACE,
                                      GarbageCollectionReason::kTesting);
  EXPECT_TRUE(tracer.IsRegisteredFromV8(first_and_second_field));
}

TEST_F(EmbedderTracingTest, FinalizeTracingIsNoopWhenNotMarking) {
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);

  // Finalize a potentially running garbage collection.
  i_isolate()->heap()->CollectGarbage(OLD_SPACE,
                                      GarbageCollectionReason::kTesting);
  EXPECT_TRUE(i_isolate()->heap()->incremental_marking()->IsStopped());

  int gc_counter = i_isolate()->heap()->gc_count();
  tracer.FinalizeTracing();
  EXPECT_TRUE(i_isolate()->heap()->incremental_marking()->IsStopped());
  EXPECT_EQ(gc_counter, i_isolate()->heap()->gc_count());
}

TEST_F(EmbedderTracingTest, FinalizeTracingWhenMarking) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc(i_isolate());
  Heap* heap = i_isolate()->heap();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);

  // Finalize a potentially running garbage collection.
  heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);
  if (heap->mark_compact_collector()->sweeping_in_progress()) {
    heap->mark_compact_collector()->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }
  heap->tracer()->StopFullCycleIfNeeded();
  EXPECT_TRUE(heap->incremental_marking()->IsStopped());

  i::IncrementalMarking* marking = heap->incremental_marking();
  {
    SafepointScope scope(heap);
    heap->tracer()->StartCycle(
        GarbageCollector::MARK_COMPACTOR, GarbageCollectionReason::kTesting,
        "collector cctest", GCTracer::MarkingType::kIncremental);
    marking->Start(GarbageCollectionReason::kTesting);
  }

  // Sweeping is not runing so we should immediately start marking.
  EXPECT_TRUE(marking->IsMarking());
  tracer.FinalizeTracing();
  EXPECT_TRUE(marking->IsStopped());
}

namespace {

void ConstructJSObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                       v8::TracedReference<v8::Object>* handle) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(v8::Object::New(isolate));
  EXPECT_FALSE(object.IsEmpty());
  *handle = v8::TracedReference<v8::Object>(isolate, object);
  EXPECT_FALSE(handle->IsEmpty());
}

template <typename T>
void ConstructJSApiObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                          T* global) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(
      ConstructTraceableJSApiObject(context, nullptr, nullptr));
  EXPECT_FALSE(object.IsEmpty());
  *global = T(isolate, object);
  EXPECT_FALSE(global->IsEmpty());
}

namespace {

bool InCorrectGeneration(HeapObject object) {
  return FLAG_single_generation ? !i::Heap::InYoungGeneration(object)
                                : i::Heap::InYoungGeneration(object);
}

template <typename GlobalOrPersistent>
bool InCorrectGeneration(v8::Isolate* isolate,
                         const GlobalOrPersistent& global) {
  v8::HandleScope scope(isolate);
  auto tmp = global.Get(isolate);
  return InCorrectGeneration(*v8::Utils::OpenHandle(*tmp));
}

}  // namespace

enum class SurvivalMode { kSurvives, kDies };

template <typename ModifierFunction, typename ConstructTracedReferenceFunction,
          typename GCFunction>
void TracedReferenceTest(v8::Isolate* isolate,
                         ConstructTracedReferenceFunction construct_function,
                         ModifierFunction modifier_function,
                         GCFunction gc_function, SurvivalMode survives) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  auto* global_handles =
      reinterpret_cast<i::Isolate*>(isolate)->global_handles();

  const size_t initial_count = global_handles->handles_count();
  auto handle = std::make_unique<v8::TracedReference<v8::Object>>();
  construct_function(isolate, context, handle.get());
  ASSERT_TRUE(InCorrectGeneration(isolate, *handle));
  modifier_function(*handle);
  const size_t after_modification_count = global_handles->handles_count();
  gc_function();
  // Cannot check the handle as it is not explicitly cleared by the GC. Instead
  // check the handles count.
  CHECK_IMPLIES(survives == SurvivalMode::kSurvives,
                after_modification_count == global_handles->handles_count());
  CHECK_IMPLIES(survives == SurvivalMode::kDies,
                initial_count == global_handles->handles_count());
}

}  // namespace

TEST_F(EmbedderTracingTest, TracedReferenceReset) {
  v8::HandleScope scope(v8_isolate());
  v8::TracedReference<v8::Object> handle;
  ConstructJSObject(v8_isolate(), v8_isolate()->GetCurrentContext(), &handle);
  EXPECT_FALSE(handle.IsEmpty());
  handle.Reset();
  EXPECT_TRUE(handle.IsEmpty());
}

TEST_F(EmbedderTracingTest, TracedReferenceCopyReferences) {
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope outer_scope(v8_isolate());
  i::GlobalHandles* global_handles = i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  auto handle1 = std::make_unique<v8::TracedReference<v8::Value>>();
  {
    v8::HandleScope scope(v8_isolate());
    handle1->Reset(v8_isolate(), v8::Object::New(v8_isolate()));
  }
  auto handle2 = std::make_unique<v8::TracedReference<v8::Value>>(*handle1);
  auto handle3 = std::make_unique<v8::TracedReference<v8::Value>>();
  *handle3 = *handle2;
  EXPECT_EQ(initial_count + 3, global_handles->handles_count());
  EXPECT_FALSE(handle1->IsEmpty());
  EXPECT_EQ(*handle1, *handle2);
  EXPECT_EQ(*handle2, *handle3);
  {
    v8::HandleScope scope(v8_isolate());
    auto tmp = v8::Local<v8::Value>::New(v8_isolate(), *handle3);
    EXPECT_FALSE(tmp.IsEmpty());
    FullGC();
  }
  EXPECT_EQ(initial_count + 3, global_handles->handles_count());
  EXPECT_FALSE(handle1->IsEmpty());
  EXPECT_EQ(*handle1, *handle2);
  EXPECT_EQ(*handle2, *handle3);
  FullGC();
  EXPECT_EQ(initial_count, global_handles->handles_count());
}

TEST_F(EmbedderTracingTest, TracedReferenceToUnmodifiedJSObjectDiesOnFullGC) {
  // When stressing incremental marking, a write barrier may keep the object
  // alive.
  if (FLAG_stress_incremental_marking) return;

  TracedReferenceTest(
      v8_isolate(), ConstructJSObject,
      [](const TracedReference<v8::Object>&) {}, [this]() { FullGC(); },
      SurvivalMode::kDies);
}

TEST_F(EmbedderTracingTest,
       TracedReferenceToUnmodifiedJSObjectSurvivesFullGCWhenHeldAlive) {
  v8::Global<v8::Object> strong_global;
  TracedReferenceTest(
      v8_isolate(), ConstructJSObject,
      [this, &strong_global](const TracedReference<v8::Object>& handle) {
        v8::HandleScope scope(v8_isolate());
        strong_global =
            v8::Global<v8::Object>(v8_isolate(), handle.Get(v8_isolate()));
      },
      [this]() { FullGC(); }, SurvivalMode::kSurvives);
}

TEST_F(EmbedderTracingTest,
       TracedReferenceToUnmodifiedJSObjectSurvivesYoungGC) {
  if (FLAG_single_generation) return;
  ManualGCScope manual_gc(i_isolate());
  TracedReferenceTest(
      v8_isolate(), ConstructJSObject,
      [](const TracedReference<v8::Object>&) {}, [this]() { YoungGC(); },
      SurvivalMode::kSurvives);
}

TEST_F(
    EmbedderTracingTest,
    TracedReferenceToUnmodifiedJSObjectSurvivesYoungGCWhenExcludedFromRoots) {
  if (FLAG_single_generation) return;
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  TracedReferenceTest(
      v8_isolate(), ConstructJSObject,
      [&tracer](const TracedReference<v8::Object>& handle) {
        tracer.DoNotConsiderAsRootForScavenge(&handle.As<v8::Value>());
      },
      [this]() { YoungGC(); }, SurvivalMode::kSurvives);
}

TEST_F(EmbedderTracingTest,
       TracedReferenceToUnmodifiedJSApiObjectSurvivesScavengePerDefault) {
  if (FLAG_single_generation) return;
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  TracedReferenceTest(
      v8_isolate(), ConstructJSApiObject<TracedReference<v8::Object>>,
      [](const TracedReference<v8::Object>&) {}, [this]() { YoungGC(); },
      SurvivalMode::kSurvives);
}

TEST_F(
    EmbedderTracingTest,
    TracedReferenceToUnmodifiedJSApiObjectDiesOnScavengeWhenExcludedFromRoots) {
  if (FLAG_single_generation) return;
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  TracedReferenceTest(
      v8_isolate(), ConstructJSApiObject<TracedReference<v8::Object>>,
      [&tracer](const TracedReference<v8::Object>& handle) {
        tracer.DoNotConsiderAsRootForScavenge(&handle.As<v8::Value>());
      },
      [this]() { YoungGC(); }, SurvivalMode::kDies);
}

TEST_F(EmbedderTracingTest, TracedReferenceWrapperClassId) {
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);

  v8::TracedReference<v8::Object> traced;
  ConstructJSObject(v8_isolate(), v8_isolate()->GetCurrentContext(), &traced);
  EXPECT_EQ(0, traced.WrapperClassId());
  traced.SetWrapperClassId(17);
  EXPECT_EQ(17, traced.WrapperClassId());
}

TEST_F(EmbedderTracingTest, TracedReferenceHandlesMarking) {
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  auto live = std::make_unique<v8::TracedReference<v8::Value>>();
  auto dead = std::make_unique<v8::TracedReference<v8::Value>>();
  live->Reset(v8_isolate(), v8::Undefined(v8_isolate()));
  dead->Reset(v8_isolate(), v8::Undefined(v8_isolate()));
  i::GlobalHandles* global_handles = i_isolate()->global_handles();
  {
    TestEmbedderHeapTracer tracer;
    heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
    tracer.AddReferenceForTracing(live.get());
    const size_t initial_count = global_handles->handles_count();
    FullGC();
    const size_t final_count = global_handles->handles_count();
    // Handles are black allocated, so the first GC does not collect them.
    EXPECT_EQ(initial_count, final_count);
  }

  {
    TestEmbedderHeapTracer tracer;
    heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
    tracer.AddReferenceForTracing(live.get());
    const size_t initial_count = global_handles->handles_count();
    FullGC();
    const size_t final_count = global_handles->handles_count();
    EXPECT_EQ(initial_count, final_count + 1);
  }
}

TEST_F(EmbedderTracingTest, TracedReferenceHandlesDoNotLeak) {
  // TracedReference handles are not cleared by the destructor of the embedder
  // object. To avoid leaks we need to mark these handles during GC.
  // This test checks that unmarked handles do not leak.
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  auto ref = std::make_unique<v8::TracedReference<v8::Value>>();
  ref->Reset(v8_isolate(), v8::Undefined(v8_isolate()));
  i::GlobalHandles* global_handles = i_isolate()->global_handles();
  const size_t initial_count = global_handles->handles_count();
  // We need two GCs because handles are black allocated.
  FullGC();
  FullGC();
  const size_t final_count = global_handles->handles_count();
  EXPECT_EQ(initial_count, final_count + 1);
}

namespace {

class TracedReferenceVisitor final
    : public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  ~TracedReferenceVisitor() override = default;

  void VisitTracedReference(const TracedReference<Value>& value) final {
    if (value.WrapperClassId() == 57) {
      count_++;
    }
  }

  size_t count() const { return count_; }

 private:
  size_t count_ = 0;
};

}  // namespace

TEST_F(EmbedderTracingTest, TracedReferenceIteration) {
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);

  auto handle = std::make_unique<v8::TracedReference<v8::Object>>();
  ConstructJSObject(v8_isolate(), v8_isolate()->GetCurrentContext(),
                    handle.get());
  EXPECT_FALSE(handle->IsEmpty());
  handle->SetWrapperClassId(57);
  TracedReferenceVisitor visitor;
  {
    v8::HandleScope new_scope(v8_isolate());
    tracer.IterateTracedGlobalHandles(&visitor);
  }
  EXPECT_EQ(1u, visitor.count());
}

TEST_F(EmbedderTracingTest, TracePrologueCallingIntoV8WriteBarrier) {
  // Regression test: https://crbug.com/940003
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc(isolate());
  v8::HandleScope scope(v8_isolate());
  v8::Global<v8::Array> global;
  {
    v8::HandleScope new_scope(v8_isolate());
    auto local = v8::Array::New(v8_isolate(), 10);
    global.Reset(v8_isolate(), local);
  }
  TestEmbedderHeapTracer tracer(TracePrologueBehavior::kCallV8WriteBarrier,
                                std::move(global));
  TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  SimulateIncrementalMarking();
  // Finish GC to avoid removing the tracer while GC is running which may end up
  // in an infinite loop because of unprocessed objects.
  FullGC();
}

TEST_F(EmbedderTracingTest, BasicTracedReference) {
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  i::GlobalHandles* global_handles = i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  char* memory = new char[sizeof(v8::TracedReference<v8::Value>)];
  auto* traced = new (memory) v8::TracedReference<v8::Value>();
  {
    v8::HandleScope new_scope(v8_isolate());
    v8::Local<v8::Value> object(ConstructTraceableJSApiObject(
        v8_isolate()->GetCurrentContext(), nullptr, nullptr));
    EXPECT_TRUE(traced->IsEmpty());
    *traced = v8::TracedReference<v8::Value>(v8_isolate(), object);
    EXPECT_FALSE(traced->IsEmpty());
    EXPECT_EQ(initial_count + 1, global_handles->handles_count());
  }
  traced->~TracedReference<v8::Value>();
  EXPECT_EQ(initial_count + 1, global_handles->handles_count());
  // GC should clear the handle.
  FullGC();
  EXPECT_EQ(initial_count, global_handles->handles_count());
  delete[] memory;
}

namespace {

class EmptyEmbedderHeapTracer : public v8::EmbedderHeapTracer {
 public:
  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) final {}

  bool AdvanceTracing(double deadline_in_ms) final { return true; }
  bool IsTracingDone() final { return true; }
  void TracePrologue(EmbedderHeapTracer::TraceFlags) final {}
  void TraceEpilogue(TraceSummary*) final {}
  void EnterFinalPause(EmbedderStackState) final {}
};

// EmbedderHeapTracer that can optimize Scavenger handling when used with
// TracedReference.
class EmbedderHeapTracerNoDestructorNonTracingClearing final
    : public EmptyEmbedderHeapTracer {
 public:
  explicit EmbedderHeapTracerNoDestructorNonTracingClearing(
      uint16_t class_id_to_optimize)
      : class_id_to_optimize_(class_id_to_optimize) {}

  bool IsRootForNonTracingGC(
      const v8::TracedReference<v8::Value>& handle) final {
    return handle.WrapperClassId() != class_id_to_optimize_;
  }

  void ResetHandleInNonTracingGC(
      const v8::TracedReference<v8::Value>& handle) final {
    if (handle.WrapperClassId() != class_id_to_optimize_) return;

    // Convention (for test): Objects that are optimized have their first field
    // set as a back pointer.
    BasicTracedReference<v8::Value>* original_handle =
        reinterpret_cast<BasicTracedReference<v8::Value>*>(
            v8::Object::GetAlignedPointerFromInternalField(
                handle.As<v8::Object>(), 0));
    original_handle->Reset();
  }

 private:
  uint16_t class_id_to_optimize_;
};

template <typename T>
void SetupOptimizedAndNonOptimizedHandle(v8::Isolate* isolate,
                                         uint16_t optimized_class_id,
                                         T* optimized_handle,
                                         T* non_optimized_handle) {
  v8::HandleScope scope(isolate);

  v8::Local<v8::Object> optimized_object(ConstructTraceableJSApiObject(
      isolate->GetCurrentContext(), optimized_handle, nullptr));
  EXPECT_TRUE(optimized_handle->IsEmpty());
  *optimized_handle = T(isolate, optimized_object);
  EXPECT_FALSE(optimized_handle->IsEmpty());
  optimized_handle->SetWrapperClassId(optimized_class_id);

  v8::Local<v8::Object> non_optimized_object(ConstructTraceableJSApiObject(
      isolate->GetCurrentContext(), nullptr, nullptr));
  EXPECT_TRUE(non_optimized_handle->IsEmpty());
  *non_optimized_handle = T(isolate, non_optimized_object);
  EXPECT_FALSE(non_optimized_handle->IsEmpty());
}

}  // namespace

TEST_F(EmbedderTracingTest, TracedReferenceNoDestructorReclaimedOnScavenge) {
  if (FLAG_single_generation) return;
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  constexpr uint16_t kClassIdToOptimize = 23;
  EmbedderHeapTracerNoDestructorNonTracingClearing tracer(kClassIdToOptimize);
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  i::GlobalHandles* global_handles = i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  auto* optimized_handle = new v8::TracedReference<v8::Value>();
  auto* non_optimized_handle = new v8::TracedReference<v8::Value>();
  SetupOptimizedAndNonOptimizedHandle(v8_isolate(), kClassIdToOptimize,
                                      optimized_handle, non_optimized_handle);
  EXPECT_EQ(initial_count + 2, global_handles->handles_count());
  YoungGC();
  EXPECT_EQ(initial_count + 1, global_handles->handles_count());
  EXPECT_TRUE(optimized_handle->IsEmpty());
  delete optimized_handle;
  EXPECT_FALSE(non_optimized_handle->IsEmpty());
  non_optimized_handle->Reset();
  delete non_optimized_handle;
  EXPECT_EQ(initial_count, global_handles->handles_count());
}

namespace {

template <typename T>
V8_NOINLINE void OnStackTest(v8::Isolate* v8_isolate,
                             TestEmbedderHeapTracer* tracer) {
  v8::Global<v8::Object> observer;
  T stack_ref;
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    stack_ref.Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
}

V8_NOINLINE void CreateTracedReferenceInDeepStack(
    v8::Isolate* isolate, v8::Global<v8::Object>* observer) {
  v8::TracedReference<v8::Value> stack_ref;
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
      isolate->GetCurrentContext(), nullptr, nullptr));
  stack_ref.Reset(isolate, object);
  observer->Reset(isolate, object);
  observer->SetWeak();
}

V8_NOINLINE void TracedReferenceNotifyEmptyStackTest(
    v8::Isolate* v8_isolate, TestEmbedderHeapTracer* tracer) {
  v8::Global<v8::Object> observer;
  CreateTracedReferenceInDeepStack(v8_isolate, &observer);
  EXPECT_FALSE(observer.IsEmpty());
  reinterpret_cast<i::Isolate*>(v8_isolate)
      ->heap()
      ->local_embedder_heap_tracer()
      ->NotifyEmptyEmbedderStack();
  FullGC(v8_isolate);
  EXPECT_TRUE(observer.IsEmpty());
}

enum class Operation {
  kCopy,
  kMove,
};

template <typename T>
void PerformOperation(Operation op, T* lhs, T* rhs) {
  switch (op) {
    case Operation::kMove:
      *lhs = std::move(*rhs);
      break;
    case Operation::kCopy:
      *lhs = *rhs;
      rhs->Reset();
      break;
  }
}

enum class TargetHandling {
  kNonInitialized,
  kInitializedYoungGen,
  kInitializedOldGen
};

V8_NOINLINE void StackToHeapTest(v8::Isolate* v8_isolate,
                                 TestEmbedderHeapTracer* tracer, Operation op,
                                 TargetHandling target_handling) {
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_handle;
  v8::TracedReference<v8::Value>* heap_handle =
      new v8::TracedReference<v8::Value>();
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> to_object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    EXPECT_TRUE(InCorrectGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (!FLAG_single_generation &&
        target_handling == TargetHandling::kInitializedOldGen) {
      YoungGC(v8_isolate);
      YoungGC(v8_isolate);
      EXPECT_FALSE(
          i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    heap_handle->Reset(v8_isolate, to_object);
  }
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    stack_handle.Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  tracer->AddReferenceForTracing(heap_handle);
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  tracer->AddReferenceForTracing(heap_handle);
  PerformOperation(op, heap_handle, &stack_handle);
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  FullGC(v8_isolate);
  EXPECT_TRUE(observer.IsEmpty());
  delete heap_handle;
}

V8_NOINLINE void HeapToStackTest(v8::Isolate* v8_isolate,
                                 TestEmbedderHeapTracer* tracer, Operation op,
                                 TargetHandling target_handling) {
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_handle;
  v8::TracedReference<v8::Value>* heap_handle =
      new v8::TracedReference<v8::Value>();
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> to_object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    EXPECT_TRUE(InCorrectGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (!FLAG_single_generation &&
        target_handling == TargetHandling::kInitializedOldGen) {
      YoungGC(v8_isolate);
      YoungGC(v8_isolate);
      EXPECT_FALSE(
          i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    stack_handle.Reset(v8_isolate, to_object);
  }
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    heap_handle->Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  tracer->AddReferenceForTracing(heap_handle);
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  PerformOperation(op, &stack_handle, heap_handle);
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  stack_handle.Reset();
  FullGC(v8_isolate);
  EXPECT_TRUE(observer.IsEmpty());
  delete heap_handle;
}

V8_NOINLINE void StackToStackTest(v8::Isolate* v8_isolate,
                                  TestEmbedderHeapTracer* tracer, Operation op,
                                  TargetHandling target_handling) {
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_handle1;
  v8::TracedReference<v8::Value> stack_handle2;
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> to_object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    EXPECT_TRUE(InCorrectGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (!FLAG_single_generation &&
        target_handling == TargetHandling::kInitializedOldGen) {
      YoungGC(v8_isolate);
      YoungGC(v8_isolate);
      EXPECT_FALSE(
          i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    stack_handle2.Reset(v8_isolate, to_object);
  }
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        v8_isolate->GetCurrentContext(), nullptr, nullptr));
    stack_handle1.Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  PerformOperation(op, &stack_handle2, &stack_handle1);
  FullGC(v8_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  stack_handle2.Reset();
  FullGC(v8_isolate);
  EXPECT_TRUE(observer.IsEmpty());
}

V8_NOINLINE void TracedReferenceCleanedTest(v8::Isolate* v8_isolate,
                                            TestEmbedderHeapTracer* tracer) {
  v8::HandleScope scope(v8_isolate);
  v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
      v8_isolate->GetCurrentContext(), nullptr, nullptr));
  const size_t before = reinterpret_cast<Isolate*>(v8_isolate)
                            ->global_handles()
                            ->NumberOfOnStackHandlesForTesting();
  for (int i = 0; i < 100; i++) {
    v8::TracedReference<v8::Value> stack_handle;
    stack_handle.Reset(v8_isolate, object);
  }
  EXPECT_EQ(before + 1, reinterpret_cast<Isolate*>(v8_isolate)
                            ->global_handles()
                            ->NumberOfOnStackHandlesForTesting());
}

}  // namespace

TEST_F(EmbedderTracingTest, TracedReferenceOnStack) {
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  tracer.SetStackStart(&manual_gc);
  OnStackTest<v8::TracedReference<v8::Value>>(v8_isolate(), &tracer);
}

TEST_F(EmbedderTracingTest, TracedReferenceCleaned) {
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  tracer.SetStackStart(&manual_gc);
  TracedReferenceCleanedTest(v8_isolate(), &tracer);
}

TEST_F(EmbedderTracingTest, TracedReferenceMove) {
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  tracer.SetStackStart(&manual_gc);
  StackToHeapTest(v8_isolate(), &tracer, Operation::kMove,
                  TargetHandling::kNonInitialized);
  StackToHeapTest(v8_isolate(), &tracer, Operation::kMove,
                  TargetHandling::kInitializedYoungGen);
  StackToHeapTest(v8_isolate(), &tracer, Operation::kMove,
                  TargetHandling::kInitializedOldGen);
  HeapToStackTest(v8_isolate(), &tracer, Operation::kMove,
                  TargetHandling::kNonInitialized);
  HeapToStackTest(v8_isolate(), &tracer, Operation::kMove,
                  TargetHandling::kInitializedYoungGen);
  HeapToStackTest(v8_isolate(), &tracer, Operation::kMove,
                  TargetHandling::kInitializedOldGen);
  StackToStackTest(v8_isolate(), &tracer, Operation::kMove,
                   TargetHandling::kNonInitialized);
  StackToStackTest(v8_isolate(), &tracer, Operation::kMove,
                   TargetHandling::kInitializedYoungGen);
  StackToStackTest(v8_isolate(), &tracer, Operation::kMove,
                   TargetHandling::kInitializedOldGen);
}

TEST_F(EmbedderTracingTest, TracedReferenceCopy) {
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  tracer.SetStackStart(&manual_gc);
  StackToHeapTest(v8_isolate(), &tracer, Operation::kCopy,
                  TargetHandling::kNonInitialized);
  StackToHeapTest(v8_isolate(), &tracer, Operation::kCopy,
                  TargetHandling::kInitializedYoungGen);
  StackToHeapTest(v8_isolate(), &tracer, Operation::kCopy,
                  TargetHandling::kInitializedOldGen);
  HeapToStackTest(v8_isolate(), &tracer, Operation::kCopy,
                  TargetHandling::kNonInitialized);
  HeapToStackTest(v8_isolate(), &tracer, Operation::kCopy,
                  TargetHandling::kInitializedYoungGen);
  HeapToStackTest(v8_isolate(), &tracer, Operation::kCopy,
                  TargetHandling::kInitializedOldGen);
  StackToStackTest(v8_isolate(), &tracer, Operation::kCopy,
                   TargetHandling::kNonInitialized);
  StackToStackTest(v8_isolate(), &tracer, Operation::kCopy,
                   TargetHandling::kInitializedYoungGen);
  StackToStackTest(v8_isolate(), &tracer, Operation::kCopy,
                   TargetHandling::kInitializedOldGen);
}

TEST_F(EmbedderTracingTest, NotifyEmptyStack) {
  ManualGCScope manual_gc(i_isolate());
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(v8_isolate(), &tracer);
  tracer.SetStackStart(&manual_gc);
  TracedReferenceNotifyEmptyStackTest(v8_isolate(), &tracer);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
