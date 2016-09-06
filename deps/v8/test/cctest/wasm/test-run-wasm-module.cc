// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "src/wasm/encoder.h"
#include "src/wasm/module-decoder.h"
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
void TestModule(Zone* zone, WasmModuleBuilder* builder,
                int32_t expected_result) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  WasmJs::InstallWasmFunctionMap(isolate, isolate->native_context());
  int32_t result =
      testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK_EQ(expected_result, result);
}

void ExportAs(WasmFunctionBuilder* f, const char* name) {
  f->SetExported();
  f->SetName(name, static_cast<int>(strlen(name)));
}

void ExportAsMain(WasmFunctionBuilder* f) {
  static const char kMainName[] = "main";
  ExportAs(f, kMainName);
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
  ExportAsMain(f);
  byte code[] = {WASM_I8(kReturnValue)};
  f->EmitCode(code, sizeof(code));
  TestModule(&zone, builder, kReturnValue);
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

  ExportAsMain(f);
  byte code2[] = {WASM_CALL_FUNCTION2(f1_index, WASM_I8(77), WASM_I8(22))};
  f->EmitCode(code2, sizeof(code2));
  TestModule(&zone, builder, 99);
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

  ExportAsMain(f);
  byte code[] = {
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I8(kDataSegmentDest0))};
  f->EmitCode(code, sizeof(code));
  byte data[] = {0xaa, 0xbb, 0xcc, 0xdd};
  builder->AddDataSegment(new (&zone) WasmDataSegmentEncoder(
      &zone, data, sizeof(data), kDataSegmentDest0));
  TestModule(&zone, builder, 0xddccbbaa);
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
  ExportAsMain(f);
  byte code[] = {WASM_BLOCK(
      WASM_WHILE(
          WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32V_3(kCheckSize)),
          WASM_IF_ELSE(
              WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(localIndex)),
              WASM_BRV(2, WASM_I8(-1)), WASM_INC_LOCAL_BY(localIndex, 4))),
      WASM_I8(11))};
  f->EmitCode(code, sizeof(code));
  TestModule(&zone, builder, 11);
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
  ExportAsMain(f);
  byte code[] = {WASM_BLOCK(
      WASM_SET_LOCAL(localIndex,
                     WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
      WASM_IF_ELSE(WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I8(5)),
                   WASM_BLOCK(WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                             WASM_INC_LOCAL(localIndex)),
                              WASM_BRV(1, WASM_CALL_FUNCTION0(0))),
                   WASM_BRV(0, WASM_I8(55))))};
  f->EmitCode(code, sizeof(code));
  TestModule(&zone, builder, 55);
}

TEST(Run_WasmModule_Global) {
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);
  TestSignatures sigs;

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint32_t global1 = builder->AddGlobal(kAstI32, 0);
  uint32_t global2 = builder->AddGlobal(kAstI32, 0);
  uint16_t f1_index = builder->AddFunction();
  WasmFunctionBuilder* f = builder->FunctionAt(f1_index);
  f->SetSignature(sigs.i_v());
  byte code1[] = {
      WASM_I32_ADD(WASM_GET_GLOBAL(global1), WASM_GET_GLOBAL(global2))};
  f->EmitCode(code1, sizeof(code1));
  uint16_t f2_index = builder->AddFunction();
  f = builder->FunctionAt(f2_index);
  f->SetSignature(sigs.i_v());
  ExportAsMain(f);
  byte code2[] = {WASM_SET_GLOBAL(global1, WASM_I32V_1(56)),
                  WASM_SET_GLOBAL(global2, WASM_I32V_1(41)),
                  WASM_RETURN1(WASM_CALL_FUNCTION0(f1_index))};
  f->EmitCode(code2, sizeof(code2));
  TestModule(&zone, builder, 97);
}

TEST(Run_WasmModule_Serialization) {
  FLAG_expose_wasm = true;
  static const char* kFunctionName = "increment";
  v8::base::AccountingAllocator allocator;
  Zone zone(&allocator);

  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  TestSignatures sigs;

  WasmFunctionBuilder* f = builder->FunctionAt(f_index);
  f->SetSignature(sigs.i_i());
  byte code[] = {WASM_GET_LOCAL(0), kExprI32Const, 1, kExprI32Add};
  f->EmitCode(code, sizeof(code));
  ExportAs(f, kFunctionName);

  ZoneBuffer buffer(&zone);
  builder->WriteTo(buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  ErrorThrower thrower(isolate, "");

  v8::WasmCompiledModule::SerializedModule data;
  {
    HandleScope scope(isolate);

    ModuleResult decoding_result = DecodeWasmModule(
        isolate, &zone, buffer.begin(), buffer.end(), false, kWasmOrigin);
    std::unique_ptr<const WasmModule> module(decoding_result.val);
    CHECK(!decoding_result.failed());

    MaybeHandle<FixedArray> compiled_module =
        module->CompileFunctions(isolate, &thrower);
    CHECK(!compiled_module.is_null());
    Handle<JSObject> module_obj =
        CreateCompiledModuleObject(isolate, compiled_module.ToHandleChecked());
    v8::Local<v8::Object> v8_module_obj = v8::Utils::ToLocal(module_obj);
    CHECK(v8_module_obj->IsWebAssemblyCompiledModule());

    v8::Local<v8::WasmCompiledModule> v8_compiled_module =
        v8_module_obj.As<v8::WasmCompiledModule>();
    data = v8_compiled_module->Serialize();
  }

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = isolate->array_buffer_allocator();

  v8::Isolate* v8_isolate = v8::Isolate::New(create_params);
  isolate = reinterpret_cast<Isolate*>(v8_isolate);
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);
    v8::HandleScope new_scope(v8_isolate);
    v8::Local<v8::Context> new_ctx = v8::Context::New(v8_isolate);
    new_ctx->Enter();

    v8::MaybeLocal<v8::WasmCompiledModule> deserialized =
        v8::WasmCompiledModule::Deserialize(v8_isolate, data);
    v8::Local<v8::WasmCompiledModule> compiled_module;
    CHECK(deserialized.ToLocal(&compiled_module));
    Handle<JSObject> module_object =
        Handle<JSObject>::cast(v8::Utils::OpenHandle(*compiled_module));
    Handle<FixedArray> compiled_part =
        handle(FixedArray::cast(module_object->GetInternalField(0)));
    Handle<JSObject> instance =
        WasmModule::Instantiate(isolate, compiled_part,
                                Handle<JSReceiver>::null(),
                                Handle<JSArrayBuffer>::null())
            .ToHandleChecked();
    Handle<Object> params[1] = {Handle<Object>(Smi::FromInt(41), isolate)};
    int32_t result = testing::CallFunction(isolate, instance, &thrower,
                                           kFunctionName, 1, params);
    CHECK(result == 42);
    new_ctx->Exit();
  }
}
