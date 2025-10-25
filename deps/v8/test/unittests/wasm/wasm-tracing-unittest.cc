// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-tracing.h"

#include <sstream>

#include "src/execution/frames.h"
#include "src/flags/flags.h"
#include "src/heap/factory.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/fuzzer-common.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8::internal::wasm {

using ::testing::HasSubstr;

struct WasmTracingTestResult {
  std::string memory_trace;
  std::string global_trace;
  bool rerun_with_tracing;
};

class WasmTracingTest : public TestWithContextAndZone {
 public:
  WasmTracingTestResult RunTracingDiffTest(base::Vector<const uint8_t> code_a,
                                           base::Vector<const uint8_t> code_b) {
    FlagScope<bool> disable_native_module_cache(
        &v8_flags.wasm_native_module_cache, false);

    WasmTracingTestResult result = {};

    Isolate* v8_isolate = reinterpret_cast<Isolate*>(isolate());
    HandleScope handle_scope(v8_isolate);

    DirectHandle<WasmInstanceObject> instance_a =
        CompileAndRun(sigs_.v_v(), code_a, nullptr);
    DirectHandle<WasmInstanceObject> instance_b =
        CompileAndRun(sigs_.v_v(), code_b, nullptr);

    Tagged<WasmTrustedInstanceData> instance_data_a =
        instance_a->trusted_data(v8_isolate);
    Tagged<WasmTrustedInstanceData> instance_data_b =
        instance_b->trusted_data(v8_isolate);

    bool globals_match =
        fuzzing::GlobalsMatch(v8_isolate, instance_a->module(), instance_data_a,
                              instance_data_b, false);
    bool memories_match =
        fuzzing::MemoriesMatch(v8_isolate, instance_a->module(),
                               instance_data_a, instance_data_b, false);

    if (globals_match && memories_match) {
      return result;
    }

    result.rerun_with_tracing = true;

    bool should_trace_globals = !globals_match;
    bool should_trace_memory = !memories_match;
    FlagScope<bool> enable_tracing_globals(&v8_flags.trace_wasm_globals,
                                           should_trace_globals);
    FlagScope<bool> enable_tracing_memories(&v8_flags.trace_wasm_memory,
                                            should_trace_memory);

    WasmTracesForTesting& traces = GetWasmTracesForTesting();
    traces.should_store_trace = true;

    NativeModule* native_module;
    CompileAndRun(sigs_.v_v(), code_a, &native_module);

    MemoryTrace memory_trace_a = std::move(traces.memory_trace);
    GlobalTrace global_trace_a = std::move(traces.global_trace);
    traces.memory_trace.clear();
    traces.global_trace.clear();

    CompileAndRun(sigs_.v_v(), code_b, nullptr);

    MemoryTrace memory_trace_b = std::move(traces.memory_trace);
    GlobalTrace global_trace_b = std::move(traces.global_trace);
    traces.memory_trace.clear();
    traces.global_trace.clear();

    if (should_trace_memory) {
      std::ostringstream memory_ss;
      fuzzing::CompareAndPrintMemoryTraces(memory_trace_a, memory_trace_b,
                                           native_module, memory_ss);
      result.memory_trace = memory_ss.str();
    }
    if (should_trace_globals) {
      std::ostringstream global_ss;
      fuzzing::CompareAndPrintGlobalTraces(global_trace_a, global_trace_b,
                                           native_module, global_ss);
      result.global_trace = global_ss.str();
    }

    traces.should_store_trace = false;

    return result;
  }

 private:
  DirectHandle<WasmInstanceObject> CompileAndRun(
      const FunctionSig* sig, base::Vector<const uint8_t> code,
      NativeModule** native_module) {
    WasmModuleBuilder builder(zone());

    builder.AddMemory(1);
    ValueType i32 = ValueType::Primitive(kI32);
    builder.AddGlobal(i32, true, WasmInitExpr::DefaultValue(i32));

    StructType::Builder type_builder(zone(), 1, false, false);
    type_builder.AddField(i32, true);
    StructType* struct_type = type_builder.Build();
    ModuleTypeIndex struct_type_index =
        builder.AddStructType(struct_type, false);
    ValueType struct_ref =
        ValueType::Ref(struct_type_index, false, RefTypeKind::kStruct);
    builder.AddGlobal(struct_ref, true,
                      WasmInitExpr::StructNewDefault(struct_type_index));

    WasmFunctionBuilder* f = builder.AddFunction(sig);

    f->AddLocal(ValueType::Primitive(kI32));
    f->builder()->AddExport(base::CStrVector("main"), f);

    f->EmitCode(code.data(), static_cast<uint32_t>(code.size()));

    ZoneBuffer buffer(zone());
    builder.WriteTo(&buffer);

    Isolate* v8_isolate = reinterpret_cast<Isolate*>(isolate());

    ErrorThrower thrower(v8_isolate, "CompileAndRun");
    DirectHandle<WasmModuleObject> module =
        testing::CompileForTesting(v8_isolate, &thrower, base::VectorOf(buffer))
            .ToHandleChecked();
    CHECK(!module.is_null());
    if (native_module) {
      *native_module = module->native_module();
    }

    DirectHandle<WasmInstanceObject> instance =
        GetWasmEngine()
            ->SyncInstantiate(v8_isolate, &thrower, module, {}, {})
            .ToHandleChecked();
    CHECK(!instance.is_null());

    testing::CallWasmFunctionForTesting(v8_isolate, instance, "main", {});

    return instance;
  }

  TestSignatures sigs_;
};

TEST_F(WasmTracingTest, TestTracingNoDiff) {
  uint8_t code[] = {
      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0), WASM_I32V(3)),
      WASM_GLOBAL_SET(0, WASM_I32V(7)),
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(0)),
      WASM_GLOBAL_GET(0),
      WASM_DROP,
      WASM_DROP,
      WASM_END};

  WasmTracingTestResult result =
      RunTracingDiffTest(base::VectorOf(code), base::VectorOf(code));
  EXPECT_FALSE(result.rerun_with_tracing);
  EXPECT_TRUE(result.memory_trace.empty());
  EXPECT_TRUE(result.global_trace.empty());
}

TEST_F(WasmTracingTest, TestTracingMemoryValueDiff) {
  // Test with both a store and a load, with the store value differing.
  uint8_t code_a[] = {
      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0), WASM_I32V(3)),
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(0)),
      WASM_DROP,
      WASM_END,
  };

  uint8_t code_b[] = {
      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0), WASM_I32V(4)),
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(0)),
      WASM_DROP,
      WASM_END,
  };

  WasmTracingTestResult result =
      RunTracingDiffTest(base::VectorOf(code_a), base::VectorOf(code_b));

  EXPECT_TRUE(result.rerun_with_tracing);
  EXPECT_THAT(result.memory_trace,
              HasSubstr("Found 2 mismatches in 2 entries"));
  EXPECT_THAT(
      result.memory_trace,
      HasSubstr("Reference: liftoff  func     0:0x61   mem 0 i32.store    to   "
                "0000000000000000 val:  i32:4 / 0x4"));
  EXPECT_THAT(
      result.memory_trace,
      HasSubstr("Actual:    liftoff  func     0:0x61   mem 0 i32.store    to   "
                "0000000000000000 val:  i32:3 / 0x3"));
  EXPECT_TRUE(result.global_trace.empty());
}

TEST_F(WasmTracingTest, TestTracingMemoryAddressDiff) {
  // Test with both a store and a load, with the addresses differing.
  uint8_t code_a[] = {
      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(2), WASM_I32V(3)),
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(2)),
      WASM_DROP,
      WASM_END,
  };

  uint8_t code_b[] = {
      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(4), WASM_I32V(3)),
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(4)),
      WASM_DROP,
      WASM_END,
  };

  WasmTracingTestResult result =
      RunTracingDiffTest(base::VectorOf(code_a), base::VectorOf(code_b));

  EXPECT_TRUE(result.rerun_with_tracing);
  EXPECT_THAT(result.memory_trace,
              HasSubstr("Found 2 mismatches in 2 entries"));
  EXPECT_THAT(
      result.memory_trace,
      HasSubstr("Reference: liftoff  func     0:0x61   mem 0 i32.store    to   "
                "0000000000000004 val:  i32:3 / 0x3"));
  EXPECT_THAT(
      result.memory_trace,
      HasSubstr("Actual:    liftoff  func     0:0x61   mem 0 i32.store    to   "
                "0000000000000002 val:  i32:3 / 0x3"));
  EXPECT_TRUE(result.global_trace.empty());
}

TEST_F(WasmTracingTest, TestTracingGlobalValueDiff) {
  uint8_t code_a[] = {WASM_GLOBAL_SET(0, WASM_I32V(7)), WASM_GLOBAL_GET(0),
                      WASM_DROP, WASM_END};

  uint8_t code_b[] = {WASM_GLOBAL_SET(0, WASM_I32V(8)), WASM_GLOBAL_GET(0),
                      WASM_DROP, WASM_END};

  WasmTracingTestResult result =
      RunTracingDiffTest(base::VectorOf(code_a), base::VectorOf(code_b));

  EXPECT_TRUE(result.rerun_with_tracing);
  EXPECT_TRUE(result.memory_trace.empty());
  EXPECT_THAT(result.global_trace,
              HasSubstr("Found 2 mismatches in 2 entries"));
  EXPECT_THAT(
      result.global_trace,
      HasSubstr(
          "Reference: liftoff  func     0:0x5b   global.set 0 val: i32:8"));
  EXPECT_THAT(
      result.global_trace,
      HasSubstr(
          "Actual:    liftoff  func     0:0x5b   global.set 0 val: i32:7"));
}

TEST_F(WasmTracingTest, TestTracingGlobalRefValueDiff) {
  uint8_t code_a[] = {
      WASM_GLOBAL_SET(1, WASM_STRUCT_NEW(0, WASM_I32V(7))),
      WASM_END,
  };
  uint8_t code_b[] = {
      WASM_GLOBAL_SET(1, WASM_STRUCT_NEW(0, WASM_I32V(8))),
      WASM_END,
  };

  WasmTracingTestResult result =
      RunTracingDiffTest(base::VectorOf(code_a), base::VectorOf(code_b));

  EXPECT_TRUE(result.rerun_with_tracing);
  EXPECT_TRUE(result.memory_trace.empty());
  EXPECT_THAT(result.global_trace,
              HasSubstr("Traces are identical (1 entries)"));
  EXPECT_THAT(
      result.global_trace,
      HasSubstr("liftoff  func     0:0x5e   global.set 1 val: (ref 5)"));
}

TEST_F(WasmTracingTest, TestTracingManyMismatches) {
  uint8_t code_a[] = {
      WASM_LOOP(
          // if i == 100, break
          WASM_BR_IF(1, WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_I32V(100))),

          // if i >= 5 && i <= 40, store 0, if i >= 70, store 1, else store i
          WASM_IF_ELSE(
              WASM_I32_LEU(WASM_LOCAL_GET(0), WASM_I32V(5)),
              WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                             WASM_LOCAL_GET(0)),
              WASM_IF_ELSE(
                  WASM_I32_LEU(WASM_LOCAL_GET(0), WASM_I32V(40)),
                  WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                                 WASM_I32V(0)),
                  WASM_IF_ELSE(
                      WASM_I32_GEU(WASM_LOCAL_GET(0), WASM_I32V(70)),
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                                     WASM_I32V(1)),
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                                     WASM_LOCAL_GET(0))))),

          WASM_INC_LOCAL_BY(0, 1), WASM_BR(0)),
      WASM_END};

  uint8_t code_b[] = {
      WASM_LOOP(
          // if i == 100, break
          WASM_BR_IF(1, WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_I32V(100))),

          // if i >= 5 && i <= 40, store 2, if i >= 70, store 3, else store i
          WASM_IF_ELSE(
              WASM_I32_LEU(WASM_LOCAL_GET(0), WASM_I32V(5)),
              WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                             WASM_LOCAL_GET(0)),
              WASM_IF_ELSE(
                  WASM_I32_LEU(WASM_LOCAL_GET(0), WASM_I32V(40)),
                  WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                                 WASM_I32V(2)),
                  WASM_IF_ELSE(
                      WASM_I32_GEU(WASM_LOCAL_GET(0), WASM_I32V(70)),
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                                     WASM_I32V(3)),
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(0),
                                     WASM_LOCAL_GET(0))))),

          WASM_INC_LOCAL_BY(0, 1), WASM_BR(0)),
      WASM_END};

  WasmTracingTestResult result =
      RunTracingDiffTest(base::VectorOf(code_a), base::VectorOf(code_b));

  EXPECT_TRUE(result.rerun_with_tracing);
  EXPECT_TRUE(result.global_trace.empty());
  EXPECT_THAT(result.memory_trace,
              HasSubstr("Found 65 mismatches in 100 entries."));
  EXPECT_THAT(result.memory_trace,
              HasSubstr("Context around the first mismatch at line 7"));

  EXPECT_THAT(
      result.memory_trace,
      HasSubstr(
          "Match    at line 6: liftoff  func     0:0x75   mem 0 i32.store "
          "   to   0000000000000000 val:  i32:5 / 0x5"));
  EXPECT_THAT(result.memory_trace, HasSubstr("[...]\nMismatch at line 71"));
  EXPECT_THAT(result.memory_trace,
              HasSubstr("[... 15 more mismatches not shown]"));
}

}  // namespace v8::internal::wasm
