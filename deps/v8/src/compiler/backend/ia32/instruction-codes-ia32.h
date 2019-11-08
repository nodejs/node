// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_IA32_INSTRUCTION_CODES_IA32_H_
#define V8_COMPILER_BACKEND_IA32_INSTRUCTION_CODES_IA32_H_

namespace v8 {
namespace internal {
namespace compiler {

// IA32-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(IA32Add)                       \
  V(IA32And)                       \
  V(IA32Cmp)                       \
  V(IA32Cmp16)                     \
  V(IA32Cmp8)                      \
  V(IA32Test)                      \
  V(IA32Test16)                    \
  V(IA32Test8)                     \
  V(IA32Or)                        \
  V(IA32Xor)                       \
  V(IA32Sub)                       \
  V(IA32Imul)                      \
  V(IA32ImulHigh)                  \
  V(IA32UmulHigh)                  \
  V(IA32Idiv)                      \
  V(IA32Udiv)                      \
  V(IA32Not)                       \
  V(IA32Neg)                       \
  V(IA32Shl)                       \
  V(IA32Shr)                       \
  V(IA32Sar)                       \
  V(IA32AddPair)                   \
  V(IA32SubPair)                   \
  V(IA32MulPair)                   \
  V(IA32ShlPair)                   \
  V(IA32ShrPair)                   \
  V(IA32SarPair)                   \
  V(IA32Ror)                       \
  V(IA32Lzcnt)                     \
  V(IA32Tzcnt)                     \
  V(IA32Popcnt)                    \
  V(IA32Bswap)                     \
  V(IA32MFence)                    \
  V(IA32LFence)                    \
  V(SSEFloat32Cmp)                 \
  V(SSEFloat32Add)                 \
  V(SSEFloat32Sub)                 \
  V(SSEFloat32Mul)                 \
  V(SSEFloat32Div)                 \
  V(SSEFloat32Abs)                 \
  V(SSEFloat32Neg)                 \
  V(SSEFloat32Sqrt)                \
  V(SSEFloat32Round)               \
  V(SSEFloat64Cmp)                 \
  V(SSEFloat64Add)                 \
  V(SSEFloat64Sub)                 \
  V(SSEFloat64Mul)                 \
  V(SSEFloat64Div)                 \
  V(SSEFloat64Mod)                 \
  V(SSEFloat32Max)                 \
  V(SSEFloat64Max)                 \
  V(SSEFloat32Min)                 \
  V(SSEFloat64Min)                 \
  V(SSEFloat64Abs)                 \
  V(SSEFloat64Neg)                 \
  V(SSEFloat64Sqrt)                \
  V(SSEFloat64Round)               \
  V(SSEFloat32ToFloat64)           \
  V(SSEFloat64ToFloat32)           \
  V(SSEFloat32ToInt32)             \
  V(SSEFloat32ToUint32)            \
  V(SSEFloat64ToInt32)             \
  V(SSEFloat64ToUint32)            \
  V(SSEInt32ToFloat32)             \
  V(SSEUint32ToFloat32)            \
  V(SSEInt32ToFloat64)             \
  V(SSEUint32ToFloat64)            \
  V(SSEFloat64ExtractLowWord32)    \
  V(SSEFloat64ExtractHighWord32)   \
  V(SSEFloat64InsertLowWord32)     \
  V(SSEFloat64InsertHighWord32)    \
  V(SSEFloat64LoadLowWord32)       \
  V(SSEFloat64SilenceNaN)          \
  V(AVXFloat32Add)                 \
  V(AVXFloat32Sub)                 \
  V(AVXFloat32Mul)                 \
  V(AVXFloat32Div)                 \
  V(AVXFloat64Add)                 \
  V(AVXFloat64Sub)                 \
  V(AVXFloat64Mul)                 \
  V(AVXFloat64Div)                 \
  V(AVXFloat64Abs)                 \
  V(AVXFloat64Neg)                 \
  V(AVXFloat32Abs)                 \
  V(AVXFloat32Neg)                 \
  V(IA32Movsxbl)                   \
  V(IA32Movzxbl)                   \
  V(IA32Movb)                      \
  V(IA32Movsxwl)                   \
  V(IA32Movzxwl)                   \
  V(IA32Movw)                      \
  V(IA32Movl)                      \
  V(IA32Movss)                     \
  V(IA32Movsd)                     \
  V(IA32Movdqu)                    \
  V(IA32BitcastFI)                 \
  V(IA32BitcastIF)                 \
  V(IA32Lea)                       \
  V(IA32Push)                      \
  V(IA32PushFloat32)               \
  V(IA32PushFloat64)               \
  V(IA32PushSimd128)               \
  V(IA32Poke)                      \
  V(IA32Peek)                      \
  V(SSEF64x2Splat)                 \
  V(AVXF64x2Splat)                 \
  V(SSEF64x2ExtractLane)           \
  V(AVXF64x2ExtractLane)           \
  V(SSEF64x2ReplaceLane)           \
  V(AVXF64x2ReplaceLane)           \
  V(IA32F64x2Sqrt)                 \
  V(IA32F64x2Add)                  \
  V(IA32F64x2Sub)                  \
  V(IA32F64x2Mul)                  \
  V(IA32F64x2Div)                  \
  V(IA32F64x2Min)                  \
  V(IA32F64x2Max)                  \
  V(IA32F64x2Eq)                   \
  V(IA32F64x2Ne)                   \
  V(IA32F64x2Lt)                   \
  V(IA32F64x2Le)                   \
  V(SSEF32x4Splat)                 \
  V(AVXF32x4Splat)                 \
  V(SSEF32x4ExtractLane)           \
  V(AVXF32x4ExtractLane)           \
  V(SSEF32x4ReplaceLane)           \
  V(AVXF32x4ReplaceLane)           \
  V(IA32F32x4SConvertI32x4)        \
  V(SSEF32x4UConvertI32x4)         \
  V(AVXF32x4UConvertI32x4)         \
  V(SSEF32x4Abs)                   \
  V(AVXF32x4Abs)                   \
  V(SSEF32x4Neg)                   \
  V(AVXF32x4Neg)                   \
  V(SSEF32x4Sqrt)                  \
  V(AVXF32x4Sqrt)                  \
  V(IA32F32x4RecipApprox)          \
  V(IA32F32x4RecipSqrtApprox)      \
  V(SSEF32x4Add)                   \
  V(AVXF32x4Add)                   \
  V(SSEF32x4AddHoriz)              \
  V(AVXF32x4AddHoriz)              \
  V(SSEF32x4Sub)                   \
  V(AVXF32x4Sub)                   \
  V(SSEF32x4Mul)                   \
  V(AVXF32x4Mul)                   \
  V(SSEF32x4Div)                   \
  V(AVXF32x4Div)                   \
  V(SSEF32x4Min)                   \
  V(AVXF32x4Min)                   \
  V(SSEF32x4Max)                   \
  V(AVXF32x4Max)                   \
  V(SSEF32x4Eq)                    \
  V(AVXF32x4Eq)                    \
  V(SSEF32x4Ne)                    \
  V(AVXF32x4Ne)                    \
  V(SSEF32x4Lt)                    \
  V(AVXF32x4Lt)                    \
  V(SSEF32x4Le)                    \
  V(AVXF32x4Le)                    \
  V(IA32I32x4Splat)                \
  V(IA32I32x4ExtractLane)          \
  V(SSEI32x4ReplaceLane)           \
  V(AVXI32x4ReplaceLane)           \
  V(SSEI32x4SConvertF32x4)         \
  V(AVXI32x4SConvertF32x4)         \
  V(IA32I32x4SConvertI16x8Low)     \
  V(IA32I32x4SConvertI16x8High)    \
  V(IA32I32x4Neg)                  \
  V(SSEI32x4Shl)                   \
  V(AVXI32x4Shl)                   \
  V(SSEI32x4ShrS)                  \
  V(AVXI32x4ShrS)                  \
  V(SSEI32x4Add)                   \
  V(AVXI32x4Add)                   \
  V(SSEI32x4AddHoriz)              \
  V(AVXI32x4AddHoriz)              \
  V(SSEI32x4Sub)                   \
  V(AVXI32x4Sub)                   \
  V(SSEI32x4Mul)                   \
  V(AVXI32x4Mul)                   \
  V(SSEI32x4MinS)                  \
  V(AVXI32x4MinS)                  \
  V(SSEI32x4MaxS)                  \
  V(AVXI32x4MaxS)                  \
  V(SSEI32x4Eq)                    \
  V(AVXI32x4Eq)                    \
  V(SSEI32x4Ne)                    \
  V(AVXI32x4Ne)                    \
  V(SSEI32x4GtS)                   \
  V(AVXI32x4GtS)                   \
  V(SSEI32x4GeS)                   \
  V(AVXI32x4GeS)                   \
  V(SSEI32x4UConvertF32x4)         \
  V(AVXI32x4UConvertF32x4)         \
  V(IA32I32x4UConvertI16x8Low)     \
  V(IA32I32x4UConvertI16x8High)    \
  V(SSEI32x4ShrU)                  \
  V(AVXI32x4ShrU)                  \
  V(SSEI32x4MinU)                  \
  V(AVXI32x4MinU)                  \
  V(SSEI32x4MaxU)                  \
  V(AVXI32x4MaxU)                  \
  V(SSEI32x4GtU)                   \
  V(AVXI32x4GtU)                   \
  V(SSEI32x4GeU)                   \
  V(AVXI32x4GeU)                   \
  V(IA32I16x8Splat)                \
  V(IA32I16x8ExtractLane)          \
  V(SSEI16x8ReplaceLane)           \
  V(AVXI16x8ReplaceLane)           \
  V(IA32I16x8SConvertI8x16Low)     \
  V(IA32I16x8SConvertI8x16High)    \
  V(IA32I16x8Neg)                  \
  V(SSEI16x8Shl)                   \
  V(AVXI16x8Shl)                   \
  V(SSEI16x8ShrS)                  \
  V(AVXI16x8ShrS)                  \
  V(SSEI16x8SConvertI32x4)         \
  V(AVXI16x8SConvertI32x4)         \
  V(SSEI16x8Add)                   \
  V(AVXI16x8Add)                   \
  V(SSEI16x8AddSaturateS)          \
  V(AVXI16x8AddSaturateS)          \
  V(SSEI16x8AddHoriz)              \
  V(AVXI16x8AddHoriz)              \
  V(SSEI16x8Sub)                   \
  V(AVXI16x8Sub)                   \
  V(SSEI16x8SubSaturateS)          \
  V(AVXI16x8SubSaturateS)          \
  V(SSEI16x8Mul)                   \
  V(AVXI16x8Mul)                   \
  V(SSEI16x8MinS)                  \
  V(AVXI16x8MinS)                  \
  V(SSEI16x8MaxS)                  \
  V(AVXI16x8MaxS)                  \
  V(SSEI16x8Eq)                    \
  V(AVXI16x8Eq)                    \
  V(SSEI16x8Ne)                    \
  V(AVXI16x8Ne)                    \
  V(SSEI16x8GtS)                   \
  V(AVXI16x8GtS)                   \
  V(SSEI16x8GeS)                   \
  V(AVXI16x8GeS)                   \
  V(IA32I16x8UConvertI8x16Low)     \
  V(IA32I16x8UConvertI8x16High)    \
  V(SSEI16x8ShrU)                  \
  V(AVXI16x8ShrU)                  \
  V(SSEI16x8UConvertI32x4)         \
  V(AVXI16x8UConvertI32x4)         \
  V(SSEI16x8AddSaturateU)          \
  V(AVXI16x8AddSaturateU)          \
  V(SSEI16x8SubSaturateU)          \
  V(AVXI16x8SubSaturateU)          \
  V(SSEI16x8MinU)                  \
  V(AVXI16x8MinU)                  \
  V(SSEI16x8MaxU)                  \
  V(AVXI16x8MaxU)                  \
  V(SSEI16x8GtU)                   \
  V(AVXI16x8GtU)                   \
  V(SSEI16x8GeU)                   \
  V(AVXI16x8GeU)                   \
  V(IA32I8x16Splat)                \
  V(IA32I8x16ExtractLane)          \
  V(SSEI8x16ReplaceLane)           \
  V(AVXI8x16ReplaceLane)           \
  V(SSEI8x16SConvertI16x8)         \
  V(AVXI8x16SConvertI16x8)         \
  V(IA32I8x16Neg)                  \
  V(SSEI8x16Shl)                   \
  V(AVXI8x16Shl)                   \
  V(IA32I8x16ShrS)                 \
  V(SSEI8x16Add)                   \
  V(AVXI8x16Add)                   \
  V(SSEI8x16AddSaturateS)          \
  V(AVXI8x16AddSaturateS)          \
  V(SSEI8x16Sub)                   \
  V(AVXI8x16Sub)                   \
  V(SSEI8x16SubSaturateS)          \
  V(AVXI8x16SubSaturateS)          \
  V(SSEI8x16Mul)                   \
  V(AVXI8x16Mul)                   \
  V(SSEI8x16MinS)                  \
  V(AVXI8x16MinS)                  \
  V(SSEI8x16MaxS)                  \
  V(AVXI8x16MaxS)                  \
  V(SSEI8x16Eq)                    \
  V(AVXI8x16Eq)                    \
  V(SSEI8x16Ne)                    \
  V(AVXI8x16Ne)                    \
  V(SSEI8x16GtS)                   \
  V(AVXI8x16GtS)                   \
  V(SSEI8x16GeS)                   \
  V(AVXI8x16GeS)                   \
  V(SSEI8x16UConvertI16x8)         \
  V(AVXI8x16UConvertI16x8)         \
  V(SSEI8x16AddSaturateU)          \
  V(AVXI8x16AddSaturateU)          \
  V(SSEI8x16SubSaturateU)          \
  V(AVXI8x16SubSaturateU)          \
  V(IA32I8x16ShrU)                 \
  V(SSEI8x16MinU)                  \
  V(AVXI8x16MinU)                  \
  V(SSEI8x16MaxU)                  \
  V(AVXI8x16MaxU)                  \
  V(SSEI8x16GtU)                   \
  V(AVXI8x16GtU)                   \
  V(SSEI8x16GeU)                   \
  V(AVXI8x16GeU)                   \
  V(IA32S128Zero)                  \
  V(SSES128Not)                    \
  V(AVXS128Not)                    \
  V(SSES128And)                    \
  V(AVXS128And)                    \
  V(SSES128Or)                     \
  V(AVXS128Or)                     \
  V(SSES128Xor)                    \
  V(AVXS128Xor)                    \
  V(SSES128Select)                 \
  V(AVXS128Select)                 \
  V(IA32S8x16Swizzle)              \
  V(IA32S8x16Shuffle)              \
  V(IA32S32x4Swizzle)              \
  V(IA32S32x4Shuffle)              \
  V(IA32S16x8Blend)                \
  V(IA32S16x8HalfShuffle1)         \
  V(IA32S16x8HalfShuffle2)         \
  V(IA32S8x16Alignr)               \
  V(IA32S16x8Dup)                  \
  V(IA32S8x16Dup)                  \
  V(SSES16x8UnzipHigh)             \
  V(AVXS16x8UnzipHigh)             \
  V(SSES16x8UnzipLow)              \
  V(AVXS16x8UnzipLow)              \
  V(SSES8x16UnzipHigh)             \
  V(AVXS8x16UnzipHigh)             \
  V(SSES8x16UnzipLow)              \
  V(AVXS8x16UnzipLow)              \
  V(IA32S64x2UnpackHigh)           \
  V(IA32S32x4UnpackHigh)           \
  V(IA32S16x8UnpackHigh)           \
  V(IA32S8x16UnpackHigh)           \
  V(IA32S64x2UnpackLow)            \
  V(IA32S32x4UnpackLow)            \
  V(IA32S16x8UnpackLow)            \
  V(IA32S8x16UnpackLow)            \
  V(SSES8x16TransposeLow)          \
  V(AVXS8x16TransposeLow)          \
  V(SSES8x16TransposeHigh)         \
  V(AVXS8x16TransposeHigh)         \
  V(SSES8x8Reverse)                \
  V(AVXS8x8Reverse)                \
  V(SSES8x4Reverse)                \
  V(AVXS8x4Reverse)                \
  V(SSES8x2Reverse)                \
  V(AVXS8x2Reverse)                \
  V(IA32S1x4AnyTrue)               \
  V(IA32S1x4AllTrue)               \
  V(IA32S1x8AnyTrue)               \
  V(IA32S1x8AllTrue)               \
  V(IA32S1x16AnyTrue)              \
  V(IA32S1x16AllTrue)              \
  V(IA32Word32AtomicPairLoad)      \
  V(IA32Word32AtomicPairStore)     \
  V(IA32Word32AtomicPairAdd)       \
  V(IA32Word32AtomicPairSub)       \
  V(IA32Word32AtomicPairAnd)       \
  V(IA32Word32AtomicPairOr)        \
  V(IA32Word32AtomicPairXor)       \
  V(IA32Word32AtomicPairExchange)  \
  V(IA32Word32AtomicPairCompareExchange)

// Addressing modes represent the "shape" of inputs to an instruction.
// Many instructions support multiple addressing modes. Addressing modes
// are encoded into the InstructionCode of the instruction and tell the
// code generator after register allocation which assembler method to call.
//
// We use the following local notation for addressing modes:
//
// M = memory operand
// R = base register
// N = index register * N for N in {1, 2, 4, 8}
// I = immediate displacement (int32_t)

#define TARGET_ADDRESSING_MODE_LIST(V) \
  V(MR)   /* [%r1            ] */      \
  V(MRI)  /* [%r1         + K] */      \
  V(MR1)  /* [%r1 + %r2*1    ] */      \
  V(MR2)  /* [%r1 + %r2*2    ] */      \
  V(MR4)  /* [%r1 + %r2*4    ] */      \
  V(MR8)  /* [%r1 + %r2*8    ] */      \
  V(MR1I) /* [%r1 + %r2*1 + K] */      \
  V(MR2I) /* [%r1 + %r2*2 + K] */      \
  V(MR4I) /* [%r1 + %r2*3 + K] */      \
  V(MR8I) /* [%r1 + %r2*4 + K] */      \
  V(M1)   /* [      %r2*1    ] */      \
  V(M2)   /* [      %r2*2    ] */      \
  V(M4)   /* [      %r2*4    ] */      \
  V(M8)   /* [      %r2*8    ] */      \
  V(M1I)  /* [      %r2*1 + K] */      \
  V(M2I)  /* [      %r2*2 + K] */      \
  V(M4I)  /* [      %r2*4 + K] */      \
  V(M8I)  /* [      %r2*8 + K] */      \
  V(MI)   /* [              K] */      \
  V(Root) /* [%root       + K] */

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_IA32_INSTRUCTION_CODES_IA32_H_
