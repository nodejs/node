// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include <atomic>

#include "src/api/api-inl.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/version.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/unittests/wasm/wasm-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_module {

using base::ReadLittleEndianValue;
using base::WriteLittleEndianValue;
using testing::CompileAndInstantiateForTesting;
using RunWasmModuleTest = WasmExecutionTestBase;

namespace {
void Cleanup(Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce()) {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}

void TestModule(Zone* zone, WasmModuleBuilder* builder,
                int32_t expected_result) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
  HandleScope scope(isolate);
  int32_t result =
      testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK_EQ(expected_result, result);
}

void TestModuleException(Zone* zone, WasmModuleBuilder* builder) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
  HandleScope scope(isolate);
  v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK(try_catch.HasCaught());
  isolate->clear_exception();
}

void ExportAsMain(WasmFunctionBuilder* f) {
  f->builder()->AddExport("main", f);
}

#define EMIT_CODE_WITH_END(f, code)  \
  do {                               \
    f->EmitCode(code, sizeof(code)); \
    f->Emit(kExprEnd);               \
  } while (false)

}  // namespace

TEST_F(RunWasmModuleTest, Run_WasmModule_Return114) {
  {
    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kReturnValue);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_CallAdd) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);

    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_ii());
    uint16_t param1 = 0;
    uint16_t param2 = 1;
    uint8_t code1[] = {
        WASM_I32_ADD(WASM_LOCAL_GET(param1), WASM_LOCAL_GET(param2))};
    EMIT_CODE_WITH_END(f1, code1);

    WasmFunctionBuilder* f2 = builder->AddFunction(sigs.i_v());

    ExportAsMain(f2);
    uint8_t code2[] = {
        WASM_CALL_FUNCTION(f1->func_index(), WASM_I32V_2(77), WASM_I32V_1(22))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 99);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_ReadLoadedDataSegment) {
  {
    static const uint8_t kDataSegmentDest0 = 12;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    ExportAsMain(f);
    uint8_t code[] = {
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(kDataSegmentDest0))};
    EMIT_CODE_WITH_END(f, code);
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    builder->AddDataSegment(data, sizeof(data), kDataSegmentDest0);
    TestModule(&zone, builder, 0xDDCCBBAA);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_CheckMemoryIsZero) {
  {
    static const int kCheckSize = 16 * 1024;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    uint8_t code[] = {WASM_BLOCK_I(
        WASM_WHILE(
            WASM_I32_LTS(WASM_LOCAL_GET(localIndex), WASM_I32V_3(kCheckSize)),
            WASM_IF_ELSE(
                WASM_LOAD_MEM(MachineType::Int32(), WASM_LOCAL_GET(localIndex)),
                WASM_BRV(3, WASM_I32V_1(-1)),
                WASM_INC_LOCAL_BY(localIndex, 4))),
        WASM_I32V_1(11))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 11);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_CallMain_recursive) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    uint8_t code[] = {
        WASM_LOCAL_SET(localIndex,
                       WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
        WASM_IF_ELSE_I(WASM_I32_LTS(WASM_LOCAL_GET(localIndex), WASM_I32V_1(5)),
                       WASM_SEQ(WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                               WASM_INC_LOCAL(localIndex)),
                                WASM_CALL_FUNCTION0(0)),
                       WASM_I32V_1(55))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 55);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    uint32_t global1 = builder->AddGlobal(kWasmI32, true, WasmInitExpr(0));
    uint32_t global2 = builder->AddGlobal(kWasmI32, true, WasmInitExpr(0));
    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_v());
    uint8_t code1[] = {
        WASM_I32_ADD(WASM_GLOBAL_GET(global1), WASM_GLOBAL_GET(global2))};
    EMIT_CODE_WITH_END(f1, code1);
    WasmFunctionBuilder* f2 = builder->AddFunction(sigs.i_v());
    ExportAsMain(f2);
    uint8_t code2[] = {WASM_GLOBAL_SET(global1, WASM_I32V_1(56)),
                       WASM_GLOBAL_SET(global2, WASM_I32V_1(41)),
                       WASM_RETURN(WASM_CALL_FUNCTION0(f1->func_index()))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 97);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, MemorySize) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_MemSize_GrowMem) {
  {
    // Initial memory size = 16 + MemoryGrow(10)
    static const int kExpectedValue = 26;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V_1(10)), WASM_DROP,
                      WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, MemoryGrowZero) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V(0))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

class InterruptThread : public v8::base::Thread {
 public:
  explicit InterruptThread(Isolate* isolate, std::atomic<int32_t>* memory)
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
      val = memory_[0].load(std::memory_order_relaxed);
      val = ReadLittleEndianValue<int32_t>(reinterpret_cast<Address>(&val));
    } while (val != signal_value_);
    isolate_->RequestInterrupt(&OnInterrupt, memory_);
  }

  Isolate* isolate_;
  std::atomic<int32_t>* memory_;
  static const int32_t interrupt_location_ = 10;
  static const int32_t interrupt_value_ = 154;
  static const int32_t signal_value_ = 1221;
};

TEST_F(RunWasmModuleTest, TestInterruptLoop) {
  {
    // Do not dump the module of this test because it contains an infinite loop.
    if (v8_flags.dump_wasm_module) return;

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
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {
        WASM_LOOP(
            WASM_IF(WASM_NOT(WASM_LOAD_MEM(
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
    ErrorThrower thrower(isolate, "Test");
    const DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(isolate, &thrower,
                                        base::VectorOf(buffer))
            .ToHandleChecked();

    CppGCManaged<BackingStore>::Ptr backing_store =
        instance->trusted_data(isolate)->memory_object(0)->backing_store();
    std::atomic<int32_t>* memory_array =
        reinterpret_cast<std::atomic<int32_t>*>(backing_store->buffer_start());

    InterruptThread thread(isolate, memory_array);
    CHECK(thread.Start());
    testing::CallWasmFunctionForTesting(isolate, instance, "main", {});
    Address address = reinterpret_cast<Address>(
        &memory_array[InterruptThread::interrupt_location_]);
    CHECK_EQ(InterruptThread::interrupt_value_,
             ReadLittleEndianValue<int32_t>(address));
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_MemoryGrowInIf) {
  {
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_IF_ELSE_I(
        WASM_I32V(0), WASM_MEMORY_GROW(WASM_I32V(1)), WASM_I32V(12))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 12);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_GrowMemOobOffset) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + MemoryGrow(10)
    static const int index = kPageSize * 17 + 4;
    int value = 0xACED;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V_1(1)),
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                     WASM_I32V(value))};
    EMIT_CODE_WITH_END(f, code);
    TestModuleException(&zone, builder);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_GrowMemOobFixedIndex) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + MemoryGrow(10)
    static const int index = kPageSize * 26 + 4;
    int value = 0xACED;
    TestSignatures sigs;
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    Zone zone(isolate->allocator(), ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_LOCAL_GET(0)), WASM_DROP,
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                     WASM_I32V(value)),
                      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    ErrorThrower thrower(isolate, "Test");
    DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(isolate, &thrower,
                                        base::VectorOf(buffer))
            .ToHandleChecked();

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (uint32_t i = 1; i < 5; i++) {
      DirectHandle<Object> params[1] = {
          direct_handle(Smi::FromInt(i), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::CallWasmFunctionForTesting(isolate, instance, "main",
                                          base::ArrayVector(params));
      CHECK(try_catch.HasCaught());
      isolate->clear_exception();
    }

    DirectHandle<Object> params[1] = {direct_handle(Smi::FromInt(1), isolate)};
    int32_t result = testing::CallWasmFunctionForTesting(
        isolate, instance, "main", base::ArrayVector(params));
    CHECK_EQ(0xACED, result);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_GrowMemOobVariableIndex) {
  {
    static const int kPageSize = 0x10000;
    int value = 0xACED;
    TestSignatures sigs;
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V_1(1)), WASM_DROP,
                      WASM_STORE_MEM(MachineType::Int32(), WASM_LOCAL_GET(0),
                                     WASM_I32V(value)),
                      WASM_LOAD_MEM(MachineType::Int32(), WASM_LOCAL_GET(0))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    ErrorThrower thrower(isolate, "Test");
    DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(isolate, &thrower,
                                        base::VectorOf(buffer))
            .ToHandleChecked();

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (int i = 1; i < 5; i++) {
      DirectHandle<Object> params[] = {
          direct_handle(Smi::FromInt((16 + i) * kPageSize - 3), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::CallWasmFunctionForTesting(isolate, instance, "main",
                                          base::ArrayVector(params));
      CHECK(try_catch.HasCaught());
      isolate->clear_exception();
    }

    for (int i = 1; i < 5; i++) {
      DirectHandle<Object> params[] = {
          direct_handle(Smi::FromInt((20 + i) * kPageSize - 4), isolate)};
      int32_t result = testing::CallWasmFunctionForTesting(
          isolate, instance, "main", base::ArrayVector(params));
      CHECK_EQ(0xACED, result);
    }

    v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    DirectHandle<Object> params[] = {
        direct_handle(Smi::FromInt(25 * kPageSize), isolate)};
    testing::CallWasmFunctionForTesting(isolate, instance, "main",
                                        base::ArrayVector(params));
    CHECK(try_catch.HasCaught());
    isolate->clear_exception();
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_init) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    uint32_t global1 =
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(777777));
    uint32_t global2 =
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(222222));
    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_v());
    uint8_t code[] = {
        WASM_I32_ADD(WASM_GLOBAL_GET(global1), WASM_GLOBAL_GET(global2))};
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

    ValueType types[] = {type};
    FunctionSig sig(1, 0, types);

    for (int padding = 0; padding < 5; padding++) {
      // Test with a simple initializer
      WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);

      for (int i = 0; i < padding; i++) {  // pad global before
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(i + 20000));
      }
      uint32_t global = builder->AddGlobal(type, false, WasmInitExpr(expected));
      for (int i = 0; i < padding; i++) {  // pad global after
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(i + 30000));
      }

      WasmFunctionBuilder* f1 = builder->AddFunction(&sig);
      uint8_t code[] = {WASM_GLOBAL_GET(global)};
      EMIT_CODE_WITH_END(f1, code);
      ExportAsMain(f1);
      TestModule(&zone, builder, expected);
    }
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_i32) {
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, -983489);
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, 11223344);
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_f32) {
  RunWasmModuleGlobalInitTest<float>(kWasmF32, -983.9f);
  RunWasmModuleGlobalInitTest<float>(kWasmF32, 1122.99f);
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_f64) {
  RunWasmModuleGlobalInitTest<double>(kWasmF64, -833.9);
  RunWasmModuleGlobalInitTest<double>(kWasmF64, 86374.25);
}

TEST_F(RunWasmModuleTest, InitDataAtTheUpperLimit) {
  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    HandleScope scope(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const uint8_t data[] = {
        WASM_MODULE_HEADER,   // --
        kMemorySectionCode,   // --
        U32V_1(4),            // section size
        ENTRY_COUNT(1),       // --
        kWithMaximum,         // --
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

    CompileAndInstantiateForTesting(isolate, &thrower, base::VectorOf(data));
    if (thrower.error()) {
      Print(*thrower.Reify());
      FATAL("compile or instantiate error");
    }
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, EmptyMemoryNonEmptyDataSegment) {
  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    HandleScope scope(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const uint8_t data[] = {
        WASM_MODULE_HEADER,  // --
        kMemorySectionCode,  // --
        U32V_1(4),           // section size
        ENTRY_COUNT(1),      // --
        kWithMaximum,        // --
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

    CompileAndInstantiateForTesting(isolate, &thrower, base::VectorOf(data));
    // It should not be possible to instantiate this module.
    CHECK(thrower.error());
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, EmptyMemoryEmptyDataSegment) {
  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    HandleScope scope(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const uint8_t data[] = {
        WASM_MODULE_HEADER,  // --
        kMemorySectionCode,  // --
        U32V_1(4),           // section size
        ENTRY_COUNT(1),      // --
        kWithMaximum,        // --
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

    CompileAndInstantiateForTesting(isolate, &thrower, base::VectorOf(data));
    // It should be possible to instantiate this module.
    CHECK(!thrower.error());
  }
  Cleanup();
}

void TestFailedInstantiation(base::Vector<const uint8_t> module_bytes,
                             const char* scenario) {
  Isolate* isolate = WasmExecutionTestBase::current_i_isolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(isolate, scenario);
  DirectHandle<WasmModuleObject> module_object =
      testing::CompileForTesting(isolate, &thrower, module_bytes)
          .ToHandleChecked();
  DirectHandle<WasmMemoryObject> memory_object =
      WasmMemoryObject::New(isolate, 1, WasmMemoryObject::kNoMaximum,
                            SharedFlag{false}, AddressType::kI32)
          .ToHandleChecked();
  DirectHandle<JSObject> ffi =
      isolate->factory()->NewJSObject(isolate->object_function());
  DirectHandle<JSObject> mod =
      isolate->factory()->NewJSObject(isolate->object_function());
  DirectHandle<String> m = isolate->factory()->InternalizeUtf8String("m");
  JSObject::AddProperty(isolate, mod, m, memory_object, NONE);
  JSObject::AddProperty(isolate, ffi, m, mod, NONE);

  MaybeDirectHandle<WasmInstanceObject> maybe_instance =
      GetWasmEngine()->SyncInstantiate(isolate, &thrower, module_object, ffi);
  CHECK(maybe_instance.is_null());
  CHECK(thrower.error());

  // Failed instantiation in Build_Phase1 must not register the instance in
  // any lists.
  CHECK_EQ(
      0u, module_object->script()->wasm_weak_instance_list()->length().value());
  CHECK_EQ(0u, memory_object->instances()->length().value());
}

TEST_F(RunWasmModuleTest, FailedInitGlobalsAndTablesUnpublishesInstance) {
  WasmExecutionTestBase::InitIsolateOnce();

  // 1. Trapping constant expression in InitGlobals() (global 0 traps with
  // kWasmTrapArrayTooLarge, leaving global 1 uninitialized).
  const uint8_t data1[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1), WASM_ARRAY_DEF(kI64Code, true)),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('m'),
              kExternalMemory, kNoMaximum, 1),
      SECTION(Global, ENTRY_COUNT(2),
              // global 0: (ref 0), traps with array.new_default -1
              kRefCode, 0, 0, kExprI32Const, 0x7f,
              WASM_GC_OP(kExprArrayNewDefault), 0, kExprEnd,
              // global 1: (ref 0), left uninitialized in tagged_globals_buffer
              kRefCode, 0, 0, kExprI32Const, 0,
              WASM_GC_OP(kExprArrayNewDefault), 0, kExprEnd),
  };
  TestFailedInstantiation(base::VectorOf(data1), "FailedInitGlobals");

  // 2. Trapping constant expression in SetTableInitialValues() (table 0 traps
  // with kWasmTrapArrayTooLarge, leaving non-nullable table 1 uninitialized).
  const uint8_t data2[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(2), SIG_ENTRY_v_v,
              WASM_ARRAY_DEF(kI64Code, true)),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('m'),
              kExternalMemory, kNoMaximum, 1),
      SECTION(Function, ENTRY_COUNT(1), 0),
      SECTION(Table, ENTRY_COUNT(2),
              // table 0: (ref 1) with trapping initializer
              0x40, 0x00, kRefCode, 1, kNoMaximum, 1, kExprI32Const, 0x7f,
              WASM_GC_OP(kExprArrayNewDefault), 1, kExprEnd,
              // table 1: non-nullable function table (ref 0)
              0x40, 0x00, kRefCode, 0, kNoMaximum, 1, kExprRefFunc, 0,
              kExprEnd),
      SECTION(Code, ENTRY_COUNT(1), ADD_COUNT(0, kExprEnd)),
  };
  TestFailedInstantiation(base::VectorOf(data2), "FailedSetTableInitialValues");

  Cleanup();
}

#undef EMIT_CODE_WITH_END

}  // namespace test_run_wasm_module
}  // namespace wasm
}  // namespace internal
}  // namespace v8
