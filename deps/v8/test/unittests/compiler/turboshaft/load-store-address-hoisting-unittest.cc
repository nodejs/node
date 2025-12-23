// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/vector.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/load-store-simplification-reducer.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// The 'restrictive load/store' architectures that don't support Turboshaft's
// full addressing pattern, as defined in LoadStoreSimplificationReducer.
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64 ||    \
    V8_TARGET_ARCH_LOONG64 || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64 || \
    V8_TARGET_ARCH_RISCV32

TEST_F(ReducerTest, HoistAddressForLoads) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    OpIndex base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    OpIndex index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    __ Load(base, index, LoadOp::Kind::Protected(),
            MemoryRepresentation::Uint32(), RegisterRepresentation::Word32(),
            0);
    __ Load(base, index, LoadOp::Kind::Protected(),
            MemoryRepresentation::Uint32(), RegisterRepresentation::Word32(),
            8);
    __ Load(base, index, LoadOp::Kind::Protected(),
            MemoryRepresentation::Uint32(), RegisterRepresentation::Word32(),
            16);
    __ Return(base);
  });

  ASSERT_EQ(test.CountOp(Opcode::kLoad), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 0u);

  test.Run<LoadStoreSimplificationReducer>();
  // After LoadStoreSimplificationReducer, offsets will not be encoded in the
  // LoadOp for some archs, so we expect an add per memory access with offset.

  ASSERT_EQ(test.CountOp(Opcode::kLoad), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 2u);

  test.Run<ValueNumberingReducer>();
  // ValueNumberingReducer should remove the redundant adds.

  ASSERT_EQ(test.CountOp(Opcode::kLoad), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 1u);
}

TEST_F(ReducerTest, HoistAddressForStores) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    OpIndex base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    OpIndex index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto data = __ HeapConstant(Asm.factory().undefined_value());
    __ Store(base, index, data, StoreOp::Kind::Protected(),
             MemoryRepresentation::Uint32(), WriteBarrierKind::kNoWriteBarrier,
             0);
    __ Store(base, index, data, StoreOp::Kind::Protected(),
             MemoryRepresentation::Uint32(), WriteBarrierKind::kNoWriteBarrier,
             8);
    __ Store(base, index, data, StoreOp::Kind::Protected(),
             MemoryRepresentation::Uint32(), WriteBarrierKind::kNoWriteBarrier,
             16);
    __ Return(base);
  });

  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 0u);

  test.Run<LoadStoreSimplificationReducer>();

  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 2u);

  test.Run<ValueNumberingReducer>();

  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 1u);
}

TEST_F(ReducerTest, NoHoistingWithBarriers) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    OpIndex base = __ BitcastTaggedToWordPtr(Asm.GetParameter(0));
    OpIndex index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto data = __ HeapConstant(Asm.factory().undefined_value());
    __ Store(base, index, data, LoadOp::Kind::Protected(),
             MemoryRepresentation::Uint32(),
             WriteBarrierKind::kFullWriteBarrier, 0);
    __ Store(base, index, data, LoadOp::Kind::Protected(),
             MemoryRepresentation::Uint32(),
             WriteBarrierKind::kFullWriteBarrier, 8);
    __ Store(base, index, data, LoadOp::Kind::Protected(),
             MemoryRepresentation::Uint32(),
             WriteBarrierKind::kFullWriteBarrier, 16);
    __ Return(base);
  });

  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 0u);

  test.Run<LoadStoreSimplificationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 2u);

  test.Run<ValueNumberingReducer>();
  // Binops will not be hoisted and so none will be removed by CSE.
  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 2u);
}

TEST_F(ReducerTest, NoHoistingTaggedStores) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    OpIndex base = Asm.GetParameter(0);
    OpIndex index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto data = __ HeapConstant(Asm.factory().undefined_value());
    __ Store(base, index, data, StoreOp::Kind::TaggedBase(),
             MemoryRepresentation::TaggedPointer(),
             WriteBarrierKind::kFullWriteBarrier, 0);
    __ Store(base, index, data, StoreOp::Kind::TaggedBase(),
             MemoryRepresentation::TaggedPointer(),
             WriteBarrierKind::kFullWriteBarrier, 8);
    __ Store(base, index, data, StoreOp::Kind::TaggedBase(),
             MemoryRepresentation::TaggedPointer(),
             WriteBarrierKind::kFullWriteBarrier, 16);
    __ Return(base);
  });

  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 0u);

  test.Run<LoadStoreSimplificationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 3u);

  test.Run<ValueNumberingReducer>();
  // Binops will not be hoisted and so none will be removed by CSE.
  ASSERT_EQ(test.CountOp(Opcode::kStore), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 3u);
}

TEST_F(ReducerTest, NoHoistingTaggedLoads) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    OpIndex base = Asm.GetParameter(0);
    OpIndex index = __ BitcastTaggedToWordPtr(Asm.GetParameter(1));
    auto ld0 = __ Load(base, index, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::TaggedPointer(),
                       RegisterRepresentation::Tagged(), 0);
    auto ld1 = __ Load(base, index, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::TaggedPointer(),
                       RegisterRepresentation::Tagged(), 8);
    auto ld2 = __ Load(base, index, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::TaggedPointer(),
                       RegisterRepresentation::Tagged(), 16);
    // We need to use the loads otherwise they'll be optimized out as dead code.
    auto ld0_word = __ BitcastTaggedToWordPtr(ld0);
    auto ld1_word = __ BitcastTaggedToWordPtr(ld1);
    auto ld2_word = __ BitcastTaggedToWordPtr(ld2);
    auto ld1_ld2 = __ WordBinop(ld1_word, ld2_word, WordBinopOp::Kind::kAdd,
                                WordRepresentation::WordPtr());
    __ Return(__ WordBinop(ld0_word, ld1_ld2, WordBinopOp::Kind::kAdd,
                           WordRepresentation::WordPtr()));
  });

  ASSERT_EQ(test.CountOp(Opcode::kLoad), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 2u);

  test.Run<LoadStoreSimplificationReducer>();
  ASSERT_EQ(test.CountOp(Opcode::kLoad), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 5u);

  test.Run<ValueNumberingReducer>();
  // Binops will not be hoisted and so none will be removed by CSE.
  ASSERT_EQ(test.CountOp(Opcode::kLoad), 3u);
  ASSERT_EQ(test.CountOp(Opcode::kWordBinop), 5u);
}

#endif

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
