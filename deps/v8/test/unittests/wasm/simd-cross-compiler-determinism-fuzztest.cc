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

class SimdCrossCompilerDeterminismTest
    : public fuzztest::PerFuzzTestFixtureAdapter<TestWithNativeContext> {
 public:
  SimdCrossCompilerDeterminismTest() = default;

  void TestTernOp(WasmOpcode opcode, std::array<Simd128, 3> inputs,
                  int preconsumed_liftoff_regs, bool allow_avx);

  static constexpr int kMaxPreconsumedLiftoffRegs =
      kFpCacheRegList.GetNumRegsSet();

 private:
  // Those structs allow to reuse the same test logic for passing or embedding
  // values differently (constants or loading from memory or mixed).
  enum InputLocation { kConstant, kDynamic };

  template <size_t N, std::array<InputLocation, N> locations>
  struct InputLocationsImpl {
    template <int param_idx>
    static auto GetInput(Simd128* memory, Simd128 value) {
      if constexpr (locations[param_idx] == kConstant) {
        return std::to_array<uint8_t>({WASM_SIMD_CONSTANT(value.bytes())});
      } else {
        // Write the value to memory and emit code to load it from there.
        memory[param_idx] = value;
        return std::to_array<uint8_t>(
            {WASM_SIMD_LOAD_MEM(WASM_I32V_1(param_idx * sizeof(Simd128)))});
      }
    }
  };

  template <InputLocation... locations>
  using InputLocations =
      InputLocationsImpl<sizeof...(locations),
                         std::array<InputLocation, sizeof...(locations)>{
                             locations...}>;

  template <typename Config>
  Simd128 GetTernOpResult(TestExecutionTier tier, WasmOpcode opcode,
                          std::array<Simd128, 3> inputs,
                          int preconsumed_liftoff_regs = 0) {
    // Instantiate a runner with some memory for storing dynamic inputs and the
    // result.
    CommonWasmRunner<void> runner(isolate(), tier);
    Simd128* memory = runner.builder().AddMemoryElems<Simd128>(3);

    // Build the bytecode.
    // Note: `kMaxBytecodeSize` is just big enough to hold all bytes we generate
    // below across all configuration. If the DCHECK below fails, increase it as
    // necessary.
    constexpr size_t kMaxBytecodeSize = 68 + 19 * kMaxPreconsumedLiftoffRegs;
    base::SmallVector<uint8_t, kMaxBytecodeSize> bytecode;

    // Preconsume some registers.
    DCHECK_LE(preconsumed_liftoff_regs, kMaxPreconsumedLiftoffRegs);
    DCHECK_IMPLIES(tier != TestExecutionTier::kLiftoff,
                   preconsumed_liftoff_regs == 0);
    for (int i = 0; i < preconsumed_liftoff_regs; ++i) {
      bytecode.insert(bytecode.end(), {WASM_SIMD_CONSTANT(Simd128{}.bytes())});
    }

    // Push the memory index for the final store.
    bytecode.insert(bytecode.end(), {WASM_ZERO});
    // Load the three inputs.
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<0>(memory, inputs[0]));
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<1>(memory, inputs[1]));
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<2>(memory, inputs[2]));
    // Emit the SIMD operation.
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(opcode)});
    // Store the result in memory.
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprS128StoreMem),
                                     ZERO_ALIGNMENT, ZERO_OFFSET});
    // Explicitly return (ignoring left-over constants pushed above).
    bytecode.push_back(kExprReturn);

    // Compile.
    DCHECK_GE(kMaxBytecodeSize, bytecode.size());
    runner.Build(base::VectorOf(bytecode));

    // Call Wasm.
    runner.Call();

    // Read result from memory.
    return memory[0];
  }
};

inline bool AllResultsEqual(base::Vector<const Simd128> results) {
  DCHECK(!results.empty());
  auto equals_first = [first = results[0]](Simd128 v) { return v == first; };
  return std::all_of(results.begin() + 1, results.end(), equals_first);
}

void SimdCrossCompilerDeterminismTest::TestTernOp(WasmOpcode opcode,
                                                  std::array<Simd128, 3> inputs,
                                                  int preconsumed_liftoff_regs,
                                                  bool allow_avx) {
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
  bool disable_avx = !allow_avx && CpuFeatures::IsSupported(AVX);
  if (disable_avx) CpuFeatures::SetUnsupported(AVX);
#endif  // V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64

  // Remember all values from all configurations.
  Simd128 results[] = {
      // Liftoff does not special-handle SIMD constants, so we only test this
      // one Liftoff mode (but with a dynamic amount of preconsumed registers).
      GetTernOpResult<InputLocations<kConstant, kConstant, kConstant>>(
          TestExecutionTier::kLiftoff, opcode, inputs,
          preconsumed_liftoff_regs),

      // Different Turbofan configs, embedding inputs as constants or having
      // dynamic inputs.
      // - all inputs constant
      GetTernOpResult<InputLocations<kConstant, kConstant, kConstant>>(
          TestExecutionTier::kTurbofan, opcode, inputs),
      // - first input constant, rest dynamic
      GetTernOpResult<InputLocations<kConstant, kDynamic, kDynamic>>(
          TestExecutionTier::kTurbofan, opcode, inputs),
      // - second input constant, rest dynamic
      GetTernOpResult<InputLocations<kConstant, kDynamic, kConstant>>(
          TestExecutionTier::kTurbofan, opcode, inputs),
      // - third input constant, rest dynamic
      GetTernOpResult<InputLocations<kDynamic, kDynamic, kConstant>>(
          TestExecutionTier::kTurbofan, opcode, inputs),
      // - all inputs dynamic
      GetTernOpResult<InputLocations<kDynamic, kDynamic, kDynamic>>(
          TestExecutionTier::kTurbofan, opcode, inputs)};

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
  if (disable_avx) CpuFeatures::SetSupported(AVX);
#endif  // V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64

  ASSERT_TRUE(AllResultsEqual(base::VectorOf(results)))
      << absl::StrFormat("Operation %s on inputs %v, %v, %v\n",
                         WasmOpcodes::OpcodeName(opcode), inputs[0], inputs[1],
                         inputs[2])
      << "Different results for different configs: "
      << PrintCollection(base::VectorOf(results));
}

inline fuzztest::Domain<Simd128> ArbitrarySimd() {
  return fuzztest::ReversibleMap(
      // Mapping function.
      [](Simd128::int32x4 bits) { return Simd128{bits}; },
      // Inverse mapping function, for mapping back initial seeds.
      [](Simd128 value) { return std::optional{std::tuple{value.to_i32x4()}}; },
      // Input to the mapping function.
      fuzztest::ArrayOf<4>(fuzztest::Arbitrary<int32_t>()));
}

constexpr std::array kTernOps = {kExprS128Select,
                                 kExprF32x4Qfma,
                                 kExprF32x4Qfms,
                                 kExprF64x2Qfma,
                                 kExprF64x2Qfms,
                                 kExprI8x16RelaxedLaneSelect,
                                 kExprI16x8RelaxedLaneSelect,
                                 kExprI32x4RelaxedLaneSelect,
                                 kExprI64x2RelaxedLaneSelect,
                                 kExprI32x4DotI8x16I7x16AddS};

constexpr std::array kSkippedTernOps = {kExprF16x8Qfma, kExprF16x8Qfms};

// Check that FP16 is still experimental; once we ship it we want to include the
// fp16 instructions in `kTernOps`.
#define IS_EXPERIMENTAL_FP16(name, ...) \
  +(std::string_view(#name) == "fp16" ? 1 : 0)
static_assert(FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(IS_EXPERIMENTAL_FP16) == 1,
              "move fp16 instructions to kTernOps before shipping fp16");
#undef IS_EXPERIMENTAL_FP16

// Check that we covered all SIMD ternary opcodes.
#define CHECK_SIMD_TERNOP(name, opcode, sig, ...)                              \
  static_assert((std::string_view(#sig) == "s_sss") ==                         \
                std::count(kTernOps.begin(), kTernOps.end(), kExpr##name) +    \
                    std::count(kSkippedTernOps.begin(), kSkippedTernOps.end(), \
                               kExpr##name));
FOREACH_SIMD_OPCODE(CHECK_SIMD_TERNOP)
#undef CHECK_SIMD_TERNOP

constexpr Simd128 kSimdQuietNanF32WithPayload1 =
    Simd128::Splat(static_cast<int32_t>(0x7fc00002));
constexpr Simd128 kSimdQuietNanF32WithPayload2 =
    Simd128::Splat(static_cast<int32_t>(0x7fc00001));
constexpr Simd128 kSimdQuietNanF64WithPayload1 =
    Simd128::Splat(static_cast<int64_t>(0x7ff8000000000001));
constexpr Simd128 kSimdQuietNanF64WithPayload2 =
    Simd128::Splat(static_cast<int64_t>(0x7ff8000000000002));
constexpr std::tuple<WasmOpcode, std::array<Simd128, 3>, int, bool>
    kTernOpSeeds[]{
        {kExprF32x4Qfms,
         {kSimdQuietNanF32WithPayload1, kSimdQuietNanF32WithPayload2,
          Simd128::Splat(int32_t{1})},
         7 % SimdCrossCompilerDeterminismTest::kMaxPreconsumedLiftoffRegs,
         true},
        {kExprF64x2Qfma,
         {kSimdQuietNanF64WithPayload1, kSimdQuietNanF64WithPayload2,
          Simd128::Splat(int64_t{1})},
         7 % SimdCrossCompilerDeterminismTest::kMaxPreconsumedLiftoffRegs,
         true}};

V8_FUZZ_TEST_F(SimdCrossCompilerDeterminismTest, TestTernOp)
    .WithDomains(
        // opcode
        fuzztest::ElementOf<WasmOpcode>(kTernOps),
        // inputs
        fuzztest::ArrayOf<3>(ArbitrarySimd()),
        // preconsumed_liftoff_regs
        fuzztest::InRange(
            0, SimdCrossCompilerDeterminismTest::kMaxPreconsumedLiftoffRegs),
        // allow_avx
        fuzztest::Arbitrary<bool>())
    .WithSeeds(kTernOpSeeds);

}  // namespace v8::internal::wasm
