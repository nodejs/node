// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/ostreams.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-run-utils.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm {

class Float32CrossCompilerDeterminismTest
    : public fuzztest::PerFuzzTestFixtureAdapter<TestWithNativeContext> {
 public:
  Float32CrossCompilerDeterminismTest() = default;

  void TestBinOp(WasmOpcode opcode, Float32 lhs, Float32 rhs);

  void TestUnOp(WasmOpcode opcode, Float32 input);

 private:
  // IntRepresentation and FloatRepresentation allow to parameterize the test
  // for either passing integer values or floating point values.
  struct IntRepresentation {
    using c_type = uint32_t;
    static c_type FromFloat32(Float32 val) { return val.get_bits(); }
    static Float32 ToFloat32(c_type val) { return Float32::FromBits(val); }

    static constexpr std::array<uint8_t, 1> to_f32{kExprF32ReinterpretI32};
    static constexpr std::array<uint8_t, 1> from_f32{kExprI32ReinterpretF32};

    static auto Constant(Float32 value) {
      return std::to_array<uint8_t>({WASM_I32V(value.get_bits())});
    }
    static auto Param(int param_idx) {
      return std::to_array<uint8_t>({WASM_LOCAL_GET(param_idx)});
    }
  };
  struct FloatRepresentation {
    using c_type = float;
    static c_type FromFloat32(Float32 val) { return val.get_scalar(); }
    static Float32 ToFloat32(c_type val) {
      return Float32::FromNonSignallingFloat(val);
    }

    static constexpr std::array<uint8_t, 0> to_f32{};
    static constexpr std::array<uint8_t, 0> from_f32{};

    static auto Constant(Float32 value) {
      return std::to_array<uint8_t>({WASM_F32(value.get_scalar())});
    }
    static auto Param(int param_idx) {
      return std::to_array<uint8_t>({WASM_LOCAL_GET(param_idx)});
    }
  };

  // Those structs allow to reuse the same test logic for passing or embedding
  // values differently (constants or parameters or mixed).
  template <typename ValueRepresentation>
  struct ConstantInputs : public ValueRepresentation {
    using c_type = ValueRepresentation::c_type;
    using runner_type = CommonWasmRunner<c_type>;

    template <int param_idx>
    static auto GetParam(Float32 value) {
      return ValueRepresentation::Constant(value);
    }

    // This can be called with one or two parameters (for unops or binops).
    template <typename... Args>
      requires(std::is_same_v<Args, c_type> && ...)
    static auto Call(runner_type& runner, Args...) {
      return runner.Call();
    }
  };
  template <typename ValueRepresentation, int kIndexOfParam>
  struct ParamAndConstantInput : public ValueRepresentation {
    using c_type = ValueRepresentation::c_type;
    using runner_type = CommonWasmRunner<c_type, c_type>;

    template <int param_idx>
    static auto GetParam(Float32 value) {
      if constexpr (param_idx == kIndexOfParam) {
        return ValueRepresentation::Param(0);
      } else {
        return ValueRepresentation::Constant(value);
      }
    }

    static auto Call(runner_type& runner, c_type lhs, c_type rhs) {
      return runner.Call(kIndexOfParam == 0 ? lhs : rhs);
    }
  };
  template <typename ValueRepresentation>
  struct TwoParamInputs : public ValueRepresentation {
    using c_type = ValueRepresentation::c_type;
    using runner_type = CommonWasmRunner<c_type, c_type, c_type>;

    template <int param_idx>
    static auto GetParam(Float32 value) {
      return ValueRepresentation::Param(param_idx);
    }

    static auto Call(runner_type& runner, c_type lhs, c_type rhs) {
      return runner.Call(lhs, rhs);
    }
  };
  template <typename ValueRepresentation>
  struct OneParamInput : public ValueRepresentation {
    using c_type = ValueRepresentation::c_type;
    using runner_type = CommonWasmRunner<c_type, c_type>;

    template <int param_idx>
    static auto GetParam(Float32 value) {
      return ValueRepresentation::Param(param_idx);
    }

    static auto Call(runner_type& runner, c_type input) {
      return runner.Call(input);
    }
  };

  template <typename Config>
  Float32 GetBinOpResult(TestExecutionTier tier, WasmOpcode opcode, Float32 lhs,
                         Float32 rhs) {
    // Instantiate the WasmRunner of the right type.
    typename Config::runner_type runner(isolate(), tier);
    constexpr size_t kMaxBytecodeSize = 16;
    // Build the bytecode.
    base::SmallVector<uint8_t, kMaxBytecodeSize> bytecode;
    bytecode.insert(bytecode.end(), Config::template GetParam<0>(lhs));
    bytecode.insert(bytecode.end(), Config::to_f32);
    bytecode.insert(bytecode.end(), Config::template GetParam<1>(rhs));
    bytecode.insert(bytecode.end(), Config::to_f32);
    bytecode.push_back(opcode);
    bytecode.insert(bytecode.end(), Config::from_f32);

    // Compile.
    DCHECK_GE(kMaxBytecodeSize, bytecode.size());
    runner.Build(base::VectorOf(bytecode));

    // Convert arguments from Float32, call Wasm, convert result to Float32.
    return Config::ToFloat32(Config::Call(runner, Config::FromFloat32(lhs),
                                          Config::FromFloat32(rhs)));
  }

  template <typename Config>
  Float32 GetUnOpResult(TestExecutionTier tier, WasmOpcode opcode,
                        Float32 input) {
    // Instantiate the WasmRunner of the right type.
    typename Config::runner_type runner(isolate(), tier);
    constexpr size_t kMaxBytecodeSize = 10;
    // Build the bytecode.
    base::SmallVector<uint8_t, kMaxBytecodeSize> bytecode;
    bytecode.insert(bytecode.end(), Config::template GetParam<0>(input));
    bytecode.insert(bytecode.end(), Config::to_f32);
    bytecode.push_back(opcode);
    bytecode.insert(bytecode.end(), Config::from_f32);

    // Compile.
    DCHECK_GE(kMaxBytecodeSize, bytecode.size());
    runner.Build(base::VectorOf(bytecode));

    // Convert argument from Float32, call Wasm, convert result to Float32.
    return Config::ToFloat32(Config::Call(runner, Config::FromFloat32(input)));
  }
};

inline bool AllResultsEqual(base::Vector<const Float32> results) {
  DCHECK(!results.empty());
  auto equals_first = [first = results[0]](Float32 v) { return v == first; };
  return std::all_of(results.begin() + 1, results.end(), equals_first);
}

void Float32CrossCompilerDeterminismTest::TestBinOp(WasmOpcode opcode,
                                                    Float32 lhs, Float32 rhs) {
  // For some binops, the bit pattern of one of the inputs is preserved,
  // including the signalling bit.
  // Those tests cannot run in the configurations that pass the input as
  // floating point numbers.
  bool preserve_signalling_nan =
      lhs.is_nan() && !lhs.is_quiet_nan() && opcode == kExprF32CopySign;
  bool has_signalling_nan_and_nan_inputs =
      lhs.is_nan() && rhs.is_nan() &&
      (!lhs.is_quiet_nan() || !rhs.is_quiet_nan());

  // Remember all values from all configurations.
  base::SmallVector<Float32, 9> results = {
      // Liftoff does not special-handle float32 constants, so we only test this
      // one Liftoff mode.
      GetBinOpResult<ConstantInputs<IntRepresentation>>(
          TestExecutionTier::kLiftoff, opcode, lhs, rhs),

      // Turbofan, inputs as i32.
      GetBinOpResult<ConstantInputs<IntRepresentation>>(
          TestExecutionTier::kTurbofan, opcode, lhs, rhs),
      GetBinOpResult<ParamAndConstantInput<IntRepresentation, 0>>(
          TestExecutionTier::kTurbofan, opcode, lhs, rhs),
      GetBinOpResult<ParamAndConstantInput<IntRepresentation, 1>>(
          TestExecutionTier::kTurbofan, opcode, lhs, rhs),
      GetBinOpResult<TwoParamInputs<IntRepresentation>>(
          TestExecutionTier::kTurbofan, opcode, lhs, rhs),
  };

  if (preserve_signalling_nan) {
    ASSERT_TRUE(results[0].is_nan() && !results[0].is_quiet_nan());
  } else {
    // Turbofan, inputs as f32.
    results.push_back(GetBinOpResult<ConstantInputs<FloatRepresentation>>(
        TestExecutionTier::kTurbofan, opcode, lhs, rhs));
    // Combining NaN and SNaN or having both inputs as SNaN could have
    // different results when passed as f32. JSToWasmWrapper currently uses
    // WasmTaggedToFloat32 to pass FP parameters which may turn SNaN params to
    // QNaN which may change the final result.
    if (!has_signalling_nan_and_nan_inputs) {
      results.push_back(
          GetBinOpResult<ParamAndConstantInput<FloatRepresentation, 0>>(
              TestExecutionTier::kTurbofan, opcode, lhs, rhs));
      results.push_back(
          GetBinOpResult<ParamAndConstantInput<FloatRepresentation, 1>>(
              TestExecutionTier::kTurbofan, opcode, lhs, rhs));
      results.push_back(GetBinOpResult<TwoParamInputs<FloatRepresentation>>(
          TestExecutionTier::kTurbofan, opcode, lhs, rhs));
    }
  }

  ASSERT_TRUE(AllResultsEqual(base::VectorOf(results)))
      << absl::StrFormat("Operation %s on inputs %v and %v\n",
                         WasmOpcodes::OpcodeName(opcode), lhs, rhs)
      << "Different results for different configs: "
      << PrintCollection(base::VectorOf(results));
}

void Float32CrossCompilerDeterminismTest::TestUnOp(WasmOpcode opcode,
                                                   Float32 input) {
  // Some unops (neg and abs) do preserve the bit pattern, including the
  // signalling bit.
  // Those tests cannot run in the configurations that pass the input as
  // floating point numbers.
  bool preserve_signalling_nan =
      input.is_nan() && !input.is_quiet_nan() &&
      (opcode == kExprF32Neg || opcode == kExprF32Abs);

  base::SmallVector<Float32, 5> results = {
      // Liftoff does not special-handle float32 constants, so we only test this
      // one Liftoff mode.
      GetUnOpResult<ConstantInputs<IntRepresentation>>(
          TestExecutionTier::kLiftoff, opcode, input),

      // Turbofan, inputs as i32.
      GetUnOpResult<ConstantInputs<IntRepresentation>>(
          TestExecutionTier::kTurbofan, opcode, input),
      GetUnOpResult<OneParamInput<IntRepresentation>>(
          TestExecutionTier::kTurbofan, opcode, input)};
  if (preserve_signalling_nan) {
    ASSERT_TRUE(results[0].is_nan() && !results[0].is_quiet_nan());
  } else {
    // Turbofan, inputs as f32.
    results.push_back(GetUnOpResult<ConstantInputs<FloatRepresentation>>(
        TestExecutionTier::kTurbofan, opcode, input));
    results.push_back(GetUnOpResult<OneParamInput<FloatRepresentation>>(
        TestExecutionTier::kTurbofan, opcode, input));
  };

  ASSERT_TRUE(AllResultsEqual(base::VectorOf(results)))
      << absl::StrFormat("Operation %s on input %v\n",
                         WasmOpcodes::OpcodeName(opcode), input)
      << "Different results for different configs: "
      << PrintCollection(base::VectorOf(results));
}

inline fuzztest::Domain<Float32> ArbitraryFloat32() {
  return fuzztest::ReversibleMap(
      // Mapping function.
      [](uint32_t bits) { return Float32::FromBits(bits); },
      // Inverse mapping function, for mapping back initial seeds.
      [](Float32 f) { return std::optional{std::tuple{f.get_bits()}}; },
      // Input to the mapping function.
      fuzztest::Arbitrary<uint32_t>());
}

constexpr std::array kBinOps = {kExprF32Add,     kExprF32Sub, kExprF32Mul,
                                kExprF32Div,     kExprF32Min, kExprF32Max,
                                kExprF32CopySign};
// Check that we covered all float32 binops.
#define CHECK_FLOAT32_BINOP(name, opcode, sig, ...)   \
  static_assert((std::string_view(#sig) == "f_ff") == \
                std::count(kBinOps.begin(), kBinOps.end(), kExpr##name));
FOREACH_SIMPLE_OPCODE(CHECK_FLOAT32_BINOP)
#undef CHECK_FLOAT32_BINOP

constexpr std::tuple<WasmOpcode, Float32, Float32> kBinOpSeeds[]{
    // NaN + NaN
    {kExprF32Add, Float32::quiet_nan(), Float32::quiet_nan()},
    // NaN + -NaN
    {kExprF32Add, Float32::quiet_nan(), -Float32::quiet_nan()},
    // inf + -inf
    {kExprF32Add, Float32::infinity(), -Float32::infinity()},
    // inf - inf
    {kExprF32Sub, Float32::infinity(), Float32::infinity()},
    // 0 * inf
    {kExprF32Mul, Float32::FromBits(0), Float32::infinity()},
    // -0 / 0
    {kExprF32Div, -Float32::FromBits(0), Float32::FromBits(0)},
    // -NaN / -0
    {kExprF32Div, -Float32::quiet_nan(), -Float32::FromBits(0)},
    // -inf / inf
    {kExprF32Div, -Float32::infinity(), Float32::infinity()},
    // min(NaN, -NaN)
    {kExprF32Min, Float32::quiet_nan(), -Float32::quiet_nan()},
    // copysign(-signalling NaN, epsilon)
    {kExprF32CopySign, Float32::FromBits(0xff97750d),
     Float32::FromBits(0x0a004929)},
};

constexpr std::array kUnOps = {kExprF32Abs,   kExprF32Neg,   kExprF32Ceil,
                               kExprF32Floor, kExprF32Trunc, kExprF32NearestInt,
                               kExprF32Sqrt};

// Check that we covered all float32 unops.
#define CHECK_FLOAT32_UNOP(name, opcode, sig, ...)   \
  static_assert((std::string_view(#sig) == "f_f") == \
                std::count(kUnOps.begin(), kUnOps.end(), kExpr##name));
FOREACH_SIMPLE_OPCODE(CHECK_FLOAT32_UNOP)
#undef CHECK_FLOAT32_UNOP

constexpr std::tuple<WasmOpcode, Float32> kUnOpSeeds[]{
    // sqrt(NaN)
    {kExprF32Sqrt, Float32::quiet_nan()},
    // neg(signalling NaN)
    {kExprF32Neg, Float32::FromBits(0x7fbefebe)},
};

V8_FUZZ_TEST_F(Float32CrossCompilerDeterminismTest, TestBinOp)
    .WithDomains(
        // opcode
        fuzztest::ElementOf<WasmOpcode>(kBinOps),
        // lhs
        ArbitraryFloat32(),
        // rhs
        ArbitraryFloat32())
    .WithSeeds(kBinOpSeeds);

V8_FUZZ_TEST_F(Float32CrossCompilerDeterminismTest, TestUnOp)
    .WithDomains(
        // opcode
        fuzztest::ElementOf<WasmOpcode>(kUnOps),
        // input
        ArbitraryFloat32())
    .WithSeeds(kUnOpSeeds);

}  // namespace v8::internal::wasm
