// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_IA32_INSTRUCTION_CODES_IA32_H_
#define V8_COMPILER_IA32_INSTRUCTION_CODES_IA32_H_

namespace v8 {
namespace internal {
namespace compiler {

// IA32-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(IA32Add)                       \
  V(IA32And)                       \
  V(IA32Cmp)                       \
  V(IA32Test)                      \
  V(IA32Or)                        \
  V(IA32Xor)                       \
  V(IA32Sub)                       \
  V(IA32Imul)                      \
  V(IA32Idiv)                      \
  V(IA32Udiv)                      \
  V(IA32Not)                       \
  V(IA32Neg)                       \
  V(IA32Shl)                       \
  V(IA32Shr)                       \
  V(IA32Sar)                       \
  V(IA32Ror)                       \
  V(SSEFloat64Cmp)                 \
  V(SSEFloat64Add)                 \
  V(SSEFloat64Sub)                 \
  V(SSEFloat64Mul)                 \
  V(SSEFloat64Div)                 \
  V(SSEFloat64Mod)                 \
  V(SSEFloat64Sqrt)                \
  V(SSECvtss2sd)                   \
  V(SSECvtsd2ss)                   \
  V(SSEFloat64ToInt32)             \
  V(SSEFloat64ToUint32)            \
  V(SSEInt32ToFloat64)             \
  V(SSEUint32ToFloat64)            \
  V(IA32Movsxbl)                   \
  V(IA32Movzxbl)                   \
  V(IA32Movb)                      \
  V(IA32Movsxwl)                   \
  V(IA32Movzxwl)                   \
  V(IA32Movw)                      \
  V(IA32Movl)                      \
  V(IA32Movss)                     \
  V(IA32Movsd)                     \
  V(IA32Push)                      \
  V(IA32StoreWriteBarrier)


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

#endif  // V8_COMPILER_IA32_INSTRUCTION_CODES_IA32_H_
