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

  void TestShuffleTree(WasmOpcode binop, WasmOpcode unop,
                       std::array<Simd128, 4> inputs,
                       std::array<uint8_t, kSimd128Size> shuffle0,
                       std::array<uint8_t, kSimd128Size> shuffle1,
                       std::array<uint8_t, kSimd128Size> shuffle2,
                       std::array<uint8_t, kSimd128Size> shuffle3,
                       std::array<uint8_t, kSimd128Size> shuffle4,
                       std::array<uint8_t, kSimd128Size> shuffle5,
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

  // Test shuffle patterns.
  template <typename Config>
  Simd128 GetShuffleTreeResult(TestExecutionTier tier, WasmOpcode binop,
                               WasmOpcode unop, std::array<Simd128, 4> inputs,
                               std::array<uint8_t, kSimd128Size> shuffle0,
                               std::array<uint8_t, kSimd128Size> shuffle1,
                               std::array<uint8_t, kSimd128Size> shuffle2,
                               std::array<uint8_t, kSimd128Size> shuffle3,
                               std::array<uint8_t, kSimd128Size> shuffle4,
                               std::array<uint8_t, kSimd128Size> shuffle5,
                               int preconsumed_liftoff_regs = 0) {
    // Instantiate a runner with some memory for storing dynamic inputs and the
    // result.
    CommonWasmRunner<void> runner(isolate(), tier);
    Simd128* memory = runner.builder().AddMemoryElems<Simd128>(8);

    // Build the bytecode.
    // Note: `kMaxBytecodeSize` is just big enough to hold all bytes we generate
    // below across all configuration. If the DCHECK below fails, increase it as
    // necessary.
    constexpr size_t kMaxBytecodeSize = 500 + 19 * kMaxPreconsumedLiftoffRegs;
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

    // x0 = shuffle(a, b)
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<0>(memory, inputs[0]));
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<1>(memory, inputs[1]));
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprI8x16Shuffle)});
    bytecode.insert(bytecode.end(), shuffle0.begin(), shuffle0.end());

    // y0 = shuffle(c, d)
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<2>(memory, inputs[2]));
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<3>(memory, inputs[3]));
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprI8x16Shuffle)});
    bytecode.insert(bytecode.end(), shuffle1.begin(), shuffle1.end());

    // z0 = shuffle(x0, y0)
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprI8x16Shuffle)});
    bytecode.insert(bytecode.end(), shuffle2.begin(), shuffle2.end());

    // x1 = shuffle(b, c)
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<1>(memory, inputs[1]));
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<2>(memory, inputs[2]));
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprI8x16Shuffle)});
    bytecode.insert(bytecode.end(), shuffle0.begin(), shuffle0.end());

    // y1 = shuffle(d, a)
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<3>(memory, inputs[3]));
    bytecode.insert(bytecode.end(),
                    Config::template GetInput<0>(memory, inputs[0]));
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprI8x16Shuffle)});
    bytecode.insert(bytecode.end(), shuffle1.begin(), shuffle1.end());

    // z1 = shuffle(x1, y1)
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(kExprI8x16Shuffle)});
    bytecode.insert(bytecode.end(), shuffle5.begin(), shuffle5.end());

    // unop(binop(z0, z1));
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(binop)});
    bytecode.insert(bytecode.end(), {WASM_SIMD_OP(unop)});

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

template <typename Size>
inline bool AllResultsEqual(base::Vector<const Size> results) {
  DCHECK(!results.empty());
  auto equals_first = [first = results[0]](Size v) { return v == first; };
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

  ASSERT_TRUE(AllResultsEqual<Simd128>(base::VectorOf(results)))
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

void SimdCrossCompilerDeterminismTest::TestShuffleTree(
    WasmOpcode binop, WasmOpcode unop, std::array<Simd128, 4> inputs,
    std::array<uint8_t, kSimd128Size> shuffle0,
    std::array<uint8_t, kSimd128Size> shuffle1,
    std::array<uint8_t, kSimd128Size> shuffle2,
    std::array<uint8_t, kSimd128Size> shuffle3,
    std::array<uint8_t, kSimd128Size> shuffle4,
    std::array<uint8_t, kSimd128Size> shuffle5, int preconsumed_liftoff_regs,
    bool allow_avx) {
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
  bool disable_avx = !allow_avx && CpuFeatures::IsSupported(AVX);
  if (disable_avx) CpuFeatures::SetUnsupported(AVX);
#endif  // V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64

  // Remember all values from all configurations.
  Simd128 results[] = {
      // Liftoff does not special-handle SIMD constants, so we only test this
      // one Liftoff mode (but with a dynamic amount of preconsumed registers).
      GetShuffleTreeResult<
          InputLocations<kConstant, kConstant, kConstant, kConstant>>(
          TestExecutionTier::kLiftoff, binop, unop, inputs, shuffle0, shuffle1,
          shuffle2, shuffle3, shuffle4, shuffle5, preconsumed_liftoff_regs),

      // Different Turbofan configs, embedding inputs as constants or having
      // dynamic inputs.
      // - input constant
      GetShuffleTreeResult<
          InputLocations<kConstant, kConstant, kConstant, kConstant>>(
          TestExecutionTier::kTurbofan, binop, unop, inputs, shuffle0, shuffle1,
          shuffle2, shuffle3, shuffle4, shuffle5),
      // - input dynamic
      GetShuffleTreeResult<
          InputLocations<kDynamic, kDynamic, kDynamic, kDynamic, kDynamic>>(
          TestExecutionTier::kTurbofan, binop, unop, inputs, shuffle0, shuffle1,
          shuffle2, shuffle3, shuffle4, shuffle5)};

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
  if (disable_avx) CpuFeatures::SetSupported(AVX);
#endif  // V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64

  ASSERT_TRUE(AllResultsEqual<Simd128>(base::VectorOf(results)))
      << absl::StrFormat("Shuffles: %v, %v, %v, %v, %vand %v\n",
                         *reinterpret_cast<Simd128*>(shuffle0.data()),
                         *reinterpret_cast<Simd128*>(shuffle1.data()),
                         *reinterpret_cast<Simd128*>(shuffle2.data()),
                         *reinterpret_cast<Simd128*>(shuffle3.data()),
                         *reinterpret_cast<Simd128*>(shuffle4.data()),
                         *reinterpret_cast<Simd128*>(shuffle5.data()))
      << "Different results for different configs: "
      << PrintCollection(base::VectorOf(results));
}

constexpr std::array kBinOps = {
    kExprI8x16Eq,
    kExprI8x16Ne,
    kExprI8x16LtS,
    kExprI8x16LtU,
    kExprI8x16GtS,
    kExprI8x16GtU,
    kExprI8x16LeS,
    kExprI8x16LeU,
    kExprI8x16GeS,
    kExprI8x16GeU,
    kExprI16x8Eq,
    kExprI16x8Ne,
    kExprI16x8LtS,
    kExprI16x8LtU,
    kExprI16x8GtS,
    kExprI16x8GtU,
    kExprI16x8LeS,
    kExprI16x8LeU,
    kExprI16x8GeS,
    kExprI16x8GeU,
    kExprI32x4Eq,
    kExprI32x4Ne,
    kExprI32x4LtS,
    kExprI32x4LtU,
    kExprI32x4GtS,
    kExprI32x4GtU,
    kExprI32x4LeS,
    kExprI32x4LeU,
    kExprI32x4GeS,
    kExprI32x4GeU,
    kExprF32x4Eq,
    kExprF32x4Ne,
    kExprF32x4Lt,
    kExprF32x4Gt,
    kExprF32x4Le,
    kExprF32x4Ge,
    kExprF64x2Eq,
    kExprF64x2Ne,
    kExprF64x2Lt,
    kExprF64x2Gt,
    kExprF64x2Le,
    kExprF64x2Ge,
    kExprS128And,
    kExprS128AndNot,
    kExprS128Or,
    kExprS128Xor,
    kExprI8x16Add,
    kExprI8x16AddSatS,
    kExprI8x16AddSatU,
    kExprI8x16Sub,
    kExprI8x16SubSatS,
    kExprI8x16SubSatU,
    kExprI8x16MinS,
    kExprI8x16MinU,
    kExprI8x16MaxS,
    kExprI8x16MaxU,
    kExprI8x16RoundingAverageU,
    kExprI8x16SConvertI16x8,
    kExprI8x16UConvertI16x8,
    kExprI16x8Q15MulRSatS,
    kExprI16x8Add,
    kExprI16x8AddSatS,
    kExprI16x8AddSatU,
    kExprI16x8Sub,
    kExprI16x8SubSatS,
    kExprI16x8SubSatU,
    kExprI16x8Mul,
    kExprI16x8MinS,
    kExprI16x8MinU,
    kExprI16x8MaxS,
    kExprI16x8MaxU,
    kExprI16x8RoundingAverageU,
    kExprI16x8ExtMulLowI8x16S,
    kExprI16x8ExtMulHighI8x16S,
    kExprI16x8ExtMulLowI8x16U,
    kExprI16x8ExtMulHighI8x16U,
    kExprI16x8SConvertI32x4,
    kExprI16x8UConvertI32x4,
    kExprI32x4Add,
    kExprI32x4Sub,
    kExprI32x4Mul,
    kExprI32x4MinS,
    kExprI32x4MinU,
    kExprI32x4MaxS,
    kExprI32x4MaxU,
    kExprI32x4DotI16x8S,
    kExprI32x4ExtMulLowI16x8S,
    kExprI32x4ExtMulHighI16x8S,
    kExprI32x4ExtMulLowI16x8U,
    kExprI32x4ExtMulHighI16x8U,
    kExprI64x2Add,
    kExprI64x2Sub,
    kExprI64x2Mul,
    kExprI64x2Eq,
    kExprI64x2Ne,
    kExprI64x2LtS,
    kExprI64x2GtS,
    kExprI64x2LeS,
    kExprI64x2GeS,
    kExprI64x2ExtMulLowI32x4S,
    kExprI64x2ExtMulHighI32x4S,
    kExprI64x2ExtMulLowI32x4U,
    kExprI64x2ExtMulHighI32x4U,
    kExprF32x4Add,
    kExprF32x4Sub,
    kExprF32x4Mul,
    kExprF32x4Min,
    kExprF32x4Max,
    kExprF32x4Pmin,
    kExprF32x4Pmax,
    kExprF64x2Add,
    kExprF64x2Sub,
    kExprF64x2Mul,
    kExprF64x2Min,
    kExprF64x2Max,
    kExprF64x2Pmin,
    kExprF64x2Pmax,
};

constexpr std::array kUnOps = {
    kExprS128Not,
    kExprF32x4DemoteF64x2Zero,
    kExprF64x2PromoteLowF32x4,
    kExprI8x16Abs,
    kExprI8x16Neg,
    kExprI8x16Popcnt,
    kExprF32x4Ceil,
    kExprF32x4Floor,
    kExprF32x4Trunc,
    kExprF32x4NearestInt,
    kExprF64x2Ceil,
    kExprF64x2Floor,
    kExprF64x2Trunc,
    kExprI16x8ExtAddPairwiseI8x16S,
    kExprI16x8ExtAddPairwiseI8x16U,
    kExprI32x4ExtAddPairwiseI16x8S,
    kExprI32x4ExtAddPairwiseI16x8U,
    kExprI16x8Abs,
    kExprI16x8Neg,
    kExprI16x8SConvertI8x16Low,
    kExprI16x8SConvertI8x16High,
    kExprI16x8UConvertI8x16Low,
    kExprI16x8UConvertI8x16High,
    kExprF64x2NearestInt,
    kExprI32x4Abs,
    kExprI32x4Neg,
    kExprI32x4SConvertI16x8Low,
    kExprI32x4SConvertI16x8High,
    kExprI32x4UConvertI16x8Low,
    kExprI32x4UConvertI16x8High,
    kExprI64x2SConvertI32x4Low,
    kExprI64x2SConvertI32x4High,
    kExprI64x2UConvertI32x4Low,
    kExprI64x2UConvertI32x4High,
    kExprI64x2Abs,
    kExprI64x2Neg,
    kExprF32x4Abs,
    kExprF32x4Neg,
    kExprF32x4Sqrt,
    kExprF64x2Abs,
    kExprF64x2Neg,
    kExprF64x2Sqrt,
    kExprI32x4SConvertF32x4,
    kExprI32x4UConvertF32x4,
    kExprF32x4SConvertI32x4,
    kExprF32x4UConvertI32x4,
    kExprI32x4TruncSatF64x2SZero,
    kExprI32x4TruncSatF64x2UZero,
    kExprF64x2ConvertLowI32x4S,
    kExprF64x2ConvertLowI32x4U,
};

V8_FUZZ_TEST_F(SimdCrossCompilerDeterminismTest, TestShuffleTree)
    .WithDomains(
        // binop
        fuzztest::ElementOf<WasmOpcode>(kBinOps),
        // unop
        fuzztest::ElementOf<WasmOpcode>(kUnOps),
        // inputs
        fuzztest::ArrayOf<4>(ArbitrarySimd()),
        // shuffle 0
        fuzztest::ArrayOf<kSimd128Size>(fuzztest::InRange<uint8_t>(0, 31)),
        // shuffle 1
        fuzztest::ArrayOf<kSimd128Size>(fuzztest::InRange<uint8_t>(0, 31)),
        // shuffle 2
        fuzztest::ArrayOf<kSimd128Size>(fuzztest::InRange<uint8_t>(0, 31)),
        // shuffle 3
        fuzztest::ArrayOf<kSimd128Size>(fuzztest::InRange<uint8_t>(0, 31)),
        // shuffle 4
        fuzztest::ArrayOf<kSimd128Size>(fuzztest::InRange<uint8_t>(0, 31)),
        // shuffle 5
        fuzztest::ArrayOf<kSimd128Size>(fuzztest::InRange<uint8_t>(0, 31)),
        // preconsumed_liftoff_regs
        fuzztest::InRange(
            0, SimdCrossCompilerDeterminismTest::kMaxPreconsumedLiftoffRegs),
        // allow_avx
        fuzztest::Arbitrary<bool>());

}  // namespace v8::internal::wasm
