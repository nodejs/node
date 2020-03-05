// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

namespace {

// Empty v8::EmbedderHeapTracer that never keeps objects alive on Scavenge. See
// |IsRootForNonTracingGC|.
class NonRootingEmbedderHeapTracer final : public v8::EmbedderHeapTracer {
 public:
  NonRootingEmbedderHeapTracer() = default;

  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) final {}
  bool AdvanceTracing(double deadline_in_ms) final { return true; }
  bool IsTracingDone() final { return true; }
  void TracePrologue(TraceFlags) final {}
  void TraceEpilogue(TraceSummary*) final {}
  void EnterFinalPause(EmbedderStackState) final {}

  bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle) final {
    return false;
  }
};

void InvokeScavenge() { CcTest::CollectGarbage(i::NEW_SPACE); }

void InvokeMarkSweep() { CcTest::CollectAllGarbage(); }

void SimpleCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_num(0));
}

struct FlagAndGlobal {
  bool flag;
  v8::Global<v8::Object> handle;
};

struct TracedGlobalWrapper {
  v8::TracedGlobal<v8::Object> handle;
};

void ResetHandleAndSetFlag(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->handle.Reset();
  data.GetParameter()->flag = true;
}

template <typename HandleContainer>
void ConstructJSObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                       HandleContainer* flag_and_persistent) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> object(v8::Object::New(isolate));
  CHECK(!object.IsEmpty());
  flag_and_persistent->handle.Reset(isolate, object);
  CHECK(!flag_and_persistent->handle.IsEmpty());
}

void ConstructJSObject(v8::Isolate* isolate, v8::Global<v8::Object>* global) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(v8::Object::New(isolate));
  CHECK(!object.IsEmpty());
  *global = v8::Global<v8::Object>(isolate, object);
  CHECK(!global->IsEmpty());
}

template <typename HandleContainer>
void ConstructJSApiObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                          HandleContainer* flag_and_persistent) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(isolate, SimpleCallback);
  v8::Local<v8::Object> object = fun->GetFunction(context)
                                     .ToLocalChecked()
                                     ->NewInstance(context)
                                     .ToLocalChecked();
  CHECK(!object.IsEmpty());
  flag_and_persistent->handle.Reset(isolate, object);
  CHECK(!flag_and_persistent->handle.IsEmpty());
}

enum class SurvivalMode { kSurvives, kDies };

template <typename ConstructFunction, typename ModifierFunction,
          typename GCFunction>
void WeakHandleTest(v8::Isolate* isolate, ConstructFunction construct_function,
                    ModifierFunction modifier_function, GCFunction gc_function,
                    SurvivalMode survives) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  FlagAndGlobal fp;
  construct_function(isolate, context, &fp);
  CHECK(heap::InYoungGeneration(isolate, fp.handle));
  fp.handle.SetWeak(&fp, &ResetHandleAndSetFlag,
                    v8::WeakCallbackType::kParameter);
  fp.flag = false;
  modifier_function(&fp);
  gc_function();
  CHECK_IMPLIES(survives == SurvivalMode::kSurvives, !fp.flag);
  CHECK_IMPLIES(survives == SurvivalMode::kDies, fp.flag);
}

template <typename ConstructFunction, typename ModifierFunction,
          typename GCFunction>
void TracedGlobalTest(v8::Isolate* isolate,
                      ConstructFunction construct_function,
                      ModifierFunction modifier_function,
                      GCFunction gc_function, SurvivalMode survives) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  NonRootingEmbedderHeapTracer tracer;
  heap::TemporaryEmbedderHeapTracerScope tracer_scope(isolate, &tracer);

  TracedGlobalWrapper fp;
  construct_function(isolate, context, &fp);
  CHECK(heap::InYoungGeneration(isolate, fp.handle));
  modifier_function(&fp);
  gc_function();
  CHECK_IMPLIES(survives == SurvivalMode::kSurvives, !fp.handle.IsEmpty());
  CHECK_IMPLIES(survives == SurvivalMode::kDies, fp.handle.IsEmpty());
}

void ResurrectingFinalizer(
    const v8::WeakCallbackInfo<v8::Global<v8::Object>>& data) {
  data.GetParameter()->ClearWeak();
}

void ResettingFinalizer(
    const v8::WeakCallbackInfo<v8::Global<v8::Object>>& data) {
  data.GetParameter()->Reset();
}

void EmptyWeakCallback(const v8::WeakCallbackInfo<void>& data) {}

void ResurrectingFinalizerSettingProperty(
    const v8::WeakCallbackInfo<v8::Global<v8::Object>>& data) {
  data.GetParameter()->ClearWeak();
  v8::Local<v8::Object> o =
      v8::Local<v8::Object>::New(data.GetIsolate(), *data.GetParameter());
  o->Set(data.GetIsolate()->GetCurrentContext(), v8_str("finalizer"),
         v8_str("was here"))
      .FromJust();
}

}  // namespace

TEST(EternalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  EternalHandles* eternal_handles = isolate->eternal_handles();

  // Create a number of handles that will not be on a block boundary
  const int kArrayLength = 2048-1;
  int indices[kArrayLength];
  v8::Eternal<v8::Value> eternals[kArrayLength];

  CHECK_EQ(0, eternal_handles->handles_count());
  for (int i = 0; i < kArrayLength; i++) {
    indices[i] = -1;
    HandleScope scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New(v8_isolate);
    object->Set(v8_isolate->GetCurrentContext(), i,
                v8::Integer::New(v8_isolate, i))
        .FromJust();
    // Create with internal api
    eternal_handles->Create(
        isolate, *v8::Utils::OpenHandle(*object), &indices[i]);
    // Create with external api
    CHECK(eternals[i].IsEmpty());
    eternals[i].Set(v8_isolate, object);
    CHECK(!eternals[i].IsEmpty());
  }

  CcTest::CollectAllAvailableGarbage();

  for (int i = 0; i < kArrayLength; i++) {
    for (int j = 0; j < 2; j++) {
      HandleScope scope(isolate);
      v8::Local<v8::Value> local;
      if (j == 0) {
        // Test internal api
        local = v8::Utils::ToLocal(eternal_handles->Get(indices[i]));
      } else {
        // Test external api
        local = eternals[i].Get(v8_isolate);
      }
      v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(local);
      v8::Local<v8::Value> value =
          object->Get(v8_isolate->GetCurrentContext(), i).ToLocalChecked();
      CHECK(value->IsInt32());
      CHECK_EQ(i,
               value->Int32Value(v8_isolate->GetCurrentContext()).FromJust());
    }
  }

  CHECK_EQ(2 * kArrayLength, eternal_handles->handles_count());

  // Create an eternal via the constructor
  {
    HandleScope scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New(v8_isolate);
    v8::Eternal<v8::Object> eternal(v8_isolate, object);
    CHECK(!eternal.IsEmpty());
    CHECK(object == eternal.Get(v8_isolate));
  }

  CHECK_EQ(2 * kArrayLength + 1, eternal_handles->handles_count());
}


TEST(PersistentBaseGetLocal) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> o = v8::Object::New(isolate);
  CHECK(!o.IsEmpty());
  v8::Persistent<v8::Object> p(isolate, o);
  CHECK(o == p.Get(isolate));
  CHECK(v8::Local<v8::Object>::New(isolate, p) == p.Get(isolate));

  v8::Global<v8::Object> g(isolate, o);
  CHECK(o == g.Get(isolate));
  CHECK(v8::Local<v8::Object>::New(isolate, g) == g.Get(isolate));
}

TEST(WeakPersistentSmi) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  v8::HandleScope scope(isolate);
  v8::Local<v8::Number> n = v8::Number::New(isolate, 0);
  v8::Global<v8::Number> g(isolate, n);

  // Should not crash.
  g.SetWeak<void>(nullptr, &EmptyWeakCallback,
                  v8::WeakCallbackType::kParameter);
}

TEST(FinalizerWeakness) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  v8::Global<v8::Object> g;
  int identity;

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> o = v8::Object::New(isolate);
    identity = o->GetIdentityHash();
    g.Reset(isolate, o);
    g.SetWeak(&g, &ResurrectingFinalizerSettingProperty,
              v8::WeakCallbackType::kFinalizer);
  }

  CcTest::CollectAllAvailableGarbage();

  CHECK(!g.IsEmpty());
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> o = v8::Local<v8::Object>::New(isolate, g);
  CHECK_EQ(identity, o->GetIdentityHash());
  CHECK(o->Has(isolate->GetCurrentContext(), v8_str("finalizer")).FromJust());
}

TEST(PhatomHandlesWithoutCallbacks) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  v8::Global<v8::Object> g1, g2;
  {
    v8::HandleScope scope(isolate);
    g1.Reset(isolate, v8::Object::New(isolate));
    g1.SetWeak();
    g2.Reset(isolate, v8::Object::New(isolate));
    g2.SetWeak();
  }

  CHECK_EQ(0u, isolate->NumberOfPhantomHandleResetsSinceLastCall());
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(2u, isolate->NumberOfPhantomHandleResetsSinceLastCall());
  CHECK_EQ(0u, isolate->NumberOfPhantomHandleResetsSinceLastCall());
}

TEST(WeakHandleToUnmodifiedJSObjectDiesOnScavenge) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {}, []() { InvokeScavenge(); },
      SurvivalMode::kDies);
}

TEST(TracedGlobalToUnmodifiedJSObjectSurvivesScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TracedGlobalTest(
      CcTest::isolate(), &ConstructJSObject<TracedGlobalWrapper>,
      [](TracedGlobalWrapper* fp) {}, []() { InvokeScavenge(); },
      SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSObjectDiesOnMarkCompact) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {}, []() { InvokeMarkSweep(); },
      SurvivalMode::kDies);
}

TEST(WeakHandleToUnmodifiedJSObjectSurvivesMarkCompactWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { InvokeMarkSweep(); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSApiObjectDiesOnScavenge) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {}, []() { InvokeScavenge(); },
      SurvivalMode::kDies);
}

TEST(TracedGlobalToUnmodifiedJSApiObjectDiesOnScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  TracedGlobalTest(
      CcTest::isolate(), &ConstructJSApiObject<TracedGlobalWrapper>,
      [](TracedGlobalWrapper* fp) {}, []() { InvokeScavenge(); },
      SurvivalMode::kDies);
}

TEST(TracedGlobalToJSApiObjectWithIdentityHashSurvivesScavenge) {
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  Isolate* i_isolate = CcTest::i_isolate();
  HandleScope scope(i_isolate);
  Handle<JSWeakMap> weakmap = i_isolate->factory()->NewJSWeakMap();

  TracedGlobalTest(
      CcTest::isolate(), &ConstructJSApiObject<TracedGlobalWrapper>,
      [&weakmap, i_isolate](TracedGlobalWrapper* fp) {
        v8::HandleScope scope(CcTest::isolate());
        Handle<JSReceiver> key =
            Utils::OpenHandle(*fp->handle.Get(CcTest::isolate()));
        Handle<Smi> smi(Smi::FromInt(23), i_isolate);
        int32_t hash = key->GetOrCreateHash(i_isolate).value();
        JSWeakCollection::Set(weakmap, key, smi, hash);
      },
      []() { InvokeScavenge(); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSApiObjectSurvivesScavengeWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { InvokeScavenge(); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSApiObjectDiesOnMarkCompact) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {}, []() { InvokeMarkSweep(); },
      SurvivalMode::kDies);
}

TEST(WeakHandleToUnmodifiedJSApiObjectSurvivesMarkCompactWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject<FlagAndGlobal>,
      [](FlagAndGlobal* fp) {
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { InvokeMarkSweep(); }, SurvivalMode::kSurvives);
}

TEST(TracedGlobalToJSApiObjectWithModifiedMapSurvivesScavenge) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  LocalContext context;

  TracedGlobal<v8::Object> handle;
  {
    v8::HandleScope scope(isolate);
    // Create an API object which does not have the same map as constructor.
    auto function_template = FunctionTemplate::New(isolate);
    auto instance_t = function_template->InstanceTemplate();
    instance_t->Set(
        v8::String::NewFromUtf8(isolate, "a", NewStringType::kNormal)
            .ToLocalChecked(),
        v8::Number::New(isolate, 10));
    auto function =
        function_template->GetFunction(context.local()).ToLocalChecked();
    auto i = function->NewInstance(context.local()).ToLocalChecked();
    handle.Reset(isolate, i);
  }
  InvokeScavenge();
  CHECK(!handle.IsEmpty());
}

TEST(TracedGlobalTOJsApiObjectWithElementsSurvivesScavenge) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  LocalContext context;

  TracedGlobal<v8::Object> handle;
  {
    v8::HandleScope scope(isolate);

    // Create an API object which has elements.
    auto function_template = FunctionTemplate::New(isolate);
    auto instance_t = function_template->InstanceTemplate();
    instance_t->Set(
        v8::String::NewFromUtf8(isolate, "1", NewStringType::kNormal)
            .ToLocalChecked(),
        v8::Number::New(isolate, 10));
    instance_t->Set(
        v8::String::NewFromUtf8(isolate, "2", NewStringType::kNormal)
            .ToLocalChecked(),
        v8::Number::New(isolate, 10));
    auto function =
        function_template->GetFunction(context.local()).ToLocalChecked();
    auto i = function->NewInstance(context.local()).ToLocalChecked();
    handle.Reset(isolate, i);
  }
  InvokeScavenge();
  CHECK(!handle.IsEmpty());
}

TEST(FinalizerOnUnmodifiedJSApiObjectDoesNotCrash) {
  // See crbug.com/v8/8586.
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  FlagAndGlobal fp;
  ConstructJSApiObject(isolate, context, &fp);
  fp.handle.SetWeak(&fp, &ResetHandleAndSetFlag,
                    v8::WeakCallbackType::kFinalizer);
  fp.flag = false;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> tmp = v8::Local<v8::Object>::New(isolate, fp.handle);
    USE(tmp);
    InvokeScavenge();
  }
}

namespace {

void ConstructFinalizerPointingPhantomHandle(
    v8::Isolate* isolate, v8::Global<v8::Object>* g1,
    v8::Global<v8::Object>* g2,
    typename v8::WeakCallbackInfo<v8::Global<v8::Object>>::Callback
        finalizer_for_g1) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> o1 =
      v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
  v8::Local<v8::Object> o2 =
      v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
  o1->Set(isolate->GetCurrentContext(), v8_str("link"), o2).FromJust();
  g1->Reset(isolate, o1);
  g2->Reset(isolate, o2);
  // g1 will be finalized but resurrected.
  g1->SetWeak(g1, finalizer_for_g1, v8::WeakCallbackType::kFinalizer);
  // g2 will be a phantom handle that is dependent on the finalizer handle
  // g1 as it is in its subgraph.
  g2->SetWeak();
}

}  // namespace

TEST(FinalizerResurrectsAndKeepsPhantomAliveOnMarkCompact) {
  // See crbug.com/772299.
  CcTest::InitializeVM();
  v8::Global<v8::Object> g1, g2;
  ConstructFinalizerPointingPhantomHandle(CcTest::isolate(), &g1, &g2,
                                          ResurrectingFinalizer);
  InvokeMarkSweep();
  // Both, g1 and g2, should stay alive as the finalizer resurrects the root
  // object that transitively keeps the other one alive.
  CHECK(!g1.IsEmpty());
  CHECK(!g2.IsEmpty());
  InvokeMarkSweep();
  // The finalizer handle is now strong, so it should keep the objects alive.
  CHECK(!g1.IsEmpty());
  CHECK(!g2.IsEmpty());
}

TEST(FinalizerDiesAndKeepsPhantomAliveOnMarkCompact) {
  CcTest::InitializeVM();
  v8::Global<v8::Object> g1, g2;
  ConstructFinalizerPointingPhantomHandle(CcTest::isolate(), &g1, &g2,
                                          ResettingFinalizer);
  InvokeMarkSweep();
  // Finalizer (g1) dies but the phantom handle (g2) is kept alive for one
  // more round as the underlying object only dies on the next GC.
  CHECK(g1.IsEmpty());
  CHECK(!g2.IsEmpty());
  InvokeMarkSweep();
  // Phantom handle dies after one more round.
  CHECK(g1.IsEmpty());
  CHECK(g2.IsEmpty());
}

namespace {

void ForceScavenge2(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->flag = true;
  InvokeScavenge();
}

void ForceScavenge1(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->handle.Reset();
  data.SetSecondPassCallback(ForceScavenge2);
}

void ForceMarkSweep2(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->flag = true;
  InvokeMarkSweep();
}

void ForceMarkSweep1(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->handle.Reset();
  data.SetSecondPassCallback(ForceMarkSweep2);
}

}  // namespace

TEST(GCFromWeakCallbacks) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Locker locker(CcTest::isolate());
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  static const int kNumberOfGCTypes = 2;
  using Callback = v8::WeakCallbackInfo<FlagAndGlobal>::Callback;
  Callback gc_forcing_callback[kNumberOfGCTypes] = {&ForceScavenge1,
                                                    &ForceMarkSweep1};

  using GCInvoker = void (*)();
  GCInvoker invoke_gc[kNumberOfGCTypes] = {&InvokeScavenge, &InvokeMarkSweep};

  for (int outer_gc = 0; outer_gc < kNumberOfGCTypes; outer_gc++) {
    for (int inner_gc = 0; inner_gc < kNumberOfGCTypes; inner_gc++) {
      FlagAndGlobal fp;
      ConstructJSApiObject(isolate, context, &fp);
      CHECK(heap::InYoungGeneration(isolate, fp.handle));
      fp.flag = false;
      fp.handle.SetWeak(&fp, gc_forcing_callback[inner_gc],
                        v8::WeakCallbackType::kParameter);
      invoke_gc[outer_gc]();
      EmptyMessageQueues(isolate);
      CHECK(fp.flag);
    }
  }
}

namespace {

void SecondPassCallback(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->flag = true;
}

void FirstPassCallback(const v8::WeakCallbackInfo<FlagAndGlobal>& data) {
  data.GetParameter()->handle.Reset();
  data.SetSecondPassCallback(SecondPassCallback);
}

}  // namespace

TEST(SecondPassPhantomCallbacks) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Locker locker(CcTest::isolate());
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  FlagAndGlobal fp;
  ConstructJSApiObject(isolate, context, &fp);
  fp.flag = false;
  fp.handle.SetWeak(&fp, FirstPassCallback, v8::WeakCallbackType::kParameter);
  CHECK(!fp.flag);
  InvokeMarkSweep();
  InvokeMarkSweep();
  CHECK(fp.flag);
}

TEST(MoveStrongGlobal) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  v8::Global<v8::Object>* global = new Global<v8::Object>();
  ConstructJSObject(isolate, global);
  InvokeMarkSweep();
  v8::Global<v8::Object> global2(std::move(*global));
  delete global;
  InvokeMarkSweep();
}

TEST(MoveWeakGlobal) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  v8::Global<v8::Object>* global = new Global<v8::Object>();
  ConstructJSObject(isolate, global);
  InvokeMarkSweep();
  global->SetWeak();
  v8::Global<v8::Object> global2(std::move(*global));
  delete global;
  InvokeMarkSweep();
}

}  // namespace internal
}  // namespace v8
