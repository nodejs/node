// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests effects of (CSP) "unsafe-eval" and "wasm-eval" callback functions.
//
// Note: These tests are in a separate test file because the tests dynamically
// change the isolate in terms of callbacks allow_code_gen_callback and
// allow_wasm_code_gen_callback.

#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// Possible values for callback pointers.
enum TestValue {
  kTestUsingNull,   // no callback.
  kTestUsingFalse,  // callback returning false.
  kTestUsingTrue,   // callbacl returning true.
};

constexpr int kNumTestValues = 3;

const char* TestValueName[kNumTestValues] = {"null", "false", "true"};

// Defined to simplify iterating over TestValues;
const TestValue AllTestValues[kNumTestValues] = {
    kTestUsingNull, kTestUsingFalse, kTestUsingTrue};

// This matrix holds the results of setting allow_code_gen_callback
// (first index) and allow_wasm_code_gen_callback (second index) using
// TestValue's. The value in the matrix is true if compilation is
// allowed, and false otherwise.
const bool ExpectedResults[kNumTestValues][kNumTestValues] = {
    {true, false, true}, {false, false, true}, {true, false, true}};

bool TrueCallback(Local<v8::Context>, Local<v8::String>) { return true; }

bool FalseCallback(Local<v8::Context>, Local<v8::String>) { return false; }

using CallbackFn = bool (*)(Local<v8::Context>, Local<v8::String>);

// Defines the Callback to use for the corresponding TestValue.
CallbackFn Callback[kNumTestValues] = {nullptr, FalseCallback, TrueCallback};

void BuildTrivialModule(Zone* zone, ZoneBuffer* buffer) {
  WasmModuleBuilder* builder = new (zone) WasmModuleBuilder(zone);
  builder->WriteTo(*buffer);
}

bool TestModule(Isolate* isolate, v8::MemorySpan<const uint8_t> wire_bytes) {
  HandleScope scope(isolate);

  v8::MemorySpan<const uint8_t> serialized_module;
  MaybeLocal<v8::WasmModuleObject> module =
      v8::WasmModuleObject::DeserializeOrCompile(
          reinterpret_cast<v8::Isolate*>(isolate), serialized_module,
          wire_bytes);
  return !module.IsEmpty();
}

}  // namespace

TEST(PropertiesOfCodegenCallbacks) {
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  ZoneBuffer buffer(&zone);
  BuildTrivialModule(&zone, &buffer);
  v8::MemorySpan<const uint8_t> wire_bytes = {buffer.begin(), buffer.size()};
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);

  for (TestValue codegen : AllTestValues) {
    for (TestValue wasm_codegen : AllTestValues) {
      fprintf(stderr, "Test codegen = %s, wasm_codegen = %s\n",
              TestValueName[codegen], TestValueName[wasm_codegen]);
      isolate->set_allow_code_gen_callback(Callback[codegen]);
      isolate->set_allow_wasm_code_gen_callback(Callback[wasm_codegen]);
      bool found = TestModule(isolate, wire_bytes);
      bool expected = ExpectedResults[codegen][wasm_codegen];
      CHECK_EQ(expected, found);
      CcTest::CollectAllAvailableGarbage();
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
