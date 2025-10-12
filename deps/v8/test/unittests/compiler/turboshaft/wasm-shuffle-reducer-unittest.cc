// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-shuffle-reducer.h"

#include "src/base/vector.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "test/common/flag-utils.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

TEST_F(ReducerTest, UnaryConvertLowShuffle) {
  // Expected reduced shuffle lengths when used solely by the given op.
  std::array test_list = {
      Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
      Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low,
      Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low,
      Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low,
      Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low,
      Simd128UnaryOp::Kind::kI64x2UConvertI32x4Low,
  };

  for (auto kind : test_list) {
    SCOPED_TRACE(kind);
    OpIndex shuffle;
    auto test = CreateFromGraph(1, [&kind, &shuffle](auto& Asm) {
      auto left =
          __ Simd128Splat(__ Word32Constant(1), Simd128SplatOp::Kind::kI32x4);
      auto right =
          __ Simd128Splat(__ Word32Constant(0), Simd128SplatOp::Kind::kI32x4);
      constexpr uint8_t shuffle_bytes[kSimd128Size] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                       0, 0, 0, 0, 0, 0, 0, 0};
      shuffle = __ Simd128Shuffle(left, right, Simd128ShuffleOp::Kind::kI8x16,
                                  shuffle_bytes);
      __ Return(__ Simd128Unary(shuffle, kind));
    });
    WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
    analyzer.Run();
    EXPECT_TRUE(analyzer.ShouldReduce());
    EXPECT_EQ(analyzer.DemandedByteLanes(&test.graph().Get(shuffle)), 0xFF);
    test.Run<WasmShuffleReducer>();
  }
}

TEST_F(ReducerTest, UnaryConvertHighShuffle) {
  // Expected reduced shuffle lengths when used only by the given op.
  std::array test_list = {
      Simd128UnaryOp::Kind::kI16x8SConvertI8x16High,
      Simd128UnaryOp::Kind::kI16x8UConvertI8x16High,
      Simd128UnaryOp::Kind::kI32x4SConvertI16x8High,
      Simd128UnaryOp::Kind::kI32x4UConvertI16x8High,
      Simd128UnaryOp::Kind::kI64x2SConvertI32x4High,
      Simd128UnaryOp::Kind::kI64x2UConvertI32x4High,
  };

  for (auto kind : test_list) {
    SCOPED_TRACE(kind);
    OpIndex shuffle;
    auto test = CreateFromGraph(1, [&kind, &shuffle](auto& Asm) {
      auto left =
          __ Simd128Splat(__ Word32Constant(1), Simd128SplatOp::Kind::kI32x4);
      auto right =
          __ Simd128Splat(__ Word32Constant(0), Simd128SplatOp::Kind::kI32x4);
      constexpr uint8_t shuffle_bytes[kSimd128Size] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                       0, 0, 0, 0, 0, 0, 0, 0};
      shuffle = __ Simd128Shuffle(left, right, Simd128ShuffleOp::Kind::kI8x16,
                                  shuffle_bytes);
      __ Return(__ Simd128Unary(shuffle, kind));
    });
    WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
    analyzer.Run();
    EXPECT_FALSE(analyzer.ShouldReduce());
    test.Run<WasmShuffleReducer>();
  }
}

TEST_F(ReducerTest, UnaryConvertTwoChainedShuffle) {
  // Expected reduced shuffle lengths when used only by the first op, itself
  // used only by the second.
  std::array<std::tuple<Simd128UnaryOp::Kind, Simd128UnaryOp::Kind, uint8_t>, 6>
      test_list = {{
          {Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
           Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low, 4},
          {Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low,
           Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low, 4},
          {Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low,
           Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low, 4},
          {Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low,
           Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low, 4},
          {Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
           Simd128UnaryOp::Kind::kI32x4SConvertI16x8High, 8},
          {Simd128UnaryOp::Kind::kI16x8UConvertI8x16High,
           Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low, 0},
      }};

  for (auto const& [first_kind, second_kind, expected_count] : test_list) {
    OpIndex shuffle;
    SCOPED_TRACE(first_kind);
    SCOPED_TRACE(second_kind);
    auto test = CreateFromGraph(1, [&first_kind, &second_kind,
                                    &shuffle](auto& Asm) {
      auto left =
          __ Simd128Splat(__ Word32Constant(1), Simd128SplatOp::Kind::kI32x4);
      auto right =
          __ Simd128Splat(__ Word32Constant(0), Simd128SplatOp::Kind::kI32x4);
      constexpr uint8_t shuffle_bytes[kSimd128Size] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                       0, 0, 0, 0, 0, 0, 0, 0};
      shuffle = __ Simd128Shuffle(left, right, Simd128ShuffleOp::Kind::kI8x16,
                                  shuffle_bytes);
      __ Return(
          __ Simd128Unary(__ Simd128Unary(shuffle, first_kind), second_kind));
    });
    WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
    analyzer.Run();
    if (expected_count == 0) {
      EXPECT_FALSE(analyzer.ShouldReduce());
    } else {
      EXPECT_TRUE(analyzer.ShouldReduce());
      auto maybe_bitset =
          analyzer.DemandedByteLanes(&test.graph().Get(shuffle));
      EXPECT_TRUE(maybe_bitset.has_value());
      if (maybe_bitset.has_value()) {
        if (expected_count == 8) {
          EXPECT_EQ(maybe_bitset.value(), 0xFF);
        } else if (expected_count == 4) {
          EXPECT_EQ(maybe_bitset.value(), 0xF);
        } else if (expected_count == 2) {
          EXPECT_EQ(maybe_bitset.value(), 0x3);
        }
      }
    }
    test.Run<WasmShuffleReducer>();
  }
}

TEST_F(ReducerTest, UnaryConvertThreeChainedShuffle) {
  // Expected reduced shuffle lengths when used in a chain of three
  // conversions, each with a single use.
  std::array<std::tuple<Simd128UnaryOp::Kind, Simd128UnaryOp::Kind,
                        Simd128UnaryOp::Kind, uint8_t>,
             2>
      test_list = {{
          {Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
           Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low,
           Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low, 2},
          {Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low,
           Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low,
           Simd128UnaryOp::Kind::kI64x2UConvertI32x4Low, 2},
      }};

  for (auto const& [first_kind, second_kind, third_kind, expected_count] :
       test_list) {
    SCOPED_TRACE(first_kind);
    SCOPED_TRACE(second_kind);
    OpIndex shuffle;
    auto test = CreateFromGraph(1, [&first_kind, &second_kind, &third_kind,
                                    &shuffle](auto& Asm) {
      auto left =
          __ Simd128Splat(__ Word32Constant(1), Simd128SplatOp::Kind::kI32x4);
      auto right =
          __ Simd128Splat(__ Word32Constant(0), Simd128SplatOp::Kind::kI32x4);
      constexpr uint8_t shuffle_bytes[kSimd128Size] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                       0, 0, 0, 0, 0, 0, 0, 0};
      shuffle = __ Simd128Shuffle(left, right, Simd128ShuffleOp::Kind::kI8x16,
                                  shuffle_bytes);
      __ Return(__ Simd128Unary(
          __ Simd128Unary(__ Simd128Unary(shuffle, first_kind), second_kind),
          third_kind));
    });
    WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
    analyzer.Run();
    EXPECT_TRUE(analyzer.ShouldReduce());
    auto maybe_bitset = analyzer.DemandedByteLanes(&test.graph().Get(shuffle));
    EXPECT_TRUE(maybe_bitset.has_value());
    if (maybe_bitset.has_value()) {
      EXPECT_EQ(maybe_bitset.value(), 0x3);
    }
    test.Run<WasmShuffleReducer>();
  }
}

TEST_F(ReducerTest, BinaryExtLowShuffle) {
  // Expected reduced shuffle lengths when used solely by the given op.
  std::array test_list = {
      Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S,
      Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16U,
      Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8S,
      Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8U,
      Simd128BinopOp::Kind::kI64x2ExtMulLowI32x4S,
      Simd128BinopOp::Kind::kI64x2ExtMulLowI32x4U,
  };
  for (auto kind : test_list) {
    SCOPED_TRACE(kind);
    OpIndex left_shuffle;
    OpIndex right_shuffle;
    auto test = CreateFromGraph(1, [&kind, &left_shuffle,
                                    &right_shuffle](auto& Asm) {
      auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;
      auto zero =
          __ Simd128Splat(__ Word32Constant(0), Simd128SplatOp::Kind::kI32x4);
      auto one =
          __ Simd128Splat(__ Word32Constant(1), Simd128SplatOp::Kind::kI32x4);
      auto two =
          __ Simd128Splat(__ Word32Constant(2), Simd128SplatOp::Kind::kI32x4);
      auto three =
          __ Simd128Splat(__ Word32Constant(3), Simd128SplatOp::Kind::kI32x4);
      constexpr uint8_t shuffle_bytes[kSimd128Size] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                       0, 0, 0, 0, 0, 0, 0, 0};
      left_shuffle = __ Simd128Shuffle(zero, one, ShuffleKind, shuffle_bytes);
      right_shuffle = __ Simd128Shuffle(two, three, ShuffleKind, shuffle_bytes);
      __ Return(__ Simd128Binop(left_shuffle, right_shuffle, kind));
    });
    WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
    analyzer.Run();
    EXPECT_TRUE(analyzer.ShouldReduce());
    EXPECT_EQ(analyzer.DemandedByteLanes(&test.graph().Get(left_shuffle)),
              0xFF);
    EXPECT_EQ(analyzer.DemandedByteLanes(&test.graph().Get(right_shuffle)),
              0xFF);
    test.Run<WasmShuffleReducer>();
  }
}

TEST_F(ReducerTest, BinaryExtLowUnaryShuffle) {
  // Expected reduced shuffle lengths when used solely by the given op.
  std::array<std::tuple<Simd128BinopOp::Kind, Simd128UnaryOp::Kind, uint8_t>,
             12>
      test_list = {{
          {Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S,
           Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low, 4},
          {Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16U,
           Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low, 4},
          {Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8S,
           Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low, 4},
          {Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8U,
           Simd128UnaryOp::Kind::kI64x2UConvertI32x4Low, 4},
          {Simd128BinopOp::Kind::kI16x8ExtMulHighI8x16S,
           Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low, 0},
          {Simd128BinopOp::Kind::kI16x8ExtMulHighI8x16U,
           Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low, 0},
          {Simd128BinopOp::Kind::kI32x4ExtMulHighI16x8S,
           Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low, 0},
          {Simd128BinopOp::Kind::kI32x4ExtMulHighI16x8U,
           Simd128UnaryOp::Kind::kI64x2UConvertI32x4Low, 0},
          {Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S,
           Simd128UnaryOp::Kind::kI32x4SConvertI16x8High, 8},
          {Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16U,
           Simd128UnaryOp::Kind::kI32x4UConvertI16x8High, 8},
          {Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8S,
           Simd128UnaryOp::Kind::kI64x2SConvertI32x4High, 8},
          {Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8U,
           Simd128UnaryOp::Kind::kI64x2UConvertI32x4High, 8},
      }};
  for (auto const& [binop_kind, unop_kind, expected_count] : test_list) {
    OpIndex left_shuffle;
    OpIndex right_shuffle;
    SCOPED_TRACE(binop_kind);
    SCOPED_TRACE(unop_kind);
    auto test = CreateFromGraph(1, [&binop_kind, unop_kind, &left_shuffle,
                                    &right_shuffle](auto& Asm) {
      auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;
      auto zero =
          __ Simd128Splat(__ Word32Constant(0), Simd128SplatOp::Kind::kI32x4);
      auto one =
          __ Simd128Splat(__ Word32Constant(1), Simd128SplatOp::Kind::kI32x4);
      auto two =
          __ Simd128Splat(__ Word32Constant(2), Simd128SplatOp::Kind::kI32x4);
      auto three =
          __ Simd128Splat(__ Word32Constant(3), Simd128SplatOp::Kind::kI32x4);
      constexpr uint8_t shuffle_bytes[kSimd128Size] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                       0, 0, 0, 0, 0, 0, 0, 0};
      left_shuffle = __ Simd128Shuffle(zero, one, ShuffleKind, shuffle_bytes);
      right_shuffle = __ Simd128Shuffle(two, three, ShuffleKind, shuffle_bytes);
      __ Return(__ Simd128Unary(
          __ Simd128Binop(left_shuffle, right_shuffle, binop_kind), unop_kind));
    });
    WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
    analyzer.Run();
    if (expected_count == 0) {
      EXPECT_FALSE(analyzer.ShouldReduce());
    } else {
      EXPECT_TRUE(analyzer.ShouldReduce());
      std::bitset<16> expected_byte_lane_mask =
          expected_count == 8 ? 0xFF : 0xF;
      EXPECT_EQ(analyzer.DemandedByteLanes(&test.graph().Get(left_shuffle)),
                expected_byte_lane_mask);
      EXPECT_EQ(analyzer.DemandedByteLanes(&test.graph().Get(right_shuffle)),
                expected_byte_lane_mask);
      test.Run<WasmShuffleReducer>();
    }
  }
}

#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

namespace {
auto ShuffleKind = Simd128ShuffleOp::Kind::kI8x16;
constexpr uint8_t shuffle_bytes_even[kSimd128Size] = {
    0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23};
constexpr uint8_t shuffle_bytes_odd[kSimd128Size] = {
    8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31};
}  // namespace

TEST_F(ReducerTest, LoadInterleaveTwo) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto ld0 = __ Load(base, index, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto ld1 = __ Load(base, index, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Add));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_TRUE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveTwoNegativeDiffOffset) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto ld0 = __ Load(base, index, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto ld1 = __ Load(base, index, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld1, ld0, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld1, ld0, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Add));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
  // TODO(sparker): This should expect TRUE when absolute difference
  // between loads offsets in deinterleaved load reduction is implemented.
}

TEST_F(ReducerTest, LoadInterleaveTwoNoIndex) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto ld0 = __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto ld1 = __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Mul));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_TRUE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveTwoWrongIndex) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    auto ld0 = __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto ld1 = __ Load(Asm.GetParameter(0), index, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Mul));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveNoIndexNegativeDiffOffset) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto ld0 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto ld1 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld1, ld0, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld1, ld0, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Add));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
  // TODO(sparker): This should expect TRUE when absolute difference
  // between loads offsets in deinterleaved load reduction is implemented.
}

TEST_F(ReducerTest, LoadInterleaveTwoNoIndexWrongOffset) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto ld0 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto ld1 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 2 * kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kI64x2Mul));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveTwoNoIndexNotSameKind) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto ld0 = __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto ld1 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kI64x2Sub));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveTwoNoIndexNotLeftAndRight) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto ld0 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto ld1 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld1, ld0, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Mul));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveTwoNoIndexCantReschedule) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto ld0 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    auto data = __ HeapConstant(Asm.factory().undefined_value());
    __ Store(Asm.GetParameter(0), data, StoreOp::Kind::TaggedBase(),
             MemoryRepresentation::TaggedPointer(),
             WriteBarrierKind::kNoWriteBarrier, 0, true);
    auto ld1 = __ Load(base, {}, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Mul));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
}

TEST_F(ReducerTest, LoadInterleaveTwoWrongBlocks) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    Block* block_a = __ NewBlock();
    Block* block_b = __ NewBlock();
    Block* block_c = __ NewBlock();
    Block* block_d = __ NewBlock();
    __ Bind(block_a);
    auto base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    auto index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    __ Goto(block_b);
    __ Bind(block_b);
    auto ld0 = __ Load(base, index, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), 0);
    __ Goto(block_c);
    __ Bind(block_c);
    auto ld1 = __ Load(base, index, LoadOp::Kind::Protected(),
                       MemoryRepresentation::Simd128(),
                       RegisterRepresentation::Simd128(), kSimd128Size);
    __ Goto(block_d);
    __ Bind(block_d);
    auto even_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_even);
    auto odd_shuffle =
        __ Simd128Shuffle(ld0, ld1, ShuffleKind, shuffle_bytes_odd);
    __ Return(__ Simd128Binop(even_shuffle, odd_shuffle,
                              Simd128BinopOp::Kind::kF64x2Add));
  });
  WasmShuffleAnalyzer analyzer(test.zone(), test.graph());
  analyzer.Run();
  EXPECT_FALSE(analyzer.ShouldReduce());
}

#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
