// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/objects-inl.h"
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

// Helper class representing a WebAssembly engine that is capable of being
// shared between multiple Isolates, sharing the underlying generated code.
class SharedEngine {
 public:
  explicit SharedEngine(size_t max_committed = kMaxWasmCodeMemory)
      : wasm_engine_(base::make_unique<WasmEngine>(
            base::make_unique<WasmCodeManager>(max_committed))) {}
  ~SharedEngine() {
    // Ensure no remaining uses exist.
    CHECK(wasm_engine_.unique());
  }

  WasmEngine* engine() const { return wasm_engine_.get(); }
  WasmCodeManager* code_manager() const { return engine()->code_manager(); }

  int NumberOfExportedEngineUses() const {
    // This class holds one implicit use itself, which we discount.
    return static_cast<int>(wasm_engine_.use_count()) - 1;
  }

  std::shared_ptr<WasmEngine> ExportEngineForSharing() { return wasm_engine_; }

 private:
  std::shared_ptr<WasmEngine> wasm_engine_;
};

// Helper type definition representing a WebAssembly module shared between
// multiple Isolates with implicit reference counting.
using SharedModule = std::shared_ptr<NativeModule>;

// Helper class representing an Isolate based on a given shared WebAssembly
// engine available at construction time.
class SharedEngineIsolate {
 public:
  explicit SharedEngineIsolate(SharedEngine* engine)
      : isolate_(v8::Isolate::Allocate()) {
    isolate()->set_wasm_engine(engine->ExportEngineForSharing());
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    v8::Isolate::Initialize(isolate_, create_params);
    v8::HandleScope handle_scope(v8_isolate());
    v8::Context::New(v8_isolate())->Enter();
    testing::SetupIsolateForWasmModule(isolate());
    zone_.reset(new Zone(isolate()->allocator(), ZONE_NAME));
  }
  ~SharedEngineIsolate() {
    zone_.reset();
    isolate_->Dispose();
  }

  Zone* zone() const { return zone_.get(); }
  v8::Isolate* v8_isolate() { return isolate_; }
  Isolate* isolate() { return reinterpret_cast<Isolate*>(isolate_); }

  Handle<WasmInstanceObject> CompileAndInstantiate(ZoneBuffer* buffer) {
    ErrorThrower thrower(isolate(), "CompileAndInstantiate");
    MaybeHandle<WasmInstanceObject> instance =
        testing::CompileAndInstantiateForTesting(
            isolate(), &thrower,
            ModuleWireBytes(buffer->begin(), buffer->end()));
    return instance.ToHandleChecked();
  }

  // TODO(mstarzinger): Switch over to a public API for sharing modules via the
  // {v8::WasmCompiledModule::TransferrableModule} class once it is ready.
  Handle<WasmInstanceObject> ImportInstance(SharedModule shared_module) {
    Vector<const byte> wire_bytes = shared_module->wire_bytes();
    Handle<Script> script = CreateWasmScript(isolate(), wire_bytes);
    Handle<WasmModuleObject> module_object =
        WasmModuleObject::New(isolate(), shared_module, script);

    // TODO(6792): Wrappers below might be cloned using {Factory::CopyCode}.
    // This requires unlocking the code space here. This should eventually be
    // moved into the allocator.
    CodeSpaceMemoryModificationScope modification_scope(isolate()->heap());
    CompileJsToWasmWrappers(isolate(), module_object);

    ErrorThrower thrower(isolate(), "ImportInstance");
    MaybeHandle<WasmInstanceObject> instance =
        isolate()->wasm_engine()->SyncInstantiate(isolate(), &thrower,
                                                  module_object, {}, {});
    return instance.ToHandleChecked();
  }

  SharedModule ExportInstance(Handle<WasmInstanceObject> instance) {
    return instance->module_object()->managed_native_module()->get();
  }

  int32_t Run(Handle<WasmInstanceObject> instance) {
    return testing::RunWasmModuleForTesting(isolate(), instance, 0, nullptr);
  }

 private:
  v8::Isolate* isolate_;
  std::unique_ptr<Zone> zone_;
};

namespace {

ZoneBuffer* BuildReturnConstantModule(Zone* zone, int constant) {
  TestSignatures sigs;
  ZoneBuffer* buffer = new (zone) ZoneBuffer(zone);
  WasmModuleBuilder* builder = new (zone) WasmModuleBuilder(zone);
  WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
  f->builder()->AddExport(CStrVector("main"), f);
  byte code[] = {WASM_I32V_2(constant)};
  f->EmitCode(code, sizeof(code));
  f->Emit(kExprEnd);
  builder->WriteTo(*buffer);
  return buffer;
}

}  // namespace

TEST(SharedEngineUseCount) {
  SharedEngine engine;
  CHECK_EQ(0, engine.NumberOfExportedEngineUses());
  {
    SharedEngineIsolate isolate(&engine);
    CHECK_EQ(1, engine.NumberOfExportedEngineUses());
  }
  CHECK_EQ(0, engine.NumberOfExportedEngineUses());
  {
    SharedEngineIsolate isolate1(&engine);
    CHECK_EQ(1, engine.NumberOfExportedEngineUses());
    SharedEngineIsolate isolate2(&engine);
    CHECK_EQ(2, engine.NumberOfExportedEngineUses());
  }
  CHECK_EQ(0, engine.NumberOfExportedEngineUses());
}

TEST(SharedEngineRunSeparated) {
  SharedEngine engine;
  {
    SharedEngineIsolate isolate(&engine);
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    Handle<WasmInstanceObject> instance = isolate.CompileAndInstantiate(buffer);
    CHECK_EQ(23, isolate.Run(instance));
  }
  {
    SharedEngineIsolate isolate(&engine);
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 42);
    Handle<WasmInstanceObject> instance = isolate.CompileAndInstantiate(buffer);
    CHECK_EQ(42, isolate.Run(instance));
  }
}

TEST(SharedEngineRunImported) {
  SharedEngine engine;
  SharedModule module;
  {
    SharedEngineIsolate isolate(&engine);
    HandleScope scope(isolate.isolate());
    ZoneBuffer* buffer = BuildReturnConstantModule(isolate.zone(), 23);
    Handle<WasmInstanceObject> instance = isolate.CompileAndInstantiate(buffer);
    module = isolate.ExportInstance(instance);
    CHECK_EQ(23, isolate.Run(instance));
    CHECK_EQ(2, module.use_count());
  }
  CHECK_EQ(1, module.use_count());
  {
    SharedEngineIsolate isolate(&engine);
    HandleScope scope(isolate.isolate());
    Handle<WasmInstanceObject> instance = isolate.ImportInstance(module);
    CHECK_EQ(23, isolate.Run(instance));
    CHECK_EQ(2, module.use_count());
  }
  CHECK_EQ(1, module.use_count());
}

}  // namespace test_wasm_shared_engine
}  // namespace wasm
}  // namespace internal
}  // namespace v8
