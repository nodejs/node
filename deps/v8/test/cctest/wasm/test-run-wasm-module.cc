// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "src/wasm/encoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "test/cctest/cctest.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;


namespace {
void TestModule(WasmModuleIndex* module, int32_t expected_result) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  int32_t result =
      CompileAndRunWasmModule(isolate, module->Begin(), module->End());
  CHECK_EQ(expected_result, result);
}
}  // namespace


// A raw test that skips the WasmModuleBuilder.
TEST(Run_WasmModule_CallAdd_rev) {
  static const byte data[] = {
      // sig#0 ------------------------------------------
      kDeclSignatures, 2, 0, kLocalI32,    // void -> int
      2, kLocalI32, kLocalI32, kLocalI32,  // int,int -> int
      // func#0 (main) ----------------------------------
      kDeclFunctions, 2, kDeclFunctionExport, 0, 0,  // sig index
      6, 0,                                          // body size
      kExprCallFunction, 1,                          // --
      kExprI8Const, 77,                              // --
      kExprI8Const, 22,                              // --
      // func#1 -----------------------------------------
      0,                 // no name, not exported
      1, 0,              // sig index
      5, 0,              // body size
      kExprI32Add,       // --
      kExprGetLocal, 0,  // --
      kExprGetLocal, 1,  // --
  };

  Isolate* isolate = CcTest::InitIsolateOnce();
  int32_t result =
      CompileAndRunWasmModule(isolate, data, data + arraysize(data));
  CHECK_EQ(99, result);
}


TEST(Run_WasmModule_Return114) {
  static const int32_t kReturnValue = 114;
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->ReturnType(kAstI32);
  f->Exported(1);
  byte code[] = {WASM_I8(kReturnValue)};
  f->EmitCode(code, sizeof(code));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), kReturnValue);
}


TEST(Run_WasmModule_CallAdd) {
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f1_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f1_index);
  f->ReturnType(kAstI32);
  uint16_t param1 = f->AddParam(kAstI32);
  uint16_t param2 = f->AddParam(kAstI32);
  byte code1[] = {WASM_I32_ADD(WASM_GET_LOCAL(param1), WASM_GET_LOCAL(param2))};
  uint32_t local_indices1[] = {2, 4};
  f->EmitCode(code1, sizeof(code1), local_indices1, sizeof(local_indices1) / 4);
  uint16_t f2_index = builder->AddFunction();
  f = builder->FunctionAt(f2_index);
  f->ReturnType(kAstI32);
  f->Exported(1);
  byte code2[] = {WASM_CALL_FUNCTION(f1_index, WASM_I8(77), WASM_I8(22))};
  f->EmitCode(code2, sizeof(code2));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 99);
}


TEST(Run_WasmModule_ReadLoadedDataSegment) {
  static const byte kDataSegmentDest0 = 12;
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->ReturnType(kAstI32);
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


#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define V8_WITH_ASAN 1
#endif
#endif


#if !defined(V8_WITH_ASAN)
// TODO(bradnelson): Figure out why this crashes under asan.
TEST(Run_WasmModule_CheckMemoryIsZero) {
  static const int kCheckSize = 16 * 1024;
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->ReturnType(kAstI32);
  uint16_t localIndex = f->AddLocal(kAstI32);
  f->Exported(1);
  byte code[] = {WASM_BLOCK(
      2,
      WASM_WHILE(
          WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32(kCheckSize)),
          WASM_IF_ELSE(
              WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(localIndex)),
              WASM_BRV(2, WASM_I8(-1)), WASM_INC_LOCAL_BY(localIndex, 4))),
      WASM_I8(11))};
  uint32_t local_indices[] = {7, 19, 25, 28};
  f->EmitCode(code, sizeof(code), local_indices, sizeof(local_indices) / 4);
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 11);
}
#endif


#if !defined(V8_WITH_ASAN)
// TODO(bradnelson): Figure out why this crashes under asan.
TEST(Run_WasmModule_CallMain_recursive) {
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->ReturnType(kAstI32);
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
  uint32_t local_indices[] = {3, 11, 21, 24};
  f->EmitCode(code, sizeof(code), local_indices, sizeof(local_indices) / 4);
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 55);
}
#endif


#if !defined(V8_WITH_ASAN)
// TODO(bradnelson): Figure out why this crashes under asan.
TEST(Run_WasmModule_Global) {
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint32_t global1 = builder->AddGlobal(MachineType::Int32(), 0);
  uint32_t global2 = builder->AddGlobal(MachineType::Int32(), 0);
  uint16_t f1_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f1_index);
  f->ReturnType(kAstI32);
  byte code1[] = {
      WASM_I32_ADD(WASM_LOAD_GLOBAL(global1), WASM_LOAD_GLOBAL(global2))};
  f->EmitCode(code1, sizeof(code1));
  uint16_t f2_index = builder->AddFunction();
  f = builder->FunctionAt(f2_index);
  f->ReturnType(kAstI32);
  f->Exported(1);
  byte code2[] = {WASM_STORE_GLOBAL(global1, WASM_I32(56)),
                  WASM_STORE_GLOBAL(global2, WASM_I32(41)),
                  WASM_RETURN(WASM_CALL_FUNCTION0(f1_index))};
  f->EmitCode(code2, sizeof(code2));
  WasmModuleWriter* writer = builder->Build(&zone);
  TestModule(writer->WriteTo(&zone), 97);
}
#endif
