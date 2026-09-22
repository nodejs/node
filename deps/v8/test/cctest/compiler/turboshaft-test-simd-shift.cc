// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/cpu-features.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/turboshaft-codegen-tester.h"
#include "test/common/value-helper.h"

#if V8_ENABLE_WEBASSEMBLY

namespace v8::internal::compiler::turboshaft {

namespace {

bool SimdIsSupported() { return CpuFeatures::SupportsSimd128(); }

}  // namespace

// Tests for I8x16ShrS (8-bit lane signed right shift)
TEST(SimdShift_I8x16ShrS_ShiftMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(7);  // lane_size - 1 for 8-bit lane
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI8x16ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int8_t src[16] = {-128, -64,  -1,  0,    1,  64,  127, -5,
                                10,   -100, 120, -127, 42, -42, 0,   127};
  alignas(16) int8_t dst[16] = {0};
  alignas(16) int8_t expected[16];
  for (int i = 0; i < 16; ++i) {
    expected[i] = src[i] >> 7;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 16; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I8x16ShrS_ShiftNonMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(3);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI8x16ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int8_t src[16] = {-128, -64,  -1,  0,    1,  64,  127, -5,
                                10,   -100, 120, -127, 42, -42, 0,   127};
  alignas(16) int8_t dst[16] = {0};
  alignas(16) int8_t expected[16];
  for (int i = 0; i < 16; ++i) {
    expected[i] = src[i] >> 3;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 16; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I8x16ShrS_VariableShift) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(
      MachineType::Pointer(), MachineType::Pointer(), MachineType::Int32());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex shift = m.Parameter(2);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI8x16ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int8_t src[16] = {-128, -64,  -1,  0,    1,  64,  127, -5,
                                10,   -100, 120, -127, 42, -42, 0,   127};
  alignas(16) int8_t dst[16] = {0};

  for (int s : {0, 1, 3, 7, 8, 9, 15}) {
    alignas(16) int8_t expected[16];
    for (int i = 0; i < 16; ++i) {
      expected[i] = src[i] >> (s % 8);
    }

    m.Call(static_cast<void*>(src), static_cast<void*>(dst), s);
    for (int i = 0; i < 16; ++i) {
      CHECK_EQ(expected[i], dst[i]);
    }
  }
}

// Tests for I16x8ShrS (16-bit lane signed right shift)
TEST(SimdShift_I16x8ShrS_ShiftMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(15);  // lane_size - 1 for 16-bit lane
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI16x8ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int16_t src[8] = {-32768, -16384, -1, 0, 1, 16384, 32767, -42};
  alignas(16) int16_t dst[8] = {0};
  alignas(16) int16_t expected[8];
  for (int i = 0; i < 8; ++i) {
    expected[i] = src[i] >> 15;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 8; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I16x8ShrS_ShiftNonMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(7);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI16x8ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int16_t src[8] = {-32768, -16384, -1, 0, 1, 16384, 32767, -42};
  alignas(16) int16_t dst[8] = {0};
  alignas(16) int16_t expected[8];
  for (int i = 0; i < 8; ++i) {
    expected[i] = src[i] >> 7;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 8; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I16x8ShrS_VariableShift) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(
      MachineType::Pointer(), MachineType::Pointer(), MachineType::Int32());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex shift = m.Parameter(2);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI16x8ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int16_t src[8] = {-32768, -16384, -1, 0, 1, 16384, 32767, -42};
  alignas(16) int16_t dst[8] = {0};

  for (int s : {0, 1, 7, 15, 16, 17, 31}) {
    alignas(16) int16_t expected[8];
    for (int i = 0; i < 8; ++i) {
      expected[i] = src[i] >> (s % 16);
    }

    m.Call(static_cast<void*>(src), static_cast<void*>(dst), s);
    for (int i = 0; i < 8; ++i) {
      CHECK_EQ(expected[i], dst[i]);
    }
  }
}

// Tests for I32x4ShrS (32-bit lane signed right shift)
TEST(SimdShift_I32x4ShrS_ShiftMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(31);  // lane_size - 1 for 32-bit lane
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI32x4ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int32_t src[4] = {-2147483647 - 1, -1000, 0, 2147483647};
  alignas(16) int32_t dst[4] = {0};
  alignas(16) int32_t expected[4];
  for (int i = 0; i < 4; ++i) {
    expected[i] = src[i] >> 31;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 4; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I32x4ShrS_ShiftNonMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(16);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI32x4ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int32_t src[4] = {-2147483647 - 1, -1000, 0, 2147483647};
  alignas(16) int32_t dst[4] = {0};
  alignas(16) int32_t expected[4];
  for (int i = 0; i < 4; ++i) {
    expected[i] = src[i] >> 16;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 4; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I32x4ShrS_VariableShift) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(
      MachineType::Pointer(), MachineType::Pointer(), MachineType::Int32());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex shift = m.Parameter(2);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI32x4ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16) int32_t src[4] = {-2147483647 - 1, -1000, 0, 2147483647};
  alignas(16) int32_t dst[4] = {0};

  for (int s : {0, 1, 16, 31, 32, 33, 63}) {
    alignas(16) int32_t expected[4];
    for (int i = 0; i < 4; ++i) {
      expected[i] = src[i] >> (s % 32);
    }

    m.Call(static_cast<void*>(src), static_cast<void*>(dst), s);
    for (int i = 0; i < 4; ++i) {
      CHECK_EQ(expected[i], dst[i]);
    }
  }
}

// Tests for I64x2ShrS (64-bit lane signed right shift)
TEST(SimdShift_I64x2ShrS_ShiftMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(63);  // lane_size - 1 for 64-bit lane
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI64x2ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16)
      int64_t src[2] = {-9223372036854775807LL - 1, 9223372036854775807LL};
  alignas(16) int64_t dst[2] = {0};
  alignas(16) int64_t expected[2];
  for (int i = 0; i < 2; ++i) {
    expected[i] = src[i] >> 63;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 2; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I64x2ShrS_ShiftNonMaxImmediate) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(MachineType::Pointer(),
                                       MachineType::Pointer());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex shift = m.Word32Constant(32);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI64x2ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16)
      int64_t src[2] = {-9223372036854775807LL - 1, 9223372036854775807LL};
  alignas(16) int64_t dst[2] = {0};
  alignas(16) int64_t expected[2];
  for (int i = 0; i < 2; ++i) {
    expected[i] = src[i] >> 32;
  }

  m.Call(static_cast<void*>(src), static_cast<void*>(dst));
  for (int i = 0; i < 2; ++i) {
    CHECK_EQ(expected[i], dst[i]);
  }
}

TEST(SimdShift_I64x2ShrS_VariableShift) {
  if (!SimdIsSupported()) return;

  RawMachineAssemblerTester<int32_t> m(
      MachineType::Pointer(), MachineType::Pointer(), MachineType::Int32());
  OpIndex src_ptr = m.Parameter(0);
  OpIndex dst_ptr = m.Parameter(1);
  OpIndex shift = m.Parameter(2);
  OpIndex input = m.Load(MachineType::Simd128(), src_ptr);
  OpIndex result =
      m.Simd128Shift(input, shift, Simd128ShiftOp::Kind::kI64x2ShrS);
  m.Store(MachineRepresentation::kSimd128, dst_ptr, result,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Word32Constant(0));

  alignas(16)
      int64_t src[2] = {-9223372036854775807LL - 1, 9223372036854775807LL};
  alignas(16) int64_t dst[2] = {0};

  for (int s : {0, 1, 32, 63, 64, 65, 127}) {
    alignas(16) int64_t expected[2];
    for (int i = 0; i < 2; ++i) {
      expected[i] = src[i] >> (s % 64);
    }

    m.Call(static_cast<void*>(src), static_cast<void*>(dst), s);
    for (int i = 0; i < 2; ++i) {
      CHECK_EQ(expected[i], dst[i]);
    }
  }
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_ENABLE_WEBASSEMBLY
