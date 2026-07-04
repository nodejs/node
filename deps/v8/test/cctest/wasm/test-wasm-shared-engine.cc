// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/base/fpu.h"
#include "src/execution/microtask-queue.h"
#include "src/objects/objects-inl.h"
#include "src/sandbox/sandboxable-thread.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_wasm_shared_engine {

// Helper type definition representing a WebAssembly module shared between
// multiple Isolates with implicit reference counting.
using SharedModule = std::shared_ptr<NativeModule>;

// Create a new Isolate and a context and enter both.
class NewIsolateScope {
 public:
  NewIsolateScope() : isolate_(v8::Isolate::Allocate()) {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    v8::Isolate::Initialize(isolate_, create_params);
    v8_isolate()->Enter();
    v8::HandleScope handle_scope(v8_isolate());
    v8::Context::New(v8_isolate())->Enter();
    zone_.reset(new Zone(isolate()->allocator(), ZONE_NAME));
  }
  ~NewIsolateScope() {
    zone_.reset();
    v8_isolate()->Exit();
    isolate_->Dispose();
  }

  Zone* zone() const { return zone_.get(); }
  v8::Isolate* v8_isolate() { return isolate_; }
  Isolate* isolate() { return reinterpret_cast<Isolate*>(isolate_); }

  DirectHandle<WasmInstanceObject> CompileAndInstantiate(ZoneBuffer* buffer) {
    ErrorThrower thrower(isolate(), "CompileAndInstantiate");
    MaybeDirectHandle<WasmInstanceObject> instance =
        testing::CompileAndInstantiateForTesting(isolate(), &thrower,
                                                 base::VectorOf(*buffer));
    return instance.ToHandleChecked();
  }

  DirectHandle<WasmInstanceObject> ImportInstance(SharedModule shared_module) {
    DirectHandle<WasmModuleObject> module_object =
        GetWasmEngine()
            ->ImportNativeModule(isolate(), shared_module, {})
            .ToHandleChecked();
    ErrorThrower thrower(isolate(), "ImportInstance");
    MaybeDirectHandle<WasmInstanceObject> instance =
        GetWasmEngine()->SyncInstantiate(isolate(), &thrower, module_object, {},
                                         {});
    return instance.ToHandleChecked();
  }

  SharedModule ExportInstance(DirectHandle<WasmInstanceObject> instance) {
    return instance->module_object()->native_module().as_shared_ptr();
  }

  int32_t Run(DirectHandle<WasmInstanceObject> instance) {
    return testing::CallWasmFunctionForTesting(isolate(), instance, "main", {});
  }

 private:
  v8::Isolate* isolate_;
  std::unique_ptr<Zone> zone_;
};

// A thread that executes a callback in a new isolate and context.
class ThreadWithNewIsolate : public v8::internal::SandboxableThread {
 public:
  explicit ThreadWithNewIsolate(std::function<void(NewIsolateScope*)> callback)
      : SandboxableThread(Options("ThreadWithNewIsolate")),
        callback_(callback) {}

  void Run() override {
    NewIsolateScope isolate;
    callback_(&isolate);
  }

 private:
  std::function<void(NewIsolateScope*)> callback_;
};

namespace {

ZoneBuffer* BuildReturnConstantModule(Zone* zone, int constant) {
  TestSignatures sigs;
  ZoneBuffer* buffer = zone->New<ZoneBuffer>(zone);
  WasmModuleBuilder* builder = zone->New<WasmModuleBuilder>(zone);
  WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
  f->builder()->AddExport(base::CStrVector("main"), f);
  f->EmitCode({WASM_I32V_2(constant), WASM_END});
  builder->WriteTo(buffer);
  return buffer;
}

class MockInstantiationResolver : public InstantiationResultResolver {
 public:
  explicit MockInstantiationResolver(IndirectHandle<Object>* out_instance)
      : out_instance_(out_instance) {}
  void OnInstantiationSucceeded(
      DirectHandle<WasmInstanceObject> result) override {
    *out_instance_->location() = result->ptr();
  }
  void OnInstantiationFailed(DirectHandle<JSAny> error_reason) override {
    UNREACHABLE();
  }

 private:
  IndirectHandle<Object>* out_instance_;
};

class MockCompilationResolver : public CompilationResultResolver {
 public:
  MockCompilationResolver(NewIsolateScope* isolate,
                          IndirectHandle<Object>* out_instance)
      : isolate_(isolate), out_instance_(out_instance) {}
  void OnCompilationSucceeded(DirectHandle<WasmModuleObject> result) override {
    GetWasmEngine()->AsyncInstantiate(
        isolate_->isolate(),
        std::make_unique<MockInstantiationResolver>(out_instance_), result, {});
  }
  void OnCompilationFailed(DirectHandle<JSAny> error_reason) override {
    UNREACHABLE();
  }

 private:
  NewIsolateScope* isolate_;
  IndirectHandle<Object>* out_instance_;
};

void PumpMessageLoop(NewIsolateScope* isolate) {
  v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                isolate->v8_isolate(),
                                platform::MessageLoopBehavior::kWaitForWork);
  isolate->isolate()->default_microtask_queue()->RunMicrotasks(
      isolate->isolate());
}

DirectHandle<WasmInstanceObject> CompileAndInstantiateAsync(
    NewIsolateScope* isolate, ZoneBuffer* buffer) {
  IndirectHandle<Object> maybe_instance(Smi::zero(), isolate->isolate());
  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate->isolate());
  constexpr const char* kAPIMethodName = "Test.CompileAndInstantiateAsync";
  GetWasmEngine()->AsyncCompile(
      isolate->isolate(), enabled_features, CompileTimeImports{},
      std::make_unique<MockCompilationResolver>(isolate, &maybe_instance),
      base::OwnedCopyOf(*buffer), kAPIMethodName);
  while (!IsWasmInstanceObject(*maybe_instance)) PumpMessageLoop(isolate);
  DirectHandle<WasmInstanceObject> instance =
      Cast<WasmInstanceObject>(maybe_instance);
  return instance;
}

}  // namespace

TEST(RunSeparated) {
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        isolate.CompileAndInstantiate(buffer);
    CHECK_EQ(23, isolate.Run(instance));
  }
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 42);
    DirectHandle<WasmInstanceObject> instance =
        isolate.CompileAndInstantiate(buffer);
    CHECK_EQ(42, isolate.Run(instance));
  }
}

TEST(RunImported) {
  SharedModule module;
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        isolate.CompileAndInstantiate(buffer);
    module = isolate.ExportInstance(instance);
    CHECK_EQ(23, isolate.Run(instance));
  }
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    DirectHandle<WasmInstanceObject> instance = isolate.ImportInstance(module);
    CHECK_EQ(23, isolate.Run(instance));
  }
}

TEST(RunThreadedBuildingSync) {
  ThreadWithNewIsolate thread1([](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate->zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        isolate->CompileAndInstantiate(buffer);
    CHECK_EQ(23, isolate->Run(instance));
  });
  ThreadWithNewIsolate thread2([](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate->zone(), 42);
    DirectHandle<WasmInstanceObject> instance =
        isolate->CompileAndInstantiate(buffer);
    CHECK_EQ(42, isolate->Run(instance));
  });
  CHECK(thread1.Start());
  CHECK(thread2.Start());
  thread1.Join();
  thread2.Join();
}

TEST(RunThreadedBuildingAsync) {
  ThreadWithNewIsolate thread1([](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate->zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateAsync(isolate, buffer);
    CHECK_EQ(23, isolate->Run(instance));
  });
  ThreadWithNewIsolate thread2([](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate->zone(), 42);
    DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateAsync(isolate, buffer);
    CHECK_EQ(42, isolate->Run(instance));
  });
  CHECK(thread1.Start());
  CHECK(thread2.Start());
  thread1.Join();
  thread2.Join();
}

TEST(RunThreadedExecution) {
  SharedModule module;
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        isolate.CompileAndInstantiate(buffer);
    module = isolate.ExportInstance(instance);
  }
  ThreadWithNewIsolate thread1([module](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    DirectHandle<WasmInstanceObject> instance = isolate->ImportInstance(module);
    CHECK_EQ(23, isolate->Run(instance));
  });
  ThreadWithNewIsolate thread2([module](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    DirectHandle<WasmInstanceObject> instance = isolate->ImportInstance(module);
    CHECK_EQ(23, isolate->Run(instance));
  });
  CHECK(thread1.Start());
  CHECK(thread2.Start());
  thread1.Join();
  thread2.Join();
}

TEST(RunThreadedTierUp) {
  SharedModule module;
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        isolate.CompileAndInstantiate(buffer);
    module = isolate.ExportInstance(instance);
  }
  constexpr int kNumberOfThreads = 5;
  std::list<ThreadWithNewIsolate> threads;
  for (int i = 0; i < kNumberOfThreads; ++i) {
    threads.emplace_back([module](NewIsolateScope* isolate) {
      constexpr int kNumberOfIterations = 100;
      HandleScope scope(isolate->isolate());
      DirectHandle<WasmInstanceObject> instance =
          isolate->ImportInstance(module);
      for (int j = 0; j < kNumberOfIterations; ++j) {
        CHECK_EQ(23, isolate->Run(instance));
      }
    });
  }
  threads.emplace_back([module](NewIsolateScope* isolate) {
    HandleScope scope(isolate->isolate());
    DirectHandle<WasmInstanceObject> instance = isolate->ImportInstance(module);
    WasmDetectedFeatures detected;
    WasmCompilationUnit::CompileWasmFunction(module.get(), &detected,
                                             &module->module()->functions[0],
                                             ExecutionTier::kTurbofan);
    CHECK_EQ(23, isolate->Run(instance));
  });
  for (auto& thread : threads) CHECK(thread.Start());
  for (auto& thread : threads) thread.Join();
}

TEST(RunImportedWithDenormalsMismatchAndSourceUrl) {
  SharedModule module;
  bool default_flush = base::FPU::GetFlushDenormals();
  constexpr char kSourceUrl[] = "wasm://test/source_url";
  {
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    DirectHandle<WasmInstanceObject> instance =
        isolate.CompileAndInstantiate(buffer);
    module = isolate.ExportInstance(instance);
    CHECK_EQ(23, isolate.Run(instance));
  }
  std::shared_ptr<NativeModule> recompiled_native_module;
  {
    base::FlushDenormalsScope flush_denormals_scope(!default_flush);
    NewIsolateScope isolate;
    HandleScope scope(isolate.isolate());

    // Call ImportNativeModule directly with a source URL
    DirectHandle<WasmModuleObject> module_object =
        GetWasmEngine()
            ->ImportNativeModule(isolate.isolate(), module,
                                 base::CStrVector(kSourceUrl))
            .ToHandleChecked();

    // Verify that the import succeeded and we have a WasmModuleObject
    CHECK(!module_object.is_null());

    recompiled_native_module = module_object->native_module().as_shared_ptr();
    // Verify that it's not the same module as the original one.
    CHECK_NE(module.get(), recompiled_native_module.get());

    // Verify that the source URL is preserved
    Tagged<Script> script = module_object->script();
    DirectHandle<String> script_name(Cast<String>(script->name()),
                                     isolate.isolate());
    size_t length;
    std::unique_ptr<char[]> cstring = script_name->ToCString(&length);
    CHECK_EQ(0, strcmp(cstring.get(), kSourceUrl));

    // Verify that the imported module has the new flags in its NativeModule.
    CHECK_EQ(!default_flush,
             module_object->native_module()->compile_imports().contains(
                 CompileTimeImport::kDisableDenormalFloats));
  }
  {
    base::FlushDenormalsScope flush_denormals_scope(!default_flush);
    NewIsolateScope isolate3;
    HandleScope scope(isolate3.isolate());
    DirectHandle<WasmModuleObject> module_object3 =
        GetWasmEngine()
            ->ImportNativeModule(isolate3.isolate(), module,
                                 base::CStrVector(kSourceUrl))
            .ToHandleChecked();

    // Verify that it reused the NativeModule created by isolate2!
    CHECK_EQ(recompiled_native_module.get(),
             module_object3->native_module().raw());
  }
}

}  // namespace test_wasm_shared_engine
}  // namespace wasm
}  // namespace internal
}  // namespace v8
