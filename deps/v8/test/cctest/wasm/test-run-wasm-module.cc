// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "src/wasm/encoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "test/cctest/cctest.h"
#include "test/cctest/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

namespace {
void TestModule(WasmModuleIndex* module, int32_t expected_result) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  WasmJs::InstallWasmFunctionMap(isolate, isolate->native_context());
  int32_t result =
      CompileAndRunWasmModule(isolate, module->Begin(), module->End());
  CHECK_EQ(expected_result, result);
}
}  // namespace

TEST(Run_WasmModule_Return114) {
  static const int32_t kReturnValue = 114;
  TestSignatures sigs;
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->SetSignature(sigs.i_v());
  f->Exported(1);
  byte code[] = {WASM_I8(kReturnValue)};
  f->EmitCode(code, sizeof(code));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), kReturnValue);
}

TEST(Run_WasmModule_CallAdd) {
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);
  TestSignatures sigs;

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);

  uint16_t f1_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f1_index);
  f->SetSignature(sigs.i_ii());
  uint16_t param1 = 0;
  uint16_t param2 = 1;
  byte code1[] = {WASM_I32_ADD(WASM_GET_LOCAL(param1), WASM_GET_LOCAL(param2))};
  f->EmitCode(code1, sizeof(code1));

  uint16_t f2_index = builder->AddFunction();
  f = builder->FunctionAt(f2_index);
  f->SetSignature(sigs.i_v());

  f->Exported(1);
  byte code2[] = {WASM_CALL_FUNCTION2(f1_index, WASM_I8(77), WASM_I8(22))};
  f->EmitCode(code2, sizeof(code2));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 99);
}

TEST(Run_WasmModule_ReadLoadedDataSegment) {
  static const byte kDataSegmentDest0 = 12;
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);
  TestSignatures sigs;

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->SetSignature(sigs.i_v());

  f->Exported(1);
  byte code[] = {
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I8(kDataSegmentDest0))};
  f->EmitCode(code, sizeof(code));
  byte data[] = {0xaa, 0xbb, 0xcc, 0xdd};
  builder->AddDataSegment(new (&zone) WasmDataSegmentEncoder(
      &zone, data, sizeof(data), kDataSegmentDest0));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 0xddccbbaa);
}

TEST(Run_WasmModule_CheckMemoryIsZero) {
  static const int kCheckSize = 16 * 1024;
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);
  TestSignatures sigs;

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->SetSignature(sigs.i_v());

  uint16_t localIndex = f->AddLocal(kAstI32);
  f->Exported(1);
  byte code[] = {WASM_BLOCK(
      2,
      WASM_WHILE(
          WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32V_3(kCheckSize)),
          WASM_IF_ELSE(
              WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(localIndex)),
              WASM_BRV(2, WASM_I8(-1)), WASM_INC_LOCAL_BY(localIndex, 4))),
      WASM_I8(11))};
  f->EmitCode(code, sizeof(code));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 11);
}

TEST(Run_WasmModule_CallMain_recursive) {
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);
  TestSignatures sigs;

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->SetSignature(sigs.i_v());

  uint16_t localIndex = f->AddLocal(kAstI32);
  f->Exported(1);
  byte code[] = {WASM_BLOCK(
      2, WASM_SET_LOCAL(localIndex,
                        WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
      WASM_IF_ELSE(WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I8(5)),
                   WASM_BLOCK(2, WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                                WASM_INC_LOCAL(localIndex)),
                              WASM_BRV(1, WASM_CALL_FUNCTION0(0))),
                   WASM_BRV(0, WASM_I8(55))))};
  f->EmitCode(code, sizeof(code));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 55);
}

TEST(Run_WasmModule_Global) {
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);
  TestSignatures sigs;

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint32_t global1 = builder->AddGlobal(MachineType::Int32(), 0);
  uint32_t global2 = builder->AddGlobal(MachineType::Int32(), 0);
  uint16_t f1_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f1_index);
  f->SetSignature(sigs.i_v());
  byte code1[] = {
      WASM_I32_ADD(WASM_LOAD_GLOBAL(global1), WASM_LOAD_GLOBAL(global2))};
  f->EmitCode(code1, sizeof(code1));
  uint16_t f2_index = builder->AddFunction();
  f = builder->FunctionAt(f2_index);
  f->SetSignature(sigs.i_v());
  f->Exported(1);
  byte code2[] = {WASM_STORE_GLOBAL(global1, WASM_I32V_1(56)),
                  WASM_STORE_GLOBAL(global2, WASM_I32V_1(41)),
                  WASM_RETURN1(WASM_CALL_FUNCTION0(f1_index))};
  f->EmitCode(code2, sizeof(code2));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 97);
}
