// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/instruction-selection-normalization-reducer.h"

#include <array>

#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

using InstructionSelectionNormalizationReducerTest = ReducerTest;

#if V8_ENABLE_WEBASSEMBLY && defined(V8_TARGET_ARCH_ARM64)
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
#endif  // V8_ENABLE_WEBASSEMBLY && V8_TARGET_ARCH_ARM64

}  // namespace v8::internal::compiler::turboshaft
