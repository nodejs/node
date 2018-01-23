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

#include "src/api.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {

void SimpleCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_num(0));
}

struct FlagAndPersistent {
  bool flag;
  v8::Global<v8::Object> handle;
};

void ResetHandleAndSetFlag(
    const v8::WeakCallbackInfo<FlagAndPersistent>& data) {
  data.GetParameter()->handle.Reset();
  data.GetParameter()->flag = true;
}

using ConstructFunction = void (*)(v8::Isolate* isolate,
                                   v8::Local<v8::Context> context,
                                   FlagAndPersistent* flag_and_persistent);

void ConstructJSObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                       FlagAndPersistent* flag_and_persistent) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> object(v8::Object::New(isolate));
  CHECK(!object.IsEmpty());
  flag_and_persistent->handle.Reset(isolate, object);
  CHECK(!flag_and_persistent->handle.IsEmpty());
}

void ConstructJSApiObject(v8::Isolate* isolate, v8::Local<v8::Context> context,
                          FlagAndPersistent* flag_and_persistent) {
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

template <typename ModifierFunction, typename GCFunction>
void WeakHandleTest(v8::Isolate* isolate, ConstructFunction construct_function,
                    ModifierFunction modifier_function, GCFunction gc_function,
                    SurvivalMode survives) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  FlagAndPersistent fp;
  construct_function(isolate, context, &fp);
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> tmp = v8::Local<v8::Object>::New(isolate, fp.handle);
    CHECK(
        CcTest::i_isolate()->heap()->InNewSpace(*v8::Utils::OpenHandle(*tmp)));
  }

  fp.handle.SetWeak(&fp, &ResetHandleAndSetFlag,
                    v8::WeakCallbackType::kParameter);
  fp.flag = false;
  modifier_function(&fp);
  gc_function();
  CHECK_IMPLIES(survives == SurvivalMode::kSurvives, !fp.flag);
  CHECK_IMPLIES(survives == SurvivalMode::kDies, fp.flag);
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

  CHECK_EQ(0, eternal_handles->NumberOfHandles());
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

  CHECK_EQ(2*kArrayLength, eternal_handles->NumberOfHandles());

  // Create an eternal via the constructor
  {
    HandleScope scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New(v8_isolate);
    v8::Eternal<v8::Object> eternal(v8_isolate, object);
    CHECK(!eternal.IsEmpty());
    CHECK(object == eternal.Get(v8_isolate));
  }

  CHECK_EQ(2*kArrayLength + 1, eternal_handles->NumberOfHandles());
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

TEST(WeakHandleToUnmodifiedJSObjectSurvivesScavenge) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSObject, [](FlagAndPersistent* fp) {},
      []() { CcTest::CollectGarbage(i::NEW_SPACE); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSObjectDiesOnMarkCompact) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSObject, [](FlagAndPersistent* fp) {},
      []() { CcTest::CollectGarbage(i::OLD_SPACE); }, SurvivalMode::kDies);
}

TEST(WeakHandleToUnmodifiedJSObjectSurvivesMarkCompactWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSObject,
      [](FlagAndPersistent* fp) {
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { CcTest::CollectGarbage(i::OLD_SPACE); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSApiObjectDiesOnScavenge) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject, [](FlagAndPersistent* fp) {},
      []() { CcTest::CollectGarbage(i::NEW_SPACE); }, SurvivalMode::kDies);
}

TEST(WeakHandleToUnmodifiedJSApiObjectSurvivesScavengeWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject,
      [](FlagAndPersistent* fp) {
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { CcTest::CollectGarbage(i::NEW_SPACE); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToUnmodifiedJSApiObjectDiesOnMarkCompact) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject, [](FlagAndPersistent* fp) {},
      []() { CcTest::CollectGarbage(i::OLD_SPACE); }, SurvivalMode::kDies);
}

TEST(WeakHandleToUnmodifiedJSApiObjectSurvivesMarkCompactWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject,
      [](FlagAndPersistent* fp) {
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { CcTest::CollectGarbage(i::OLD_SPACE); }, SurvivalMode::kSurvives);
}

TEST(WeakHandleToActiveUnmodifiedJSApiObjectSurvivesScavenge) {
  CcTest::InitializeVM();
  WeakHandleTest(CcTest::isolate(), &ConstructJSApiObject,
                 [](FlagAndPersistent* fp) { fp->handle.MarkActive(); },
                 []() { CcTest::CollectGarbage(i::NEW_SPACE); },
                 SurvivalMode::kSurvives);
}

TEST(WeakHandleToActiveUnmodifiedJSApiObjectDiesOnMarkCompact) {
  CcTest::InitializeVM();
  WeakHandleTest(CcTest::isolate(), &ConstructJSApiObject,
                 [](FlagAndPersistent* fp) { fp->handle.MarkActive(); },
                 []() { CcTest::CollectGarbage(i::OLD_SPACE); },
                 SurvivalMode::kDies);
}

TEST(WeakHandleToActiveUnmodifiedJSApiObjectSurvivesMarkCompactWhenInHandle) {
  CcTest::InitializeVM();
  WeakHandleTest(
      CcTest::isolate(), &ConstructJSApiObject,
      [](FlagAndPersistent* fp) {
        fp->handle.MarkActive();
        v8::Local<v8::Object> handle =
            v8::Local<v8::Object>::New(CcTest::isolate(), fp->handle);
        USE(handle);
      },
      []() { CcTest::CollectGarbage(i::OLD_SPACE); }, SurvivalMode::kSurvives);
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
  CcTest::CollectGarbage(i::OLD_SPACE);
  // Both, g1 and g2, should stay alive as the finalizer resurrects the root
  // object that transitively keeps the other one alive.
  CHECK(!g1.IsEmpty());
  CHECK(!g2.IsEmpty());
  CcTest::CollectGarbage(i::OLD_SPACE);
  // The finalizer handle is now strong, so it should keep the objects alive.
  CHECK(!g1.IsEmpty());
  CHECK(!g2.IsEmpty());
}

TEST(FinalizerDiesAndKeepsPhantomAliveOnMarkCompact) {
  CcTest::InitializeVM();
  v8::Global<v8::Object> g1, g2;
  ConstructFinalizerPointingPhantomHandle(CcTest::isolate(), &g1, &g2,
                                          ResettingFinalizer);
  CcTest::CollectGarbage(i::OLD_SPACE);
  // Finalizer (g1) dies but the phantom handle (g2) is kept alive for one
  // more round as the underlying object only dies on the next GC.
  CHECK(g1.IsEmpty());
  CHECK(!g2.IsEmpty());
  CcTest::CollectGarbage(i::OLD_SPACE);
  // Phantom handle dies after one more round.
  CHECK(g1.IsEmpty());
  CHECK(g2.IsEmpty());
}

namespace {

void InvokeScavenge() { CcTest::CollectGarbage(i::NEW_SPACE); }

void InvokeMarkSweep() { CcTest::CollectAllGarbage(); }

void ForceScavenge2(const v8::WeakCallbackInfo<FlagAndPersistent>& data) {
  data.GetParameter()->flag = true;
  InvokeScavenge();
}

void ForceScavenge1(const v8::WeakCallbackInfo<FlagAndPersistent>& data) {
  data.GetParameter()->handle.Reset();
  data.SetSecondPassCallback(ForceScavenge2);
}

void ForceMarkSweep2(const v8::WeakCallbackInfo<FlagAndPersistent>& data) {
  data.GetParameter()->flag = true;
  InvokeMarkSweep();
}

void ForceMarkSweep1(const v8::WeakCallbackInfo<FlagAndPersistent>& data) {
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
  typedef v8::WeakCallbackInfo<FlagAndPersistent>::Callback Callback;
  Callback gc_forcing_callback[kNumberOfGCTypes] = {&ForceScavenge1,
                                                    &ForceMarkSweep1};

  typedef void (*GCInvoker)();
  GCInvoker invoke_gc[kNumberOfGCTypes] = {&InvokeScavenge, &InvokeMarkSweep};

  for (int outer_gc = 0; outer_gc < kNumberOfGCTypes; outer_gc++) {
    for (int inner_gc = 0; inner_gc < kNumberOfGCTypes; inner_gc++) {
      FlagAndPersistent fp;
      ConstructJSApiObject(isolate, context, &fp);
      {
        v8::HandleScope scope(isolate);
        v8::Local<v8::Object> tmp =
            v8::Local<v8::Object>::New(isolate, fp.handle);
        CHECK(CcTest::i_isolate()->heap()->InNewSpace(
            *v8::Utils::OpenHandle(*tmp)));
      }
      fp.flag = false;
      fp.handle.SetWeak(&fp, gc_forcing_callback[inner_gc],
                        v8::WeakCallbackType::kParameter);
      invoke_gc[outer_gc]();
      EmptyMessageQueues(isolate);
      CHECK(fp.flag);
    }
  }
}

}  // namespace internal
}  // namespace v8
