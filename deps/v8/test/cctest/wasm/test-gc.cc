// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_gc {

WASM_EXEC_TEST(BasicStruct) {
  // TODO(7748): Implement support in other tiers.
  if (execution_tier == ExecutionTier::kLiftoff) return;
  if (execution_tier == ExecutionTier::kInterpreter) return;
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(gc);
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  StructType::Builder type_builder(&zone, 2);
  type_builder.AddField(kWasmI32);
  type_builder.AddField(kWasmI32);
  int32_t type_index = builder->AddStructType(type_builder.Build());
  ValueType kRefTypes[] = {ValueType(ValueType::kRef, type_index)};
  ValueType kOptRefType = ValueType(ValueType::kOptRef, type_index);
  FunctionSig sig_q_v(1, 0, kRefTypes);

  // Test struct.new and struct.get.
  WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
  f->builder()->AddExport(CStrVector("f"), f);
  byte f_code[] = {WASM_STRUCT_GET(type_index, 0,
                                   WASM_STRUCT_NEW(type_index, WASM_I32V(42),
                                                   WASM_I32V(64))),
                   kExprEnd};
  f->EmitCode(f_code, sizeof(f_code));

  // Test struct.new and struct.get.
  WasmFunctionBuilder* g = builder->AddFunction(sigs.i_v());
  g->builder()->AddExport(CStrVector("g"), g);
  byte g_code[] = {WASM_STRUCT_GET(type_index, 1,
                                   WASM_STRUCT_NEW(type_index, WASM_I32V(42),
                                                   WASM_I32V(64))),
                   kExprEnd};
  g->EmitCode(g_code, sizeof(g_code));

  // Test struct.new, returning struct references to JS.
  WasmFunctionBuilder* h = builder->AddFunction(&sig_q_v);
  h->builder()->AddExport(CStrVector("h"), h);
  byte h_code[] = {WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(64)),
                   kExprEnd};
  h->EmitCode(h_code, sizeof(h_code));

  // Test struct.set, struct refs types in locals.
  WasmFunctionBuilder* j = builder->AddFunction(sigs.i_v());
  uint32_t j_local_index = j->AddLocal(kOptRefType);
  uint32_t j_field_index = 0;
  j->builder()->AddExport(CStrVector("j"), j);
  byte j_code[] = {
      WASM_SET_LOCAL(j_local_index,
                     WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(64))),
      WASM_STRUCT_SET(type_index, j_field_index, WASM_GET_LOCAL(j_local_index),
                      WASM_I32V(-99)),
      WASM_STRUCT_GET(type_index, j_field_index, WASM_GET_LOCAL(j_local_index)),
      kExprEnd};
  j->EmitCode(j_code, sizeof(j_code));

  // Test struct.set, ref.as_non_null,
  // struct refs types in globals and if-results.
  uint32_t k_global_index = builder->AddGlobal(kOptRefType, true);
  WasmFunctionBuilder* k = builder->AddFunction(sigs.i_v());
  uint32_t k_field_index = 0;
  k->builder()->AddExport(CStrVector("k"), k);
  byte k_code[] = {
      WASM_SET_GLOBAL(k_global_index, WASM_STRUCT_NEW(type_index, WASM_I32V(55),
                                                      WASM_I32V(66))),
      WASM_STRUCT_GET(type_index, k_field_index,
                      WASM_REF_AS_NON_NULL(WASM_IF_ELSE_R(
                          kOptRefType, WASM_I32V(1),
                          WASM_GET_GLOBAL(k_global_index), WASM_REF_NULL))),
      kExprEnd};
  k->EmitCode(k_code, sizeof(k_code));

  // Test br_on_null 1.
  WasmFunctionBuilder* l = builder->AddFunction(sigs.i_v());
  uint32_t l_local_index = l->AddLocal(kOptRefType);
  l->builder()->AddExport(CStrVector("l"), l);
  byte l_code[] = {
      WASM_BLOCK_I(WASM_I32V(42),
                   // Branch will be taken.
                   // 42 left on stack outside the block (not 52).
                   WASM_BR_ON_NULL(0, WASM_GET_LOCAL(l_local_index)),
                   WASM_I32V(52), WASM_BR(0)),
      kExprEnd};
  l->EmitCode(l_code, sizeof(l_code));

  // Test br_on_null 2.
  WasmFunctionBuilder* m = builder->AddFunction(sigs.i_v());
  uint32_t m_field_index = 0;
  m->builder()->AddExport(CStrVector("m"), m);
  byte m_code[] = {
      WASM_BLOCK_I(
          WASM_I32V(42),
          WASM_STRUCT_GET(
              type_index, m_field_index,
              // Branch will not be taken.
              // 52 left on stack outside the block (not 42).
              WASM_BR_ON_NULL(0, WASM_STRUCT_NEW(type_index, WASM_I32V(52),
                                                 WASM_I32V(62)))),
          WASM_BR(0)),
      kExprEnd};
  m->EmitCode(m_code, sizeof(m_code));

  // Test ref.eq
  WasmFunctionBuilder* n = builder->AddFunction(sigs.i_v());
  uint32_t n_local_index = n->AddLocal(kOptRefType);
  n->builder()->AddExport(CStrVector("n"), n);
  byte n_code[] = {
      WASM_SET_LOCAL(n_local_index,
                     WASM_STRUCT_NEW(type_index, WASM_I32V(55), WASM_I32V(66))),
      WASM_I32_ADD(
          WASM_I32_SHL(
              WASM_REF_EQ(  // true
                  WASM_GET_LOCAL(n_local_index), WASM_GET_LOCAL(n_local_index)),
              WASM_I32V(0)),
          WASM_I32_ADD(
              WASM_I32_SHL(WASM_REF_EQ(  // false
                               WASM_GET_LOCAL(n_local_index),
                               WASM_STRUCT_NEW(type_index, WASM_I32V(55),
                                               WASM_I32V(66))),
                           WASM_I32V(1)),
              WASM_I32_ADD(
                  WASM_I32_SHL(  // false
                      WASM_REF_EQ(WASM_GET_LOCAL(n_local_index), WASM_REF_NULL),
                      WASM_I32V(2)),
                  WASM_I32_SHL(WASM_REF_EQ(  // true
                                   WASM_REF_NULL, WASM_REF_NULL),
                               WASM_I32V(3))))),
      kExprEnd};
  n->EmitCode(n_code, sizeof(n_code));
  // Result: 0b1001

  ZoneBuffer buffer(&zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  ErrorThrower thrower(isolate, "Test");
  Handle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(
          isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
          .ToHandleChecked();

  CHECK_EQ(42, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "f", 0, nullptr));
  CHECK_EQ(64, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "g", 0, nullptr));

  // TODO(7748): This uses the JavaScript interface to retrieve the plain
  // WasmStruct. Once the JS interaction story is settled, this may well
  // need to be changed.
  Handle<WasmExportedFunction> h_export =
      testing::GetExportedFunction(isolate, instance, "h").ToHandleChecked();
  Handle<Object> undefined = isolate->factory()->undefined_value();
  Handle<Object> ref_result =
      Execution::Call(isolate, h_export, undefined, 0, nullptr)
          .ToHandleChecked();
  CHECK(ref_result->IsWasmStruct());

  CHECK_EQ(-99, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                    "j", 0, nullptr));

  CHECK_EQ(55, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "k", 0, nullptr));

  CHECK_EQ(42, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "l", 0, nullptr));

  CHECK_EQ(52, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "m", 0, nullptr));

  CHECK_EQ(0b1001, testing::CallWasmFunctionForTesting(
                       isolate, instance, &thrower, "n", 0, nullptr));
}

WASM_EXEC_TEST(BasicArray) {
  // TODO(7748): Implement support in other tiers.
  if (execution_tier == ExecutionTier::kLiftoff) return;
  if (execution_tier == ExecutionTier::kInterpreter) return;
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(gc);
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  ArrayType type(wasm::kWasmI32);
  int32_t type_index = builder->AddArrayType(&type);
  ValueType kRefTypes[] = {ValueType(ValueType::kRef, type_index)};
  FunctionSig sig_q_v(1, 0, kRefTypes);
  ValueType kOptRefType = ValueType(ValueType::kOptRef, type_index);

  WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
  uint32_t local_index = f->AddLocal(kOptRefType);
  f->builder()->AddExport(CStrVector("f"), f);
  // f: a = [12, 12, 12]; a[1] = 42; return a[arg0]
  byte f_code[] = {
      WASM_SET_LOCAL(local_index,
                     WASM_ARRAY_NEW(type_index, WASM_I32V(12), WASM_I32V(3))),
      WASM_ARRAY_SET(type_index, WASM_GET_LOCAL(local_index), WASM_I32V(1),
                     WASM_I32V(42)),
      WASM_ARRAY_GET(type_index, WASM_GET_LOCAL(local_index),
                     WASM_GET_LOCAL(0)),
      kExprEnd};
  f->EmitCode(f_code, sizeof(f_code));

  // Reads and returns an array's length.
  WasmFunctionBuilder* g = builder->AddFunction(sigs.i_v());
  f->builder()->AddExport(CStrVector("g"), g);
  byte g_code[] = {
      WASM_ARRAY_LEN(type_index,
                     WASM_ARRAY_NEW(type_index, WASM_I32V(0), WASM_I32V(42))),
      kExprEnd};
  g->EmitCode(g_code, sizeof(g_code));

  WasmFunctionBuilder* h = builder->AddFunction(&sig_q_v);
  h->builder()->AddExport(CStrVector("h"), h);
  // Create an array of length 2, initialized to [42, 42].
  byte h_code[] = {WASM_ARRAY_NEW(type_index, WASM_I32V(42), WASM_I32V(2)),
                   kExprEnd};
  h->EmitCode(h_code, sizeof(h_code));

  ZoneBuffer buffer(&zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  ErrorThrower thrower(isolate, "Test");
  Handle<WasmInstanceObject> instance =
      testing::CompileAndInstantiateForTesting(
          isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
          .ToHandleChecked();

  Handle<Object> argv[] = {handle(Smi::FromInt(0), isolate)};
  CHECK_EQ(12, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "f", 1, argv));
  argv[0] = handle(Smi::FromInt(1), isolate);
  CHECK_EQ(42, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "f", 1, argv));
  argv[0] = handle(Smi::FromInt(2), isolate);
  CHECK_EQ(12, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "f", 1, argv));
  Handle<Object> undefined = isolate->factory()->undefined_value();
  {
    Handle<WasmExportedFunction> f_export =
        testing::GetExportedFunction(isolate, instance, "f").ToHandleChecked();
    TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    argv[0] = handle(Smi::FromInt(3), isolate);
    MaybeHandle<Object> no_result =
        Execution::Call(isolate, f_export, undefined, 1, argv);
    CHECK(no_result.is_null());
    CHECK(try_catch.HasCaught());
    isolate->clear_pending_exception();
    argv[0] = handle(Smi::FromInt(-1), isolate);
    no_result = Execution::Call(isolate, f_export, undefined, 1, argv);
    CHECK(no_result.is_null());
    CHECK(try_catch.HasCaught());
    isolate->clear_pending_exception();
  }

  CHECK_EQ(42, testing::CallWasmFunctionForTesting(isolate, instance, &thrower,
                                                   "g", 0, nullptr));

  // TODO(7748): This uses the JavaScript interface to retrieve the plain
  // WasmArray. Once the JS interaction story is settled, this may well
  // need to be changed.
  Handle<WasmExportedFunction> h_export =
      testing::GetExportedFunction(isolate, instance, "h").ToHandleChecked();
  Handle<Object> ref_result =
      Execution::Call(isolate, h_export, undefined, 0, nullptr)
          .ToHandleChecked();
  CHECK(ref_result->IsWasmArray());
#if OBJECT_PRINT
  ref_result->Print();
#endif
}

}  // namespace test_gc
}  // namespace wasm
}  // namespace internal
}  // namespace v8
