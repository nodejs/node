// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_X87_INSTRUCTION_CODES_X87_H_
#define V8_COMPILER_X87_INSTRUCTION_CODES_X87_H_

#include "src/compiler/instruction.h"
#include "src/compiler/instruction-codes.h"
namespace v8 {
namespace internal {
namespace compiler {

// X87-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(X87Add)                        \
  V(X87And)                        \
  V(X87Cmp)                        \
  V(X87Cmp16)                      \
  V(X87Cmp8)                       \
  V(X87Test)                       \
  V(X87Test16)                     \
  V(X87Test8)                      \
  V(X87Or)                         \
  V(X87Xor)                        \
  V(X87Sub)                        \
  V(X87Imul)                       \
  V(X87ImulHigh)                   \
  V(X87UmulHigh)                   \
  V(X87Idiv)                       \
  V(X87Udiv)                       \
  V(X87Not)                        \
  V(X87Neg)                        \
  V(X87Shl)                        \
  V(X87Shr)                        \
  V(X87Sar)                        \
  V(X87AddPair)                    \
  V(X87SubPair)                    \
  V(X87MulPair)                    \
  V(X87ShlPair)                    \
  V(X87ShrPair)                    \
  V(X87SarPair)                    \
  V(X87Ror)                        \
  V(X87Lzcnt)                      \
  V(X87Popcnt)                     \
  V(X87Float32Cmp)                 \
  V(X87Float32Add)                 \
  V(X87Float32Sub)                 \
  V(X87Float32Mul)                 \
  V(X87Float32Div)                 \
  V(X87Float32Abs)                 \
  V(X87Float32Neg)                 \
  V(X87Float32Sqrt)                \
  V(X87Float32Round)               \
  V(X87LoadFloat64Constant)        \
  V(X87Float64Add)                 \
  V(X87Float64Sub)                 \
  V(X87Float64Mul)                 \
  V(X87Float64Div)                 \
  V(X87Float64Mod)                 \
  V(X87Float32Max)                 \
  V(X87Float64Max)                 \
  V(X87Float32Min)                 \
  V(X87Float64Min)                 \
  V(X87Float64Abs)                 \
  V(X87Float64Neg)                 \
  V(X87Int32ToFloat32)             \
  V(X87Uint32ToFloat32)            \
  V(X87Int32ToFloat64)             \
  V(X87Float32ToFloat64)           \
  V(X87Uint32ToFloat64)            \
  V(X87Float64ToInt32)             \
  V(X87Float32ToInt32)             \
  V(X87Float32ToUint32)            \
  V(X87Float64ToFloat32)           \
  V(X87Float64ToUint32)            \
  V(X87Float64ExtractHighWord32)   \
  V(X87Float64ExtractLowWord32)    \
  V(X87Float64InsertHighWord32)    \
  V(X87Float64InsertLowWord32)     \
  V(X87Float64Sqrt)                \
  V(X87Float64Round)               \
  V(X87Float64Cmp)                 \
  V(X87Float64SilenceNaN)          \
  V(X87Movsxbl)                    \
  V(X87Movzxbl)                    \
  V(X87Movb)                       \
  V(X87Movsxwl)                    \
  V(X87Movzxwl)                    \
  V(X87Movw)                       \
  V(X87Movl)                       \
  V(X87Movss)                      \
  V(X87Movsd)                      \
  V(X87Lea)                        \
  V(X87BitcastFI)                  \
  V(X87BitcastIF)                  \
  V(X87Push)                       \
  V(X87PushFloat64)                \
  V(X87PushFloat32)                \
  V(X87Poke)                       \
  V(X87StackCheck)                 \
  V(X87Xchgb)                      \
  V(X87Xchgw)                      \
  V(X87Xchgl)

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
  V(MI)   /* [              K] */

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_X87_INSTRUCTION_CODES_X87_H_
