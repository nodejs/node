// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/vector.h"
#include "src/common/globals.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/dead-code-elimination-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/wasm-shuffle-reducer.h"
#include "test/common/flag-utils.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class WasmSimdTest : public ReducerTest {};

TEST_F(WasmSimdTest, UpperToLowerF32x4AddReduce) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto SplatKind = Simd128SplatOp::Kind::kF32x4;
    auto AddKind = Simd128BinopOp::Kind::kF32x4Add;
    auto ExtractKind = Simd128ExtractLaneOp::Kind::kF32x4;
    auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;

    constexpr uint8_t upper_to_lower_1[kSimd128Size] = {
        8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr uint8_t upper_to_lower_2[kSimd128Size] = {4, 5, 6, 7, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};

    V<Simd128> input = __ Simd128Splat(__ Float32Constant(1.0), SplatKind);
    V<Simd128> first_shuffle =
        __ Simd128Shuffle(input, input, ShuffleKind, upper_to_lower_1);
    V<Simd128> first_add = __ Simd128Binop(input, first_shuffle, AddKind);
    V<Simd128> second_shuffle =
        __ Simd128Shuffle(first_add, first_add, ShuffleKind, upper_to_lower_2);
    V<Simd128> second_add = __ Simd128Binop(first_add, second_shuffle, AddKind);
    __ Return(__ Simd128ExtractLane(second_add, ExtractKind, 0));
  });

  test.Run<MachineOptimizationReducer>();
  test.Run<DeadCodeEliminationReducer>();
  // We can only match pairwise fp operations.
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 0u);
}

TEST_F(WasmSimdTest, AlmostUpperToLowerI16x8AddReduce) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto SplatKind = Simd128SplatOp::Kind::kI16x8;
    auto AddKind = Simd128BinopOp::Kind::kI16x8Add;
    auto ExtractKind = Simd128ExtractLaneOp::Kind::kI16x8U;
    auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;

    constexpr uint8_t almost_upper_to_lower_1[kSimd128Size] = {
        0, 0, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0,
    };
    constexpr uint8_t upper_to_lower_2[kSimd128Size] = {4, 5, 6, 7, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};
    constexpr uint8_t upper_to_lower_3[kSimd128Size] = {2, 3, 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};

    V<Simd128> input = __ Simd128Splat(Asm.GetParameter(0), SplatKind);
    V<Simd128> first_shuffle =
        __ Simd128Shuffle(input, input, ShuffleKind, almost_upper_to_lower_1);
    V<Simd128> first_add = __ Simd128Binop(input, first_shuffle, AddKind);
    V<Simd128> second_shuffle =
        __ Simd128Shuffle(first_add, first_add, ShuffleKind, upper_to_lower_2);
    V<Simd128> second_add = __ Simd128Binop(first_add, second_shuffle, AddKind);
    V<Simd128> third_shuffle = __ Simd128Shuffle(second_add, second_add,
                                                 ShuffleKind, upper_to_lower_3);
    V<Simd128> third_add = __ Simd128Binop(second_add, third_shuffle, AddKind);
    __ Return(__ Simd128ExtractLane(third_add, ExtractKind, 0));
  });

  test.Run<MachineOptimizationReducer>();
  test.Run<DeadCodeEliminationReducer>();

  // The first shuffle is not the one we're looking for.
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 0u);
}

TEST_F(WasmSimdTest, UpperToLowerI32x4AddReduce) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto SplatKind = Simd128SplatOp::Kind::kI32x4;
    auto AddKind = Simd128BinopOp::Kind::kI32x4Add;
    auto ExtractKind = Simd128ExtractLaneOp::Kind::kI32x4;
    auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;

    constexpr uint8_t upper_to_lower_1[kSimd128Size] = {
        8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr uint8_t upper_to_lower_2[kSimd128Size] = {4, 5, 6, 7, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};

    V<Simd128> input = __ Simd128Splat(Asm.GetParameter(0), SplatKind);
    V<Simd128> first_shuffle =
        __ Simd128Shuffle(input, input, ShuffleKind, upper_to_lower_1);
    V<Simd128> first_add = __ Simd128Binop(input, first_shuffle, AddKind);
    V<Simd128> second_shuffle =
        __ Simd128Shuffle(first_add, first_add, ShuffleKind, upper_to_lower_2);
    V<Simd128> second_add = __ Simd128Binop(first_add, second_shuffle, AddKind);
    __ Return(__ Simd128ExtractLane(second_add, ExtractKind, 0));
  });

  test.Run<MachineOptimizationReducer>();
  test.Run<DeadCodeEliminationReducer>();

#ifdef V8_TARGET_ARCH_ARM64
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Shuffle), 0u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128ExtractLane), 1u);
#else
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 0u);
#endif
}

TEST_F(WasmSimdTest, PairwiseF32x4AddReduce) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto SplatKind = Simd128SplatOp::Kind::kF32x4;
    auto AddKind = Simd128BinopOp::Kind::kF32x4Add;
    auto ExtractKind = Simd128ExtractLaneOp::Kind::kF32x4;
    auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;

    constexpr uint8_t upper_to_lower_1[kSimd128Size] = {
        4, 5, 6, 7, 0, 0, 0, 0, 12, 13, 14, 15, 0, 0, 0, 0};
    constexpr uint8_t upper_to_lower_2[kSimd128Size] = {
        8, 9, 10, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    V<Simd128> input = __ Simd128Splat(__ Float32Constant(1.0), SplatKind);
    V<Simd128> first_shuffle =
        __ Simd128Shuffle(input, input, ShuffleKind, upper_to_lower_1);
    V<Simd128> first_add = __ Simd128Binop(input, first_shuffle, AddKind);
    V<Simd128> second_shuffle =
        __ Simd128Shuffle(first_add, first_add, ShuffleKind, upper_to_lower_2);
    V<Simd128> second_add = __ Simd128Binop(first_add, second_shuffle, AddKind);
    __ Return(__ Simd128ExtractLane(second_add, ExtractKind, 0));
  });

  test.Run<MachineOptimizationReducer>();
  test.Run<DeadCodeEliminationReducer>();

#ifdef V8_TARGET_ARCH_ARM64
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Shuffle), 0u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128ExtractLane), 1u);
#else
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 0u);
#endif
}

TEST_F(WasmSimdTest, AlmostPairwiseF32x4AddReduce) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto SplatKind = Simd128SplatOp::Kind::kF32x4;
    auto AddKind = Simd128BinopOp::Kind::kF32x4Add;
    auto ExtractKind = Simd128ExtractLaneOp::Kind::kF32x4;
    auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;

    constexpr uint8_t upper_to_lower_1[kSimd128Size] = {
        4, 5, 6, 7, 0, 0, 0, 0, 12, 13, 14, 15, 0, 0, 0, 0};
    constexpr uint8_t upper_to_lower_2[kSimd128Size] = {
        8, 9, 10, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    V<Simd128> input = __ Simd128Splat(__ Float32Constant(1.0), SplatKind);
    V<Simd128> first_shuffle =
        __ Simd128Shuffle(input, input, ShuffleKind, upper_to_lower_1);
    V<Simd128> first_add = __ Simd128Binop(input, first_shuffle, AddKind);
    V<Simd128> second_shuffle =
        __ Simd128Shuffle(first_add, first_add, ShuffleKind, upper_to_lower_2);
    V<Simd128> tricksy_add = __ Simd128Binop(first_add, first_add, AddKind);
    V<Simd128> second_add =
        __ Simd128Binop(tricksy_add, second_shuffle, AddKind);
    __ Return(__ Simd128ExtractLane(second_add, ExtractKind, 0));
  });

  test.Run<MachineOptimizationReducer>();
  test.Run<DeadCodeEliminationReducer>();

  // There's an additional addition.
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Reduce), 0u);
}

TEST_F(WasmSimdTest, SingleExtMulI8Dot) {
  std::array converts = {
      Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
      Simd128UnaryOp::Kind::kI16x8SConvertI8x16High,
      Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low,
      Simd128UnaryOp::Kind::kI16x8UConvertI8x16High,
  };

  for (Simd128UnaryOp::Kind unop_kind : converts) {
    auto test = CreateFromGraph(1, [&unop_kind](auto& Asm) {
      constexpr auto SplatKind = Simd128SplatOp::Kind::kI8x16;
      V<Simd128> input = __ Simd128Splat(__ Word32Constant(16), SplatKind);
      V<Simd128> left = __ Simd128Unary(input, unop_kind);
      V<Simd128> right = __ Simd128Unary(input, unop_kind);
      __ Return(
          __ Simd128Binop(left, right, Simd128BinopOp::Kind::kI32x4DotI16x8S));
    });

    test.Run<MachineOptimizationReducer>();
    test.Run<DeadCodeEliminationReducer>();

    ASSERT_EQ(test.CountOp(Opcode::kSimd128Unary), 1u);
    ASSERT_EQ(test.CountOp(Opcode::kSimd128Binop), 1u);
  }
}

TEST_F(WasmSimdTest, AddPairwiseMulI8Dot) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    constexpr auto SplatKind = Simd128SplatOp::Kind::kI8x16;
    V<Simd128> input = __ Simd128Splat(__ Word32Constant(16), SplatKind);
    V<Simd128> left =
        __ Simd128Unary(input, Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low);
    V<Simd128> right =
        __ Simd128Unary(input, Simd128UnaryOp::Kind::kI16x8UConvertI8x16High);
    __ Return(
        __ Simd128Binop(left, right, Simd128BinopOp::Kind::kI32x4DotI16x8S));
  });

  test.Run<MachineOptimizationReducer>();
  test.Run<DeadCodeEliminationReducer>();

#ifdef V8_TARGET_ARCH_ARM64
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Unary), 2u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Binop), 3u);
#else
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Unary), 2u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Binop), 1u);
#endif
}

#ifdef V8_ENABLE_WASM_SIMD256_REVEC

TEST_F(WasmSimdTest, Simd256Extract128Lane_ConstantFolding) {
  auto test = CreateFromGraph(0, [](auto& Asm) {
    const uint8_t cst[kSimd256Size] = {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    V<Simd256> simd256_cst = __ Simd256Constant(cst);

    V<Simd128> low = __ Simd256Extract128Lane(simd256_cst, 0);
    DCHECK(__ output_graph().Get(low).template Is<Simd256Extract128LaneOp>());
    Asm.Capture(low, "low");

    V<Simd128> high = __ Simd256Extract128Lane(simd256_cst, 1);
    DCHECK(__ output_graph().Get(high).template Is<Simd256Extract128LaneOp>());
    Asm.Capture(high, "high");

    __ Return(__ Simd128Binop(high, low, Simd128BinopOp::Kind::kF16x8Add));
  });

  // Running MachineOptimizationReduce to optimize the Extract.
  test.Run<MachineOptimizationReducer>();
  // Running an empty phase to remove the unused operations.
  test.Run<>();

  // The Simd256Constant and the Extract should have been eliminated.
  ASSERT_EQ(test.CountOp(Opcode::kSimd256Constant), 0u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd256Extract128Lane), 0u);

  // Testing that `low` and `high` have the right value.
  const uint8_t expected_low[kSimd128Size] = {0, 1, 2,  3,  4,  5,  6,  7,
                                              8, 9, 10, 11, 12, 13, 14, 15};
  const uint8_t expected_high[kSimd128Size] = {16, 17, 18, 19, 20, 21, 22, 23,
                                               24, 25, 26, 27, 28, 29, 30, 31};

  const uint8_t* actual_low =
      test.GetCapture("low").GetAs<Simd128ConstantOp>()->value;
  const uint8_t* actual_high =
      test.GetCapture("high").GetAs<Simd128ConstantOp>()->value;

  for (int i = 0; i < kSimd128Size; i++) {
    ASSERT_EQ(expected_low[i], actual_low[i]);
    ASSERT_EQ(expected_high[i], actual_high[i]);
  }
}

#endif

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
