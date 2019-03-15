// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "src/assembler-inl.h"
#include "src/base/overflowing-math.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

/**
 * We test the interface from C to compiled wasm code by generating a wasm
 * function, creating a corresponding signature, compiling the c wasm entry for
 * that signature, and then calling that entry using different test values.
 * The result is compared against the expected result, computed from a lambda
 * passed to the CWasmEntryArgTester.
 */
namespace {

template <typename ReturnType, typename... Args>
class CWasmEntryArgTester {
 public:
  CWasmEntryArgTester(std::initializer_list<uint8_t> wasm_function_bytes,
                      std::function<ReturnType(Args...)> expected_fn)
      : runner_(ExecutionTier::kOptimized),
        isolate_(runner_.main_isolate()),
        expected_fn_(expected_fn),
        sig_(runner_.template CreateSig<ReturnType, Args...>()) {
    std::vector<uint8_t> code{wasm_function_bytes};
    runner_.Build(code.data(), code.data() + code.size());
    wasm_code_ = runner_.builder().GetFunctionCode(0);
    Handle<WasmInstanceObject> instance(runner_.builder().instance_object());
    Handle<WasmDebugInfo> debug_info =
        WasmInstanceObject::GetOrCreateDebugInfo(instance);
    c_wasm_entry_fn_ = WasmDebugInfo::GetCWasmEntry(debug_info, sig_);
  }

  template <typename... Rest>
  void WriteToBuffer(Address buf, Rest... rest) {
    static_assert(sizeof...(rest) == 0, "this is the base case");
  }

  template <typename First, typename... Rest>
  void WriteToBuffer(Address buf, First first, Rest... rest) {
    WriteUnalignedValue(buf, first);
    WriteToBuffer(buf + sizeof(first), rest...);
  }

  void CheckCall(Args... args) {
    std::vector<uint8_t> arg_buffer(sizeof...(args) * 8);
    WriteToBuffer(reinterpret_cast<Address>(arg_buffer.data()), args...);

    Handle<Object> receiver = isolate_->factory()->undefined_value();
    Handle<Object> buffer_obj(
        Object(reinterpret_cast<Address>(arg_buffer.data())), isolate_);
    CHECK(!buffer_obj->IsHeapObject());
    Handle<Object> code_entry_obj(Object(wasm_code_->instruction_start()),
                                  isolate_);
    CHECK(!code_entry_obj->IsHeapObject());
    Handle<Object> call_args[]{code_entry_obj,
                               runner_.builder().instance_object(), buffer_obj};
    static_assert(
        arraysize(call_args) == compiler::CWasmEntryParameters::kNumParameters,
        "adapt this test");
    wasm_code_->native_module()->SetExecutable(true);
    MaybeHandle<Object> return_obj = Execution::Call(
        isolate_, c_wasm_entry_fn_, receiver, arraysize(call_args), call_args);
    CHECK(!return_obj.is_null());
    CHECK(return_obj.ToHandleChecked()->IsSmi());
    CHECK_EQ(0, Smi::ToInt(*return_obj.ToHandleChecked()));

    // Check the result.
    ReturnType result = ReadUnalignedValue<ReturnType>(
        reinterpret_cast<Address>(arg_buffer.data()));
    ReturnType expected = expected_fn_(args...);
    if (std::is_floating_point<ReturnType>::value) {
      CHECK_DOUBLE_EQ(expected, result);
    } else {
      CHECK_EQ(expected, result);
    }
  }

 private:
  WasmRunner<ReturnType, Args...> runner_;
  Isolate* isolate_;
  std::function<ReturnType(Args...)> expected_fn_;
  FunctionSig* sig_;
  Handle<JSFunction> c_wasm_entry_fn_;
  WasmCode* wasm_code_;
};

}  // namespace

// Pass int32_t, return int32_t.
TEST(TestCWasmEntryArgPassing_int32) {
  CWasmEntryArgTester<int32_t, int32_t> tester(
      {// Return 2*<0> + 1.
       WASM_I32_ADD(WASM_I32_MUL(WASM_I32V_1(2), WASM_GET_LOCAL(0)), WASM_ONE)},
      [](int32_t a) {
        return base::AddWithWraparound(base::MulWithWraparound(2, a), 1);
      });

  FOR_INT32_INPUTS(v) { tester.CheckCall(v); }
}

// Pass int64_t, return double.
TEST(TestCWasmEntryArgPassing_double_int64) {
  CWasmEntryArgTester<double, int64_t> tester(
      {// Return (double)<0>.
       WASM_F64_SCONVERT_I64(WASM_GET_LOCAL(0))},
      [](int64_t a) { return static_cast<double>(a); });

  FOR_INT64_INPUTS(v) { tester.CheckCall(v); }
}

// Pass double, return int64_t.
TEST(TestCWasmEntryArgPassing_int64_double) {
  CWasmEntryArgTester<int64_t, double> tester(
      {// Return (int64_t)<0>.
       WASM_I64_SCONVERT_F64(WASM_GET_LOCAL(0))},
      [](double d) { return static_cast<int64_t>(d); });

  FOR_INT64_INPUTS(i) { tester.CheckCall(i); }
}

// Pass float, return double.
TEST(TestCWasmEntryArgPassing_float_double) {
  CWasmEntryArgTester<double, float> tester(
      {// Return 2*(double)<0> + 1.
       WASM_F64_ADD(
           WASM_F64_MUL(WASM_F64(2), WASM_F64_CONVERT_F32(WASM_GET_LOCAL(0))),
           WASM_F64(1))},
      [](float f) { return 2. * static_cast<double>(f) + 1.; });

  FOR_FLOAT32_INPUTS(f) { tester.CheckCall(f); }
}

// Pass two doubles, return double.
TEST(TestCWasmEntryArgPassing_double_double) {
  CWasmEntryArgTester<double, double, double> tester(
      {// Return <0> + <1>.
       WASM_F64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))},
      [](double a, double b) { return a + b; });

  FOR_FLOAT64_INPUTS(d1) {
    FOR_FLOAT64_INPUTS(d2) { tester.CheckCall(d1, d2); }
  }
}

// Pass int32_t, int64_t, float and double, return double.
TEST(TestCWasmEntryArgPassing_AllTypes) {
  CWasmEntryArgTester<double, int32_t, int64_t, float, double> tester(
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
      [](int32_t a, int64_t b, float c, double d) {
        return 0. + a + b + c + d;
      });

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
    tester.CheckCall(i32, i64, f32, f64);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
