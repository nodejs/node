// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "src/base/overflowing-math.h"
#include "src/codegen/assembler-inl.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

/**
 * We test the interface from Wasm compiled code to the Wasm interpreter by
 * building a module with two functions. The external function is called from
 * this test, and will be compiled code. It takes its arguments and passes them
 * on to the internal function, which will be redirected to the interpreter.
 * If the internal function has an i64 parameter, is has to be replaced by two
 * i32 parameters on the external function.
 * The internal function just converts all its arguments to f64, sums them up
 * and returns the sum.
 */
namespace {

template <typename T>
class ArgPassingHelper {
 public:
  ArgPassingHelper(WasmRunnerBase& runner, WasmFunctionCompiler& inner_compiler,
                   std::initializer_list<uint8_t> bytes_inner_function,
                   std::initializer_list<uint8_t> bytes_outer_function,
                   const T& expected_lambda)
      : isolate_(runner.main_isolate()),
        expected_lambda_(expected_lambda),
        debug_info_(WasmInstanceObject::GetOrCreateDebugInfo(
            runner.builder().instance_object())) {
    std::vector<uint8_t> inner_code{bytes_inner_function};
    inner_compiler.Build(inner_code.data(),
                         inner_code.data() + inner_code.size());

    std::vector<uint8_t> outer_code{bytes_outer_function};
    runner.Build(outer_code.data(), outer_code.data() + outer_code.size());

    int funcs_to_redict[] = {static_cast<int>(inner_compiler.function_index())};
    runner.builder().SetExecutable();
    WasmDebugInfo::RedirectToInterpreter(debug_info_,
                                         ArrayVector(funcs_to_redict));
    main_fun_wrapper_ = runner.builder().WrapCode(runner.function_index());
  }

  template <typename... Args>
  void CheckCall(Args... args) {
    Handle<Object> arg_objs[] = {isolate_->factory()->NewNumber(args)...};

    uint64_t num_interpreted_before = debug_info_->NumInterpretedCalls();
    Handle<Object> global(isolate_->context().global_object(), isolate_);
    MaybeHandle<Object> retval = Execution::Call(
        isolate_, main_fun_wrapper_, global, arraysize(arg_objs), arg_objs);
    uint64_t num_interpreted_after = debug_info_->NumInterpretedCalls();
    // Check that we really went through the interpreter.
    CHECK_EQ(num_interpreted_before + 1, num_interpreted_after);
    // Check the result.
    double result = retval.ToHandleChecked()->Number();
    double expected = expected_lambda_(args...);
    CHECK_DOUBLE_EQ(expected, result);
  }

 private:
  Isolate* isolate_;
  T expected_lambda_;
  Handle<WasmDebugInfo> debug_info_;
  Handle<JSFunction> main_fun_wrapper_;
};

template <typename T>
static ArgPassingHelper<T> GetHelper(
    WasmRunnerBase& runner, WasmFunctionCompiler& inner_compiler,
    std::initializer_list<uint8_t> bytes_inner_function,
    std::initializer_list<uint8_t> bytes_outer_function,
    const T& expected_lambda) {
  return ArgPassingHelper<T>(runner, inner_compiler, bytes_inner_function,
                             bytes_outer_function, expected_lambda);
}

}  // namespace

// Pass int32_t, return int32_t.
TEST(TestArgumentPassing_int32) {
  WasmRunner<int32_t, int32_t> runner(ExecutionTier::kTurbofan);
  WasmFunctionCompiler& f2 = runner.NewFunction<int32_t, int32_t>();

  auto helper = GetHelper(
      runner, f2,
      {// Return 2*<0> + 1.
       WASM_I32_ADD(WASM_I32_MUL(WASM_I32V_1(2), WASM_GET_LOCAL(0)), WASM_ONE)},
      {// Call f2 with param <0>.
       WASM_GET_LOCAL(0), WASM_CALL_FUNCTION0(f2.function_index())},
      [](int32_t a) {
        return base::AddWithWraparound(base::MulWithWraparound(2, a), 1);
      });

  FOR_INT32_INPUTS(v) { helper.CheckCall(v); }
}

// Pass int64_t, return double.
TEST(TestArgumentPassing_double_int64) {
  WasmRunner<double, int32_t, int32_t> runner(ExecutionTier::kTurbofan);
  WasmFunctionCompiler& f2 = runner.NewFunction<double, int64_t>();

  auto helper = GetHelper(
      runner, f2,
      {// Return (double)<0>.
       WASM_F64_SCONVERT_I64(WASM_GET_LOCAL(0))},
      {// Call f2 with param (<0> | (<1> << 32)).
       WASM_I64_IOR(WASM_I64_UCONVERT_I32(WASM_GET_LOCAL(0)),
                    WASM_I64_SHL(WASM_I64_UCONVERT_I32(WASM_GET_LOCAL(1)),
                                 WASM_I64V_1(32))),
       WASM_CALL_FUNCTION0(f2.function_index())},
      [](int32_t a, int32_t b) {
        int64_t a64 = static_cast<int64_t>(a) & 0xFFFFFFFF;
        int64_t b64 = static_cast<uint64_t>(static_cast<int64_t>(b)) << 32;
        return static_cast<double>(a64 | b64);
      });

  FOR_INT32_INPUTS(v1) {
    FOR_INT32_INPUTS(v2) { helper.CheckCall(v1, v2); }
  }

  FOR_INT64_INPUTS(v) {
    int32_t v1 = static_cast<int32_t>(v);
    int32_t v2 = static_cast<int32_t>(v >> 32);
    helper.CheckCall(v1, v2);
    helper.CheckCall(v2, v1);
  }
}

// Pass double, return int64_t.
TEST(TestArgumentPassing_int64_double) {
  // Outer function still returns double.
  WasmRunner<double, double> runner(ExecutionTier::kTurbofan);
  WasmFunctionCompiler& f2 = runner.NewFunction<int64_t, double>();

  auto helper = GetHelper(
      runner, f2,
      {// Return (int64_t)<0>.
       WASM_I64_SCONVERT_F64(WASM_GET_LOCAL(0))},
      {// Call f2 with param <0>, convert returned value back to double.
       WASM_F64_SCONVERT_I64(WASM_SEQ(
           WASM_GET_LOCAL(0), WASM_CALL_FUNCTION0(f2.function_index())))},
      [](double d) { return d; });

  for (int64_t i : compiler::ValueHelper::int64_vector()) {
    helper.CheckCall(i);
  }
}

// Pass float, return double.
TEST(TestArgumentPassing_float_double) {
  WasmRunner<double, float> runner(ExecutionTier::kTurbofan);
  WasmFunctionCompiler& f2 = runner.NewFunction<double, float>();

  auto helper = GetHelper(
      runner, f2,
      {// Return 2*(double)<0> + 1.
       WASM_F64_ADD(
           WASM_F64_MUL(WASM_F64(2), WASM_F64_CONVERT_F32(WASM_GET_LOCAL(0))),
           WASM_F64(1))},
      {// Call f2 with param <0>.
       WASM_GET_LOCAL(0), WASM_CALL_FUNCTION0(f2.function_index())},
      [](float f) { return 2. * static_cast<double>(f) + 1.; });

  FOR_FLOAT32_INPUTS(f) { helper.CheckCall(f); }
}

// Pass two doubles, return double.
TEST(TestArgumentPassing_double_double) {
  WasmRunner<double, double, double> runner(ExecutionTier::kTurbofan);
  WasmFunctionCompiler& f2 = runner.NewFunction<double, double, double>();

  auto helper = GetHelper(runner, f2,
                          {// Return <0> + <1>.
                           WASM_F64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))},
                          {// Call f2 with params <0>, <1>.
                           WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                           WASM_CALL_FUNCTION0(f2.function_index())},
                          [](double a, double b) { return a + b; });

  FOR_FLOAT64_INPUTS(d1) {
    FOR_FLOAT64_INPUTS(d2) { helper.CheckCall(d1, d2); }
  }
}

// Pass int32_t, int64_t, float and double, return double.
TEST(TestArgumentPassing_AllTypes) {
  // The second and third argument will be combined to an i64.
  WasmRunner<double, int32_t, int32_t, int32_t, float, double> runner(
      ExecutionTier::kTurbofan);
  WasmFunctionCompiler& f2 =
      runner.NewFunction<double, int32_t, int64_t, float, double>();

  auto helper = GetHelper(
      runner, f2,
      {
          // Convert all arguments to double, add them and return the sum.
          WASM_F64_ADD(          // <0+1+2> + <3>
              WASM_F64_ADD(      // <0+1> + <2>
                  WASM_F64_ADD(  // <0> + <1>
                      WASM_F64_SCONVERT_I32(
                          WASM_GET_LOCAL(0)),  // <0> to double
                      WASM_F64_SCONVERT_I64(
                          WASM_GET_LOCAL(1))),               // <1> to double
                  WASM_F64_CONVERT_F32(WASM_GET_LOCAL(2))),  // <2> to double
              WASM_GET_LOCAL(3))                             // <3>
      },
      {WASM_GET_LOCAL(0),                                      // first arg
       WASM_I64_IOR(WASM_I64_UCONVERT_I32(WASM_GET_LOCAL(1)),  // second arg
                    WASM_I64_SHL(WASM_I64_UCONVERT_I32(WASM_GET_LOCAL(2)),
                                 WASM_I64V_1(32))),
       WASM_GET_LOCAL(3),  // third arg
       WASM_GET_LOCAL(4),  // fourth arg
       WASM_CALL_FUNCTION0(f2.function_index())},
      [](int32_t a, int32_t b, int32_t c, float d, double e) {
        return 0. + a + (static_cast<int64_t>(b) & 0xFFFFFFFF) +
               ((static_cast<int64_t>(c) & 0xFFFFFFFF) << 32) + d + e;
      });

  auto CheckCall = [&](int32_t a, int64_t b, float c, double d) {
    int32_t b0 = static_cast<int32_t>(b);
    int32_t b1 = static_cast<int32_t>(b >> 32);
    helper.CheckCall(a, b0, b1, c, d);
    helper.CheckCall(a, b1, b0, c, d);
  };

  Vector<const int32_t> test_values_i32 = compiler::ValueHelper::int32_vector();
  Vector<const int64_t> test_values_i64 = compiler::ValueHelper::int64_vector();
  Vector<const float> test_values_f32 = compiler::ValueHelper::float32_vector();
  Vector<const double> test_values_f64 =
      compiler::ValueHelper::float64_vector();
  size_t max_len =
      std::max(std::max(test_values_i32.size(), test_values_i64.size()),
               std::max(test_values_f32.size(), test_values_f64.size()));
  for (size_t i = 0; i < max_len; ++i) {
    int32_t i32 = test_values_i32[i % test_values_i32.size()];
    int64_t i64 = test_values_i64[i % test_values_i64.size()];
    float f32 = test_values_f32[i % test_values_f32.size()];
    double f64 = test_values_f64[i % test_values_f64.size()];
    CheckCall(i32, i64, f32, f64);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
