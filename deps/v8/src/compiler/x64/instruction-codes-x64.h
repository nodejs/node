// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_X64_INSTRUCTION_CODES_X64_H_
#define V8_COMPILER_X64_INSTRUCTION_CODES_X64_H_

namespace v8 {
namespace internal {
namespace compiler {

// X64-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(X64Add)                        \
  V(X64Add32)                      \
  V(X64And)                        \
  V(X64And32)                      \
  V(X64Cmp)                        \
  V(X64Cmp32)                      \
  V(X64Test)                       \
  V(X64Test32)                     \
  V(X64Or)                         \
  V(X64Or32)                       \
  V(X64Xor)                        \
  V(X64Xor32)                      \
  V(X64Sub)                        \
  V(X64Sub32)                      \
  V(X64Imul)                       \
  V(X64Imul32)                     \
  V(X64ImulHigh32)                 \
  V(X64UmulHigh32)                 \
  V(X64Idiv)                       \
  V(X64Idiv32)                     \
  V(X64Udiv)                       \
  V(X64Udiv32)                     \
  V(X64Not)                        \
  V(X64Not32)                      \
  V(X64Neg)                        \
  V(X64Neg32)                      \
  V(X64Shl)                        \
  V(X64Shl32)                      \
  V(X64Shr)                        \
  V(X64Shr32)                      \
  V(X64Sar)                        \
  V(X64Sar32)                      \
  V(X64Ror)                        \
  V(X64Ror32)                      \
  V(SSEFloat64Cmp)                 \
  V(SSEFloat64Add)                 \
  V(SSEFloat64Sub)                 \
  V(SSEFloat64Mul)                 \
  V(SSEFloat64Div)                 \
  V(SSEFloat64Mod)                 \
  V(SSEFloat64Sqrt)                \
  V(SSEFloat64Floor)               \
  V(SSEFloat64Ceil)                \
  V(SSEFloat64RoundTruncate)       \
  V(SSECvtss2sd)                   \
  V(SSECvtsd2ss)                   \
  V(SSEFloat64ToInt32)             \
  V(SSEFloat64ToUint32)            \
  V(SSEInt32ToFloat64)             \
  V(SSEUint32ToFloat64)            \
  V(X64Movsxbl)                    \
  V(X64Movzxbl)                    \
  V(X64Movb)                       \
  V(X64Movsxwl)                    \
  V(X64Movzxwl)                    \
  V(X64Movw)                       \
  V(X64Movl)                       \
  V(X64Movsxlq)                    \
  V(X64Movq)                       \
  V(X64Movsd)                      \
  V(X64Movss)                      \
  V(X64Lea32)                      \
  V(X64Lea)                        \
  V(X64Push)                       \
  V(X64StoreWriteBarrier)


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
// I = immediate displacement (32-bit signed integer)

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
  V(M8I)  /* [      %r2*8 + K] */

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_X64_INSTRUCTION_CODES_X64_H_
