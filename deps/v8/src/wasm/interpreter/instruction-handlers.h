// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_INTERPRETER_INSTRUCTION_HANDLERS_H_
#define V8_WASM_INTERPRETER_INSTRUCTION_HANDLERS_H_

#define GENERATE_MEM64_INSTR_HANDLER(V, name) V(name##_Idx64)

#define FOREACH_MEM64_LOAD_STORE_INSTR_HANDLER(V) \
  FOREACH_LOAD_STORE_INSTR_HANDLER(GENERATE_MEM64_INSTR_HANDLER, V)

#define FOREACH_LOAD_STORE_INSTR_HANDLER(V, ...)           \
  /* LoadMem */                                            \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I32LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I32LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I32LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I32LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem32S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem32U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_I64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_F32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2r_F64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem32S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem32U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_F32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_F64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I32LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I32LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I32LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I32LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem32S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem32U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_I64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_F32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2r_F64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem8S)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem8U)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem16S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem16U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem32S)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem32U)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F32LoadMem)             \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F64LoadMem)             \
  /* LoadMem_LocalSet */                                   \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem8S_LocalSet)  \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem8U_LocalSet)  \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem16S_LocalSet) \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem16U_LocalSet) \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem8S_LocalSet)  \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem8U_LocalSet)  \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem16S_LocalSet) \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem16U_LocalSet) \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem32S_LocalSet) \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem32U_LocalSet) \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadMem_LocalSet)    \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadMem_LocalSet)    \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F32LoadMem_LocalSet)    \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F64LoadMem_LocalSet)    \
  /* StoreMem */                                           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32StoreMem8)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32StoreMem16)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64StoreMem8)           \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64StoreMem16)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64StoreMem32)          \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_F32StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_F64StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32StoreMem8)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32StoreMem16)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64StoreMem8)           \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64StoreMem16)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64StoreMem32)          \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F32StoreMem)            \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F64StoreMem)            \
  /* LoadStoreMem */                                       \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I32LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_I64LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_F32LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) r2s_F64LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I32LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_I64LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F32LoadStoreMem)        \
  V(__VA_ARGS__ __VA_OPT__(, ) s2s_F64LoadStoreMem)

#define FOREACH_LOAD_STORE_DUPLICATED_INSTR_HANDLER(V) \
  /* LoadMem_LocalSet */                               \
  V(r2s_I32LoadMem8S_LocalSet)                         \
  V(r2s_I32LoadMem8U_LocalSet)                         \
  V(r2s_I32LoadMem16S_LocalSet)                        \
  V(r2s_I32LoadMem16U_LocalSet)                        \
  V(r2s_I64LoadMem8S_LocalSet)                         \
  V(r2s_I64LoadMem8U_LocalSet)                         \
  V(r2s_I64LoadMem16S_LocalSet)                        \
  V(r2s_I64LoadMem16U_LocalSet)                        \
  V(r2s_I64LoadMem32S_LocalSet)                        \
  V(r2s_I64LoadMem32U_LocalSet)                        \
  V(r2s_I32LoadMem_LocalSet)                           \
  V(r2s_I64LoadMem_LocalSet)                           \
  V(r2s_F32LoadMem_LocalSet)                           \
  V(r2s_F64LoadMem_LocalSet)                           \
  /* LocalGet_StoreMem */                              \
  V(s2s_LocalGet_I32StoreMem8)                         \
  V(s2s_LocalGet_I32StoreMem16)                        \
  V(s2s_LocalGet_I64StoreMem8)                         \
  V(s2s_LocalGet_I64StoreMem16)                        \
  V(s2s_LocalGet_I64StoreMem32)                        \
  V(s2s_LocalGet_I32StoreMem)                          \
  V(s2s_LocalGet_I64StoreMem)                          \
  V(s2s_LocalGet_F32StoreMem)                          \
  V(s2s_LocalGet_F64StoreMem)

#define FOREACH_NO_BOUNDSCHECK_INSTR_HANDLER(V) \
  /* GlobalGet */                               \
  V(s2r_I32GlobalGet)                           \
  V(s2r_I64GlobalGet)                           \
  V(s2r_F32GlobalGet)                           \
  V(s2r_F64GlobalGet)                           \
  V(s2s_I32GlobalGet)                           \
  V(s2s_I64GlobalGet)                           \
  V(s2s_F32GlobalGet)                           \
  V(s2s_F64GlobalGet)                           \
  V(s2s_S128GlobalGet)                          \
  V(s2s_RefGlobalGet)                           \
  /* GlobalSet */                               \
  V(r2s_I32GlobalSet)                           \
  V(r2s_I64GlobalSet)                           \
  V(r2s_F32GlobalSet)                           \
  V(r2s_F64GlobalSet)                           \
  V(s2s_I32GlobalSet)                           \
  V(s2s_I64GlobalSet)                           \
  V(s2s_F32GlobalSet)                           \
  V(s2s_F64GlobalSet)                           \
  V(s2s_S128GlobalSet)                          \
  V(s2s_RefGlobalSet)                           \
  /* Drop */                                    \
  V(r2s_I32Drop)                                \
  V(r2s_I64Drop)                                \
  V(r2s_F32Drop)                                \
  V(r2s_F64Drop)                                \
  V(r2s_RefDrop)                                \
  V(s2s_I32Drop)                                \
  V(s2s_I64Drop)                                \
  V(s2s_F32Drop)                                \
  V(s2s_F64Drop)                                \
  V(s2s_S128Drop)                               \
  V(s2s_RefDrop)                                \
  /* Select */                                  \
  V(r2r_I32Select)                              \
  V(r2r_I64Select)                              \
  V(r2r_F32Select)                              \
  V(r2r_F64Select)                              \
  V(r2s_I32Select)                              \
  V(r2s_I64Select)                              \
  V(r2s_F32Select)                              \
  V(r2s_F64Select)                              \
  V(r2s_S128Select)                             \
  V(r2s_RefSelect)                              \
  V(s2r_I32Select)                              \
  V(s2r_I64Select)                              \
  V(s2r_F32Select)                              \
  V(s2r_F64Select)                              \
  V(s2s_I32Select)                              \
  V(s2s_I64Select)                              \
  V(s2s_F32Select)                              \
  V(s2s_F64Select)                              \
  V(s2s_S128Select)                             \
  V(s2s_RefSelect)                              \
  /* Binary arithmetic operators. */            \
  V(r2r_I32Add)                                 \
  V(r2r_I32Sub)                                 \
  V(r2r_I32Mul)                                 \
  V(r2r_I32And)                                 \
  V(r2r_I32Ior)                                 \
  V(r2r_I32Xor)                                 \
  V(r2r_I64Add)                                 \
  V(r2r_I64Sub)                                 \
  V(r2r_I64Mul)                                 \
  V(r2r_I64And)                                 \
  V(r2r_I64Ior)                                 \
  V(r2r_I64Xor)                                 \
  V(r2r_F32Add)                                 \
  V(r2r_F32Sub)                                 \
  V(r2r_F32Mul)                                 \
  V(r2r_F64Add)                                 \
  V(r2r_F64Sub)                                 \
  V(r2r_F64Mul)                                 \
  V(r2r_I32DivS)                                \
  V(r2r_I64DivS)                                \
  V(r2r_I32DivU)                                \
  V(r2r_I64DivU)                                \
  V(r2r_F32Div)                                 \
  V(r2r_F64Div)                                 \
  V(r2r_I32RemS)                                \
  V(r2r_I64RemS)                                \
  V(r2r_I32RemU)                                \
  V(r2r_I64RemU)                                \
  V(r2s_I32Add)                                 \
  V(r2s_I32Sub)                                 \
  V(r2s_I32Mul)                                 \
  V(r2s_I32And)                                 \
  V(r2s_I32Ior)                                 \
  V(r2s_I32Xor)                                 \
  V(r2s_I64Add)                                 \
  V(r2s_I64Sub)                                 \
  V(r2s_I64Mul)                                 \
  V(r2s_I64And)                                 \
  V(r2s_I64Ior)                                 \
  V(r2s_I64Xor)                                 \
  V(r2s_F32Add)                                 \
  V(r2s_F32Sub)                                 \
  V(r2s_F32Mul)                                 \
  V(r2s_F64Add)                                 \
  V(r2s_F64Sub)                                 \
  V(r2s_F64Mul)                                 \
  V(r2s_I32DivS)                                \
  V(r2s_I64DivS)                                \
  V(r2s_I32DivU)                                \
  V(r2s_I64DivU)                                \
  V(r2s_F32Div)                                 \
  V(r2s_F64Div)                                 \
  V(r2s_I32RemS)                                \
  V(r2s_I64RemS)                                \
  V(r2s_I32RemU)                                \
  V(r2s_I64RemU)                                \
  V(s2r_I32Add)                                 \
  V(s2r_I32Sub)                                 \
  V(s2r_I32Mul)                                 \
  V(s2r_I32And)                                 \
  V(s2r_I32Ior)                                 \
  V(s2r_I32Xor)                                 \
  V(s2r_I64Add)                                 \
  V(s2r_I64Sub)                                 \
  V(s2r_I64Mul)                                 \
  V(s2r_I64And)                                 \
  V(s2r_I64Ior)                                 \
  V(s2r_I64Xor)                                 \
  V(s2r_F32Add)                                 \
  V(s2r_F32Sub)                                 \
  V(s2r_F32Mul)                                 \
  V(s2r_F64Add)                                 \
  V(s2r_F64Sub)                                 \
  V(s2r_F64Mul)                                 \
  V(s2r_I32DivS)                                \
  V(s2r_I64DivS)                                \
  V(s2r_I32DivU)                                \
  V(s2r_I64DivU)                                \
  V(s2r_F32Div)                                 \
  V(s2r_F64Div)                                 \
  V(s2r_I32RemS)                                \
  V(s2r_I64RemS)                                \
  V(s2r_I32RemU)                                \
  V(s2r_I64RemU)                                \
  V(s2s_I32Add)                                 \
  V(s2s_I32Sub)                                 \
  V(s2s_I32Mul)                                 \
  V(s2s_I32And)                                 \
  V(s2s_I32Ior)                                 \
  V(s2s_I32Xor)                                 \
  V(s2s_I64Add)                                 \
  V(s2s_I64Sub)                                 \
  V(s2s_I64Mul)                                 \
  V(s2s_I64And)                                 \
  V(s2s_I64Ior)                                 \
  V(s2s_I64Xor)                                 \
  V(s2s_F32Add)                                 \
  V(s2s_F32Sub)                                 \
  V(s2s_F32Mul)                                 \
  V(s2s_F64Add)                                 \
  V(s2s_F64Sub)                                 \
  V(s2s_F64Mul)                                 \
  V(s2s_I32DivS)                                \
  V(s2s_I64DivS)                                \
  V(s2s_I32DivU)                                \
  V(s2s_I64DivU)                                \
  V(s2s_F32Div)                                 \
  V(s2s_F64Div)                                 \
  V(s2s_I32RemS)                                \
  V(s2s_I64RemS)                                \
  V(s2s_I32RemU)                                \
  V(s2s_I64RemU)                                \
  /* Comparison operators. */                   \
  V(r2r_I32Eq)                                  \
  V(r2r_I32Ne)                                  \
  V(r2r_I32LtU)                                 \
  V(r2r_I32LeU)                                 \
  V(r2r_I32GtU)                                 \
  V(r2r_I32GeU)                                 \
  V(r2r_I32LtS)                                 \
  V(r2r_I32LeS)                                 \
  V(r2r_I32GtS)                                 \
  V(r2r_I32GeS)                                 \
  V(r2r_I64Eq)                                  \
  V(r2r_I64Ne)                                  \
  V(r2r_I64LtU)                                 \
  V(r2r_I64LeU)                                 \
  V(r2r_I64GtU)                                 \
  V(r2r_I64GeU)                                 \
  V(r2r_I64LtS)                                 \
  V(r2r_I64LeS)                                 \
  V(r2r_I64GtS)                                 \
  V(r2r_I64GeS)                                 \
  V(r2r_F32Eq)                                  \
  V(r2r_F32Ne)                                  \
  V(r2r_F32Lt)                                  \
  V(r2r_F32Le)                                  \
  V(r2r_F32Gt)                                  \
  V(r2r_F32Ge)                                  \
  V(r2r_F64Eq)                                  \
  V(r2r_F64Ne)                                  \
  V(r2r_F64Lt)                                  \
  V(r2r_F64Le)                                  \
  V(r2r_F64Gt)                                  \
  V(r2r_F64Ge)                                  \
  V(r2s_I32Eq)                                  \
  V(r2s_I32Ne)                                  \
  V(r2s_I32LtU)                                 \
  V(r2s_I32LeU)                                 \
  V(r2s_I32GtU)                                 \
  V(r2s_I32GeU)                                 \
  V(r2s_I32LtS)                                 \
  V(r2s_I32LeS)                                 \
  V(r2s_I32GtS)                                 \
  V(r2s_I32GeS)                                 \
  V(r2s_I64Eq)                                  \
  V(r2s_I64Ne)                                  \
  V(r2s_I64LtU)                                 \
  V(r2s_I64LeU)                                 \
  V(r2s_I64GtU)                                 \
  V(r2s_I64GeU)                                 \
  V(r2s_I64LtS)                                 \
  V(r2s_I64LeS)                                 \
  V(r2s_I64GtS)                                 \
  V(r2s_I64GeS)                                 \
  V(r2s_F32Eq)                                  \
  V(r2s_F32Ne)                                  \
  V(r2s_F32Lt)                                  \
  V(r2s_F32Le)                                  \
  V(r2s_F32Gt)                                  \
  V(r2s_F32Ge)                                  \
  V(r2s_F64Eq)                                  \
  V(r2s_F64Ne)                                  \
  V(r2s_F64Lt)                                  \
  V(r2s_F64Le)                                  \
  V(r2s_F64Gt)                                  \
  V(r2s_F64Ge)                                  \
  V(s2r_I32Eq)                                  \
  V(s2r_I32Ne)                                  \
  V(s2r_I32LtU)                                 \
  V(s2r_I32LeU)                                 \
  V(s2r_I32GtU)                                 \
  V(s2r_I32GeU)                                 \
  V(s2r_I32LtS)                                 \
  V(s2r_I32LeS)                                 \
  V(s2r_I32GtS)                                 \
  V(s2r_I32GeS)                                 \
  V(s2r_I64Eq)                                  \
  V(s2r_I64Ne)                                  \
  V(s2r_I64LtU)                                 \
  V(s2r_I64LeU)                                 \
  V(s2r_I64GtU)                                 \
  V(s2r_I64GeU)                                 \
  V(s2r_I64LtS)                                 \
  V(s2r_I64LeS)                                 \
  V(s2r_I64GtS)                                 \
  V(s2r_I64GeS)                                 \
  V(s2r_F32Eq)                                  \
  V(s2r_F32Ne)                                  \
  V(s2r_F32Lt)                                  \
  V(s2r_F32Le)                                  \
  V(s2r_F32Gt)                                  \
  V(s2r_F32Ge)                                  \
  V(s2r_F64Eq)                                  \
  V(s2r_F64Ne)                                  \
  V(s2r_F64Lt)                                  \
  V(s2r_F64Le)                                  \
  V(s2r_F64Gt)                                  \
  V(s2r_F64Ge)                                  \
  V(s2s_I32Eq)                                  \
  V(s2s_I32Ne)                                  \
  V(s2s_I32LtU)                                 \
  V(s2s_I32LeU)                                 \
  V(s2s_I32GtU)                                 \
  V(s2s_I32GeU)                                 \
  V(s2s_I32LtS)                                 \
  V(s2s_I32LeS)                                 \
  V(s2s_I32GtS)                                 \
  V(s2s_I32GeS)                                 \
  V(s2s_I64Eq)                                  \
  V(s2s_I64Ne)                                  \
  V(s2s_I64LtU)                                 \
  V(s2s_I64LeU)                                 \
  V(s2s_I64GtU)                                 \
  V(s2s_I64GeU)                                 \
  V(s2s_I64LtS)                                 \
  V(s2s_I64LeS)                                 \
  V(s2s_I64GtS)                                 \
  V(s2s_I64GeS)                                 \
  V(s2s_F32Eq)                                  \
  V(s2s_F32Ne)                                  \
  V(s2s_F32Lt)                                  \
  V(s2s_F32Le)                                  \
  V(s2s_F32Gt)                                  \
  V(s2s_F32Ge)                                  \
  V(s2s_F64Eq)                                  \
  V(s2s_F64Ne)                                  \
  V(s2s_F64Lt)                                  \
  V(s2s_F64Le)                                  \
  V(s2s_F64Gt)                                  \
  V(s2s_F64Ge)                                  \
  /* More binary operators. */                  \
  V(r2r_I32Shl)                                 \
  V(r2r_I32ShrU)                                \
  V(r2r_I32ShrS)                                \
  V(r2r_I64Shl)                                 \
  V(r2r_I64ShrU)                                \
  V(r2r_I64ShrS)                                \
  V(r2r_I32Rol)                                 \
  V(r2r_I32Ror)                                 \
  V(r2r_I64Rol)                                 \
  V(r2r_I64Ror)                                 \
  V(r2r_F32Min)                                 \
  V(r2r_F32Max)                                 \
  V(r2r_F64Min)                                 \
  V(r2r_F64Max)                                 \
  V(r2r_F32CopySign)                            \
  V(r2r_F64CopySign)                            \
  V(r2s_I32Shl)                                 \
  V(r2s_I32ShrU)                                \
  V(r2s_I32ShrS)                                \
  V(r2s_I64Shl)                                 \
  V(r2s_I64ShrU)                                \
  V(r2s_I64ShrS)                                \
  V(r2s_I32Rol)                                 \
  V(r2s_I32Ror)                                 \
  V(r2s_I64Rol)                                 \
  V(r2s_I64Ror)                                 \
  V(r2s_F32Min)                                 \
  V(r2s_F32Max)                                 \
  V(r2s_F64Min)                                 \
  V(r2s_F64Max)                                 \
  V(r2s_F32CopySign)                            \
  V(r2s_F64CopySign)                            \
  V(s2r_I32Shl)                                 \
  V(s2r_I32ShrU)                                \
  V(s2r_I32ShrS)                                \
  V(s2r_I64Shl)                                 \
  V(s2r_I64ShrU)                                \
  V(s2r_I64ShrS)                                \
  V(s2r_I32Rol)                                 \
  V(s2r_I32Ror)                                 \
  V(s2r_I64Rol)                                 \
  V(s2r_I64Ror)                                 \
  V(s2r_F32Min)                                 \
  V(s2r_F32Max)                                 \
  V(s2r_F64Min)                                 \
  V(s2r_F64Max)                                 \
  V(s2r_F32CopySign)                            \
  V(s2r_F64CopySign)                            \
  V(s2s_I32Shl)                                 \
  V(s2s_I32ShrU)                                \
  V(s2s_I32ShrS)                                \
  V(s2s_I64Shl)                                 \
  V(s2s_I64ShrU)                                \
  V(s2s_I64ShrS)                                \
  V(s2s_I32Rol)                                 \
  V(s2s_I32Ror)                                 \
  V(s2s_I64Rol)                                 \
  V(s2s_I64Ror)                                 \
  V(s2s_F32Min)                                 \
  V(s2s_F32Max)                                 \
  V(s2s_F64Min)                                 \
  V(s2s_F64Max)                                 \
  V(s2s_F32CopySign)                            \
  V(s2s_F64CopySign)                            \
  /* Unary operators. */                        \
  V(r2r_F32Abs)                                 \
  V(r2r_F32Neg)                                 \
  V(r2r_F32Ceil)                                \
  V(r2r_F32Floor)                               \
  V(r2r_F32Trunc)                               \
  V(r2r_F32NearestInt)                          \
  V(r2r_F32Sqrt)                                \
  V(r2r_F64Abs)                                 \
  V(r2r_F64Neg)                                 \
  V(r2r_F64Ceil)                                \
  V(r2r_F64Floor)                               \
  V(r2r_F64Trunc)                               \
  V(r2r_F64NearestInt)                          \
  V(r2r_F64Sqrt)                                \
  V(r2s_F32Abs)                                 \
  V(r2s_F32Neg)                                 \
  V(r2s_F32Ceil)                                \
  V(r2s_F32Floor)                               \
  V(r2s_F32Trunc)                               \
  V(r2s_F32NearestInt)                          \
  V(r2s_F32Sqrt)                                \
  V(r2s_F64Abs)                                 \
  V(r2s_F64Neg)                                 \
  V(r2s_F64Ceil)                                \
  V(r2s_F64Floor)                               \
  V(r2s_F64Trunc)                               \
  V(r2s_F64NearestInt)                          \
  V(r2s_F64Sqrt)                                \
  V(s2r_F32Abs)                                 \
  V(s2r_F32Neg)                                 \
  V(s2r_F32Ceil)                                \
  V(s2r_F32Floor)                               \
  V(s2r_F32Trunc)                               \
  V(s2r_F32NearestInt)                          \
  V(s2r_F32Sqrt)                                \
  V(s2r_F64Abs)                                 \
  V(s2r_F64Neg)                                 \
  V(s2r_F64Ceil)                                \
  V(s2r_F64Floor)                               \
  V(s2r_F64Trunc)                               \
  V(s2r_F64NearestInt)                          \
  V(s2r_F64Sqrt)                                \
  V(s2s_F32Abs)                                 \
  V(s2s_F32Neg)                                 \
  V(s2s_F32Ceil)                                \
  V(s2s_F32Floor)                               \
  V(s2s_F32Trunc)                               \
  V(s2s_F32NearestInt)                          \
  V(s2s_F32Sqrt)                                \
  V(s2s_F64Abs)                                 \
  V(s2s_F64Neg)                                 \
  V(s2s_F64Ceil)                                \
  V(s2s_F64Floor)                               \
  V(s2s_F64Trunc)                               \
  V(s2s_F64NearestInt)                          \
  V(s2s_F64Sqrt)                                \
  /* Numeric conversion operators. */           \
  V(r2r_I32ConvertI64)                          \
  V(r2r_I64SConvertF32)                         \
  V(r2r_I64SConvertF64)                         \
  V(r2r_I64UConvertF32)                         \
  V(r2r_I64UConvertF64)                         \
  V(r2r_I32SConvertF32)                         \
  V(r2r_I32UConvertF32)                         \
  V(r2r_I32SConvertF64)                         \
  V(r2r_I32UConvertF64)                         \
  V(r2r_I64SConvertI32)                         \
  V(r2r_I64UConvertI32)                         \
  V(r2r_F32SConvertI32)                         \
  V(r2r_F32UConvertI32)                         \
  V(r2r_F32SConvertI64)                         \
  V(r2r_F32UConvertI64)                         \
  V(r2r_F32ConvertF64)                          \
  V(r2r_F64SConvertI32)                         \
  V(r2r_F64UConvertI32)                         \
  V(r2r_F64SConvertI64)                         \
  V(r2r_F64UConvertI64)                         \
  V(r2r_F64ConvertF32)                          \
  V(r2s_I32ConvertI64)                          \
  V(r2s_I64SConvertF32)                         \
  V(r2s_I64SConvertF64)                         \
  V(r2s_I64UConvertF32)                         \
  V(r2s_I64UConvertF64)                         \
  V(r2s_I32SConvertF32)                         \
  V(r2s_I32UConvertF32)                         \
  V(r2s_I32SConvertF64)                         \
  V(r2s_I32UConvertF64)                         \
  V(r2s_I64SConvertI32)                         \
  V(r2s_I64UConvertI32)                         \
  V(r2s_F32SConvertI32)                         \
  V(r2s_F32UConvertI32)                         \
  V(r2s_F32SConvertI64)                         \
  V(r2s_F32UConvertI64)                         \
  V(r2s_F32ConvertF64)                          \
  V(r2s_F64SConvertI32)                         \
  V(r2s_F64UConvertI32)                         \
  V(r2s_F64SConvertI64)                         \
  V(r2s_F64UConvertI64)                         \
  V(r2s_F64ConvertF32)                          \
  V(s2r_I32ConvertI64)                          \
  V(s2r_I64SConvertF32)                         \
  V(s2r_I64SConvertF64)                         \
  V(s2r_I64UConvertF32)                         \
  V(s2r_I64UConvertF64)                         \
  V(s2r_I32SConvertF32)                         \
  V(s2r_I32UConvertF32)                         \
  V(s2r_I32SConvertF64)                         \
  V(s2r_I32UConvertF64)                         \
  V(s2r_I64SConvertI32)                         \
  V(s2r_I64UConvertI32)                         \
  V(s2r_F32SConvertI32)                         \
  V(s2r_F32UConvertI32)                         \
  V(s2r_F32SConvertI64)                         \
  V(s2r_F32UConvertI64)                         \
  V(s2r_F32ConvertF64)                          \
  V(s2r_F64SConvertI32)                         \
  V(s2r_F64UConvertI32)                         \
  V(s2r_F64SConvertI64)                         \
  V(s2r_F64UConvertI64)                         \
  V(s2r_F64ConvertF32)                          \
  V(s2s_I32ConvertI64)                          \
  V(s2s_I64SConvertF32)                         \
  V(s2s_I64SConvertF64)                         \
  V(s2s_I64UConvertF32)                         \
  V(s2s_I64UConvertF64)                         \
  V(s2s_I32SConvertF32)                         \
  V(s2s_I32UConvertF32)                         \
  V(s2s_I32SConvertF64)                         \
  V(s2s_I32UConvertF64)                         \
  V(s2s_I64SConvertI32)                         \
  V(s2s_I64UConvertI32)                         \
  V(s2s_F32SConvertI32)                         \
  V(s2s_F32UConvertI32)                         \
  V(s2s_F32SConvertI64)                         \
  V(s2s_F32UConvertI64)                         \
  V(s2s_F32ConvertF64)                          \
  V(s2s_F64SConvertI32)                         \
  V(s2s_F64UConvertI32)                         \
  V(s2s_F64SConvertI64)                         \
  V(s2s_F64UConvertI64)                         \
  V(s2s_F64ConvertF32)                          \
  /* Numeric reinterpret operators. */          \
  V(r2r_F32ReinterpretI32)                      \
  V(r2r_F64ReinterpretI64)                      \
  V(r2r_I32ReinterpretF32)                      \
  V(r2r_I64ReinterpretF64)                      \
  V(r2s_F32ReinterpretI32)                      \
  V(r2s_F64ReinterpretI64)                      \
  V(r2s_I32ReinterpretF32)                      \
  V(r2s_I64ReinterpretF64)                      \
  V(s2r_F32ReinterpretI32)                      \
  V(s2r_F64ReinterpretI64)                      \
  V(s2r_I32ReinterpretF32)                      \
  V(s2r_I64ReinterpretF64)                      \
  V(s2s_F32ReinterpretI32)                      \
  V(s2s_F64ReinterpretI64)                      \
  V(s2s_I32ReinterpretF32)                      \
  V(s2s_I64ReinterpretF64)                      \
  /* Bit operators. */                          \
  V(r2r_I32Clz)                                 \
  V(r2r_I32Ctz)                                 \
  V(r2r_I32Popcnt)                              \
  V(r2r_I32Eqz)                                 \
  V(r2r_I64Clz)                                 \
  V(r2r_I64Ctz)                                 \
  V(r2r_I64Popcnt)                              \
  V(r2r_I64Eqz)                                 \
  V(r2s_I32Clz)                                 \
  V(r2s_I32Ctz)                                 \
  V(r2s_I32Popcnt)                              \
  V(r2s_I32Eqz)                                 \
  V(r2s_I64Clz)                                 \
  V(r2s_I64Ctz)                                 \
  V(r2s_I64Popcnt)                              \
  V(r2s_I64Eqz)                                 \
  V(s2r_I32Clz)                                 \
  V(s2r_I32Ctz)                                 \
  V(s2r_I32Popcnt)                              \
  V(s2r_I32Eqz)                                 \
  V(s2r_I64Clz)                                 \
  V(s2r_I64Ctz)                                 \
  V(s2r_I64Popcnt)                              \
  V(s2r_I64Eqz)                                 \
  V(s2s_I32Clz)                                 \
  V(s2s_I32Ctz)                                 \
  V(s2s_I32Popcnt)                              \
  V(s2s_I32Eqz)                                 \
  V(s2s_I64Clz)                                 \
  V(s2s_I64Ctz)                                 \
  V(s2s_I64Popcnt)                              \
  V(s2s_I64Eqz)                                 \
  /* Sign extension operators. */               \
  V(r2r_I32SExtendI8)                           \
  V(r2r_I32SExtendI16)                          \
  V(r2r_I64SExtendI8)                           \
  V(r2r_I64SExtendI16)                          \
  V(r2r_I64SExtendI32)                          \
  V(r2s_I32SExtendI8)                           \
  V(r2s_I32SExtendI16)                          \
  V(r2s_I64SExtendI8)                           \
  V(r2s_I64SExtendI16)                          \
  V(r2s_I64SExtendI32)                          \
  V(s2r_I32SExtendI8)                           \
  V(s2r_I32SExtendI16)                          \
  V(s2r_I64SExtendI8)                           \
  V(s2r_I64SExtendI16)                          \
  V(s2r_I64SExtendI32)                          \
  V(s2s_I32SExtendI8)                           \
  V(s2s_I32SExtendI16)                          \
  V(s2s_I64SExtendI8)                           \
  V(s2s_I64SExtendI16)                          \
  V(s2s_I64SExtendI32)                          \
  /* Saturated truncation operators. */         \
  V(r2r_I32SConvertSatF32)                      \
  V(r2r_I32UConvertSatF32)                      \
  V(r2r_I32SConvertSatF64)                      \
  V(r2r_I32UConvertSatF64)                      \
  V(r2r_I64SConvertSatF32)                      \
  V(r2r_I64UConvertSatF32)                      \
  V(r2r_I64SConvertSatF64)                      \
  V(r2r_I64UConvertSatF64)                      \
  V(r2s_I32SConvertSatF32)                      \
  V(r2s_I32UConvertSatF32)                      \
  V(r2s_I32SConvertSatF64)                      \
  V(r2s_I32UConvertSatF64)                      \
  V(r2s_I64SConvertSatF32)                      \
  V(r2s_I64UConvertSatF32)                      \
  V(r2s_I64SConvertSatF64)                      \
  V(r2s_I64UConvertSatF64)                      \
  V(s2r_I32SConvertSatF32)                      \
  V(s2r_I32UConvertSatF32)                      \
  V(s2r_I32SConvertSatF64)                      \
  V(s2r_I32UConvertSatF64)                      \
  V(s2r_I64SConvertSatF32)                      \
  V(s2r_I64UConvertSatF32)                      \
  V(s2r_I64SConvertSatF64)                      \
  V(s2r_I64UConvertSatF64)                      \
  V(s2s_I32SConvertSatF32)                      \
  V(s2s_I32UConvertSatF32)                      \
  V(s2s_I32SConvertSatF64)                      \
  V(s2s_I32UConvertSatF64)                      \
  V(s2s_I64SConvertSatF32)                      \
  V(s2s_I64UConvertSatF32)                      \
  V(s2s_I64SConvertSatF64)                      \
  V(s2s_I64UConvertSatF64)                      \
  /* Other instruction handlers. */             \
  V(s2s_MemoryGrow)                             \
  V(s2s_Memory64Grow)                           \
  V(s2s_MemorySize)                             \
  V(s2s_Memory64Size)                           \
  V(s2s_Return)                                 \
  V(s2s_Branch)                                 \
  V(r2s_BranchIf)                               \
  V(s2s_BranchIf)                               \
  V(r2s_BranchIfWithParams)                     \
  V(s2s_BranchIfWithParams)                     \
  V(r2s_If)                                     \
  V(s2s_If)                                     \
  V(s2s_Else)                                   \
  V(s2s_CallFunction)                           \
  V(s2s_ReturnCall)                             \
  V(s2s_CallImportedFunction)                   \
  V(s2s_ReturnCallImportedFunction)             \
  V(s2s_CallIndirect)                           \
  V(s2s_CallIndirect64)                         \
  V(s2s_ReturnCallIndirect)                     \
  V(s2s_ReturnCallIndirect64)                   \
  V(r2s_BrTable)                                \
  V(s2s_BrTable)                                \
  V(s2s_CopySlotMulti)                          \
  V(s2s_CopySlot_ll)                            \
  V(s2s_CopySlot_lq)                            \
  V(s2s_CopySlot_ql)                            \
  V(s2s_CopySlot_qq)                            \
  V(s2s_CopySlot32)                             \
  V(s2s_CopySlot32x2)                           \
  V(s2s_CopySlot64)                             \
  V(s2s_CopySlot64x2)                           \
  V(s2s_CopySlot128)                            \
  V(s2s_CopySlotRef)                            \
  V(s2s_PreserveCopySlot32)                     \
  V(s2s_PreserveCopySlot64)                     \
  V(s2s_PreserveCopySlot128)                    \
  V(r2s_CopyR0ToSlot32)                         \
  V(r2s_CopyR0ToSlot64)                         \
  V(r2s_CopyFp0ToSlot32)                        \
  V(r2s_CopyFp0ToSlot64)                        \
  V(r2s_PreserveCopyR0ToSlot32)                 \
  V(r2s_PreserveCopyR0ToSlot64)                 \
  V(r2s_PreserveCopyFp0ToSlot32)                \
  V(r2s_PreserveCopyFp0ToSlot64)                \
  V(s2s_RefNull)                                \
  V(s2s_RefIsNull)                              \
  V(s2s_RefFunc)                                \
  V(s2s_RefEq)                                  \
  V(s2s_MemoryInit)                             \
  V(s2s_Memory64Init)                           \
  V(s2s_DataDrop)                               \
  V(s2s_MemoryCopy)                             \
  V(s2s_Memory64Copy)                           \
  V(s2s_MemoryFill)                             \
  V(s2s_Memory64Fill)                           \
  V(s2s_TableGet)                               \
  V(s2s_Table64Get)                             \
  V(s2s_TableSet)                               \
  V(s2s_Table64Set)                             \
  V(s2s_TableInit)                              \
  V(s2s_Table64Init)                            \
  V(s2s_ElemDrop)                               \
  V(s2s_TableCopy)                              \
  V(s2s_Table64Copy_32_64_32)                   \
  V(s2s_Table64Copy_64_32_32)                   \
  V(s2s_Table64Copy_64_64_64)                   \
  V(s2s_TableGrow)                              \
  V(s2s_Table64Grow)                            \
  V(s2s_TableSize)                              \
  V(s2s_Table64Size)                            \
  V(s2s_TableFill)                              \
  V(s2s_Table64Fill)                            \
  V(s2s_Unreachable)                            \
  V(s2s_Unwind)                                 \
  V(s2s_OnLoopBegin)                            \
  V(s2s_OnLoopBeginNoRefSlots)                  \
  /* Exception handling */                      \
  V(s2s_Throw)                                  \
  V(s2s_Rethrow)                                \
  V(s2s_Catch)                                  \
  /* Atomics */                                 \
  V(s2s_AtomicNotify)                           \
  V(s2s_AtomicNotify_Idx64)                     \
  V(s2s_I32AtomicWait)                          \
  V(s2s_I32AtomicWait_Idx64)                    \
  V(s2s_I64AtomicWait)                          \
  V(s2s_I64AtomicWait_Idx64)                    \
  V(s2s_AtomicFence)                            \
  V(s2s_I32AtomicAdd)                           \
  V(s2s_I32AtomicAdd_Idx64)                     \
  V(s2s_I32AtomicAdd8U)                         \
  V(s2s_I32AtomicAdd8U_Idx64)                   \
  V(s2s_I32AtomicAdd16U)                        \
  V(s2s_I32AtomicAdd16U_Idx64)                  \
  V(s2s_I32AtomicSub)                           \
  V(s2s_I32AtomicSub_Idx64)                     \
  V(s2s_I32AtomicSub8U)                         \
  V(s2s_I32AtomicSub8U_Idx64)                   \
  V(s2s_I32AtomicSub16U)                        \
  V(s2s_I32AtomicSub16U_Idx64)                  \
  V(s2s_I32AtomicAnd)                           \
  V(s2s_I32AtomicAnd_Idx64)                     \
  V(s2s_I32AtomicAnd8U)                         \
  V(s2s_I32AtomicAnd8U_Idx64)                   \
  V(s2s_I32AtomicAnd16U)                        \
  V(s2s_I32AtomicAnd16U_Idx64)                  \
  V(s2s_I32AtomicOr)                            \
  V(s2s_I32AtomicOr_Idx64)                      \
  V(s2s_I32AtomicOr8U)                          \
  V(s2s_I32AtomicOr8U_Idx64)                    \
  V(s2s_I32AtomicOr16U)                         \
  V(s2s_I32AtomicOr16U_Idx64)                   \
  V(s2s_I32AtomicXor)                           \
  V(s2s_I32AtomicXor_Idx64)                     \
  V(s2s_I32AtomicXor8U)                         \
  V(s2s_I32AtomicXor8U_Idx64)                   \
  V(s2s_I32AtomicXor16U)                        \
  V(s2s_I32AtomicXor16U_Idx64)                  \
  V(s2s_I32AtomicExchange)                      \
  V(s2s_I32AtomicExchange_Idx64)                \
  V(s2s_I32AtomicExchange8U)                    \
  V(s2s_I32AtomicExchange8U_Idx64)              \
  V(s2s_I32AtomicExchange16U)                   \
  V(s2s_I32AtomicExchange16U_Idx64)             \
  V(s2s_I64AtomicAdd)                           \
  V(s2s_I64AtomicAdd_Idx64)                     \
  V(s2s_I64AtomicAdd8U)                         \
  V(s2s_I64AtomicAdd8U_Idx64)                   \
  V(s2s_I64AtomicAdd16U)                        \
  V(s2s_I64AtomicAdd16U_Idx64)                  \
  V(s2s_I64AtomicAdd32U)                        \
  V(s2s_I64AtomicAdd32U_Idx64)                  \
  V(s2s_I64AtomicSub)                           \
  V(s2s_I64AtomicSub_Idx64)                     \
  V(s2s_I64AtomicSub8U)                         \
  V(s2s_I64AtomicSub8U_Idx64)                   \
  V(s2s_I64AtomicSub16U)                        \
  V(s2s_I64AtomicSub16U_Idx64)                  \
  V(s2s_I64AtomicSub32U)                        \
  V(s2s_I64AtomicSub32U_Idx64)                  \
  V(s2s_I64AtomicAnd)                           \
  V(s2s_I64AtomicAnd_Idx64)                     \
  V(s2s_I64AtomicAnd8U)                         \
  V(s2s_I64AtomicAnd8U_Idx64)                   \
  V(s2s_I64AtomicAnd16U)                        \
  V(s2s_I64AtomicAnd16U_Idx64)                  \
  V(s2s_I64AtomicAnd32U)                        \
  V(s2s_I64AtomicAnd32U_Idx64)                  \
  V(s2s_I64AtomicOr)                            \
  V(s2s_I64AtomicOr_Idx64)                      \
  V(s2s_I64AtomicOr8U)                          \
  V(s2s_I64AtomicOr8U_Idx64)                    \
  V(s2s_I64AtomicOr16U)                         \
  V(s2s_I64AtomicOr16U_Idx64)                   \
  V(s2s_I64AtomicOr32U)                         \
  V(s2s_I64AtomicOr32U_Idx64)                   \
  V(s2s_I64AtomicXor)                           \
  V(s2s_I64AtomicXor_Idx64)                     \
  V(s2s_I64AtomicXor8U)                         \
  V(s2s_I64AtomicXor8U_Idx64)                   \
  V(s2s_I64AtomicXor16U)                        \
  V(s2s_I64AtomicXor16U_Idx64)                  \
  V(s2s_I64AtomicXor32U)                        \
  V(s2s_I64AtomicXor32U_Idx64)                  \
  V(s2s_I64AtomicExchange)                      \
  V(s2s_I64AtomicExchange_Idx64)                \
  V(s2s_I64AtomicExchange8U)                    \
  V(s2s_I64AtomicExchange8U_Idx64)              \
  V(s2s_I64AtomicExchange16U)                   \
  V(s2s_I64AtomicExchange16U_Idx64)             \
  V(s2s_I64AtomicExchange32U)                   \
  V(s2s_I64AtomicExchange32U_Idx64)             \
  V(s2s_I32AtomicCompareExchange)               \
  V(s2s_I32AtomicCompareExchange_Idx64)         \
  V(s2s_I32AtomicCompareExchange8U)             \
  V(s2s_I32AtomicCompareExchange8U_Idx64)       \
  V(s2s_I32AtomicCompareExchange16U)            \
  V(s2s_I32AtomicCompareExchange16U_Idx64)      \
  V(s2s_I64AtomicCompareExchange)               \
  V(s2s_I64AtomicCompareExchange_Idx64)         \
  V(s2s_I64AtomicCompareExchange8U)             \
  V(s2s_I64AtomicCompareExchange8U_Idx64)       \
  V(s2s_I64AtomicCompareExchange16U)            \
  V(s2s_I64AtomicCompareExchange16U_Idx64)      \
  V(s2s_I64AtomicCompareExchange32U)            \
  V(s2s_I64AtomicCompareExchange32U_Idx64)      \
  V(s2s_I32AtomicLoad)                          \
  V(s2s_I32AtomicLoad_Idx64)                    \
  V(s2s_I32AtomicLoad8U)                        \
  V(s2s_I32AtomicLoad8U_Idx64)                  \
  V(s2s_I32AtomicLoad16U)                       \
  V(s2s_I32AtomicLoad16U_Idx64)                 \
  V(s2s_I64AtomicLoad)                          \
  V(s2s_I64AtomicLoad_Idx64)                    \
  V(s2s_I64AtomicLoad8U)                        \
  V(s2s_I64AtomicLoad8U_Idx64)                  \
  V(s2s_I64AtomicLoad16U)                       \
  V(s2s_I64AtomicLoad16U_Idx64)                 \
  V(s2s_I64AtomicLoad32U)                       \
  V(s2s_I64AtomicLoad32U_Idx64)                 \
  V(s2s_I32AtomicStore)                         \
  V(s2s_I32AtomicStore_Idx64)                   \
  V(s2s_I32AtomicStore8U)                       \
  V(s2s_I32AtomicStore8U_Idx64)                 \
  V(s2s_I32AtomicStore16U)                      \
  V(s2s_I32AtomicStore16U_Idx64)                \
  V(s2s_I64AtomicStore)                         \
  V(s2s_I64AtomicStore_Idx64)                   \
  V(s2s_I64AtomicStore8U)                       \
  V(s2s_I64AtomicStore8U_Idx64)                 \
  V(s2s_I64AtomicStore16U)                      \
  V(s2s_I64AtomicStore16U_Idx64)                \
  V(s2s_I64AtomicStore32U)                      \
  V(s2s_I64AtomicStore32U_Idx64)                \
  /* SIMD */                                    \
  V(s2s_SimdF64x2Splat)                         \
  V(s2s_SimdF32x4Splat)                         \
  V(s2s_SimdI64x2Splat)                         \
  V(s2s_SimdI32x4Splat)                         \
  V(s2s_SimdI16x8Splat)                         \
  V(s2s_SimdI8x16Splat)                         \
  V(s2s_SimdF64x2ExtractLane)                   \
  V(s2s_SimdF32x4ExtractLane)                   \
  V(s2s_SimdI64x2ExtractLane)                   \
  V(s2s_SimdI32x4ExtractLane)                   \
  V(s2s_SimdI16x8ExtractLaneS)                  \
  V(s2s_SimdI16x8ExtractLaneU)                  \
  V(s2s_SimdI8x16ExtractLaneS)                  \
  V(s2s_SimdI8x16ExtractLaneU)                  \
  V(s2s_SimdF64x2Add)                           \
  V(s2s_SimdF64x2Sub)                           \
  V(s2s_SimdF64x2Mul)                           \
  V(s2s_SimdF64x2Div)                           \
  V(s2s_SimdF64x2Min)                           \
  V(s2s_SimdF64x2Max)                           \
  V(s2s_SimdF64x2Pmin)                          \
  V(s2s_SimdF64x2Pmax)                          \
  V(s2s_SimdF32x4RelaxedMin)                    \
  V(s2s_SimdF32x4RelaxedMax)                    \
  V(s2s_SimdF64x2RelaxedMin)                    \
  V(s2s_SimdF64x2RelaxedMax)                    \
  V(s2s_SimdF32x4Add)                           \
  V(s2s_SimdF32x4Sub)                           \
  V(s2s_SimdF32x4Mul)                           \
  V(s2s_SimdF32x4Div)                           \
  V(s2s_SimdF32x4Min)                           \
  V(s2s_SimdF32x4Max)                           \
  V(s2s_SimdF32x4Pmin)                          \
  V(s2s_SimdF32x4Pmax)                          \
  V(s2s_SimdI64x2Add)                           \
  V(s2s_SimdI64x2Sub)                           \
  V(s2s_SimdI64x2Mul)                           \
  V(s2s_SimdI32x4Add)                           \
  V(s2s_SimdI32x4Sub)                           \
  V(s2s_SimdI32x4Mul)                           \
  V(s2s_SimdI32x4MinS)                          \
  V(s2s_SimdI32x4MinU)                          \
  V(s2s_SimdI32x4MaxS)                          \
  V(s2s_SimdI32x4MaxU)                          \
  V(s2s_SimdS128And)                            \
  V(s2s_SimdS128Or)                             \
  V(s2s_SimdS128Xor)                            \
  V(s2s_SimdS128AndNot)                         \
  V(s2s_SimdI16x8Add)                           \
  V(s2s_SimdI16x8Sub)                           \
  V(s2s_SimdI16x8Mul)                           \
  V(s2s_SimdI16x8MinS)                          \
  V(s2s_SimdI16x8MinU)                          \
  V(s2s_SimdI16x8MaxS)                          \
  V(s2s_SimdI16x8MaxU)                          \
  V(s2s_SimdI16x8AddSatS)                       \
  V(s2s_SimdI16x8AddSatU)                       \
  V(s2s_SimdI16x8SubSatS)                       \
  V(s2s_SimdI16x8SubSatU)                       \
  V(s2s_SimdI16x8RoundingAverageU)              \
  V(s2s_SimdI16x8Q15MulRSatS)                   \
  V(s2s_SimdI16x8RelaxedQ15MulRS)               \
  V(s2s_SimdI8x16Add)                           \
  V(s2s_SimdI8x16Sub)                           \
  V(s2s_SimdI8x16MinS)                          \
  V(s2s_SimdI8x16MinU)                          \
  V(s2s_SimdI8x16MaxS)                          \
  V(s2s_SimdI8x16MaxU)                          \
  V(s2s_SimdI8x16AddSatS)                       \
  V(s2s_SimdI8x16AddSatU)                       \
  V(s2s_SimdI8x16SubSatS)                       \
  V(s2s_SimdI8x16SubSatU)                       \
  V(s2s_SimdI8x16RoundingAverageU)              \
  V(s2s_SimdF64x2Abs)                           \
  V(s2s_SimdF64x2Neg)                           \
  V(s2s_SimdF64x2Sqrt)                          \
  V(s2s_SimdF64x2Ceil)                          \
  V(s2s_SimdF64x2Floor)                         \
  V(s2s_SimdF64x2Trunc)                         \
  V(s2s_SimdF64x2NearestInt)                    \
  V(s2s_SimdF32x4Abs)                           \
  V(s2s_SimdF32x4Neg)                           \
  V(s2s_SimdF32x4Sqrt)                          \
  V(s2s_SimdF32x4Ceil)                          \
  V(s2s_SimdF32x4Floor)                         \
  V(s2s_SimdF32x4Trunc)                         \
  V(s2s_SimdF32x4NearestInt)                    \
  V(s2s_SimdI64x2Neg)                           \
  V(s2s_SimdI32x4Neg)                           \
  V(s2s_SimdI64x2Abs)                           \
  V(s2s_SimdI32x4Abs)                           \
  V(s2s_SimdS128Not)                            \
  V(s2s_SimdI16x8Neg)                           \
  V(s2s_SimdI16x8Abs)                           \
  V(s2s_SimdI8x16Neg)                           \
  V(s2s_SimdI8x16Abs)                           \
  V(s2s_SimdI8x16Popcnt)                        \
  V(s2s_SimdI8x16BitMask)                       \
  V(s2s_SimdI16x8BitMask)                       \
  V(s2s_SimdI32x4BitMask)                       \
  V(s2s_SimdI64x2BitMask)                       \
  V(s2s_SimdF64x2Eq)                            \
  V(s2s_SimdF64x2Ne)                            \
  V(s2s_SimdF64x2Gt)                            \
  V(s2s_SimdF64x2Ge)                            \
  V(s2s_SimdF64x2Lt)                            \
  V(s2s_SimdF64x2Le)                            \
  V(s2s_SimdF32x4Eq)                            \
  V(s2s_SimdF32x4Ne)                            \
  V(s2s_SimdF32x4Gt)                            \
  V(s2s_SimdF32x4Ge)                            \
  V(s2s_SimdF32x4Lt)                            \
  V(s2s_SimdF32x4Le)                            \
  V(s2s_SimdI64x2Eq)                            \
  V(s2s_SimdI64x2Ne)                            \
  V(s2s_SimdI64x2LtS)                           \
  V(s2s_SimdI64x2GtS)                           \
  V(s2s_SimdI64x2LeS)                           \
  V(s2s_SimdI64x2GeS)                           \
  V(s2s_SimdI32x4Eq)                            \
  V(s2s_SimdI32x4Ne)                            \
  V(s2s_SimdI32x4GtS)                           \
  V(s2s_SimdI32x4GeS)                           \
  V(s2s_SimdI32x4LtS)                           \
  V(s2s_SimdI32x4LeS)                           \
  V(s2s_SimdI32x4GtU)                           \
  V(s2s_SimdI32x4GeU)                           \
  V(s2s_SimdI32x4LtU)                           \
  V(s2s_SimdI32x4LeU)                           \
  V(s2s_SimdI16x8Eq)                            \
  V(s2s_SimdI16x8Ne)                            \
  V(s2s_SimdI16x8GtS)                           \
  V(s2s_SimdI16x8GeS)                           \
  V(s2s_SimdI16x8LtS)                           \
  V(s2s_SimdI16x8LeS)                           \
  V(s2s_SimdI16x8GtU)                           \
  V(s2s_SimdI16x8GeU)                           \
  V(s2s_SimdI16x8LtU)                           \
  V(s2s_SimdI16x8LeU)                           \
  V(s2s_SimdI8x16Eq)                            \
  V(s2s_SimdI8x16Ne)                            \
  V(s2s_SimdI8x16GtS)                           \
  V(s2s_SimdI8x16GeS)                           \
  V(s2s_SimdI8x16LtS)                           \
  V(s2s_SimdI8x16LeS)                           \
  V(s2s_SimdI8x16GtU)                           \
  V(s2s_SimdI8x16GeU)                           \
  V(s2s_SimdI8x16LtU)                           \
  V(s2s_SimdI8x16LeU)                           \
  V(s2s_SimdF64x2ReplaceLane)                   \
  V(s2s_SimdF32x4ReplaceLane)                   \
  V(s2s_SimdI64x2ReplaceLane)                   \
  V(s2s_SimdI32x4ReplaceLane)                   \
  V(s2s_SimdI16x8ReplaceLane)                   \
  V(s2s_SimdI8x16ReplaceLane)                   \
  V(s2s_SimdS128LoadMem)                        \
  V(s2s_SimdS128LoadMem_Idx64)                  \
  V(s2s_SimdS128StoreMem)                       \
  V(s2s_SimdS128StoreMem_Idx64)                 \
  V(s2s_SimdI64x2Shl)                           \
  V(s2s_SimdI64x2ShrS)                          \
  V(s2s_SimdI64x2ShrU)                          \
  V(s2s_SimdI32x4Shl)                           \
  V(s2s_SimdI32x4ShrS)                          \
  V(s2s_SimdI32x4ShrU)                          \
  V(s2s_SimdI16x8Shl)                           \
  V(s2s_SimdI16x8ShrS)                          \
  V(s2s_SimdI16x8ShrU)                          \
  V(s2s_SimdI8x16Shl)                           \
  V(s2s_SimdI8x16ShrS)                          \
  V(s2s_SimdI8x16ShrU)                          \
  V(s2s_SimdI16x8ExtMulLowI8x16S)               \
  V(s2s_SimdI16x8ExtMulHighI8x16S)              \
  V(s2s_SimdI16x8ExtMulLowI8x16U)               \
  V(s2s_SimdI16x8ExtMulHighI8x16U)              \
  V(s2s_SimdI32x4ExtMulLowI16x8S)               \
  V(s2s_SimdI32x4ExtMulHighI16x8S)              \
  V(s2s_SimdI32x4ExtMulLowI16x8U)               \
  V(s2s_SimdI32x4ExtMulHighI16x8U)              \
  V(s2s_SimdI64x2ExtMulLowI32x4S)               \
  V(s2s_SimdI64x2ExtMulHighI32x4S)              \
  V(s2s_SimdI64x2ExtMulLowI32x4U)               \
  V(s2s_SimdI64x2ExtMulHighI32x4U)              \
  V(s2s_SimdF32x4SConvertI32x4)                 \
  V(s2s_SimdF32x4UConvertI32x4)                 \
  V(s2s_SimdI32x4SConvertF32x4)                 \
  V(s2s_SimdI32x4UConvertF32x4)                 \
  V(s2s_SimdI32x4RelaxedTruncF32x4S)            \
  V(s2s_SimdI32x4RelaxedTruncF32x4U)            \
  V(s2s_SimdI64x2SConvertI32x4Low)              \
  V(s2s_SimdI64x2SConvertI32x4High)             \
  V(s2s_SimdI64x2UConvertI32x4Low)              \
  V(s2s_SimdI64x2UConvertI32x4High)             \
  V(s2s_SimdI32x4SConvertI16x8High)             \
  V(s2s_SimdI32x4UConvertI16x8High)             \
  V(s2s_SimdI32x4SConvertI16x8Low)              \
  V(s2s_SimdI32x4UConvertI16x8Low)              \
  V(s2s_SimdI16x8SConvertI8x16High)             \
  V(s2s_SimdI16x8UConvertI8x16High)             \
  V(s2s_SimdI16x8SConvertI8x16Low)              \
  V(s2s_SimdI16x8UConvertI8x16Low)              \
  V(s2s_SimdF64x2ConvertLowI32x4S)              \
  V(s2s_SimdF64x2ConvertLowI32x4U)              \
  V(s2s_SimdI32x4TruncSatF64x2SZero)            \
  V(s2s_SimdI32x4TruncSatF64x2UZero)            \
  V(s2s_SimdI32x4RelaxedTruncF64x2SZero)        \
  V(s2s_SimdI32x4RelaxedTruncF64x2UZero)        \
  V(s2s_SimdF32x4DemoteF64x2Zero)               \
  V(s2s_SimdF64x2PromoteLowF32x4)               \
  V(s2s_SimdI16x8SConvertI32x4)                 \
  V(s2s_SimdI16x8UConvertI32x4)                 \
  V(s2s_SimdI8x16SConvertI16x8)                 \
  V(s2s_SimdI8x16UConvertI16x8)                 \
  V(s2s_SimdI8x16RelaxedLaneSelect)             \
  V(s2s_SimdI16x8RelaxedLaneSelect)             \
  V(s2s_SimdI32x4RelaxedLaneSelect)             \
  V(s2s_SimdI64x2RelaxedLaneSelect)             \
  V(s2s_SimdS128Select)                         \
  V(s2s_SimdI32x4DotI16x8S)                     \
  V(s2s_SimdI16x8DotI8x16I7x16S)                \
  V(s2s_SimdI32x4DotI8x16I7x16AddS)             \
  V(s2s_SimdI8x16RelaxedSwizzle)                \
  V(s2s_SimdI8x16Swizzle)                       \
  V(s2s_SimdV128AnyTrue)                        \
  V(s2s_SimdI8x16Shuffle)                       \
  V(s2s_SimdI64x2AllTrue)                       \
  V(s2s_SimdI32x4AllTrue)                       \
  V(s2s_SimdI16x8AllTrue)                       \
  V(s2s_SimdI8x16AllTrue)                       \
  V(s2s_SimdF32x4Qfma)                          \
  V(s2s_SimdF32x4Qfms)                          \
  V(s2s_SimdF64x2Qfma)                          \
  V(s2s_SimdF64x2Qfms)                          \
  V(s2s_SimdS128Load8Splat)                     \
  V(s2s_SimdS128Load8Splat_Idx64)               \
  V(s2s_SimdS128Load16Splat)                    \
  V(s2s_SimdS128Load16Splat_Idx64)              \
  V(s2s_SimdS128Load32Splat)                    \
  V(s2s_SimdS128Load32Splat_Idx64)              \
  V(s2s_SimdS128Load64Splat)                    \
  V(s2s_SimdS128Load64Splat_Idx64)              \
  V(s2s_SimdS128Load8x8S)                       \
  V(s2s_SimdS128Load8x8S_Idx64)                 \
  V(s2s_SimdS128Load8x8U)                       \
  V(s2s_SimdS128Load8x8U_Idx64)                 \
  V(s2s_SimdS128Load16x4S)                      \
  V(s2s_SimdS128Load16x4S_Idx64)                \
  V(s2s_SimdS128Load16x4U)                      \
  V(s2s_SimdS128Load16x4U_Idx64)                \
  V(s2s_SimdS128Load32x2S)                      \
  V(s2s_SimdS128Load32x2S_Idx64)                \
  V(s2s_SimdS128Load32x2U)                      \
  V(s2s_SimdS128Load32x2U_Idx64)                \
  V(s2s_SimdS128Load32Zero)                     \
  V(s2s_SimdS128Load32Zero_Idx64)               \
  V(s2s_SimdS128Load64Zero)                     \
  V(s2s_SimdS128Load64Zero_Idx64)               \
  V(s2s_SimdS128Load8Lane)                      \
  V(s2s_SimdS128Load8Lane_Idx64)                \
  V(s2s_SimdS128Load16Lane)                     \
  V(s2s_SimdS128Load16Lane_Idx64)               \
  V(s2s_SimdS128Load32Lane)                     \
  V(s2s_SimdS128Load32Lane_Idx64)               \
  V(s2s_SimdS128Load64Lane)                     \
  V(s2s_SimdS128Load64Lane_Idx64)               \
  V(s2s_SimdS128Store8Lane)                     \
  V(s2s_SimdS128Store8Lane_Idx64)               \
  V(s2s_SimdS128Store16Lane)                    \
  V(s2s_SimdS128Store16Lane_Idx64)              \
  V(s2s_SimdS128Store32Lane)                    \
  V(s2s_SimdS128Store32Lane_Idx64)              \
  V(s2s_SimdS128Store64Lane)                    \
  V(s2s_SimdS128Store64Lane_Idx64)              \
  V(s2s_SimdI32x4ExtAddPairwiseI16x8S)          \
  V(s2s_SimdI32x4ExtAddPairwiseI16x8U)          \
  V(s2s_SimdI16x8ExtAddPairwiseI8x16S)          \
  V(s2s_SimdI16x8ExtAddPairwiseI8x16U)          \
  /* GC */                                      \
  V(s2s_BranchOnNull)                           \
  V(s2s_BranchOnNullWithParams)                 \
  V(s2s_BranchOnNonNull)                        \
  V(s2s_BranchOnNonNullWithParams)              \
  V(s2s_BranchOnCast)                           \
  V(s2s_BranchOnCastFail)                       \
  V(s2s_StructNew)                              \
  V(s2s_StructNewDefault)                       \
  V(s2s_I8SStructGet)                           \
  V(s2s_I8UStructGet)                           \
  V(s2s_I16SStructGet)                          \
  V(s2s_I16UStructGet)                          \
  V(s2s_I32StructGet)                           \
  V(s2s_I64StructGet)                           \
  V(s2s_F32StructGet)                           \
  V(s2s_F64StructGet)                           \
  V(s2s_S128StructGet)                          \
  V(s2s_RefStructGet)                           \
  V(s2s_I8StructSet)                            \
  V(s2s_I16StructSet)                           \
  V(s2s_I32StructSet)                           \
  V(s2s_I64StructSet)                           \
  V(s2s_F32StructSet)                           \
  V(s2s_F64StructSet)                           \
  V(s2s_S128StructSet)                          \
  V(s2s_RefStructSet)                           \
  V(s2s_I8ArrayNew)                             \
  V(s2s_I16ArrayNew)                            \
  V(s2s_I32ArrayNew)                            \
  V(s2s_I64ArrayNew)                            \
  V(s2s_F32ArrayNew)                            \
  V(s2s_F64ArrayNew)                            \
  V(s2s_S128ArrayNew)                           \
  V(s2s_RefArrayNew)                            \
  V(s2s_ArrayNewDefault)                        \
  V(s2s_ArrayNewFixed)                          \
  V(s2s_ArrayNewData)                           \
  V(s2s_ArrayNewElem)                           \
  V(s2s_ArrayInitData)                          \
  V(s2s_ArrayInitElem)                          \
  V(s2s_ArrayLen)                               \
  V(s2s_ArrayCopy)                              \
  V(s2s_I8SArrayGet)                            \
  V(s2s_I8UArrayGet)                            \
  V(s2s_I16SArrayGet)                           \
  V(s2s_I16UArrayGet)                           \
  V(s2s_I32ArrayGet)                            \
  V(s2s_I64ArrayGet)                            \
  V(s2s_F32ArrayGet)                            \
  V(s2s_F64ArrayGet)                            \
  V(s2s_S128ArrayGet)                           \
  V(s2s_RefArrayGet)                            \
  V(s2s_I8ArraySet)                             \
  V(s2s_I16ArraySet)                            \
  V(s2s_I32ArraySet)                            \
  V(s2s_I64ArraySet)                            \
  V(s2s_F32ArraySet)                            \
  V(s2s_F64ArraySet)                            \
  V(s2s_S128ArraySet)                           \
  V(s2s_RefArraySet)                            \
  V(s2s_I8ArrayFill)                            \
  V(s2s_I16ArrayFill)                           \
  V(s2s_I32ArrayFill)                           \
  V(s2s_I64ArrayFill)                           \
  V(s2s_F32ArrayFill)                           \
  V(s2s_F64ArrayFill)                           \
  V(s2s_S128ArrayFill)                          \
  V(s2s_RefArrayFill)                           \
  V(s2s_RefI31)                                 \
  V(s2s_I31GetS)                                \
  V(s2s_I31GetU)                                \
  V(s2s_RefCast)                                \
  V(s2s_RefCastNull)                            \
  V(s2s_RefTest)                                \
  V(s2s_RefTestNull)                            \
  V(s2s_RefAsNonNull)                           \
  V(s2s_CallRef)                                \
  V(s2s_ReturnCallRef)                          \
  V(s2s_AnyConvertExtern)                       \
  V(s2s_ExternConvertAny)                       \
  V(s2s_AssertNullTypecheck)                    \
  V(s2s_AssertNotNullTypecheck)                 \
  V(s2s_TrapIllegalCast)                        \
  V(s2s_RefTestSucceeds)                        \
  V(s2s_RefTestFails)                           \
  V(s2s_RefIsNonNull)                           \
  FOREACH_MEM64_LOAD_STORE_INSTR_HANDLER(V)

#ifdef V8_DRUMBRAKE_BOUNDS_CHECKS
#define FOREACH_INSTR_HANDLER(V)                 \
  FOREACH_LOAD_STORE_INSTR_HANDLER(V)            \
  FOREACH_LOAD_STORE_DUPLICATED_INSTR_HANDLER(V) \
  FOREACH_NO_BOUNDSCHECK_INSTR_HANDLER(V)
#else
#define FOREACH_INSTR_HANDLER(V)      \
  FOREACH_LOAD_STORE_INSTR_HANDLER(V) \
  FOREACH_NO_BOUNDSCHECK_INSTR_HANDLER(V)
#endif  // V8_DRUMBRAKE_BOUNDS_CHECKS

#ifdef V8_ENABLE_DRUMBRAKE_TRACING
#define FOREACH_TRACE_INSTR_HANDLER(V) \
  /* Tracing instruction handlers. */  \
  V(s2s_TraceInstruction)              \
  V(trace_UpdateStack)                 \
  V(trace_PushConstI32Slot)            \
  V(trace_PushConstI64Slot)            \
  V(trace_PushConstF32Slot)            \
  V(trace_PushConstF64Slot)            \
  V(trace_PushConstS128Slot)           \
  V(trace_PushConstRefSlot)            \
  V(trace_PushCopySlot)                \
  V(trace_PopSlot)                     \
  V(trace_SetSlotType)
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

#endif  // V8_WASM_INTERPRETER_INSTRUCTION_HANDLERS_H_
