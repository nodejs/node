#include <cppgc/allocation.h>
#include <cppgc/garbage-collected.h>
#include <cppgc/heap.h>
#include <node.h>
#include <v8-cppgc.h>
#include <v8-external-memory-accounter.h>
#include <v8-sandbox.h>
#include <v8.h>
#include "cppgc_helpers-inl.h"
#include "node_realm-inl.h"
#include "node_test_fixture.h"

// This tests that Node.js can work with an existing CppHeap.

// Mimic a class that does not know about Node.js.
class CppGCed : public v8::Object::Wrappable {
 public:
  static int kConstructCount;
  static int kDestructCount;
  static int kTraceCount;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Object> js_object = args.This();
    auto* heap = isolate->GetCppHeap();
    CHECK_NOT_NULL(heap);
    CppGCed* gc_object =
        cppgc::MakeGarbageCollected<CppGCed>(heap->GetAllocationHandle());
    node::SetCppgcReference(isolate, js_object, gc_object);
    kConstructCount++;
    args.GetReturnValue().Set(js_object);
  }

  static v8::Local<v8::Function> GetConstructor(
      v8::Local<v8::Context> context) {
    auto ft = v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), New);
    return ft->GetFunction(context).ToLocalChecked();
  }

  CppGCed() = default;

  ~CppGCed() { kDestructCount++; }

  void Trace(cppgc::Visitor* visitor) const { kTraceCount++; }
};

int CppGCed::kConstructCount = 0;
int CppGCed::kDestructCount = 0;
int CppGCed::kTraceCount = 0;

TEST_F(NodeZeroIsolateTestFixture, ExistingCppHeapTest) {
  node::IsolateSettings settings;
  settings.cpp_heap =
      v8::CppHeap::Create(platform.get(), v8::CppHeapCreateParams{{}})
          .release();
  v8::Isolate* isolate = node::NewIsolate(
      allocator.get(), &current_loop, platform.get(), nullptr, settings);

  // Try creating Context + IsolateData + Environment.
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    std::unique_ptr<node::IsolateData, decltype(&node::FreeIsolateData)>
        isolate_data{
            node::CreateIsolateData(isolate, &current_loop, platform.get()),
            node::FreeIsolateData};
    CHECK(isolate_data);

    {
      auto context = node::NewContext(isolate);
      CHECK(!context.IsEmpty());
      v8::Context::Scope context_scope(context);

      // An Environment should already contain a few BaseObjects that are
      // supposed to have non-cppgc IDs.
      std::unique_ptr<node::Environment, decltype(&node::FreeEnvironment)>
          environment{
              node::CreateEnvironment(isolate_data.get(), context, {}, {}),
              node::FreeEnvironment};
      CHECK(environment);

      context->Global()
          ->Set(context,
                v8::String::NewFromUtf8(isolate, "CppGCed").ToLocalChecked(),
                CppGCed::GetConstructor(context))
          .FromJust();

      const char* code =
          "globalThis.array = [];"
          "for (let i = 0; i < 100; ++i) { array.push(new CppGCed()); }";
      node::LoadEnvironment(environment.get(), code).ToLocalChecked();
      // Request a GC and check if CppGCed::Trace() is invoked.
      isolate->LowMemoryNotification();
      int exit_code = SpinEventLoop(environment.get()).FromJust();
      EXPECT_EQ(exit_code, 0);
    }

    platform->DrainTasks(isolate);
  }

  platform->DisposeIsolate(isolate);

  // Check that all the objects are created and destroyed properly.
  EXPECT_EQ(CppGCed::kConstructCount, 100);
  EXPECT_EQ(CppGCed::kDestructCount, 100);
  // GC does not have to feel pressured enough to visit all of them to
  // reclaim memory before we are done with the isolate and the entire
  // heap can be reclaimed. So just check at least some of them are traced.
  EXPECT_GT(CppGCed::kTraceCount, 0);
}

class CppgcTest : public EnvironmentTestFixture {
 protected:
  // Above the external memory hard limit, so V8 runs a full GC synchronously
  // and leaves sweeping (and cppgc destructors) for later.
  static constexpr size_t kExternalMemoryPressure = size_t{8} << 30;

  void CollectGarbageLeavingSweepingPending() {
    v8::ExternalMemoryAccounter pressure;
    pressure.Increase(isolate_, kExternalMemoryPressure);
    pressure.Decrease(isolate_, kExternalMemoryPressure);
  }

  void FinishSweeping() {
    isolate_->LowMemoryNotification();
    platform->DrainTasks(isolate_);
  }
};

using node::CppgcMixin;

class RealmBoundWrap final : CPPGC_MIXIN(RealmBoundWrap) {
 public:
  SET_CPPGC_NAME(RealmBoundWrap)
  DEFAULT_CPPGC_TRACE()
  SET_NO_MEMORY_INFO()

  static node::Realm* live_realm;
  static int clean_count;
  static int clean_with_dead_realm_count;
  static int destructor_count;

  RealmBoundWrap(node::Environment* env, v8::Local<v8::Object> object) {
    CppgcMixin::Wrap(this, env, object);
  }
  ~RealmBoundWrap() {
    Finalize();
    destructor_count++;
  }
  void Clean(node::Realm* realm) override {
    clean_count++;
    if (realm != live_realm) clean_with_dead_realm_count++;
  }
};

node::Realm* RealmBoundWrap::live_realm = nullptr;
int RealmBoundWrap::clean_count = 0;
int RealmBoundWrap::clean_with_dead_realm_count = 0;
int RealmBoundWrap::destructor_count = 0;

TEST_F(CppgcTest, CleanIsNotCalledWithFreedRealm) {
  constexpr int kCount = 32;
  {
    const v8::HandleScope handle_scope(isolate_);
    Env env{handle_scope, Argv()};
    RealmBoundWrap::live_realm = (*env)->principal_realm();

    v8::Local<v8::FunctionTemplate> ctor = v8::FunctionTemplate::New(isolate_);
    ctor->InstanceTemplate()->SetInternalFieldCount(
        node::CppgcMixin::kInternalFieldCount);
    v8::Local<v8::Function> fn =
        ctor->GetFunction(env.context()).ToLocalChecked();
    {
      v8::HandleScope inner_scope(isolate_);
      for (int i = 0; i <= kCount; i++) {
        v8::Local<v8::Object> obj =
            fn->NewInstance(env.context()).ToLocalChecked();
        cppgc::MakeGarbageCollected<RealmBoundWrap>(
            (*env)->cppgc_allocation_handle(), *env, obj);
        if (i < kCount) continue;
        env.context()
            ->Global()
            ->Set(env.context(),
                  v8::String::NewFromUtf8Literal(isolate_, "kept"),
                  obj)
            .Check();
      }
    }

    CollectGarbageLeavingSweepingPending();
    EXPECT_LT(RealmBoundWrap::destructor_count, kCount);
  }
  RealmBoundWrap::live_realm = nullptr;
  FinishSweeping();

  EXPECT_GE(RealmBoundWrap::clean_count, 1);
  EXPECT_EQ(RealmBoundWrap::clean_with_dead_realm_count, 0);
}

TEST_F(CppgcTest, WrappersAliveAtFreeEnvironmentDoNotLeak) {
  const v8::HandleScope handle_scope(isolate_);
  Env env{handle_scope, Argv()};
  node::LoadEnvironment(*env,
                        "globalThis.script = new (require('vm').Script)('1');"
                        "globalThis.context = require('vm').createContext();")
      .ToLocalChecked();
}

TEST_F(CppgcTest, VmScriptCollectedBeforeFreeEnvironmentSweptAfter) {
  {
    const v8::HandleScope handle_scope(isolate_);
    Env env{handle_scope, Argv()};
    node::LoadEnvironment(*env,
                          "const { Script } = require('vm');"
                          "for (let i = 0; i < 64; i++) new Script('1');"
                          "undefined;")
        .ToLocalChecked();
    CollectGarbageLeavingSweepingPending();
  }
  FinishSweeping();
}
