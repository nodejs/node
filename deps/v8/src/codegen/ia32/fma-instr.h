// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef V8_CODEGEN_IA32_FMA_INSTR_H_
#define V8_CODEGEN_IA32_FMA_INSTR_H_

#define FMA_SD_INSTRUCTION_LIST(V)          \
  V(vfmadd132sd, L128, 66, 0F, 38, W1, 99)  \
  V(vfmadd213sd, L128, 66, 0F, 38, W1, a9)  \
  V(vfmadd231sd, L128, 66, 0F, 38, W1, b9)  \
  V(vfmsub132sd, L128, 66, 0F, 38, W1, 9b)  \
  V(vfmsub213sd, L128, 66, 0F, 38, W1, ab)  \
  V(vfmsub231sd, L128, 66, 0F, 38, W1, bb)  \
  V(vfnmadd132sd, L128, 66, 0F, 38, W1, 9d) \
  V(vfnmadd213sd, L128, 66, 0F, 38, W1, ad) \
  V(vfnmadd231sd, L128, 66, 0F, 38, W1, bd) \
  V(vfnmsub132sd, L128, 66, 0F, 38, W1, 9f) \
  V(vfnmsub213sd, L128, 66, 0F, 38, W1, af) \
  V(vfnmsub231sd, L128, 66, 0F, 38, W1, bf)

#define FMA_SS_INSTRUCTION_LIST(V)         \
  V(vfmadd132ss, LIG, 66, 0F, 38, W0, 99)  \
  V(vfmadd213ss, LIG, 66, 0F, 38, W0, a9)  \
  V(vfmadd231ss, LIG, 66, 0F, 38, W0, b9)  \
  V(vfmsub132ss, LIG, 66, 0F, 38, W0, 9b)  \
  V(vfmsub213ss, LIG, 66, 0F, 38, W0, ab)  \
  V(vfmsub231ss, LIG, 66, 0F, 38, W0, bb)  \
  V(vfnmadd132ss, LIG, 66, 0F, 38, W0, 9d) \
  V(vfnmadd213ss, LIG, 66, 0F, 38, W0, ad) \
  V(vfnmadd231ss, LIG, 66, 0F, 38, W0, bd) \
  V(vfnmsub132ss, LIG, 66, 0F, 38, W0, 9f) \
  V(vfnmsub213ss, LIG, 66, 0F, 38, W0, af) \
  V(vfnmsub231ss, LIG, 66, 0F, 38, W0, bf)

#define FMA_PS_INSTRUCTION_LIST(V)          \
  V(vfmadd132ps, L128, 66, 0F, 38, W0, 98)  \
  V(vfmadd213ps, L128, 66, 0F, 38, W0, a8)  \
  V(vfmadd231ps, L128, 66, 0F, 38, W0, b8)  \
  V(vfnmadd132ps, L128, 66, 0F, 38, W0, 9c) \
  V(vfnmadd213ps, L128, 66, 0F, 38, W0, ac) \
  V(vfnmadd231ps, L128, 66, 0F, 38, W0, bc)

#define FMA_PD_INSTRUCTION_LIST(V)          \
  V(vfmadd132pd, L128, 66, 0F, 38, W1, 98)  \
  V(vfmadd213pd, L128, 66, 0F, 38, W1, a8)  \
  V(vfmadd231pd, L128, 66, 0F, 38, W1, b8)  \
  V(vfnmadd132pd, L128, 66, 0F, 38, W1, 9c) \
  V(vfnmadd213pd, L128, 66, 0F, 38, W1, ac) \
  V(vfnmadd231pd, L128, 66, 0F, 38, W1, bc)

#define FMA_INSTRUCTION_LIST(V) \
  FMA_SD_INSTRUCTION_LIST(V)    \
  FMA_SS_INSTRUCTION_LIST(V)    \
  FMA_PS_INSTRUCTION_LIST(V)    \
  FMA_PD_INSTRUCTION_LIST(V)

#endif  // V8_CODEGEN_IA32_FMA_INSTR_H_
