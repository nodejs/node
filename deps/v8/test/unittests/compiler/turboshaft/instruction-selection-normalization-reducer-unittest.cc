// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/instruction-selection-normalization-reducer.h"

#include <algorithm>
#include <array>

#include "src/codegen/cpu-features.h"
#include "src/compiler/turboshaft/dead-code-elimination-reducer.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

using InstructionSelectionNormalizationReducerTest = ReducerTest;

#if V8_ENABLE_SIMD128 && defined(V8_TARGET_ARCH_ARM64)
TEST_F(InstructionSelectionNormalizationReducerTest,
       ReduceWideningLoadTransforms) {
  using TransformKind = Simd128LoadTransformOp::TransformKind;
  using UnaryKind = Simd128UnaryOp::Kind;
  struct TestCase {
    TransformKind transform_kind;
    UnaryKind extension_kind;
  };
  constexpr std::array test_cases = {
      TestCase{TransformKind::k8x8S, UnaryKind::kI16x8SConvertI8x16Low},
      TestCase{TransformKind::k8x8U, UnaryKind::kI16x8UConvertI8x16Low},
      TestCase{TransformKind::k16x4S, UnaryKind::kI32x4SConvertI16x8Low},
      TestCase{TransformKind::k16x4U, UnaryKind::kI32x4UConvertI16x8Low},
      TestCase{TransformKind::k32x2S, UnaryKind::kI64x2SConvertI32x4Low},
      TestCase{TransformKind::k32x2U, UnaryKind::kI64x2UConvertI32x4Low},
  };
  const std::array parameter_reps = {RegisterRepresentation::WordPtr(),
                                     RegisterRepresentation::WordPtr()};

  for (const TestCase& test_case : test_cases) {
    auto test = CreateFromGraph(
        base::VectorOf(parameter_reps),
        [test_case](TestInstance& t) {
          V<WordPtr> base = t.GetParameter<WordPtr>(0);
          V<WordPtr> index = t.GetParameter<WordPtr>(1);
          V<Simd128> load = t.Capture(t.Asm().Simd128LoadTransform(
                                          base, index, LoadOp::Kind::Trapping(),
                                          test_case.transform_kind, 8),
                                      "load");
          t.Asm().Return(load);
        },
        true);

    test.Run<InstructionSelectionNormalizationReducer>();

    const auto& reduction = test.GetCapture("load");
    const Simd128LoadTransformOp* load =
        reduction.GetFirst<Simd128LoadTransformOp>();
    const Simd128UnaryOp* extension = reduction.GetFirst<Simd128UnaryOp>();

    ASSERT_NE(load, nullptr);
    EXPECT_EQ(TransformKind::k64Zero, load->transform_kind);
    EXPECT_EQ(LoadOp::Kind::Trapping(), load->load_kind);
    EXPECT_EQ(8, load->offset);
    ASSERT_NE(extension, nullptr);
    EXPECT_EQ(test_case.extension_kind, extension->kind);
    EXPECT_EQ(test.graph().Index(*load), extension->input());
  }
}

TEST_F(InstructionSelectionNormalizationReducerTest, ReduceWasmDotsToArmDot) {
  // Input:
  //   I32x4Add(
  //       I32x4DotI16x8S(I16x8SConvertI8x16Low(left),
  //                      I16x8SConvertI8x16Low(right)),
  //       I32x4DotI16x8S(I16x8SConvertI8x16High(left),
  //                      I16x8SConvertI8x16High(right)))
  // Expected:
  //   I32x4DotI8x16S(Shuffle(left), Shuffle(right))
  if (!CpuFeatures::IsSupported(DOTPROD)) GTEST_SKIP();

  using BinopKind = Simd128BinopOp::Kind;
  using UnaryKind = Simd128UnaryOp::Kind;
  constexpr std::array parameter_reps = {RegisterRepresentation::Simd128(),
                                         RegisterRepresentation::Simd128()};
  auto test = CreateFromGraph(
      base::VectorOf(parameter_reps),
      [](TestInstance& test_instance) {
        V<Simd128> left = test_instance.Capture(
            test_instance.GetParameter<Simd128>(0), "left");
        V<Simd128> right = test_instance.Capture(
            test_instance.GetParameter<Simd128>(1), "right");
        auto WasmDot = [&](UnaryKind extension_kind) {
          V<Simd128> left_extension =
              test_instance.Asm().Simd128Unary(left, extension_kind);
          V<Simd128> right_extension =
              test_instance.Asm().Simd128Unary(right, extension_kind);
          return test_instance.Asm().Simd128Binop(
              left_extension, right_extension, BinopKind::kI32x4DotI16x8S);
        };
        V<Simd128> low_dot = WasmDot(UnaryKind::kI16x8SConvertI8x16Low);
        V<Simd128> high_dot = WasmDot(UnaryKind::kI16x8SConvertI8x16High);
        V<Simd128> add = test_instance.Asm().Simd128Binop(low_dot, high_dot,
                                                          BinopKind::kI32x4Add);
        test_instance.Asm().Return(test_instance.Capture(add, "add"));
      },
      true);

  test.Run<InstructionSelectionNormalizationReducer>();
  test.Run<DeadCodeEliminationReducer>();

  ASSERT_EQ(test.CountOp(Opcode::kSimd128Unary), 0u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Binop), 1u);
  ASSERT_EQ(test.CountOp(Opcode::kSimd128Shuffle), 2u);
  const auto* dot = test.GetCapture("add").GetFirst<Simd128BinopOp>();
  ASSERT_NE(dot, nullptr);
  EXPECT_EQ(BinopKind::kI32x4DotI8x16S, dot->kind);

  constexpr std::array expected_shuffle = {0, 1, 8,  9,  2, 3, 10, 11,
                                           4, 5, 12, 13, 6, 7, 14, 15};
  auto CheckShuffle = [&](OpIndex input, const char* capture) {
    const auto* shuffle = test.graph().Get(input).TryCast<Simd128ShuffleOp>();
    ASSERT_NE(shuffle, nullptr);
    EXPECT_EQ(Simd128ShuffleOp::Kind::kI8x16, shuffle->kind);
    EXPECT_TRUE(test.GetCapture(capture).Is(shuffle->left()));
    EXPECT_EQ(shuffle->left(), shuffle->right());
    EXPECT_TRUE(std::equal(expected_shuffle.begin(), expected_shuffle.end(),
                           std::begin(shuffle->shuffle)));
  };
  CheckShuffle(dot->left(), "left");
  CheckShuffle(dot->right(), "right");
}

TEST_F(InstructionSelectionNormalizationReducerTest,
       ReduceWasmDotOfWideningLoads) {
  // Input:
  //   I32x4DotI16x8S(Load8x8S/U(left), Load8x8S/U(right))
  // Expected:
  //   I32x4ExtAddPairwiseI16x8S/U(
  //       I16x8ExtMulLowI8x16S/U(Load64Zero(left), Load64Zero(right)))
  using BinopKind = Simd128BinopOp::Kind;
  using TransformKind = Simd128LoadTransformOp::TransformKind;
  using UnaryKind = Simd128UnaryOp::Kind;
  constexpr std::array parameter_reps = {RegisterRepresentation::WordPtr(),
                                         RegisterRepresentation::WordPtr()};

  for (TransformKind transform_kind :
       {TransformKind::k8x8S, TransformKind::k8x8U}) {
    auto test = CreateFromGraph(
        base::VectorOf(parameter_reps),
        [transform_kind](TestInstance& test_instance) {
          V<Simd128> left = test_instance.Capture(
              test_instance.Asm().Simd128LoadTransform(
                  test_instance.GetParameter<WordPtr>(0),
                  test_instance.Asm().WordPtrConstant(0),
                  LoadOp::Kind::Trapping(), transform_kind, 0),
              "left");
          V<Simd128> right = test_instance.Capture(
              test_instance.Asm().Simd128LoadTransform(
                  test_instance.GetParameter<WordPtr>(1),
                  test_instance.Asm().WordPtrConstant(0),
                  LoadOp::Kind::Trapping(), transform_kind, 0),
              "right");
          V<Simd128> dot = test_instance.Capture(
              test_instance.Asm().Simd128Binop(left, right,
                                               BinopKind::kI32x4DotI16x8S),
              "dot");
          test_instance.Asm().Return(dot);
        },
        true);

    test.Run<InstructionSelectionNormalizationReducer>();
    test.Run<DeadCodeEliminationReducer>();

    ASSERT_EQ(test.CountOp(Opcode::kSimd128LoadTransform), 2u);
    ASSERT_EQ(test.CountOp(Opcode::kSimd128Unary), 1u);
    ASSERT_EQ(test.CountOp(Opcode::kSimd128Shuffle), 0u);
    ASSERT_EQ(test.CountOp(Opcode::kSimd128Binop), 1u);
    const bool is_signed = transform_kind == TransformKind::k8x8S;
    const auto* pairwise = test.GetCapture("dot").GetFirst<Simd128UnaryOp>();
    ASSERT_NE(pairwise, nullptr);
    EXPECT_EQ(is_signed ? UnaryKind::kI32x4ExtAddPairwiseI16x8S
                        : UnaryKind::kI32x4ExtAddPairwiseI16x8U,
              pairwise->kind);
    const auto& product =
        test.graph().Get(pairwise->input()).Cast<Simd128BinopOp>();
    EXPECT_EQ(is_signed ? BinopKind::kI16x8ExtMulLowI8x16S
                        : BinopKind::kI16x8ExtMulLowI8x16U,
              product.kind);
    EXPECT_TRUE(test.GetCapture("left").Is(product.left()));
    EXPECT_TRUE(test.GetCapture("right").Is(product.right()));
    for (OpIndex input : product.inputs()) {
      const auto& load = test.graph().Get(input).Cast<Simd128LoadTransformOp>();
      EXPECT_EQ(TransformKind::k64Zero, load.transform_kind);
    }
  }
}

TEST_F(InstructionSelectionNormalizationReducerTest,
       ReduceWideningMultiplication) {
  using BinopKind = Simd128BinopOp::Kind;
  using UnaryKind = Simd128UnaryOp::Kind;
  struct TestCase {
    BinopKind mul_kind;
    UnaryKind extension_kind;
    BinopKind extmul_kind;
  };
  constexpr std::array test_cases = {
      TestCase{BinopKind::kI16x8Mul, UnaryKind::kI16x8SConvertI8x16Low,
               BinopKind::kI16x8ExtMulLowI8x16S},
      TestCase{BinopKind::kI16x8Mul, UnaryKind::kI16x8SConvertI8x16High,
               BinopKind::kI16x8ExtMulHighI8x16S},
      TestCase{BinopKind::kI16x8Mul, UnaryKind::kI16x8UConvertI8x16Low,
               BinopKind::kI16x8ExtMulLowI8x16U},
      TestCase{BinopKind::kI16x8Mul, UnaryKind::kI16x8UConvertI8x16High,
               BinopKind::kI16x8ExtMulHighI8x16U},
      TestCase{BinopKind::kI32x4Mul, UnaryKind::kI32x4SConvertI16x8Low,
               BinopKind::kI32x4ExtMulLowI16x8S},
      TestCase{BinopKind::kI32x4Mul, UnaryKind::kI32x4SConvertI16x8High,
               BinopKind::kI32x4ExtMulHighI16x8S},
      TestCase{BinopKind::kI32x4Mul, UnaryKind::kI32x4UConvertI16x8Low,
               BinopKind::kI32x4ExtMulLowI16x8U},
      TestCase{BinopKind::kI32x4Mul, UnaryKind::kI32x4UConvertI16x8High,
               BinopKind::kI32x4ExtMulHighI16x8U},
      TestCase{BinopKind::kI64x2Mul, UnaryKind::kI64x2SConvertI32x4Low,
               BinopKind::kI64x2ExtMulLowI32x4S},
      TestCase{BinopKind::kI64x2Mul, UnaryKind::kI64x2SConvertI32x4High,
               BinopKind::kI64x2ExtMulHighI32x4S},
      TestCase{BinopKind::kI64x2Mul, UnaryKind::kI64x2UConvertI32x4Low,
               BinopKind::kI64x2ExtMulLowI32x4U},
      TestCase{BinopKind::kI64x2Mul, UnaryKind::kI64x2UConvertI32x4High,
               BinopKind::kI64x2ExtMulHighI32x4U},
  };
  constexpr std::array parameter_reps = {RegisterRepresentation::Simd128(),
                                         RegisterRepresentation::Simd128()};

  for (const TestCase& test_case : test_cases) {
    auto test = CreateFromGraph(
        base::VectorOf(parameter_reps),
        [test_case](TestInstance& t) {
          V<Simd128> left = t.Capture(t.GetParameter<Simd128>(0), "left");
          V<Simd128> right = t.Capture(t.GetParameter<Simd128>(1), "right");
          V<Simd128> left_extension =
              t.Asm().Simd128Unary(left, test_case.extension_kind);
          V<Simd128> right_extension =
              t.Asm().Simd128Unary(right, test_case.extension_kind);
          V<Simd128> mul = t.Asm().Simd128Binop(left_extension, right_extension,
                                                test_case.mul_kind);
          t.Asm().Return(t.Capture(mul, "mul"));
        },
        true);

    test.Run<InstructionSelectionNormalizationReducer>();

    const Simd128BinopOp* extmul =
        test.GetCapture("mul").GetFirst<Simd128BinopOp>();
    ASSERT_NE(extmul, nullptr);
    EXPECT_EQ(test_case.extmul_kind, extmul->kind);
    EXPECT_TRUE(test.GetCapture("left").Is(extmul->left()));
    EXPECT_TRUE(test.GetCapture("right").Is(extmul->right()));
  }
}
#endif  // V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64

}  // namespace v8::internal::compiler::turboshaft
