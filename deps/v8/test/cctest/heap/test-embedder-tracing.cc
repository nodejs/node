// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include <vector>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/module.h"
#include "src/objects/objects-inl.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {

namespace internal {
namespace heap {

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
  CHECK(!instance.IsEmpty());
  i::Handle<i::JSReceiver> js_obj = v8::Utils::OpenHandle(*instance);
  CHECK_EQ(i::JS_API_OBJECT_TYPE, js_obj->map().instance_type());
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

  void AddReferenceForTracing(v8::TracedGlobal<v8::Value>* global) {
    to_register_with_v8_.push_back(global);
  }

  void AddReferenceForTracing(v8::TracedReference<v8::Value>* ref) {
    to_register_with_v8_references_.push_back(ref);
  }

  bool AdvanceTracing(double deadline_in_ms) final {
    for (auto global : to_register_with_v8_) {
      RegisterEmbedderReference(global->As<v8::Data>());
    }
    to_register_with_v8_.clear();
    for (auto ref : to_register_with_v8_references_) {
      RegisterEmbedderReference(ref->As<v8::Data>());
    }
    to_register_with_v8_references_.clear();
    return true;
  }

  bool IsTracingDone() final { return to_register_with_v8_.empty(); }

  void TracePrologue(EmbedderHeapTracer::TraceFlags) final {
    if (prologue_behavior_ == TracePrologueBehavior::kCallV8WriteBarrier) {
      auto local = array_.Get(isolate());
      local->Set(local->CreationContext(), 0, v8::Object::New(isolate()))
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

  void ConsiderTracedGlobalAsRoot(bool value) {
    consider_traced_global_as_root_ = value;
  }

  bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle) final {
    return consider_traced_global_as_root_;
  }

 private:
  std::vector<std::pair<void*, void*>> registered_from_v8_;
  std::vector<v8::TracedGlobal<v8::Value>*> to_register_with_v8_;
  std::vector<v8::TracedReference<v8::Value>*> to_register_with_v8_references_;
  bool consider_traced_global_as_root_ = true;
  TracePrologueBehavior prologue_behavior_ = TracePrologueBehavior::kNoop;
  v8::Global<v8::Array> array_;
};

}  // namespace

TEST(V8RegisteringEmbedderReference) {
  // Tests that wrappers are properly registered with the embedder heap
  // tracer.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  void* first_field = reinterpret_cast<void*>(0x2);
  v8::Local<v8::Object> api_object =
      ConstructTraceableJSApiObject(context, first_field, nullptr);
  CHECK(!api_object.IsEmpty());
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(tracer.IsRegisteredFromV8(first_field));
}

TEST(EmbedderRegisteringV8Reference) {
  // Tests that references that are registered by the embedder heap tracer are
  // considered live by V8.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::TracedGlobal<v8::Value> g;
  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Value> o =
        v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
    g.Reset(isolate, o);
  }
  tracer.AddReferenceForTracing(&g);
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(!g.IsEmpty());
}

namespace {

void ResurrectingFinalizer(
    const v8::WeakCallbackInfo<v8::Global<v8::Object>>& data) {
  data.GetParameter()->ClearWeak();
}

}  // namespace

TEST(TracingInRevivedSubgraph) {
  // Tests that wrappers are traced when they are contained with in a subgraph
  // that is revived by a finalizer.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Global<v8::Object> g;
  void* first_field = reinterpret_cast<void*>(0x4);
  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Object> api_object =
        ConstructTraceableJSApiObject(context, first_field, nullptr);
    CHECK(!api_object.IsEmpty());
    v8::Local<v8::Object> o =
        v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
    o->Set(context, v8_str("link"), api_object).FromJust();
    g.Reset(isolate, o);
    g.SetWeak(&g, ResurrectingFinalizer, v8::WeakCallbackType::kFinalizer);
  }
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(tracer.IsRegisteredFromV8(first_field));
}

TEST(TracingInEphemerons) {
  // Tests that wrappers that are part of ephemerons are traced.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> key =
      v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
  void* first_field = reinterpret_cast<void*>(0x8);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Handle<JSWeakMap> weak_map = i_isolate->factory()->NewJSWeakMap();
  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Object> api_object =
        ConstructTraceableJSApiObject(context, first_field, nullptr);
    CHECK(!api_object.IsEmpty());
    Handle<JSObject> js_key =
        handle(JSObject::cast(*v8::Utils::OpenHandle(*key)), i_isolate);
    Handle<JSReceiver> js_api_object = v8::Utils::OpenHandle(*api_object);
    int32_t hash = js_key->GetOrCreateHash(i_isolate).value();
    JSWeakCollection::Set(weak_map, js_key, js_api_object, hash);
  }
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(tracer.IsRegisteredFromV8(first_field));
}

TEST(FinalizeTracingIsNoopWhenNotMarking) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  Isolate* i_isolate = CcTest::i_isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  // Finalize a potentially running garbage collection.
  i_isolate->heap()->CollectGarbage(OLD_SPACE,
                                    GarbageCollectionReason::kTesting);
  CHECK(i_isolate->heap()->incremental_marking()->IsStopped());

  int gc_counter = i_isolate->heap()->gc_count();
  tracer.FinalizeTracing();
  CHECK(i_isolate->heap()->incremental_marking()->IsStopped());
  CHECK_EQ(gc_counter, i_isolate->heap()->gc_count());
}

TEST(FinalizeTracingWhenMarking) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  Isolate* i_isolate = CcTest::i_isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  // Finalize a potentially running garbage collection.
  i_isolate->heap()->CollectGarbage(OLD_SPACE,
                                    GarbageCollectionReason::kTesting);
  if (i_isolate->heap()->mark_compact_collector()->sweeping_in_progress()) {
    i_isolate->heap()->mark_compact_collector()->EnsureSweepingCompleted();
  }
  CHECK(i_isolate->heap()->incremental_marking()->IsStopped());

  i::IncrementalMarking* marking = i_isolate->heap()->incremental_marking();
  marking->Start(i::GarbageCollectionReason::kTesting);
  // Sweeping is not runing so we should immediately start marking.
  CHECK(marking->IsMarking());
  tracer.FinalizeTracing();
  CHECK(marking->IsStopped());
}

TEST(GarbageCollectionForTesting) {
  ManualGCScope manual_gc;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  Isolate* i_isolate = CcTest::i_isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  int saved_gc_counter = i_isolate->heap()->gc_count();
  tracer.GarbageCollectionForTesting(
      EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers);
  CHECK_GT(i_isolate->heap()->gc_count(), saved_gc_counter);
}

namespace {

void ConstructJSObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                       v8::TracedGlobal<v8::Object>* global) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(v8::Object::New(isolate));
  CHECK(!object.IsEmpty());
  *global = v8::TracedGlobal<v8::Object>(isolate, object);
  CHECK(!global->IsEmpty());
}

template <typename T>
void ConstructJSApiObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                          T* global) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(
      ConstructTraceableJSApiObject(context, nullptr, nullptr));
  CHECK(!object.IsEmpty());
  *global = T(isolate, object);
  CHECK(!global->IsEmpty());
}

enum class SurvivalMode { kSurvives, kDies };

template <typename ModifierFunction, typename ConstructTracedGlobalFunction>
void TracedGlobalTest(v8::Isolate* isolate,
                      ConstructTracedGlobalFunction construct_function,
                      ModifierFunction modifier_function, void (*gc_function)(),
                      SurvivalMode survives) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::TracedGlobal<v8::Object> global;
  construct_function(isolate, context, &global);
  CHECK(InYoungGeneration(isolate, global));
  modifier_function(global);
  gc_function();
  CHECK_IMPLIES(survives == SurvivalMode::kSurvives, !global.IsEmpty());
  CHECK_IMPLIES(survives == SurvivalMode::kDies, global.IsEmpty());
}

}  // namespace

TEST(TracedGlobalReset) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  v8::TracedGlobal<v8::Object> traced;
  ConstructJSObject(isolate, isolate->GetCurrentContext(), &traced);
  CHECK(!traced.IsEmpty());
  traced.Reset();
  CHECK(traced.IsEmpty());
}

TEST(TracedGlobalInStdVector) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  std::vector<v8::TracedGlobal<v8::Object>> vec;
  {
    v8::HandleScope scope(isolate);
    vec.emplace_back(isolate, v8::Object::New(isolate));
  }
  CHECK(!vec[0].IsEmpty());
  InvokeMarkSweep();
  CHECK(vec[0].IsEmpty());
}

TEST(TracedGlobalCopyWithDestructor) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  v8::TracedGlobal<v8::Object> global1;
  {
    v8::HandleScope scope(isolate);
    global1.Reset(isolate, v8::Object::New(isolate));
  }
  v8::TracedGlobal<v8::Object> global2(global1);
  v8::TracedGlobal<v8::Object> global3;
  global3 = global2;
  CHECK_EQ(initial_count + 3, global_handles->handles_count());
  CHECK(!global1.IsEmpty());
  CHECK_EQ(global1, global2);
  CHECK_EQ(global2, global3);
  {
    v8::HandleScope scope(isolate);
    auto tmp = v8::Local<v8::Object>::New(isolate, global3);
    CHECK(!tmp.IsEmpty());
    InvokeMarkSweep();
  }
  CHECK_EQ(initial_count + 3, global_handles->handles_count());
  CHECK(!global1.IsEmpty());
  CHECK_EQ(global1, global2);
  CHECK_EQ(global2, global3);
  InvokeMarkSweep();
  CHECK_EQ(initial_count, global_handles->handles_count());
  CHECK(global1.IsEmpty());
  CHECK_EQ(global1, global2);
  CHECK_EQ(global2, global3);
}

TEST(TracedGlobalCopyNoDestructor) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  v8::TracedReference<v8::Value> global1;
  {
    v8::HandleScope scope(isolate);
    global1.Reset(isolate, v8::Object::New(isolate));
  }
  v8::TracedReference<v8::Value> global2(global1);
  v8::TracedReference<v8::Value> global3;
  global3 = global2;
  CHECK_EQ(initial_count + 3, global_handles->handles_count());
  CHECK(!global1.IsEmpty());
  CHECK_EQ(global1, global2);
  CHECK_EQ(global2, global3);
  {
    v8::HandleScope scope(isolate);
    auto tmp = v8::Local<v8::Value>::New(isolate, global3);
    CHECK(!tmp.IsEmpty());
    InvokeMarkSweep();
  }
  CHECK_EQ(initial_count + 3, global_handles->handles_count());
  CHECK(!global1.IsEmpty());
  CHECK_EQ(global1, global2);
  CHECK_EQ(global2, global3);
  InvokeMarkSweep();
  CHECK_EQ(initial_count, global_handles->handles_count());
}

TEST(TracedGlobalInStdUnorderedMap) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  std::unordered_map<int, v8::TracedGlobal<v8::Object>> map;
  {
    v8::HandleScope scope(isolate);
    map.emplace(std::piecewise_construct, std::forward_as_tuple(1),
                std::forward_as_tuple(isolate, v8::Object::New(isolate)));
  }
  CHECK(!map[1].IsEmpty());
  InvokeMarkSweep();
  CHECK(map[1].IsEmpty());
}

TEST(TracedGlobalToUnmodifiedJSObjectDiesOnMarkSweep) {
  CcTest::InitializeVM();
  TracedGlobalTest(
      CcTest::isolate(), ConstructJSObject,
      [](const TracedGlobal<v8::Object>& global) {}, InvokeMarkSweep,
      SurvivalMode::kDies);
}

TEST(TracedGlobalToUnmodifiedJSObjectSurvivesMarkSweepWhenHeldAliveOtherwise) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> strong_global;
  TracedGlobalTest(
      CcTest::isolate(), ConstructJSObject,
      [isolate, &strong_global](const TracedGlobal<v8::Object>& global) {
        v8::HandleScope scope(isolate);
        strong_global = v8::Global<v8::Object>(isolate, global.Get(isolate));
      },
      InvokeMarkSweep, SurvivalMode::kSurvives);
}

TEST(TracedGlobalToUnmodifiedJSObjectSurvivesScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TracedGlobalTest(
      CcTest::isolate(), ConstructJSObject,
      [](const TracedGlobal<v8::Object>& global) {}, InvokeScavenge,
      SurvivalMode::kSurvives);
}

TEST(TracedGlobalToUnmodifiedJSObjectSurvivesScavengeWhenExcludedFromRoots) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  tracer.ConsiderTracedGlobalAsRoot(false);
  TracedGlobalTest(
      CcTest::isolate(), ConstructJSObject,
      [](const TracedGlobal<v8::Object>& global) {}, InvokeScavenge,
      SurvivalMode::kSurvives);
}

TEST(TracedGlobalToUnmodifiedJSApiObjectSurvivesScavengePerDefault) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  tracer.ConsiderTracedGlobalAsRoot(true);
  TracedGlobalTest(
      CcTest::isolate(), ConstructJSApiObject<TracedGlobal<v8::Object>>,
      [](const TracedGlobal<v8::Object>& global) {}, InvokeScavenge,
      SurvivalMode::kSurvives);
}

TEST(TracedGlobalToUnmodifiedJSApiObjectDiesOnScavengeWhenExcludedFromRoots) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  tracer.ConsiderTracedGlobalAsRoot(false);
  TracedGlobalTest(
      CcTest::isolate(), ConstructJSApiObject<TracedGlobal<v8::Object>>,
      [](const TracedGlobal<v8::Object>& global) {}, InvokeScavenge,
      SurvivalMode::kDies);
}

TEST(TracedGlobalWrapperClassId) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  v8::TracedGlobal<v8::Object> traced;
  ConstructJSObject(isolate, isolate->GetCurrentContext(), &traced);
  CHECK_EQ(0, traced.WrapperClassId());
  traced.SetWrapperClassId(17);
  CHECK_EQ(17, traced.WrapperClassId());
}

TEST(TracedReferenceHandlesMarking) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::TracedReference<v8::Value> live;
  v8::TracedReference<v8::Value> dead;
  live.Reset(isolate, v8::Undefined(isolate));
  dead.Reset(isolate, v8::Undefined(isolate));
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  {
    TestEmbedderHeapTracer tracer;
    heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
    tracer.AddReferenceForTracing(&live);
    const size_t initial_count = global_handles->handles_count();
    InvokeMarkSweep();
    const size_t final_count = global_handles->handles_count();
    // Handles are black allocated, so the first GC does not collect them.
    CHECK_EQ(initial_count, final_count);
  }

  {
    TestEmbedderHeapTracer tracer;
    heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
    tracer.AddReferenceForTracing(&live);
    const size_t initial_count = global_handles->handles_count();
    InvokeMarkSweep();
    const size_t final_count = global_handles->handles_count();
    CHECK_EQ(initial_count, final_count + 1);
  }
}

TEST(TracedReferenceHandlesDoNotLeak) {
  // TracedReference handles are not cleared by the destructor of the embedder
  // object. To avoid leaks we need to mark these handles during GC.
  // This test checks that unmarked handles do not leak.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::TracedReference<v8::Value> ref;
  ref.Reset(isolate, v8::Undefined(isolate));
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  const size_t initial_count = global_handles->handles_count();
  // We need two GCs because handles are black allocated.
  InvokeMarkSweep();
  InvokeMarkSweep();
  const size_t final_count = global_handles->handles_count();
  CHECK_EQ(initial_count, final_count + 1);
}

TEST(TracedGlobalHandlesAreRetained) {
  // TracedGlobal handles are cleared by the destructor of the embedder object.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::TracedGlobal<v8::Value> global;
  global.Reset(isolate, v8::Undefined(isolate));
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  const size_t initial_count = global_handles->handles_count();
  // We need two GCs because handles are black allocated.
  InvokeMarkSweep();
  InvokeMarkSweep();
  const size_t final_count = global_handles->handles_count();
  CHECK_EQ(initial_count, final_count);
}

namespace {

class TracedGlobalVisitor final
    : public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  ~TracedGlobalVisitor() override = default;
  void VisitTracedGlobalHandle(const TracedGlobal<Value>& value) final {
    if (value.WrapperClassId() == 57) {
      count_++;
    }
  }

  size_t count() const { return count_; }

 private:
  size_t count_ = 0;
};

}  // namespace

TEST(TracedGlobalIteration) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  v8::TracedGlobal<v8::Object> traced;
  ConstructJSObject(isolate, isolate->GetCurrentContext(), &traced);
  CHECK(!traced.IsEmpty());
  traced.SetWrapperClassId(57);
  TracedGlobalVisitor visitor;
  {
    v8::HandleScope scope(isolate);
    tracer.IterateTracedGlobalHandles(&visitor);
  }
  CHECK_EQ(1, visitor.count());
}

namespace {

void FinalizationCallback(const WeakCallbackInfo<void>& data) {
  v8::TracedGlobal<v8::Object>* traced =
      reinterpret_cast<v8::TracedGlobal<v8::Object>*>(data.GetParameter());
  CHECK_EQ(reinterpret_cast<void*>(0x4), data.GetInternalField(0));
  CHECK_EQ(reinterpret_cast<void*>(0x8), data.GetInternalField(1));
  traced->Reset();
}

}  // namespace

TEST(TracedGlobalSetFinalizationCallbackScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  TestEmbedderHeapTracer tracer;
  tracer.ConsiderTracedGlobalAsRoot(false);
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  v8::TracedGlobal<v8::Object> traced;
  ConstructJSApiObject(isolate, isolate->GetCurrentContext(), &traced);
  CHECK(!traced.IsEmpty());
  {
    v8::HandleScope scope(isolate);
    auto local = traced.Get(isolate);
    local->SetAlignedPointerInInternalField(0, reinterpret_cast<void*>(0x4));
    local->SetAlignedPointerInInternalField(1, reinterpret_cast<void*>(0x8));
  }
  traced.SetFinalizationCallback(&traced, FinalizationCallback);
  heap::InvokeScavenge();
  CHECK(traced.IsEmpty());
}

TEST(TracedGlobalSetFinalizationCallbackMarkSweep) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  v8::TracedGlobal<v8::Object> traced;
  ConstructJSApiObject(isolate, isolate->GetCurrentContext(), &traced);
  CHECK(!traced.IsEmpty());
  {
    v8::HandleScope scope(isolate);
    auto local = traced.Get(isolate);
    local->SetAlignedPointerInInternalField(0, reinterpret_cast<void*>(0x4));
    local->SetAlignedPointerInInternalField(1, reinterpret_cast<void*>(0x8));
  }
  traced.SetFinalizationCallback(&traced, FinalizationCallback);
  heap::InvokeMarkSweep();
  CHECK(traced.IsEmpty());
}

TEST(TracePrologueCallingIntoV8WriteBarrier) {
  // Regression test: https://crbug.com/940003
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Global<v8::Array> global;
  {
    v8::HandleScope scope(isolate);
    auto local = v8::Array::New(isolate, 10);
    global.Reset(isolate, local);
  }
  TestEmbedderHeapTracer tracer(TracePrologueBehavior::kCallV8WriteBarrier,
                                std::move(global));
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  SimulateIncrementalMarking(CcTest::i_isolate()->heap());
  // Finish GC to avoid removing the tracer while GC is running which may end up
  // in an infinite loop because of unprocessed objects.
  heap::InvokeMarkSweep();
}

TEST(TracedGlobalWithDestructor) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  auto* traced = new v8::TracedGlobal<v8::Object>();
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    CHECK(traced->IsEmpty());
    *traced = v8::TracedGlobal<v8::Object>(isolate, object);
    CHECK(!traced->IsEmpty());
    CHECK_EQ(initial_count + 1, global_handles->handles_count());
  }
  delete traced;
  CHECK_EQ(initial_count, global_handles->handles_count());
  // GC should not need to clear the handle.
  heap::InvokeMarkSweep();
  CHECK_EQ(initial_count, global_handles->handles_count());
}

TEST(TracedGlobalNoDestructor) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  char* memory = new char[sizeof(v8::TracedReference<v8::Value>)];
  auto* traced = new (memory) v8::TracedReference<v8::Value>();
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    CHECK(traced->IsEmpty());
    *traced = v8::TracedReference<v8::Value>(isolate, object);
    CHECK(!traced->IsEmpty());
    CHECK_EQ(initial_count + 1, global_handles->handles_count());
  }
  traced->~TracedReference<v8::Value>();
  CHECK_EQ(initial_count + 1, global_handles->handles_count());
  // GC should clear the handle.
  heap::InvokeMarkSweep();
  CHECK_EQ(initial_count, global_handles->handles_count());
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
// TraceGlobal handles that have destructors.
class EmbedderHeapTracerDestructorNonTracingClearing final
    : public EmptyEmbedderHeapTracer {
 public:
  explicit EmbedderHeapTracerDestructorNonTracingClearing(
      uint16_t class_id_to_optimize)
      : class_id_to_optimize_(class_id_to_optimize) {}

  bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle) final {
    return handle.WrapperClassId() != class_id_to_optimize_;
  }

 private:
  uint16_t class_id_to_optimize_;
};

// EmbedderHeapTracer that can optimize Scavenger handling when used with
// TraceGlobal handles without destructors.
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
    TracedReferenceBase<v8::Value>* original_handle =
        reinterpret_cast<TracedReferenceBase<v8::Value>*>(
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
  CHECK(optimized_handle->IsEmpty());
  *optimized_handle = T(isolate, optimized_object);
  CHECK(!optimized_handle->IsEmpty());
  optimized_handle->SetWrapperClassId(optimized_class_id);

  v8::Local<v8::Object> non_optimized_object(ConstructTraceableJSApiObject(
      isolate->GetCurrentContext(), nullptr, nullptr));
  CHECK(non_optimized_handle->IsEmpty());
  *non_optimized_handle = T(isolate, non_optimized_object);
  CHECK(!non_optimized_handle->IsEmpty());
}

}  // namespace

TEST(TracedGlobalDestructorReclaimedOnScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  constexpr uint16_t kClassIdToOptimize = 17;
  EmbedderHeapTracerDestructorNonTracingClearing tracer(kClassIdToOptimize);
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  auto* optimized_handle = new v8::TracedGlobal<v8::Object>();
  auto* non_optimized_handle = new v8::TracedGlobal<v8::Object>();
  SetupOptimizedAndNonOptimizedHandle(isolate, kClassIdToOptimize,
                                      optimized_handle, non_optimized_handle);
  CHECK_EQ(initial_count + 2, global_handles->handles_count());
  heap::InvokeScavenge();
  CHECK_EQ(initial_count + 1, global_handles->handles_count());
  CHECK(optimized_handle->IsEmpty());
  delete optimized_handle;
  CHECK(!non_optimized_handle->IsEmpty());
  delete non_optimized_handle;
  CHECK_EQ(initial_count, global_handles->handles_count());
}

TEST(TracedGlobalNoDestructorReclaimedOnScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  constexpr uint16_t kClassIdToOptimize = 23;
  EmbedderHeapTracerNoDestructorNonTracingClearing tracer(kClassIdToOptimize);
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);
  i::GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();

  const size_t initial_count = global_handles->handles_count();
  auto* optimized_handle = new v8::TracedReference<v8::Value>();
  auto* non_optimized_handle = new v8::TracedReference<v8::Value>();
  SetupOptimizedAndNonOptimizedHandle(isolate, kClassIdToOptimize,
                                      optimized_handle, non_optimized_handle);
  CHECK_EQ(initial_count + 2, global_handles->handles_count());
  heap::InvokeScavenge();
  CHECK_EQ(initial_count + 1, global_handles->handles_count());
  CHECK(optimized_handle->IsEmpty());
  delete optimized_handle;
  CHECK(!non_optimized_handle->IsEmpty());
  non_optimized_handle->Reset();
  delete non_optimized_handle;
  CHECK_EQ(initial_count, global_handles->handles_count());
}

namespace {

template <typename T>
V8_NOINLINE void OnStackTest(TestEmbedderHeapTracer* tracer) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> observer;
  T stack_ref;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    stack_ref.Reset(isolate, object);
    observer.Reset(isolate, object);
    observer.SetWeak();
  }
  CHECK(!observer.IsEmpty());
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
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
    TestEmbedderHeapTracer* tracer) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> observer;
  CreateTracedReferenceInDeepStack(isolate, &observer);
  CHECK(!observer.IsEmpty());
  tracer->NotifyEmptyEmbedderStack();
  heap::InvokeMarkSweep();
  CHECK(observer.IsEmpty());
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

template <typename T>
V8_NOINLINE void StackToHeapTest(TestEmbedderHeapTracer* tracer, Operation op,
                                 TargetHandling target_handling) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> observer;
  T stack_handle;
  T* heap_handle = new T();
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> to_object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    CHECK(i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (target_handling == TargetHandling::kInitializedOldGen) {
      heap::InvokeScavenge();
      heap::InvokeScavenge();
      CHECK(!i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    heap_handle->Reset(isolate, to_object);
  }
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    stack_handle.Reset(isolate, object);
    observer.Reset(isolate, object);
    observer.SetWeak();
  }
  CHECK(!observer.IsEmpty());
  tracer->AddReferenceForTracing(heap_handle);
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
  tracer->AddReferenceForTracing(heap_handle);
  PerformOperation(op, heap_handle, &stack_handle);
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
  heap::InvokeMarkSweep();
  CHECK(observer.IsEmpty());
  delete heap_handle;
}

template <typename T>
V8_NOINLINE void HeapToStackTest(TestEmbedderHeapTracer* tracer, Operation op,
                                 TargetHandling target_handling) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> observer;
  T stack_handle;
  T* heap_handle = new T();
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> to_object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    CHECK(i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (target_handling == TargetHandling::kInitializedOldGen) {
      heap::InvokeScavenge();
      heap::InvokeScavenge();
      CHECK(!i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    stack_handle.Reset(isolate, to_object);
  }
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    heap_handle->Reset(isolate, object);
    observer.Reset(isolate, object);
    observer.SetWeak();
  }
  CHECK(!observer.IsEmpty());
  tracer->AddReferenceForTracing(heap_handle);
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
  PerformOperation(op, &stack_handle, heap_handle);
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
  stack_handle.Reset();
  heap::InvokeMarkSweep();
  CHECK(observer.IsEmpty());
  delete heap_handle;
}

template <typename T>
V8_NOINLINE void StackToStackTest(TestEmbedderHeapTracer* tracer, Operation op,
                                  TargetHandling target_handling) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> observer;
  T stack_handle1;
  T stack_handle2;
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> to_object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    CHECK(i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (target_handling == TargetHandling::kInitializedOldGen) {
      heap::InvokeScavenge();
      heap::InvokeScavenge();
      CHECK(!i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    stack_handle2.Reset(isolate, to_object);
  }
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
        isolate->GetCurrentContext(), nullptr, nullptr));
    stack_handle1.Reset(isolate, object);
    observer.Reset(isolate, object);
    observer.SetWeak();
  }
  CHECK(!observer.IsEmpty());
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
  PerformOperation(op, &stack_handle2, &stack_handle1);
  heap::InvokeMarkSweep();
  CHECK(!observer.IsEmpty());
  stack_handle2.Reset();
  heap::InvokeMarkSweep();
  CHECK(observer.IsEmpty());
}

template <typename T>
V8_NOINLINE void TracedReferenceCleanedTest(TestEmbedderHeapTracer* tracer) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
      isolate->GetCurrentContext(), nullptr, nullptr));
  const size_t before =
      CcTest::i_isolate()->global_handles()->NumberOfOnStackHandlesForTesting();
  for (int i = 0; i < 100; i++) {
    T stack_handle;
    stack_handle.Reset(isolate, object);
  }
  CHECK_EQ(before + 1, CcTest::i_isolate()
                           ->global_handles()
                           ->NumberOfOnStackHandlesForTesting());
}

V8_NOINLINE void TracedGlobalDestructorTest(TestEmbedderHeapTracer* tracer) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Global<v8::Object> observer;
  {
    v8::TracedGlobal<v8::Value> stack_handle;
    {
      v8::HandleScope scope(isolate);
      v8::Local<v8::Object> object(ConstructTraceableJSApiObject(
          isolate->GetCurrentContext(), nullptr, nullptr));
      stack_handle.Reset(isolate, object);
      observer.Reset(isolate, object);
      observer.SetWeak();
    }
    CHECK(!observer.IsEmpty());
    heap::InvokeMarkSweep();
    CHECK(!observer.IsEmpty());
  }
  heap::InvokeMarkSweep();
  CHECK(observer.IsEmpty());
}

}  // namespace

TEST(TracedReferenceOnStack) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  OnStackTest<v8::TracedReference<v8::Value>>(&tracer);
}

TEST(TracedGlobalOnStack) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  OnStackTest<v8::TracedGlobal<v8::Value>>(&tracer);
}

TEST(TracedReferenceCleaned) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  TracedReferenceCleanedTest<v8::TracedReference<v8::Value>>(&tracer);
}

TEST(TracedGlobalCleaned) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  TracedReferenceCleanedTest<v8::TracedGlobal<v8::Value>>(&tracer);
}

TEST(TracedReferenceMove) {
  using ReferenceType = v8::TracedReference<v8::Value>;
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kNonInitialized);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedYoungGen);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedOldGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kNonInitialized);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedYoungGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedOldGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                  TargetHandling::kNonInitialized);
  StackToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                  TargetHandling::kInitializedYoungGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                  TargetHandling::kInitializedOldGen);
}

TEST(TracedReferenceCopy) {
  using ReferenceType = v8::TracedReference<v8::Value>;
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kNonInitialized);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedYoungGen);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedOldGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kNonInitialized);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedYoungGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedOldGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                  TargetHandling::kNonInitialized);
  StackToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                  TargetHandling::kInitializedYoungGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                  TargetHandling::kInitializedOldGen);
}

TEST(TracedGlobalMove) {
  using ReferenceType = v8::TracedGlobal<v8::Value>;
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kNonInitialized);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedYoungGen);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedOldGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kNonInitialized);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedYoungGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                 TargetHandling::kInitializedOldGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                  TargetHandling::kNonInitialized);
  StackToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                  TargetHandling::kInitializedYoungGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kMove,
                                  TargetHandling::kInitializedOldGen);
}

TEST(TracedGlobalCopy) {
  using ReferenceType = v8::TracedGlobal<v8::Value>;
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kNonInitialized);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedYoungGen);
  StackToHeapTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedOldGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kNonInitialized);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedYoungGen);
  HeapToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                 TargetHandling::kInitializedOldGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                  TargetHandling::kNonInitialized);
  StackToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                  TargetHandling::kInitializedYoungGen);
  StackToStackTest<ReferenceType>(&tracer, Operation::kCopy,
                                  TargetHandling::kInitializedOldGen);
}

TEST(TracedGlobalDestructor) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  TracedGlobalDestructorTest(&tracer);
}

TEST(NotifyEmptyStack) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TestEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(CcTest::isolate(),
                                                      &tracer);
  tracer.SetStackStart(&manual_gc);
  TracedReferenceNotifyEmptyStackTest(&tracer);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
