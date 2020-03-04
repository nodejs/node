// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/wasm-debug.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
void CheckDeterministicCompilation(
    std::initializer_list<ValueType> return_types,
    std::initializer_list<ValueType> param_types,
    std::initializer_list<uint8_t> raw_function_bytes) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope handle_scope(isolate);
  Zone zone(isolate->allocator(), ZONE_NAME);
  // Set up a module builder for the test module.
  TestingModuleBuilder module_builder(&zone, nullptr, ExecutionTier::kLiftoff,
                                      kNoRuntimeExceptionSupport, kNoLowerSimd);
  std::vector<ValueType> sig_types(return_types);
  sig_types.insert(sig_types.end(), param_types.begin(), param_types.end());
  FunctionSig sig(return_types.size(), param_types.size(), sig_types.data());
  module_builder.AddSignature(&sig);
  // Add a table of length 1, for indirect calls.
  module_builder.AddIndirectFunctionTable(nullptr, 1);
  int func_index =
      module_builder.AddFunction(&sig, "f", TestingModuleBuilder::kWasm);
  WasmFunction* function = module_builder.GetFunctionAt(func_index);

  // Build the function bytes by prepending the locals decl and appending an
  // "end" opcode.
  std::vector<uint8_t> function_bytes;
  function_bytes.push_back(WASM_NO_LOCALS);
  function_bytes.insert(function_bytes.end(), raw_function_bytes.begin(),
                        raw_function_bytes.end());
  function_bytes.push_back(WASM_END);
  FunctionBody func_body{&sig, 0, function_bytes.data(),
                         function_bytes.data() + function_bytes.size()};
  function->code = {module_builder.AddBytes(VectorOf(function_bytes)),
                    static_cast<uint32_t>(function_bytes.size())};

  // Now compile the function with Liftoff two times.
  auto env = module_builder.CreateCompilationEnv();
  WasmFeatures detected1;
  WasmFeatures detected2;
  WasmCompilationResult result1 = ExecuteLiftoffCompilation(
      isolate->allocator(), &env, func_body, function->func_index,
      isolate->counters(), &detected1);
  WasmCompilationResult result2 = ExecuteLiftoffCompilation(
      isolate->allocator(), &env, func_body, function->func_index,
      isolate->counters(), &detected2);

  CHECK(result1.succeeded());
  CHECK(result2.succeeded());

  // Check that the generated code matches.
  auto code1 = VectorOf(result1.code_desc.buffer, result1.code_desc.instr_size);
  auto code2 = VectorOf(result2.code_desc.buffer, result2.code_desc.instr_size);
  CHECK_EQ(code1, code2);
  // TODO(clemensb): Add operator== to WasmFeatures and enable this check.
  // CHECK_EQ(detected1, detected2);
}
}  // namespace

TEST(Liftoff_deterministic_simple) {
  CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))});
}

TEST(Liftoff_deterministic_call) {
  CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32},
      {WASM_I32_ADD(WASM_CALL_FUNCTION(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0))});
}

TEST(Liftoff_deterministic_indirect_call) {
  CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32},
      {WASM_I32_ADD(WASM_CALL_INDIRECT(0, WASM_GET_LOCAL(0), WASM_I32V_1(47)),
                    WASM_GET_LOCAL(0))});
}

TEST(Liftoff_deterministic_loop) {
  CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32},
      {WASM_LOOP(WASM_BR_IF(0, WASM_GET_LOCAL(0))), WASM_GET_LOCAL(0)});
}

TEST(Liftoff_deterministic_trap) {
  CheckDeterministicCompilation(
      {kWasmI32}, {kWasmI32, kWasmI32},
      {WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))});
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
