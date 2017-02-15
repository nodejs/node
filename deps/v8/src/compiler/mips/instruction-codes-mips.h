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
  V(MipsMaddfS)                    \
  V(MipsMaddfD)                    \
  V(MipsMsubS)                     \
  V(MipsMsubD)                     \
  V(MipsMsubfS)                    \
  V(MipsMsubfD)                    \
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
  V(MipsSeh)

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
