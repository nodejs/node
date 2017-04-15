// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

#define BUILD(r, ...)                      \
  do {                                     \
    byte code[] = {__VA_ARGS__};           \
    r.Build(code, code + arraysize(code)); \
  } while (false)

#define ADD_CODE(vec, ...)                                              \
  do {                                                                  \
    byte __buf[] = {__VA_ARGS__};                                       \
    for (size_t i = 0; i < sizeof(__buf); i++) vec.push_back(__buf[i]); \
  } while (false)

namespace {
// A helper for generating predictable but unique argument values that
// are easy to debug (e.g. with misaligned stacks).
class PredictableInputValues {
 public:
  int base_;
  explicit PredictableInputValues(int base) : base_(base) {}
  double arg_d(int which) { return base_ * which + ((which & 1) * 0.5); }
  float arg_f(int which) { return base_ * which + ((which & 1) * 0.25); }
  int32_t arg_i(int which) { return base_ * which + ((which & 1) * kMinInt); }
  int64_t arg_l(int which) {
    return base_ * which + ((which & 1) * (0x04030201LL << 32));
  }
};

uint32_t AddJSSelector(TestingModule* module, FunctionSig* sig, int which) {
  const int kMaxParams = 11;
  static const char* formals[kMaxParams] = {"",
                                            "a",
                                            "a,b",
                                            "a,b,c",
                                            "a,b,c,d",
                                            "a,b,c,d,e",
                                            "a,b,c,d,e,f",
                                            "a,b,c,d,e,f,g",
                                            "a,b,c,d,e,f,g,h",
                                            "a,b,c,d,e,f,g,h,i",
                                            "a,b,c,d,e,f,g,h,i,j"};
  CHECK_LT(which, static_cast<int>(sig->parameter_count()));
  CHECK_LT(static_cast<int>(sig->parameter_count()), kMaxParams);

  i::EmbeddedVector<char, 256> source;
  char param = 'a' + which;
  SNPrintF(source, "(function(%s) { return %c; })",
           formals[sig->parameter_count()], param);

  return module->AddJsFunction(sig, source.start());
}

void EXPECT_CALL(double expected, Handle<JSFunction> jsfunc,
                 Handle<Object>* buffer, int count) {
  Isolate* isolate = jsfunc->GetIsolate();
  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, jsfunc, global, count, buffer);

  CHECK(!retval.is_null());
  Handle<Object> result = retval.ToHandleChecked();
  if (result->IsSmi()) {
    CHECK_EQ(expected, Smi::cast(*result)->value());
  } else {
    CHECK(result->IsHeapNumber());
    CheckFloatEq(expected, HeapNumber::cast(*result)->value());
  }
}

void EXPECT_CALL(double expected, Handle<JSFunction> jsfunc, double a,
                 double b) {
  Isolate* isolate = jsfunc->GetIsolate();
  Handle<Object> buffer[] = {isolate->factory()->NewNumber(a),
                             isolate->factory()->NewNumber(b)};
  EXPECT_CALL(expected, jsfunc, buffer, 2);
}
}  // namespace

TEST(Run_Int32Sub_jswrapped) {
  WasmRunner<int, int, int> r(kExecuteCompiled);
  BUILD(r, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = r.module().WrapCode(r.function()->func_index);

  EXPECT_CALL(33, jsfunc, 44, 11);
  EXPECT_CALL(-8723487, jsfunc, -8000000, 723487);
}

TEST(Run_Float32Div_jswrapped) {
  WasmRunner<float, float, float> r(kExecuteCompiled);
  BUILD(r, WASM_F32_DIV(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = r.module().WrapCode(r.function()->func_index);

  EXPECT_CALL(92, jsfunc, 46, 0.5);
  EXPECT_CALL(64, jsfunc, -16, -0.25);
}

TEST(Run_Float64Add_jswrapped) {
  WasmRunner<double, double, double> r(kExecuteCompiled);
  BUILD(r, WASM_F64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = r.module().WrapCode(r.function()->func_index);

  EXPECT_CALL(3, jsfunc, 2, 1);
  EXPECT_CALL(-5.5, jsfunc, -5.25, -0.25);
}

TEST(Run_I32Popcount_jswrapped) {
  WasmRunner<int, int> r(kExecuteCompiled);
  BUILD(r, WASM_I32_POPCNT(WASM_GET_LOCAL(0)));
  Handle<JSFunction> jsfunc = r.module().WrapCode(r.function()->func_index);

  EXPECT_CALL(2, jsfunc, 9, 0);
  EXPECT_CALL(3, jsfunc, 11, 0);
  EXPECT_CALL(6, jsfunc, 0x3F, 0);
}

TEST(Run_CallJS_Add_jswrapped) {
  WasmRunner<int, int> r(kExecuteCompiled);
  TestSignatures sigs;
  uint32_t js_index =
      r.module().AddJsFunction(sigs.i_i(), "(function(a) { return a + 99; })");
  BUILD(r, WASM_CALL_FUNCTION(js_index, WASM_GET_LOCAL(0)));

  Handle<JSFunction> jsfunc = r.module().WrapCode(r.function()->func_index);

  EXPECT_CALL(101, jsfunc, 2, -8);
  EXPECT_CALL(199, jsfunc, 100, -1);
  EXPECT_CALL(-666666801, jsfunc, -666666900, -1);
}

void RunJSSelectTest(int which) {
  const int kMaxParams = 8;
  PredictableInputValues inputs(0x100);
  ValueType type = kWasmF64;
  ValueType types[kMaxParams + 1] = {type, type, type, type, type,
                                     type, type, type, type};
  for (int num_params = which + 1; num_params < kMaxParams; num_params++) {
    HandleScope scope(CcTest::InitIsolateOnce());
    FunctionSig sig(1, num_params, types);

    WasmRunner<void> r(kExecuteCompiled);
    uint32_t js_index = AddJSSelector(&r.module(), &sig, which);
    WasmFunctionCompiler& t = r.NewFunction(&sig);

    {
      std::vector<byte> code;

      for (int i = 0; i < num_params; i++) {
        ADD_CODE(code, WASM_F64(inputs.arg_d(i)));
      }

      ADD_CODE(code, kExprCallFunction, static_cast<byte>(js_index));

      size_t end = code.size();
      code.push_back(0);
      t.Build(&code[0], &code[end]);
    }

    Handle<JSFunction> jsfunc = r.module().WrapCode(t.function_index());
    double expected = inputs.arg_d(which);
    EXPECT_CALL(expected, jsfunc, 0.0, 0.0);
  }
}

TEST(Run_JSSelect_0) {
  CcTest::InitializeVM();
  RunJSSelectTest(0);
}

TEST(Run_JSSelect_1) {
  CcTest::InitializeVM();
  RunJSSelectTest(1);
}

TEST(Run_JSSelect_2) {
  CcTest::InitializeVM();
  RunJSSelectTest(2);
}

TEST(Run_JSSelect_3) {
  CcTest::InitializeVM();
  RunJSSelectTest(3);
}

TEST(Run_JSSelect_4) {
  CcTest::InitializeVM();
  RunJSSelectTest(4);
}

TEST(Run_JSSelect_5) {
  CcTest::InitializeVM();
  RunJSSelectTest(5);
}

TEST(Run_JSSelect_6) {
  CcTest::InitializeVM();
  RunJSSelectTest(6);
}

TEST(Run_JSSelect_7) {
  CcTest::InitializeVM();
  RunJSSelectTest(7);
}

void RunWASMSelectTest(int which) {
  PredictableInputValues inputs(0x200);
  Isolate* isolate = CcTest::InitIsolateOnce();
  const int kMaxParams = 8;
  for (int num_params = which + 1; num_params < kMaxParams; num_params++) {
    ValueType type = kWasmF64;
    ValueType types[kMaxParams + 1] = {type, type, type, type, type,
                                       type, type, type, type};
    FunctionSig sig(1, num_params, types);

    WasmRunner<void> r(kExecuteCompiled);
    WasmFunctionCompiler& t = r.NewFunction(&sig);
    BUILD(t, WASM_GET_LOCAL(which));
    Handle<JSFunction> jsfunc = r.module().WrapCode(t.function_index());

    Handle<Object> args[] = {
        isolate->factory()->NewNumber(inputs.arg_d(0)),
        isolate->factory()->NewNumber(inputs.arg_d(1)),
        isolate->factory()->NewNumber(inputs.arg_d(2)),
        isolate->factory()->NewNumber(inputs.arg_d(3)),
        isolate->factory()->NewNumber(inputs.arg_d(4)),
        isolate->factory()->NewNumber(inputs.arg_d(5)),
        isolate->factory()->NewNumber(inputs.arg_d(6)),
        isolate->factory()->NewNumber(inputs.arg_d(7)),
    };

    double expected = inputs.arg_d(which);
    EXPECT_CALL(expected, jsfunc, args, kMaxParams);
  }
}

TEST(Run_WASMSelect_0) {
  CcTest::InitializeVM();
  RunWASMSelectTest(0);
}

TEST(Run_WASMSelect_1) {
  CcTest::InitializeVM();
  RunWASMSelectTest(1);
}

TEST(Run_WASMSelect_2) {
  CcTest::InitializeVM();
  RunWASMSelectTest(2);
}

TEST(Run_WASMSelect_3) {
  CcTest::InitializeVM();
  RunWASMSelectTest(3);
}

TEST(Run_WASMSelect_4) {
  CcTest::InitializeVM();
  RunWASMSelectTest(4);
}

TEST(Run_WASMSelect_5) {
  CcTest::InitializeVM();
  RunWASMSelectTest(5);
}

TEST(Run_WASMSelect_6) {
  CcTest::InitializeVM();
  RunWASMSelectTest(6);
}

TEST(Run_WASMSelect_7) {
  CcTest::InitializeVM();
  RunWASMSelectTest(7);
}

void RunWASMSelectAlignTest(int num_args, int num_params) {
  PredictableInputValues inputs(0x300);
  Isolate* isolate = CcTest::InitIsolateOnce();
  const int kMaxParams = 10;
  DCHECK_LE(num_args, kMaxParams);
  ValueType type = kWasmF64;
  ValueType types[kMaxParams + 1] = {type, type, type, type, type, type,
                                     type, type, type, type, type};
  FunctionSig sig(1, num_params, types);

  for (int which = 0; which < num_params; which++) {
    WasmRunner<void> r(kExecuteCompiled);
    WasmFunctionCompiler& t = r.NewFunction(&sig);
    BUILD(t, WASM_GET_LOCAL(which));
    Handle<JSFunction> jsfunc = r.module().WrapCode(t.function_index());

    Handle<Object> args[] = {isolate->factory()->NewNumber(inputs.arg_d(0)),
                             isolate->factory()->NewNumber(inputs.arg_d(1)),
                             isolate->factory()->NewNumber(inputs.arg_d(2)),
                             isolate->factory()->NewNumber(inputs.arg_d(3)),
                             isolate->factory()->NewNumber(inputs.arg_d(4)),
                             isolate->factory()->NewNumber(inputs.arg_d(5)),
                             isolate->factory()->NewNumber(inputs.arg_d(6)),
                             isolate->factory()->NewNumber(inputs.arg_d(7)),
                             isolate->factory()->NewNumber(inputs.arg_d(8)),
                             isolate->factory()->NewNumber(inputs.arg_d(9))};

    double nan = std::numeric_limits<double>::quiet_NaN();
    double expected = which < num_args ? inputs.arg_d(which) : nan;
    EXPECT_CALL(expected, jsfunc, args, num_args);
  }
}

TEST(Run_WASMSelectAlign_0) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(0, 1);
  RunWASMSelectAlignTest(0, 2);
}

TEST(Run_WASMSelectAlign_1) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(1, 2);
  RunWASMSelectAlignTest(1, 3);
}

TEST(Run_WASMSelectAlign_2) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(2, 3);
  RunWASMSelectAlignTest(2, 4);
}

TEST(Run_WASMSelectAlign_3) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(3, 3);
  RunWASMSelectAlignTest(3, 4);
}

TEST(Run_WASMSelectAlign_4) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(4, 3);
  RunWASMSelectAlignTest(4, 4);
}

TEST(Run_WASMSelectAlign_7) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(7, 5);
  RunWASMSelectAlignTest(7, 6);
  RunWASMSelectAlignTest(7, 7);
}

TEST(Run_WASMSelectAlign_8) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(8, 5);
  RunWASMSelectAlignTest(8, 6);
  RunWASMSelectAlignTest(8, 7);
  RunWASMSelectAlignTest(8, 8);
}

TEST(Run_WASMSelectAlign_9) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(9, 6);
  RunWASMSelectAlignTest(9, 7);
  RunWASMSelectAlignTest(9, 8);
  RunWASMSelectAlignTest(9, 9);
}

TEST(Run_WASMSelectAlign_10) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(10, 7);
  RunWASMSelectAlignTest(10, 8);
  RunWASMSelectAlignTest(10, 9);
  RunWASMSelectAlignTest(10, 10);
}

void RunJSSelectAlignTest(int num_args, int num_params) {
  PredictableInputValues inputs(0x400);
  Isolate* isolate = CcTest::InitIsolateOnce();
  Factory* factory = isolate->factory();
  const int kMaxParams = 10;
  CHECK_LE(num_args, kMaxParams);
  CHECK_LE(num_params, kMaxParams);
  ValueType type = kWasmF64;
  ValueType types[kMaxParams + 1] = {type, type, type, type, type, type,
                                     type, type, type, type, type};
  FunctionSig sig(1, num_params, types);
  i::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  // Build the calling code.
  std::vector<byte> code;

  for (int i = 0; i < num_params; i++) {
    ADD_CODE(code, WASM_GET_LOCAL(i));
  }

  uint8_t predicted_js_index = 1;
  ADD_CODE(code, kExprCallFunction, predicted_js_index);

  size_t end = code.size();
  code.push_back(0);

  // Call different select JS functions.
  for (int which = 0; which < num_params; which++) {
    WasmRunner<void> r(kExecuteCompiled);
    uint32_t js_index = AddJSSelector(&r.module(), &sig, which);
    CHECK_EQ(predicted_js_index, js_index);
    WasmFunctionCompiler& t = r.NewFunction(&sig);
    t.Build(&code[0], &code[end]);

    Handle<JSFunction> jsfunc = r.module().WrapCode(t.function_index());

    Handle<Object> args[] = {
        factory->NewNumber(inputs.arg_d(0)),
        factory->NewNumber(inputs.arg_d(1)),
        factory->NewNumber(inputs.arg_d(2)),
        factory->NewNumber(inputs.arg_d(3)),
        factory->NewNumber(inputs.arg_d(4)),
        factory->NewNumber(inputs.arg_d(5)),
        factory->NewNumber(inputs.arg_d(6)),
        factory->NewNumber(inputs.arg_d(7)),
        factory->NewNumber(inputs.arg_d(8)),
        factory->NewNumber(inputs.arg_d(9)),
    };

    double nan = std::numeric_limits<double>::quiet_NaN();
    double expected = which < num_args ? inputs.arg_d(which) : nan;
    EXPECT_CALL(expected, jsfunc, args, num_args);
  }
}

TEST(Run_JSSelectAlign_0) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(0, 1);
  RunJSSelectAlignTest(0, 2);
}

TEST(Run_JSSelectAlign_1) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(1, 2);
  RunJSSelectAlignTest(1, 3);
}

TEST(Run_JSSelectAlign_2) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(2, 3);
  RunJSSelectAlignTest(2, 4);
}

TEST(Run_JSSelectAlign_3) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(3, 3);
  RunJSSelectAlignTest(3, 4);
}

TEST(Run_JSSelectAlign_4) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(4, 3);
  RunJSSelectAlignTest(4, 4);
}

TEST(Run_JSSelectAlign_7) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(7, 3);
  RunJSSelectAlignTest(7, 4);
  RunJSSelectAlignTest(7, 4);
  RunJSSelectAlignTest(7, 4);
}

TEST(Run_JSSelectAlign_8) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(8, 5);
  RunJSSelectAlignTest(8, 6);
  RunJSSelectAlignTest(8, 7);
  RunJSSelectAlignTest(8, 8);
}

TEST(Run_JSSelectAlign_9) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(9, 6);
  RunJSSelectAlignTest(9, 7);
  RunJSSelectAlignTest(9, 8);
  RunJSSelectAlignTest(9, 9);
}

TEST(Run_JSSelectAlign_10) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(10, 7);
  RunJSSelectAlignTest(10, 8);
  RunJSSelectAlignTest(10, 9);
  RunJSSelectAlignTest(10, 10);
}
