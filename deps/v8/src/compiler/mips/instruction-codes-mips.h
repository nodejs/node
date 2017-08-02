// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MIPS_INSTRUCTION_CODES_MIPS_H_
#define V8_COMPILER_MIPS_INSTRUCTION_CODES_MIPS_H_

namespace v8 {
namespace internal {
namespace compiler {

// MIPS-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(MipsAdd)                       \
  V(MipsAddOvf)                    \
  V(MipsSub)                       \
  V(MipsSubOvf)                    \
  V(MipsMul)                       \
  V(MipsMulOvf)                    \
  V(MipsMulHigh)                   \
  V(MipsMulHighU)                  \
  V(MipsDiv)                       \
  V(MipsDivU)                      \
  V(MipsMod)                       \
  V(MipsModU)                      \
  V(MipsAnd)                       \
  V(MipsOr)                        \
  V(MipsNor)                       \
  V(MipsXor)                       \
  V(MipsClz)                       \
  V(MipsCtz)                       \
  V(MipsPopcnt)                    \
  V(MipsLsa)                       \
  V(MipsShl)                       \
  V(MipsShr)                       \
  V(MipsSar)                       \
  V(MipsShlPair)                   \
  V(MipsShrPair)                   \
  V(MipsSarPair)                   \
  V(MipsExt)                       \
  V(MipsIns)                       \
  V(MipsRor)                       \
  V(MipsMov)                       \
  V(MipsTst)                       \
  V(MipsCmp)                       \
  V(MipsCmpS)                      \
  V(MipsAddS)                      \
  V(MipsSubS)                      \
  V(MipsMulS)                      \
  V(MipsDivS)                      \
  V(MipsModS)                      \
  V(MipsAbsS)                      \
  V(MipsSqrtS)                     \
  V(MipsMaxS)                      \
  V(MipsMinS)                      \
  V(MipsCmpD)                      \
  V(MipsAddD)                      \
  V(MipsSubD)                      \
  V(MipsMulD)                      \
  V(MipsDivD)                      \
  V(MipsModD)                      \
  V(MipsAbsD)                      \
  V(MipsSqrtD)                     \
  V(MipsMaxD)                      \
  V(MipsMinD)                      \
  V(MipsNegS)                      \
  V(MipsNegD)                      \
  V(MipsAddPair)                   \
  V(MipsSubPair)                   \
  V(MipsMulPair)                   \
  V(MipsMaddS)                     \
  V(MipsMaddD)                     \
  V(MipsMsubS)                     \
  V(MipsMsubD)                     \
  V(MipsFloat32RoundDown)          \
  V(MipsFloat32RoundTruncate)      \
  V(MipsFloat32RoundUp)            \
  V(MipsFloat32RoundTiesEven)      \
  V(MipsFloat64RoundDown)          \
  V(MipsFloat64RoundTruncate)      \
  V(MipsFloat64RoundUp)            \
  V(MipsFloat64RoundTiesEven)      \
  V(MipsCvtSD)                     \
  V(MipsCvtDS)                     \
  V(MipsTruncWD)                   \
  V(MipsRoundWD)                   \
  V(MipsFloorWD)                   \
  V(MipsCeilWD)                    \
  V(MipsTruncWS)                   \
  V(MipsRoundWS)                   \
  V(MipsFloorWS)                   \
  V(MipsCeilWS)                    \
  V(MipsTruncUwD)                  \
  V(MipsTruncUwS)                  \
  V(MipsCvtDW)                     \
  V(MipsCvtDUw)                    \
  V(MipsCvtSW)                     \
  V(MipsCvtSUw)                    \
  V(MipsLb)                        \
  V(MipsLbu)                       \
  V(MipsSb)                        \
  V(MipsLh)                        \
  V(MipsUlh)                       \
  V(MipsLhu)                       \
  V(MipsUlhu)                      \
  V(MipsSh)                        \
  V(MipsUsh)                       \
  V(MipsLw)                        \
  V(MipsUlw)                       \
  V(MipsSw)                        \
  V(MipsUsw)                       \
  V(MipsLwc1)                      \
  V(MipsUlwc1)                     \
  V(MipsSwc1)                      \
  V(MipsUswc1)                     \
  V(MipsLdc1)                      \
  V(MipsUldc1)                     \
  V(MipsSdc1)                      \
  V(MipsUsdc1)                     \
  V(MipsFloat64ExtractLowWord32)   \
  V(MipsFloat64ExtractHighWord32)  \
  V(MipsFloat64InsertLowWord32)    \
  V(MipsFloat64InsertHighWord32)   \
  V(MipsFloat64SilenceNaN)         \
  V(MipsFloat32Max)                \
  V(MipsFloat64Max)                \
  V(MipsFloat32Min)                \
  V(MipsFloat64Min)                \
  V(MipsPush)                      \
  V(MipsStoreToStackSlot)          \
  V(MipsByteSwap32)                \
  V(MipsStackClaim)                \
  V(MipsSeb)                       \
  V(MipsSeh)                       \
  V(MipsS128Zero)                  \
  V(MipsI32x4Splat)                \
  V(MipsI32x4ExtractLane)          \
  V(MipsI32x4ReplaceLane)          \
  V(MipsI32x4Add)                  \
  V(MipsI32x4Sub)                  \
  V(MipsF32x4Splat)                \
  V(MipsF32x4ExtractLane)          \
  V(MipsF32x4ReplaceLane)          \
  V(MipsF32x4SConvertI32x4)        \
  V(MipsF32x4UConvertI32x4)        \
  V(MipsI32x4Mul)                  \
  V(MipsI32x4MaxS)                 \
  V(MipsI32x4MinS)                 \
  V(MipsI32x4Eq)                   \
  V(MipsI32x4Ne)                   \
  V(MipsI32x4Shl)                  \
  V(MipsI32x4ShrS)                 \
  V(MipsI32x4ShrU)                 \
  V(MipsI32x4MaxU)                 \
  V(MipsI32x4MinU)                 \
  V(MipsS32x4Select)               \
  V(MipsF32x4Abs)                  \
  V(MipsF32x4Neg)                  \
  V(MipsF32x4RecipApprox)          \
  V(MipsF32x4RecipSqrtApprox)      \
  V(MipsF32x4Add)                  \
  V(MipsF32x4Sub)                  \
  V(MipsF32x4Mul)                  \
  V(MipsF32x4Max)                  \
  V(MipsF32x4Min)                  \
  V(MipsF32x4Eq)                   \
  V(MipsF32x4Ne)                   \
  V(MipsF32x4Lt)                   \
  V(MipsF32x4Le)                   \
  V(MipsI32x4SConvertF32x4)        \
  V(MipsI32x4UConvertF32x4)        \
  V(MipsI32x4Neg)                  \
  V(MipsI32x4LtS)                  \
  V(MipsI32x4LeS)                  \
  V(MipsI32x4LtU)                  \
  V(MipsI32x4LeU)                  \
  V(MipsI16x8Splat)                \
  V(MipsI16x8ExtractLane)          \
  V(MipsI16x8ReplaceLane)          \
  V(MipsI16x8Neg)                  \
  V(MipsI16x8Shl)                  \
  V(MipsI16x8ShrS)                 \
  V(MipsI16x8ShrU)                 \
  V(MipsI16x8Add)                  \
  V(MipsI16x8AddSaturateS)         \
  V(MipsI16x8Sub)                  \
  V(MipsI16x8SubSaturateS)         \
  V(MipsI16x8Mul)                  \
  V(MipsI16x8MaxS)                 \
  V(MipsI16x8MinS)                 \
  V(MipsI16x8Eq)                   \
  V(MipsI16x8Ne)                   \
  V(MipsI16x8LtS)                  \
  V(MipsI16x8LeS)                  \
  V(MipsI16x8AddSaturateU)         \
  V(MipsI16x8SubSaturateU)         \
  V(MipsI16x8MaxU)                 \
  V(MipsI16x8MinU)                 \
  V(MipsI16x8LtU)                  \
  V(MipsI16x8LeU)                  \
  V(MipsI8x16Splat)                \
  V(MipsI8x16ExtractLane)          \
  V(MipsI8x16ReplaceLane)          \
  V(MipsI8x16Neg)                  \
  V(MipsI8x16Shl)                  \
  V(MipsI8x16ShrS)                 \
  V(MipsS16x8Select)               \
  V(MipsS8x16Select)

// Addressing modes represent the "shape" of inputs to an instruction.
// Many instructions support multiple addressing modes. Addressing modes
// are encoded into the InstructionCode of the instruction and tell the
// code generator after register allocation which assembler method to call.
//
// We use the following local notation for addressing modes:
//
// R = register
// O = register or stack slot
// D = double register
// I = immediate (handle, external, int32)
// MRI = [register + immediate]
// MRR = [register + register]
// TODO(plind): Add the new r6 address modes.
#define TARGET_ADDRESSING_MODE_LIST(V) \
  V(MRI) /* [%r0 + K] */               \
  V(MRR) /* [%r0 + %r1] */


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MIPS_INSTRUCTION_CODES_MIPS_H_
