// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "src/api/api-inl.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/version.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_module {

using base::ReadLittleEndianValue;
using base::WriteLittleEndianValue;
using testing::CompileAndInstantiateForTesting;

namespace {
void Cleanup(Isolate* isolate = CcTest::InitIsolateOnce()) {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}

void TestModule(Zone* zone, WasmModuleBuilder* builder,
                int32_t expected_result) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  int32_t result =
      testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK_EQ(expected_result, result);
}

void TestModuleException(Zone* zone, WasmModuleBuilder* builder) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK(try_catch.HasCaught());
  isolate->clear_pending_exception();
}

void ExportAsMain(WasmFunctionBuilder* f) {
  f->builder()->AddExport(CStrVector("main"), f);
}

#define EMIT_CODE_WITH_END(f, code)  \
  do {                               \
    f->EmitCode(code, sizeof(code)); \
    f->Emit(kExprEnd);               \
  } while (false)

}  // namespace

TEST(Run_WasmModule_Return114) {
  {
    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kReturnValue);
  }
  Cleanup();
}

TEST(Run_WasmModule_CompilationHintsLazy) {
  if (!FLAG_wasm_tier_up || !FLAG_liftoff) return;
  {
    EXPERIMENTAL_FLAG_SCOPE(compilation_hints);

    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    // Build module with one lazy function.
    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    f->SetCompilationHint(WasmCompilationHintStrategy::kLazy,
                          WasmCompilationHintTier::kBaseline,
                          WasmCompilationHintTier::kOptimized);

    // Compile module. No function is actually compiled as the function is lazy.
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
    MaybeHandle<WasmModuleObject> module = testing::CompileForTesting(
        isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));
    CHECK(!module.is_null());

    // Lazy function was not invoked and therefore not compiled yet.
    static const int kFuncIndex = 0;
    NativeModule* native_module = module.ToHandleChecked()->native_module();
    CHECK(!native_module->HasCode(kFuncIndex));
    auto* compilation_state = native_module->compilation_state();
    CHECK(compilation_state->baseline_compilation_finished());

    // Instantiate and invoke function.
    MaybeHandle<WasmInstanceObject> instance =
        isolate->wasm_engine()->SyncInstantiate(
            isolate, &thrower, module.ToHandleChecked(), {}, {});
    CHECK(!instance.is_null());
    int32_t result = testing::RunWasmModuleForTesting(
        isolate, instance.ToHandleChecked(), 0, nullptr);
    CHECK_EQ(kReturnValue, result);

    // Lazy function was invoked and therefore compiled.
    CHECK(native_module->HasCode(kFuncIndex));
    WasmCodeRefScope code_ref_scope;
    ExecutionTier actual_tier = native_module->GetCode(kFuncIndex)->tier();
    static_assert(ExecutionTier::kInterpreter < ExecutionTier::kLiftoff &&
                      ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                  "Assume an order on execution tiers");
    ExecutionTier baseline_tier = ExecutionTier::kLiftoff;
    CHECK_LE(baseline_tier, actual_tier);
    CHECK(compilation_state->baseline_compilation_finished());
  }
  Cleanup();
}

TEST(Run_WasmModule_CompilationHintsNoTiering) {
  if (!FLAG_wasm_tier_up || !FLAG_liftoff) return;
  {
    EXPERIMENTAL_FLAG_SCOPE(compilation_hints);

    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    // Build module with regularly compiled function (no tiering).
    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    f->SetCompilationHint(WasmCompilationHintStrategy::kEager,
                          WasmCompilationHintTier::kBaseline,
                          WasmCompilationHintTier::kBaseline);

    // Compile module.
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
    MaybeHandle<WasmModuleObject> module = testing::CompileForTesting(
        isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));
    CHECK(!module.is_null());

    // Synchronous compilation finished and no tiering units were initialized.
    static const int kFuncIndex = 0;
    NativeModule* native_module = module.ToHandleChecked()->native_module();
    CHECK(native_module->HasCode(kFuncIndex));
    ExecutionTier expected_tier = ExecutionTier::kLiftoff;
    WasmCodeRefScope code_ref_scope;
    ExecutionTier actual_tier = native_module->GetCode(kFuncIndex)->tier();
    CHECK_EQ(expected_tier, actual_tier);
    auto* compilation_state = native_module->compilation_state();
    CHECK(compilation_state->baseline_compilation_finished());
    CHECK(compilation_state->top_tier_compilation_finished());
  }
  Cleanup();
}

TEST(Run_WasmModule_CompilationHintsTierUp) {
  if (!FLAG_wasm_tier_up || !FLAG_liftoff) return;
  {
    EXPERIMENTAL_FLAG_SCOPE(compilation_hints);

    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    // Build module with tiering compilation hint.
    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    f->SetCompilationHint(WasmCompilationHintStrategy::kEager,
                          WasmCompilationHintTier::kBaseline,
                          WasmCompilationHintTier::kOptimized);

    // Compile module.
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
    MaybeHandle<WasmModuleObject> module = testing::CompileForTesting(
        isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));
    CHECK(!module.is_null());

    // Expect baseline or top tier code.
    static const int kFuncIndex = 0;
    NativeModule* native_module = module.ToHandleChecked()->native_module();
    auto* compilation_state = native_module->compilation_state();
    static_assert(ExecutionTier::kInterpreter < ExecutionTier::kLiftoff &&
                      ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                  "Assume an order on execution tiers");
    ExecutionTier baseline_tier = ExecutionTier::kLiftoff;
    {
      CHECK(native_module->HasCode(kFuncIndex));
      WasmCodeRefScope code_ref_scope;
      ExecutionTier actual_tier = native_module->GetCode(kFuncIndex)->tier();
      CHECK_LE(baseline_tier, actual_tier);
      CHECK(compilation_state->baseline_compilation_finished());
    }

    // Busy wait for top tier compilation to finish.
    while (!compilation_state->top_tier_compilation_finished()) {
    }

    // Expect top tier code.
    ExecutionTier top_tier = ExecutionTier::kTurbofan;
    {
      CHECK(native_module->HasCode(kFuncIndex));
      WasmCodeRefScope code_ref_scope;
      ExecutionTier actual_tier = native_module->GetCode(kFuncIndex)->tier();
      CHECK_EQ(top_tier, actual_tier);
      CHECK(compilation_state->baseline_compilation_finished());
      CHECK(compilation_state->top_tier_compilation_finished());
    }
  }
  Cleanup();
}

TEST(Run_WasmModule_CompilationHintsLazyBaselineEagerTopTier) {
  if (!FLAG_wasm_tier_up || !FLAG_liftoff) return;
  {
    EXPERIMENTAL_FLAG_SCOPE(compilation_hints);

    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    // Build module with tiering compilation hint.
    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    f->SetCompilationHint(
        WasmCompilationHintStrategy::kLazyBaselineEagerTopTier,
        WasmCompilationHintTier::kBaseline,
        WasmCompilationHintTier::kOptimized);

    // Compile module.
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
    MaybeHandle<WasmModuleObject> module = testing::CompileForTesting(
        isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));
    CHECK(!module.is_null());

    NativeModule* native_module = module.ToHandleChecked()->native_module();
    auto* compilation_state = native_module->compilation_state();

    // Busy wait for top tier compilation to finish.
    while (!compilation_state->top_tier_compilation_finished()) {
    }

    // Expect top tier code.
    static_assert(ExecutionTier::kInterpreter < ExecutionTier::kLiftoff &&
                      ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                  "Assume an order on execution tiers");
    static const int kFuncIndex = 0;
    ExecutionTier top_tier = ExecutionTier::kTurbofan;
    {
      CHECK(native_module->HasCode(kFuncIndex));
      WasmCodeRefScope code_ref_scope;
      ExecutionTier actual_tier = native_module->GetCode(kFuncIndex)->tier();
      CHECK_EQ(top_tier, actual_tier);
      CHECK(compilation_state->baseline_compilation_finished());
      CHECK(compilation_state->top_tier_compilation_finished());
    }
  }
  Cleanup();
}

TEST(Run_WasmModule_CallAdd) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);

    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_ii());
    uint16_t param1 = 0;
    uint16_t param2 = 1;
    byte code1[] = {
        WASM_I32_ADD(WASM_GET_LOCAL(param1), WASM_GET_LOCAL(param2))};
    EMIT_CODE_WITH_END(f1, code1);

    WasmFunctionBuilder* f2 = builder->AddFunction(sigs.i_v());

    ExportAsMain(f2);
    byte code2[] = {
        WASM_CALL_FUNCTION(f1->func_index(), WASM_I32V_2(77), WASM_I32V_1(22))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 99);
  }
  Cleanup();
}

TEST(Run_WasmModule_ReadLoadedDataSegment) {
  {
    static const byte kDataSegmentDest0 = 12;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    ExportAsMain(f);
    byte code[] = {
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(kDataSegmentDest0))};
    EMIT_CODE_WITH_END(f, code);
    byte data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    builder->AddDataSegment(data, sizeof(data), kDataSegmentDest0);
    TestModule(&zone, builder, 0xDDCCBBAA);
  }
  Cleanup();
}

TEST(Run_WasmModule_CheckMemoryIsZero) {
  {
    static const int kCheckSize = 16 * 1024;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    byte code[] = {WASM_BLOCK_I(
        WASM_WHILE(
            WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32V_3(kCheckSize)),
            WASM_IF_ELSE(
                WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(localIndex)),
                WASM_BRV(3, WASM_I32V_1(-1)),
                WASM_INC_LOCAL_BY(localIndex, 4))),
        WASM_I32V_1(11))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 11);
  }
  Cleanup();
}

TEST(Run_WasmModule_CallMain_recursive) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    byte code[] = {
        WASM_SET_LOCAL(localIndex,
                       WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
        WASM_IF_ELSE_I(WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32V_1(5)),
                       WASM_SEQ(WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                               WASM_INC_LOCAL(localIndex)),
                                WASM_CALL_FUNCTION0(0)),
                       WASM_I32V_1(55))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 55);
  }
  Cleanup();
}

TEST(Run_WasmModule_Global) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    uint32_t global1 = builder->AddGlobal(kWasmI32);
    uint32_t global2 = builder->AddGlobal(kWasmI32);
    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_v());
    byte code1[] = {
        WASM_I32_ADD(WASM_GET_GLOBAL(global1), WASM_GET_GLOBAL(global2))};
    EMIT_CODE_WITH_END(f1, code1);
    WasmFunctionBuilder* f2 = builder->AddFunction(sigs.i_v());
    ExportAsMain(f2);
    byte code2[] = {WASM_SET_GLOBAL(global1, WASM_I32V_1(56)),
                    WASM_SET_GLOBAL(global2, WASM_I32V_1(41)),
                    WASM_RETURN1(WASM_CALL_FUNCTION0(f1->func_index()))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 97);
  }
  Cleanup();
}

TEST(MemorySize) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST(Run_WasmModule_MemSize_GrowMem) {
  {
    // Initial memory size = 16 + MemoryGrow(10)
    static const int kExpectedValue = 26;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(10)), WASM_DROP,
                   WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST(MemoryGrowZero) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V(0))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

class InterruptThread : public v8::base::Thread {
 public:
  explicit InterruptThread(Isolate* isolate, int32_t* memory)
      : Thread(Options("TestInterruptLoop")),
        isolate_(isolate),
        memory_(memory) {}

  static void OnInterrupt(v8::Isolate* isolate, void* data) {
    int32_t* m = reinterpret_cast<int32_t*>(data);
    // Set the interrupt location to 0 to break the loop in {TestInterruptLoop}.
    Address ptr = reinterpret_cast<Address>(&m[interrupt_location_]);
    WriteLittleEndianValue<int32_t>(ptr, interrupt_value_);
  }

  void Run() override {
    // Wait for the main thread to write the signal value.
    int32_t val = 0;
    do {
      val = memory_[0];
      val = ReadLittleEndianValue<int32_t>(reinterpret_cast<Address>(&val));
    } while (val != signal_value_);
    isolate_->RequestInterrupt(&OnInterrupt, const_cast<int32_t*>(memory_));
  }

  Isolate* isolate_;
  volatile int32_t* memory_;
  static const int32_t interrupt_location_ = 10;
  static const int32_t interrupt_value_ = 154;
  static const int32_t signal_value_ = 1221;
};

TEST(TestInterruptLoop) {
  {
    // Do not dump the module of this test because it contains an infinite loop.
    if (FLAG_dump_wasm_module) return;

    // This test tests that WebAssembly loops can be interrupted, i.e. that if
    // an
    // InterruptCallback is registered by {Isolate::RequestInterrupt}, then the
    // InterruptCallback is eventually called even if a loop in WebAssembly code
    // is executed.
    // Test setup:
    // The main thread executes a WebAssembly function with a loop. In the loop
    // {signal_value_} is written to memory to signal a helper thread that the
    // main thread reached the loop in the WebAssembly program. When the helper
    // thread reads {signal_value_} from memory, it registers the
    // InterruptCallback. Upon exeution, the InterruptCallback write into the
    // WebAssemblyMemory to end the loop in the WebAssembly program.
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {
        WASM_LOOP(
            WASM_IFB(WASM_NOT(WASM_LOAD_MEM(
                         MachineType::Int32(),
                         WASM_I32V(InterruptThread::interrupt_location_ * 4))),
                     WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                    WASM_I32V(InterruptThread::signal_value_)),
                     WASM_BR(1))),
        WASM_I32V(121)};
    EMIT_CODE_WITH_END(f, code);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "Test");
    const Handle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(
            isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
            .ToHandleChecked();

    Handle<JSArrayBuffer> memory(instance->memory_object().array_buffer(),
                                 isolate);
    int32_t* memory_array = reinterpret_cast<int32_t*>(memory->backing_store());

    InterruptThread thread(isolate, memory_array);
    thread.Start();
    testing::RunWasmModuleForTesting(isolate, instance, 0, nullptr);
    Address address = reinterpret_cast<Address>(
        &memory_array[InterruptThread::interrupt_location_]);
    CHECK_EQ(InterruptThread::interrupt_value_,
             ReadLittleEndianValue<int32_t>(address));
  }
  Cleanup();
}

TEST(Run_WasmModule_MemoryGrowInIf) {
  {
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_IF_ELSE_I(WASM_I32V(0), WASM_GROW_MEMORY(WASM_I32V(1)),
                                  WASM_I32V(12))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 12);
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemOobOffset) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + MemoryGrow(10)
    static const int index = kPageSize * 17 + 4;
    int value = 0xACED;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(1)),
                   WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                  WASM_I32V(value))};
    EMIT_CODE_WITH_END(f, code);
    TestModuleException(&zone, builder);
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemOobFixedIndex) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + MemoryGrow(10)
    static const int index = kPageSize * 26 + 4;
    int value = 0xACED;
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    Zone zone(isolate->allocator(), ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_GET_LOCAL(0)), WASM_DROP,
                   WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                  WASM_I32V(value)),
                   WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Test");
    Handle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(
            isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
            .ToHandleChecked();

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (uint32_t i = 1; i < 5; i++) {
      Handle<Object> params[1] = {Handle<Object>(Smi::FromInt(i), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::RunWasmModuleForTesting(isolate, instance, 1, params);
      CHECK(try_catch.HasCaught());
      isolate->clear_pending_exception();
    }

    Handle<Object> params[1] = {Handle<Object>(Smi::FromInt(1), isolate)};
    int32_t result =
        testing::RunWasmModuleForTesting(isolate, instance, 1, params);
    CHECK_EQ(0xACED, result);
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemOobVariableIndex) {
  {
    static const int kPageSize = 0x10000;
    int value = 0xACED;
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(1)), WASM_DROP,
                   WASM_STORE_MEM(MachineType::Int32(), WASM_GET_LOCAL(0),
                                  WASM_I32V(value)),
                   WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Test");
    Handle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(
            isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
            .ToHandleChecked();

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (int i = 1; i < 5; i++) {
      Handle<Object> params[1] = {
          Handle<Object>(Smi::FromInt((16 + i) * kPageSize - 3), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::RunWasmModuleForTesting(isolate, instance, 1, params);
      CHECK(try_catch.HasCaught());
      isolate->clear_pending_exception();
    }

    for (int i = 1; i < 5; i++) {
      Handle<Object> params[1] = {
          Handle<Object>(Smi::FromInt((20 + i) * kPageSize - 4), isolate)};
      int32_t result =
          testing::RunWasmModuleForTesting(isolate, instance, 1, params);
      CHECK_EQ(0xACED, result);
    }

    v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    Handle<Object> params[1] = {
        Handle<Object>(Smi::FromInt(25 * kPageSize), isolate)};
    testing::RunWasmModuleForTesting(isolate, instance, 1, params);
    CHECK(try_catch.HasCaught());
    isolate->clear_pending_exception();
  }
  Cleanup();
}

TEST(Run_WasmModule_Global_init) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    uint32_t global1 =
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(777777));
    uint32_t global2 =
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(222222));
    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_v());
    byte code[] = {
        WASM_I32_ADD(WASM_GET_GLOBAL(global1), WASM_GET_GLOBAL(global2))};
    EMIT_CODE_WITH_END(f1, code);
    ExportAsMain(f1);
    TestModule(&zone, builder, 999999);
  }
  Cleanup();
}

template <typename CType>
static void RunWasmModuleGlobalInitTest(ValueType type, CType expected) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    ValueType types[] = {type};
    FunctionSig sig(1, 0, types);

    for (int padding = 0; padding < 5; padding++) {
      // Test with a simple initializer
      WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);

      for (int i = 0; i < padding; i++) {  // pad global before
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(i + 20000));
      }
      uint32_t global = builder->AddGlobal(type, false, WasmInitExpr(expected));
      for (int i = 0; i < padding; i++) {  // pad global after
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(i + 30000));
      }

      WasmFunctionBuilder* f1 = builder->AddFunction(&sig);
      byte code[] = {WASM_GET_GLOBAL(global)};
      EMIT_CODE_WITH_END(f1, code);
      ExportAsMain(f1);
      TestModule(&zone, builder, expected);
    }
  }
  Cleanup();
}

TEST(Run_WasmModule_Global_i32) {
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, -983489);
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, 11223344);
}

TEST(Run_WasmModule_Global_f32) {
  RunWasmModuleGlobalInitTest<float>(kWasmF32, -983.9f);
  RunWasmModuleGlobalInitTest<float>(kWasmF32, 1122.99f);
}

TEST(Run_WasmModule_Global_f64) {
  RunWasmModuleGlobalInitTest<double>(kWasmF64, -833.9);
  RunWasmModuleGlobalInitTest<double>(kWasmF64, 86374.25);
}

TEST(InitDataAtTheUpperLimit) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,   // --
        kMemorySectionCode,   // --
        U32V_1(4),            // section size
        ENTRY_COUNT(1),       // --
        kHasMaximumFlag,      // --
        1,                    // initial size
        2,                    // maximum size
        kDataSectionCode,     // --
        U32V_1(9),            // section size
        ENTRY_COUNT(1),       // --
        0,                    // linear memory index
        WASM_I32V_3(0xFFFF),  // destination offset
        kExprEnd,
        U32V_1(1),  // source size
        'c'         // data bytes
    };

    CompileAndInstantiateForTesting(
        isolate, &thrower, ModuleWireBytes(data, data + arraysize(data)));
    if (thrower.error()) {
      thrower.Reify()->Print();
      FATAL("compile or instantiate error");
    }
  }
  Cleanup();
}

TEST(EmptyMemoryNonEmptyDataSegment) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,  // --
        kMemorySectionCode,  // --
        U32V_1(4),           // section size
        ENTRY_COUNT(1),      // --
        kHasMaximumFlag,     // --
        0,                   // initial size
        0,                   // maximum size
        kDataSectionCode,    // --
        U32V_1(7),           // section size
        ENTRY_COUNT(1),      // --
        0,                   // linear memory index
        WASM_I32V_1(8),      // destination offset
        kExprEnd,
        U32V_1(1),  // source size
        'c'         // data bytes
    };

    CompileAndInstantiateForTesting(
        isolate, &thrower, ModuleWireBytes(data, data + arraysize(data)));
    // It should not be possible to instantiate this module.
    CHECK(thrower.error());
  }
  Cleanup();
}

TEST(EmptyMemoryEmptyDataSegment) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,  // --
        kMemorySectionCode,  // --
        U32V_1(4),           // section size
        ENTRY_COUNT(1),      // --
        kHasMaximumFlag,     // --
        0,                   // initial size
        0,                   // maximum size
        kDataSectionCode,    // --
        U32V_1(6),           // section size
        ENTRY_COUNT(1),      // --
        0,                   // linear memory index
        WASM_I32V_1(0),      // destination offset
        kExprEnd,
        U32V_1(0),  // source size
    };

    CompileAndInstantiateForTesting(
        isolate, &thrower, ModuleWireBytes(data, data + arraysize(data)));
    // It should be possible to instantiate this module.
    CHECK(!thrower.error());
  }
  Cleanup();
}

TEST(MemoryWithOOBEmptyDataSegment) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,      // --
        kMemorySectionCode,      // --
        U32V_1(4),               // section size
        ENTRY_COUNT(1),          // --
        kHasMaximumFlag,         // --
        1,                       // initial size
        1,                       // maximum size
        kDataSectionCode,        // --
        U32V_1(9),               // section size
        ENTRY_COUNT(1),          // --
        0,                       // linear memory index
        WASM_I32V_4(0x2468ACE),  // destination offset
        kExprEnd,
        U32V_1(0),  // source size
    };

    CompileAndInstantiateForTesting(
        isolate, &thrower, ModuleWireBytes(data, data + arraysize(data)));
    // It should not be possible to instantiate this module.
    CHECK(thrower.error());
  }
  Cleanup();
}

// Utility to free the allocated memory for a buffer that is manually
// externalized in a test.
struct ManuallyExternalizedBuffer {
  Isolate* isolate_;
  Handle<JSArrayBuffer> buffer_;
  void* allocation_base_;
  size_t allocation_length_;
  bool const should_free_;

  ManuallyExternalizedBuffer(JSArrayBuffer buffer, Isolate* isolate)
      : isolate_(isolate),
        buffer_(buffer, isolate),
        allocation_base_(buffer.allocation_base()),
        allocation_length_(buffer.allocation_length()),
        should_free_(!isolate_->wasm_engine()->memory_tracker()->IsWasmMemory(
            buffer.backing_store())) {
    if (!isolate_->wasm_engine()->memory_tracker()->IsWasmMemory(
            buffer.backing_store())) {
      v8::Utils::ToLocal(buffer_)->Externalize();
    }
  }
  ~ManuallyExternalizedBuffer() {
    if (should_free_) {
      buffer_->FreeBackingStoreFromMainThread();
    }
  }
};

TEST(Run_WasmModule_Buffer_Externalized_GrowMem) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(6)), WASM_DROP,
                   WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);

    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "Test");
    const Handle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(
            isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
            .ToHandleChecked();
    Handle<WasmMemoryObject> memory_object(instance->memory_object(), isolate);

    // Fake the Embedder flow by externalizing the array buffer.
    ManuallyExternalizedBuffer buffer1(memory_object->array_buffer(), isolate);

    // Grow using the API.
    uint32_t result = WasmMemoryObject::Grow(isolate, memory_object, 4);
    CHECK_EQ(16, result);
    CHECK(buffer1.buffer_->was_detached());  // growing always detaches
    CHECK_EQ(0, buffer1.buffer_->byte_length());

    CHECK_NE(*buffer1.buffer_, memory_object->array_buffer());

    // Fake the Embedder flow by externalizing the array buffer.
    ManuallyExternalizedBuffer buffer2(memory_object->array_buffer(), isolate);

    // Grow using an internal WASM bytecode.
    result = testing::RunWasmModuleForTesting(isolate, instance, 0, nullptr);
    CHECK_EQ(26, result);
    CHECK(buffer2.buffer_->was_detached());  // growing always detaches
    CHECK_EQ(0, buffer2.buffer_->byte_length());
    CHECK_NE(*buffer2.buffer_, memory_object->array_buffer());
  }
  Cleanup();
}

TEST(Run_WasmModule_Buffer_Externalized_GrowMemMemSize) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    Handle<JSArrayBuffer> buffer;
    CHECK(wasm::NewArrayBuffer(isolate, 16 * kWasmPageSize).ToHandle(&buffer));
    Handle<WasmMemoryObject> mem_obj =
        WasmMemoryObject::New(isolate, buffer, 100);
    auto const contents = v8::Utils::ToLocal(buffer)->Externalize();
    int32_t result = WasmMemoryObject::Grow(isolate, mem_obj, 0);
    CHECK_EQ(16, result);
    constexpr bool is_wasm_memory = true;
    const JSArrayBuffer::Allocation allocation{contents.AllocationBase(),
                                               contents.AllocationLength(),
                                               contents.Data(), is_wasm_memory};
    JSArrayBuffer::FreeBackingStore(isolate, allocation);
  }
  Cleanup();
}

TEST(Run_WasmModule_Buffer_Externalized_Detach) {
  {
    // Regression test for
    // https://bugs.chromium.org/p/chromium/issues/detail?id=731046
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    Handle<JSArrayBuffer> buffer;
    CHECK(wasm::NewArrayBuffer(isolate, 16 * kWasmPageSize).ToHandle(&buffer));
    auto const contents = v8::Utils::ToLocal(buffer)->Externalize();
    wasm::DetachMemoryBuffer(isolate, buffer, true);
    constexpr bool is_wasm_memory = true;
    const JSArrayBuffer::Allocation allocation{contents.AllocationBase(),
                                               contents.AllocationLength(),
                                               contents.Data(), is_wasm_memory};
    JSArrayBuffer::FreeBackingStore(isolate, allocation);
  }
  Cleanup();
}

TEST(Run_WasmModule_Buffer_Externalized_Regression_UseAfterFree) {
  // Regresion test for https://crbug.com/813876
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  Handle<JSArrayBuffer> buffer;
  CHECK(wasm::NewArrayBuffer(isolate, 16 * kWasmPageSize).ToHandle(&buffer));
  Handle<WasmMemoryObject> mem = WasmMemoryObject::New(isolate, buffer, 128);
  auto contents = v8::Utils::ToLocal(buffer)->Externalize();
  WasmMemoryObject::Grow(isolate, mem, 0);
  constexpr bool is_wasm_memory = true;
  JSArrayBuffer::FreeBackingStore(
      isolate, JSArrayBuffer::Allocation(contents.AllocationBase(),
                                         contents.AllocationLength(),
                                         contents.Data(), is_wasm_memory));
  // Make sure we can write to the buffer without crashing
  uint32_t* int_buffer =
      reinterpret_cast<uint32_t*>(mem->array_buffer().backing_store());
  int_buffer[0] = 0;
}

#if V8_TARGET_ARCH_64_BIT
TEST(Run_WasmModule_Reclaim_Memory) {
  // Make sure we can allocate memories without running out of address space.
  Isolate* isolate = CcTest::InitIsolateOnce();
  Handle<JSArrayBuffer> buffer;
  for (int i = 0; i < 256; ++i) {
    HandleScope scope(isolate);
    CHECK(NewArrayBuffer(isolate, kWasmPageSize).ToHandle(&buffer));
  }
}
#endif

TEST(AtomicOpDisassembly) {
  {
    EXPERIMENTAL_FLAG_SCOPE(threads);
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    builder->SetHasSharedMemory();
    builder->SetMaxMemorySize(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    byte code[] = {
        WASM_ATOMICS_STORE_OP(kExprI32AtomicStore, WASM_ZERO, WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord32),
        WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad, WASM_ZERO,
                             MachineRepresentation::kWord32)};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Test");
    auto enabled_features = WasmFeaturesFromIsolate(isolate);
    MaybeHandle<WasmModuleObject> module_object =
        isolate->wasm_engine()->SyncCompile(
            isolate, enabled_features, &thrower,
            ModuleWireBytes(buffer.begin(), buffer.end()));

    module_object.ToHandleChecked()->DisassembleFunction(0);
  }
  Cleanup();
}

#undef EMIT_CODE_WITH_END

}  // namespace test_run_wasm_module
}  // namespace wasm
}  // namespace internal
}  // namespace v8
