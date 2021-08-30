// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_OPCODES_H_
#define V8_WASM_WASM_OPCODES_H_

#include <memory>

#include "src/base/platform/wrappers.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-constants.h"

namespace v8 {
namespace internal {

namespace wasm {

class WasmFeatures;
struct WasmModule;

std::ostream& operator<<(std::ostream& os, const FunctionSig& function);
bool V8_EXPORT_PRIVATE IsJSCompatibleSignature(const FunctionSig* sig,
                                               const WasmModule* module,
                                               const WasmFeatures&);

// Control expressions and blocks.
#define FOREACH_CONTROL_OPCODE(V)               \
  V(Unreachable, 0x00, _)                       \
  V(Nop, 0x01, _)                               \
  V(Block, 0x02, _)                             \
  V(Loop, 0x03, _)                              \
  V(If, 0x04, _)                                \
  V(Else, 0x05, _)                              \
  V(Try, 0x06, _ /* eh_prototype */)            \
  V(Catch, 0x07, _ /* eh_prototype */)          \
  V(Throw, 0x08, _ /* eh_prototype */)          \
  V(Rethrow, 0x09, _ /* eh_prototype */)        \
  V(End, 0x0b, _)                               \
  V(Br, 0x0c, _)                                \
  V(BrIf, 0x0d, _)                              \
  V(BrTable, 0x0e, _)                           \
  V(Return, 0x0f, _)                            \
  V(Let, 0x17, _ /* typed_funcref prototype */) \
  V(Delegate, 0x18, _ /* eh_prototype */)       \
  V(CatchAll, 0x19, _ /* eh_prototype */)       \
  V(BrOnNull, 0xd4, _ /* gc prototype */)       \
  V(BrOnNonNull, 0xd6, _ /* gc prototype */)    \
  V(NopForTestingUnsupportedInLiftoff, 0x16, _)

// Constants, locals, globals, and calls.
#define FOREACH_MISC_OPCODE(V)                            \
  V(CallFunction, 0x10, _)                                \
  V(CallIndirect, 0x11, _)                                \
  V(ReturnCall, 0x12, _)                                  \
  V(ReturnCallIndirect, 0x13, _)                          \
  V(CallRef, 0x14, _ /* typed_funcref prototype */)       \
  V(ReturnCallRef, 0x15, _ /* typed_funcref prototype */) \
  V(Drop, 0x1a, _)                                        \
  V(Select, 0x1b, _)                                      \
  V(SelectWithType, 0x1c, _)                              \
  V(LocalGet, 0x20, _)                                    \
  V(LocalSet, 0x21, _)                                    \
  V(LocalTee, 0x22, _)                                    \
  V(GlobalGet, 0x23, _)                                   \
  V(GlobalSet, 0x24, _)                                   \
  V(TableGet, 0x25, _)                                    \
  V(TableSet, 0x26, _)                                    \
  V(I32Const, 0x41, _)                                    \
  V(I64Const, 0x42, _)                                    \
  V(F32Const, 0x43, _)                                    \
  V(F64Const, 0x44, _)                                    \
  V(RefNull, 0xd0, _)                                     \
  V(RefIsNull, 0xd1, _)                                   \
  V(RefFunc, 0xd2, _)                                     \
  V(RefAsNonNull, 0xd3, _ /* typed_funcref prototype */)

// Load memory expressions.
#define FOREACH_LOAD_MEM_OPCODE(V) \
  V(I32LoadMem, 0x28, i_i)         \
  V(I64LoadMem, 0x29, l_i)         \
  V(F32LoadMem, 0x2a, f_i)         \
  V(F64LoadMem, 0x2b, d_i)         \
  V(I32LoadMem8S, 0x2c, i_i)       \
  V(I32LoadMem8U, 0x2d, i_i)       \
  V(I32LoadMem16S, 0x2e, i_i)      \
  V(I32LoadMem16U, 0x2f, i_i)      \
  V(I64LoadMem8S, 0x30, l_i)       \
  V(I64LoadMem8U, 0x31, l_i)       \
  V(I64LoadMem16S, 0x32, l_i)      \
  V(I64LoadMem16U, 0x33, l_i)      \
  V(I64LoadMem32S, 0x34, l_i)      \
  V(I64LoadMem32U, 0x35, l_i)

// Store memory expressions.
#define FOREACH_STORE_MEM_OPCODE(V) \
  V(I32StoreMem, 0x36, v_ii)        \
  V(I64StoreMem, 0x37, v_il)        \
  V(F32StoreMem, 0x38, v_if)        \
  V(F64StoreMem, 0x39, v_id)        \
  V(I32StoreMem8, 0x3a, v_ii)       \
  V(I32StoreMem16, 0x3b, v_ii)      \
  V(I64StoreMem8, 0x3c, v_il)       \
  V(I64StoreMem16, 0x3d, v_il)      \
  V(I64StoreMem32, 0x3e, v_il)

// Miscellaneous memory expressions
#define FOREACH_MISC_MEM_OPCODE(V) \
  V(MemorySize, 0x3f, i_v)         \
  V(MemoryGrow, 0x40, i_i)

// Expressions with signatures.
#define FOREACH_SIMPLE_OPCODE(V)  \
  V(I32Eqz, 0x45, i_i)            \
  V(I32Eq, 0x46, i_ii)            \
  V(I32Ne, 0x47, i_ii)            \
  V(I32LtS, 0x48, i_ii)           \
  V(I32LtU, 0x49, i_ii)           \
  V(I32GtS, 0x4a, i_ii)           \
  V(I32GtU, 0x4b, i_ii)           \
  V(I32LeS, 0x4c, i_ii)           \
  V(I32LeU, 0x4d, i_ii)           \
  V(I32GeS, 0x4e, i_ii)           \
  V(I32GeU, 0x4f, i_ii)           \
  V(I64Eqz, 0x50, i_l)            \
  V(I64Eq, 0x51, i_ll)            \
  V(I64Ne, 0x52, i_ll)            \
  V(I64LtS, 0x53, i_ll)           \
  V(I64LtU, 0x54, i_ll)           \
  V(I64GtS, 0x55, i_ll)           \
  V(I64GtU, 0x56, i_ll)           \
  V(I64LeS, 0x57, i_ll)           \
  V(I64LeU, 0x58, i_ll)           \
  V(I64GeS, 0x59, i_ll)           \
  V(I64GeU, 0x5a, i_ll)           \
  V(F32Eq, 0x5b, i_ff)            \
  V(F32Ne, 0x5c, i_ff)            \
  V(F32Lt, 0x5d, i_ff)            \
  V(F32Gt, 0x5e, i_ff)            \
  V(F32Le, 0x5f, i_ff)            \
  V(F32Ge, 0x60, i_ff)            \
  V(F64Eq, 0x61, i_dd)            \
  V(F64Ne, 0x62, i_dd)            \
  V(F64Lt, 0x63, i_dd)            \
  V(F64Gt, 0x64, i_dd)            \
  V(F64Le, 0x65, i_dd)            \
  V(F64Ge, 0x66, i_dd)            \
  V(I32Clz, 0x67, i_i)            \
  V(I32Ctz, 0x68, i_i)            \
  V(I32Popcnt, 0x69, i_i)         \
  V(I32Add, 0x6a, i_ii)           \
  V(I32Sub, 0x6b, i_ii)           \
  V(I32Mul, 0x6c, i_ii)           \
  V(I32DivS, 0x6d, i_ii)          \
  V(I32DivU, 0x6e, i_ii)          \
  V(I32RemS, 0x6f, i_ii)          \
  V(I32RemU, 0x70, i_ii)          \
  V(I32And, 0x71, i_ii)           \
  V(I32Ior, 0x72, i_ii)           \
  V(I32Xor, 0x73, i_ii)           \
  V(I32Shl, 0x74, i_ii)           \
  V(I32ShrS, 0x75, i_ii)          \
  V(I32ShrU, 0x76, i_ii)          \
  V(I32Rol, 0x77, i_ii)           \
  V(I32Ror, 0x78, i_ii)           \
  V(I64Clz, 0x79, l_l)            \
  V(I64Ctz, 0x7a, l_l)            \
  V(I64Popcnt, 0x7b, l_l)         \
  V(I64Add, 0x7c, l_ll)           \
  V(I64Sub, 0x7d, l_ll)           \
  V(I64Mul, 0x7e, l_ll)           \
  V(I64DivS, 0x7f, l_ll)          \
  V(I64DivU, 0x80, l_ll)          \
  V(I64RemS, 0x81, l_ll)          \
  V(I64RemU, 0x82, l_ll)          \
  V(I64And, 0x83, l_ll)           \
  V(I64Ior, 0x84, l_ll)           \
  V(I64Xor, 0x85, l_ll)           \
  V(I64Shl, 0x86, l_ll)           \
  V(I64ShrS, 0x87, l_ll)          \
  V(I64ShrU, 0x88, l_ll)          \
  V(I64Rol, 0x89, l_ll)           \
  V(I64Ror, 0x8a, l_ll)           \
  V(F32Abs, 0x8b, f_f)            \
  V(F32Neg, 0x8c, f_f)            \
  V(F32Ceil, 0x8d, f_f)           \
  V(F32Floor, 0x8e, f_f)          \
  V(F32Trunc, 0x8f, f_f)          \
  V(F32NearestInt, 0x90, f_f)     \
  V(F32Sqrt, 0x91, f_f)           \
  V(F32Add, 0x92, f_ff)           \
  V(F32Sub, 0x93, f_ff)           \
  V(F32Mul, 0x94, f_ff)           \
  V(F32Div, 0x95, f_ff)           \
  V(F32Min, 0x96, f_ff)           \
  V(F32Max, 0x97, f_ff)           \
  V(F32CopySign, 0x98, f_ff)      \
  V(F64Abs, 0x99, d_d)            \
  V(F64Neg, 0x9a, d_d)            \
  V(F64Ceil, 0x9b, d_d)           \
  V(F64Floor, 0x9c, d_d)          \
  V(F64Trunc, 0x9d, d_d)          \
  V(F64NearestInt, 0x9e, d_d)     \
  V(F64Sqrt, 0x9f, d_d)           \
  V(F64Add, 0xa0, d_dd)           \
  V(F64Sub, 0xa1, d_dd)           \
  V(F64Mul, 0xa2, d_dd)           \
  V(F64Div, 0xa3, d_dd)           \
  V(F64Min, 0xa4, d_dd)           \
  V(F64Max, 0xa5, d_dd)           \
  V(F64CopySign, 0xa6, d_dd)      \
  V(I32ConvertI64, 0xa7, i_l)     \
  V(I32SConvertF32, 0xa8, i_f)    \
  V(I32UConvertF32, 0xa9, i_f)    \
  V(I32SConvertF64, 0xaa, i_d)    \
  V(I32UConvertF64, 0xab, i_d)    \
  V(I64SConvertI32, 0xac, l_i)    \
  V(I64UConvertI32, 0xad, l_i)    \
  V(I64SConvertF32, 0xae, l_f)    \
  V(I64UConvertF32, 0xaf, l_f)    \
  V(I64SConvertF64, 0xb0, l_d)    \
  V(I64UConvertF64, 0xb1, l_d)    \
  V(F32SConvertI32, 0xb2, f_i)    \
  V(F32UConvertI32, 0xb3, f_i)    \
  V(F32SConvertI64, 0xb4, f_l)    \
  V(F32UConvertI64, 0xb5, f_l)    \
  V(F32ConvertF64, 0xb6, f_d)     \
  V(F64SConvertI32, 0xb7, d_i)    \
  V(F64UConvertI32, 0xb8, d_i)    \
  V(F64SConvertI64, 0xb9, d_l)    \
  V(F64UConvertI64, 0xba, d_l)    \
  V(F64ConvertF32, 0xbb, d_f)     \
  V(I32ReinterpretF32, 0xbc, i_f) \
  V(I64ReinterpretF64, 0xbd, l_d) \
  V(F32ReinterpretI32, 0xbe, f_i) \
  V(F64ReinterpretI64, 0xbf, d_l) \
  V(I32SExtendI8, 0xc0, i_i)      \
  V(I32SExtendI16, 0xc1, i_i)     \
  V(I64SExtendI8, 0xc2, l_l)      \
  V(I64SExtendI16, 0xc3, l_l)     \
  V(I64SExtendI32, 0xc4, l_l)

#define FOREACH_SIMPLE_PROTOTYPE_OPCODE(V) V(RefEq, 0xd5, i_qq)

// For compatibility with Asm.js.
// These opcodes are not spec'ed (or visible) externally; the idea is
// to use unused ranges for internal purposes.
#define FOREACH_ASMJS_COMPAT_OPCODE(V) \
  V(F64Acos, 0xdc, d_d)                \
  V(F64Asin, 0xdd, d_d)                \
  V(F64Atan, 0xde, d_d)                \
  V(F64Cos, 0xdf, d_d)                 \
  V(F64Sin, 0xe0, d_d)                 \
  V(F64Tan, 0xe1, d_d)                 \
  V(F64Exp, 0xe2, d_d)                 \
  V(F64Log, 0xe3, d_d)                 \
  V(F64Atan2, 0xe4, d_dd)              \
  V(F64Pow, 0xe5, d_dd)                \
  V(F64Mod, 0xe6, d_dd)                \
  V(I32AsmjsDivS, 0xe7, i_ii)          \
  V(I32AsmjsDivU, 0xe8, i_ii)          \
  V(I32AsmjsRemS, 0xe9, i_ii)          \
  V(I32AsmjsRemU, 0xea, i_ii)          \
  V(I32AsmjsLoadMem8S, 0xeb, i_i)      \
  V(I32AsmjsLoadMem8U, 0xec, i_i)      \
  V(I32AsmjsLoadMem16S, 0xed, i_i)     \
  V(I32AsmjsLoadMem16U, 0xee, i_i)     \
  V(I32AsmjsLoadMem, 0xef, i_i)        \
  V(F32AsmjsLoadMem, 0xf0, f_i)        \
  V(F64AsmjsLoadMem, 0xf1, d_i)        \
  V(I32AsmjsStoreMem8, 0xf2, i_ii)     \
  V(I32AsmjsStoreMem16, 0xf3, i_ii)    \
  V(I32AsmjsStoreMem, 0xf4, i_ii)      \
  V(F32AsmjsStoreMem, 0xf5, f_if)      \
  V(F64AsmjsStoreMem, 0xf6, d_id)      \
  V(I32AsmjsSConvertF32, 0xf7, i_f)    \
  V(I32AsmjsUConvertF32, 0xf8, i_f)    \
  V(I32AsmjsSConvertF64, 0xf9, i_d)    \
  V(I32AsmjsUConvertF64, 0xfa, i_d)

#define FOREACH_SIMD_MEM_OPCODE(V) \
  V(S128LoadMem, 0xfd00, s_i)      \
  V(S128Load8x8S, 0xfd01, s_i)     \
  V(S128Load8x8U, 0xfd02, s_i)     \
  V(S128Load16x4S, 0xfd03, s_i)    \
  V(S128Load16x4U, 0xfd04, s_i)    \
  V(S128Load32x2S, 0xfd05, s_i)    \
  V(S128Load32x2U, 0xfd06, s_i)    \
  V(S128Load8Splat, 0xfd07, s_i)   \
  V(S128Load16Splat, 0xfd08, s_i)  \
  V(S128Load32Splat, 0xfd09, s_i)  \
  V(S128Load64Splat, 0xfd0a, s_i)  \
  V(S128StoreMem, 0xfd0b, v_is)    \
  V(S128Load32Zero, 0xfd5c, s_i)   \
  V(S128Load64Zero, 0xfd5d, s_i)

#define FOREACH_SIMD_MEM_1_OPERAND_OPCODE(V) \
  V(S128Load8Lane, 0xfd54, s_is)             \
  V(S128Load16Lane, 0xfd55, s_is)            \
  V(S128Load32Lane, 0xfd56, s_is)            \
  V(S128Load64Lane, 0xfd57, s_is)            \
  V(S128Store8Lane, 0xfd58, v_is)            \
  V(S128Store16Lane, 0xfd59, v_is)           \
  V(S128Store32Lane, 0xfd5a, v_is)           \
  V(S128Store64Lane, 0xfd5b, v_is)

#define FOREACH_SIMD_CONST_OPCODE(V) V(S128Const, 0xfd0c, _)

#define FOREACH_SIMD_MASK_OPERAND_OPCODE(V) V(I8x16Shuffle, 0xfd0d, s_ss)

#define FOREACH_SIMD_MVP_0_OPERAND_OPCODE(V) \
  V(I8x16Swizzle, 0xfd0e, s_ss)              \
  V(I8x16Splat, 0xfd0f, s_i)                 \
  V(I16x8Splat, 0xfd10, s_i)                 \
  V(I32x4Splat, 0xfd11, s_i)                 \
  V(I64x2Splat, 0xfd12, s_l)                 \
  V(F32x4Splat, 0xfd13, s_f)                 \
  V(F64x2Splat, 0xfd14, s_d)                 \
  V(I8x16Eq, 0xfd23, s_ss)                   \
  V(I8x16Ne, 0xfd24, s_ss)                   \
  V(I8x16LtS, 0xfd25, s_ss)                  \
  V(I8x16LtU, 0xfd26, s_ss)                  \
  V(I8x16GtS, 0xfd27, s_ss)                  \
  V(I8x16GtU, 0xfd28, s_ss)                  \
  V(I8x16LeS, 0xfd29, s_ss)                  \
  V(I8x16LeU, 0xfd2a, s_ss)                  \
  V(I8x16GeS, 0xfd2b, s_ss)                  \
  V(I8x16GeU, 0xfd2c, s_ss)                  \
  V(I16x8Eq, 0xfd2d, s_ss)                   \
  V(I16x8Ne, 0xfd2e, s_ss)                   \
  V(I16x8LtS, 0xfd2f, s_ss)                  \
  V(I16x8LtU, 0xfd30, s_ss)                  \
  V(I16x8GtS, 0xfd31, s_ss)                  \
  V(I16x8GtU, 0xfd32, s_ss)                  \
  V(I16x8LeS, 0xfd33, s_ss)                  \
  V(I16x8LeU, 0xfd34, s_ss)                  \
  V(I16x8GeS, 0xfd35, s_ss)                  \
  V(I16x8GeU, 0xfd36, s_ss)                  \
  V(I32x4Eq, 0xfd37, s_ss)                   \
  V(I32x4Ne, 0xfd38, s_ss)                   \
  V(I32x4LtS, 0xfd39, s_ss)                  \
  V(I32x4LtU, 0xfd3a, s_ss)                  \
  V(I32x4GtS, 0xfd3b, s_ss)                  \
  V(I32x4GtU, 0xfd3c, s_ss)                  \
  V(I32x4LeS, 0xfd3d, s_ss)                  \
  V(I32x4LeU, 0xfd3e, s_ss)                  \
  V(I32x4GeS, 0xfd3f, s_ss)                  \
  V(I32x4GeU, 0xfd40, s_ss)                  \
  V(F32x4Eq, 0xfd41, s_ss)                   \
  V(F32x4Ne, 0xfd42, s_ss)                   \
  V(F32x4Lt, 0xfd43, s_ss)                   \
  V(F32x4Gt, 0xfd44, s_ss)                   \
  V(F32x4Le, 0xfd45, s_ss)                   \
  V(F32x4Ge, 0xfd46, s_ss)                   \
  V(F64x2Eq, 0xfd47, s_ss)                   \
  V(F64x2Ne, 0xfd48, s_ss)                   \
  V(F64x2Lt, 0xfd49, s_ss)                   \
  V(F64x2Gt, 0xfd4a, s_ss)                   \
  V(F64x2Le, 0xfd4b, s_ss)                   \
  V(F64x2Ge, 0xfd4c, s_ss)                   \
  V(S128Not, 0xfd4d, s_s)                    \
  V(S128And, 0xfd4e, s_ss)                   \
  V(S128AndNot, 0xfd4f, s_ss)                \
  V(S128Or, 0xfd50, s_ss)                    \
  V(S128Xor, 0xfd51, s_ss)                   \
  V(S128Select, 0xfd52, s_sss)               \
  V(V128AnyTrue, 0xfd53, i_s)                \
  V(F32x4DemoteF64x2Zero, 0xfd5e, s_s)       \
  V(F64x2PromoteLowF32x4, 0xfd5f, s_s)       \
  V(I8x16Abs, 0xfd60, s_s)                   \
  V(I8x16Neg, 0xfd61, s_s)                   \
  V(I8x16Popcnt, 0xfd62, s_s)                \
  V(I8x16AllTrue, 0xfd63, i_s)               \
  V(I8x16BitMask, 0xfd64, i_s)               \
  V(I8x16SConvertI16x8, 0xfd65, s_ss)        \
  V(I8x16UConvertI16x8, 0xfd66, s_ss)        \
  V(F32x4Ceil, 0xfd67, s_s)                  \
  V(F32x4Floor, 0xfd68, s_s)                 \
  V(F32x4Trunc, 0xfd69, s_s)                 \
  V(F32x4NearestInt, 0xfd6a, s_s)            \
  V(I8x16Shl, 0xfd6b, s_si)                  \
  V(I8x16ShrS, 0xfd6c, s_si)                 \
  V(I8x16ShrU, 0xfd6d, s_si)                 \
  V(I8x16Add, 0xfd6e, s_ss)                  \
  V(I8x16AddSatS, 0xfd6f, s_ss)              \
  V(I8x16AddSatU, 0xfd70, s_ss)              \
  V(I8x16Sub, 0xfd71, s_ss)                  \
  V(I8x16SubSatS, 0xfd72, s_ss)              \
  V(I8x16SubSatU, 0xfd73, s_ss)              \
  V(F64x2Ceil, 0xfd74, s_s)                  \
  V(F64x2Floor, 0xfd75, s_s)                 \
  V(I8x16MinS, 0xfd76, s_ss)                 \
  V(I8x16MinU, 0xfd77, s_ss)                 \
  V(I8x16MaxS, 0xfd78, s_ss)                 \
  V(I8x16MaxU, 0xfd79, s_ss)                 \
  V(F64x2Trunc, 0xfd7a, s_s)                 \
  V(I8x16RoundingAverageU, 0xfd7b, s_ss)     \
  V(I16x8ExtAddPairwiseI8x16S, 0xfd7c, s_s)  \
  V(I16x8ExtAddPairwiseI8x16U, 0xfd7d, s_s)  \
  V(I32x4ExtAddPairwiseI16x8S, 0xfd7e, s_s)  \
  V(I32x4ExtAddPairwiseI16x8U, 0xfd7f, s_s)  \
  V(I16x8Abs, 0xfd80, s_s)                   \
  V(I16x8Neg, 0xfd81, s_s)                   \
  V(I16x8Q15MulRSatS, 0xfd82, s_ss)          \
  V(I16x8AllTrue, 0xfd83, i_s)               \
  V(I16x8BitMask, 0xfd84, i_s)               \
  V(I16x8SConvertI32x4, 0xfd85, s_ss)        \
  V(I16x8UConvertI32x4, 0xfd86, s_ss)        \
  V(I16x8SConvertI8x16Low, 0xfd87, s_s)      \
  V(I16x8SConvertI8x16High, 0xfd88, s_s)     \
  V(I16x8UConvertI8x16Low, 0xfd89, s_s)      \
  V(I16x8UConvertI8x16High, 0xfd8a, s_s)     \
  V(I16x8Shl, 0xfd8b, s_si)                  \
  V(I16x8ShrS, 0xfd8c, s_si)                 \
  V(I16x8ShrU, 0xfd8d, s_si)                 \
  V(I16x8Add, 0xfd8e, s_ss)                  \
  V(I16x8AddSatS, 0xfd8f, s_ss)              \
  V(I16x8AddSatU, 0xfd90, s_ss)              \
  V(I16x8Sub, 0xfd91, s_ss)                  \
  V(I16x8SubSatS, 0xfd92, s_ss)              \
  V(I16x8SubSatU, 0xfd93, s_ss)              \
  V(F64x2NearestInt, 0xfd94, s_s)            \
  V(I16x8Mul, 0xfd95, s_ss)                  \
  V(I16x8MinS, 0xfd96, s_ss)                 \
  V(I16x8MinU, 0xfd97, s_ss)                 \
  V(I16x8MaxS, 0xfd98, s_ss)                 \
  V(I16x8MaxU, 0xfd99, s_ss)                 \
  V(I16x8RoundingAverageU, 0xfd9b, s_ss)     \
  V(I16x8ExtMulLowI8x16S, 0xfd9c, s_ss)      \
  V(I16x8ExtMulHighI8x16S, 0xfd9d, s_ss)     \
  V(I16x8ExtMulLowI8x16U, 0xfd9e, s_ss)      \
  V(I16x8ExtMulHighI8x16U, 0xfd9f, s_ss)     \
  V(I32x4Abs, 0xfda0, s_s)                   \
  V(I32x4Neg, 0xfda1, s_s)                   \
  V(I32x4AllTrue, 0xfda3, i_s)               \
  V(I32x4BitMask, 0xfda4, i_s)               \
  V(I32x4SConvertI16x8Low, 0xfda7, s_s)      \
  V(I32x4SConvertI16x8High, 0xfda8, s_s)     \
  V(I32x4UConvertI16x8Low, 0xfda9, s_s)      \
  V(I32x4UConvertI16x8High, 0xfdaa, s_s)     \
  V(I32x4Shl, 0xfdab, s_si)                  \
  V(I32x4ShrS, 0xfdac, s_si)                 \
  V(I32x4ShrU, 0xfdad, s_si)                 \
  V(I32x4Add, 0xfdae, s_ss)                  \
  V(I32x4Sub, 0xfdb1, s_ss)                  \
  V(I32x4Mul, 0xfdb5, s_ss)                  \
  V(I32x4MinS, 0xfdb6, s_ss)                 \
  V(I32x4MinU, 0xfdb7, s_ss)                 \
  V(I32x4MaxS, 0xfdb8, s_ss)                 \
  V(I32x4MaxU, 0xfdb9, s_ss)                 \
  V(I32x4DotI16x8S, 0xfdba, s_ss)            \
  V(I32x4ExtMulLowI16x8S, 0xfdbc, s_ss)      \
  V(I32x4ExtMulHighI16x8S, 0xfdbd, s_ss)     \
  V(I32x4ExtMulLowI16x8U, 0xfdbe, s_ss)      \
  V(I32x4ExtMulHighI16x8U, 0xfdbf, s_ss)     \
  V(I64x2Abs, 0xfdc0, s_s)                   \
  V(I64x2Neg, 0xfdc1, s_s)                   \
  V(I64x2AllTrue, 0xfdc3, i_s)               \
  V(I64x2BitMask, 0xfdc4, i_s)               \
  V(I64x2SConvertI32x4Low, 0xfdc7, s_s)      \
  V(I64x2SConvertI32x4High, 0xfdc8, s_s)     \
  V(I64x2UConvertI32x4Low, 0xfdc9, s_s)      \
  V(I64x2UConvertI32x4High, 0xfdca, s_s)     \
  V(I64x2Shl, 0xfdcb, s_si)                  \
  V(I64x2ShrS, 0xfdcc, s_si)                 \
  V(I64x2ShrU, 0xfdcd, s_si)                 \
  V(I64x2Add, 0xfdce, s_ss)                  \
  V(I64x2Sub, 0xfdd1, s_ss)                  \
  V(I64x2Mul, 0xfdd5, s_ss)                  \
  V(I64x2Eq, 0xfdd6, s_ss)                   \
  V(I64x2Ne, 0xfdd7, s_ss)                   \
  V(I64x2LtS, 0xfdd8, s_ss)                  \
  V(I64x2GtS, 0xfdd9, s_ss)                  \
  V(I64x2LeS, 0xfdda, s_ss)                  \
  V(I64x2GeS, 0xfddb, s_ss)                  \
  V(I64x2ExtMulLowI32x4S, 0xfddc, s_ss)      \
  V(I64x2ExtMulHighI32x4S, 0xfddd, s_ss)     \
  V(I64x2ExtMulLowI32x4U, 0xfdde, s_ss)      \
  V(I64x2ExtMulHighI32x4U, 0xfddf, s_ss)     \
  V(F32x4Abs, 0xfde0, s_s)                   \
  V(F32x4Neg, 0xfde1, s_s)                   \
  V(F32x4Sqrt, 0xfde3, s_s)                  \
  V(F32x4Add, 0xfde4, s_ss)                  \
  V(F32x4Sub, 0xfde5, s_ss)                  \
  V(F32x4Mul, 0xfde6, s_ss)                  \
  V(F32x4Div, 0xfde7, s_ss)                  \
  V(F32x4Min, 0xfde8, s_ss)                  \
  V(F32x4Max, 0xfde9, s_ss)                  \
  V(F32x4Pmin, 0xfdea, s_ss)                 \
  V(F32x4Pmax, 0xfdeb, s_ss)                 \
  V(F64x2Abs, 0xfdec, s_s)                   \
  V(F64x2Neg, 0xfded, s_s)                   \
  V(F64x2Sqrt, 0xfdef, s_s)                  \
  V(F64x2Add, 0xfdf0, s_ss)                  \
  V(F64x2Sub, 0xfdf1, s_ss)                  \
  V(F64x2Mul, 0xfdf2, s_ss)                  \
  V(F64x2Div, 0xfdf3, s_ss)                  \
  V(F64x2Min, 0xfdf4, s_ss)                  \
  V(F64x2Max, 0xfdf5, s_ss)                  \
  V(F64x2Pmin, 0xfdf6, s_ss)                 \
  V(F64x2Pmax, 0xfdf7, s_ss)                 \
  V(I32x4SConvertF32x4, 0xfdf8, s_s)         \
  V(I32x4UConvertF32x4, 0xfdf9, s_s)         \
  V(F32x4SConvertI32x4, 0xfdfa, s_s)         \
  V(F32x4UConvertI32x4, 0xfdfb, s_s)         \
  V(I32x4TruncSatF64x2SZero, 0xfdfc, s_s)    \
  V(I32x4TruncSatF64x2UZero, 0xfdfd, s_s)    \
  V(F64x2ConvertLowI32x4S, 0xfdfe, s_s)      \
  V(F64x2ConvertLowI32x4U, 0xfdff, s_s)

#define FOREACH_RELAXED_SIMD_OPCODE(V) \
  V(F32x4Qfma, 0xfdaf, s_sss)          \
  V(F32x4Qfms, 0xfdb0, s_sss)          \
  V(F64x2Qfma, 0xfdcf, s_sss)          \
  V(F64x2Qfms, 0xfdd0, s_sss)          \
  V(F32x4RecipApprox, 0xfdd2, s_s)     \
  V(F32x4RecipSqrtApprox, 0xfdd3, s_s)

#define FOREACH_SIMD_1_OPERAND_1_PARAM_OPCODE(V) \
  V(I8x16ExtractLaneS, 0xfd15, _)                \
  V(I8x16ExtractLaneU, 0xfd16, _)                \
  V(I16x8ExtractLaneS, 0xfd18, _)                \
  V(I16x8ExtractLaneU, 0xfd19, _)                \
  V(I32x4ExtractLane, 0xfd1b, _)                 \
  V(I64x2ExtractLane, 0xfd1d, _)                 \
  V(F32x4ExtractLane, 0xfd1f, _)                 \
  V(F64x2ExtractLane, 0xfd21, _)

#define FOREACH_SIMD_1_OPERAND_2_PARAM_OPCODE(V) \
  V(I8x16ReplaceLane, 0xfd17, _)                 \
  V(I16x8ReplaceLane, 0xfd1a, _)                 \
  V(I32x4ReplaceLane, 0xfd1c, _)                 \
  V(I64x2ReplaceLane, 0xfd1e, _)                 \
  V(F32x4ReplaceLane, 0xfd20, _)                 \
  V(F64x2ReplaceLane, 0xfd22, _)

#define FOREACH_SIMD_0_OPERAND_OPCODE(V) \
  FOREACH_SIMD_MVP_0_OPERAND_OPCODE(V)   \
  FOREACH_RELAXED_SIMD_OPCODE(V)

#define FOREACH_SIMD_1_OPERAND_OPCODE(V)   \
  FOREACH_SIMD_1_OPERAND_1_PARAM_OPCODE(V) \
  FOREACH_SIMD_1_OPERAND_2_PARAM_OPCODE(V)

#define FOREACH_SIMD_OPCODE(V)         \
  FOREACH_SIMD_0_OPERAND_OPCODE(V)     \
  FOREACH_SIMD_1_OPERAND_OPCODE(V)     \
  FOREACH_SIMD_MASK_OPERAND_OPCODE(V)  \
  FOREACH_SIMD_MEM_OPCODE(V)           \
  FOREACH_SIMD_MEM_1_OPERAND_OPCODE(V) \
  FOREACH_SIMD_CONST_OPCODE(V)

#define FOREACH_NUMERIC_OPCODE(V)                         \
  V(I32SConvertSatF32, 0xfc00, i_f)                       \
  V(I32UConvertSatF32, 0xfc01, i_f)                       \
  V(I32SConvertSatF64, 0xfc02, i_d)                       \
  V(I32UConvertSatF64, 0xfc03, i_d)                       \
  V(I64SConvertSatF32, 0xfc04, l_f)                       \
  V(I64UConvertSatF32, 0xfc05, l_f)                       \
  V(I64SConvertSatF64, 0xfc06, l_d)                       \
  V(I64UConvertSatF64, 0xfc07, l_d)                       \
  V(MemoryInit, 0xfc08, v_iii)                            \
  V(DataDrop, 0xfc09, v_v)                                \
  V(MemoryCopy, 0xfc0a, v_iii)                            \
  V(MemoryFill, 0xfc0b, v_iii)                            \
  V(TableInit, 0xfc0c, v_iii)                             \
  V(ElemDrop, 0xfc0d, v_v)                                \
  V(TableCopy, 0xfc0e, v_iii)                             \
  /* TableGrow is polymorphic in the first parameter. */  \
  /* It's whatever the table type is. */                  \
  V(TableGrow, 0xfc0f, i_ci)                              \
  V(TableSize, 0xfc10, i_v)                               \
  /* TableFill is polymorphic in the second parameter. */ \
  /* It's whatever the table type is. */                  \
  V(TableFill, 0xfc11, v_iii)

#define FOREACH_ATOMIC_OPCODE(V)                \
  V(AtomicNotify, 0xfe00, i_ii)                 \
  V(I32AtomicWait, 0xfe01, i_iil)               \
  V(I64AtomicWait, 0xfe02, i_ill)               \
  V(I32AtomicLoad, 0xfe10, i_i)                 \
  V(I64AtomicLoad, 0xfe11, l_i)                 \
  V(I32AtomicLoad8U, 0xfe12, i_i)               \
  V(I32AtomicLoad16U, 0xfe13, i_i)              \
  V(I64AtomicLoad8U, 0xfe14, l_i)               \
  V(I64AtomicLoad16U, 0xfe15, l_i)              \
  V(I64AtomicLoad32U, 0xfe16, l_i)              \
  V(I32AtomicStore, 0xfe17, v_ii)               \
  V(I64AtomicStore, 0xfe18, v_il)               \
  V(I32AtomicStore8U, 0xfe19, v_ii)             \
  V(I32AtomicStore16U, 0xfe1a, v_ii)            \
  V(I64AtomicStore8U, 0xfe1b, v_il)             \
  V(I64AtomicStore16U, 0xfe1c, v_il)            \
  V(I64AtomicStore32U, 0xfe1d, v_il)            \
  V(I32AtomicAdd, 0xfe1e, i_ii)                 \
  V(I64AtomicAdd, 0xfe1f, l_il)                 \
  V(I32AtomicAdd8U, 0xfe20, i_ii)               \
  V(I32AtomicAdd16U, 0xfe21, i_ii)              \
  V(I64AtomicAdd8U, 0xfe22, l_il)               \
  V(I64AtomicAdd16U, 0xfe23, l_il)              \
  V(I64AtomicAdd32U, 0xfe24, l_il)              \
  V(I32AtomicSub, 0xfe25, i_ii)                 \
  V(I64AtomicSub, 0xfe26, l_il)                 \
  V(I32AtomicSub8U, 0xfe27, i_ii)               \
  V(I32AtomicSub16U, 0xfe28, i_ii)              \
  V(I64AtomicSub8U, 0xfe29, l_il)               \
  V(I64AtomicSub16U, 0xfe2a, l_il)              \
  V(I64AtomicSub32U, 0xfe2b, l_il)              \
  V(I32AtomicAnd, 0xfe2c, i_ii)                 \
  V(I64AtomicAnd, 0xfe2d, l_il)                 \
  V(I32AtomicAnd8U, 0xfe2e, i_ii)               \
  V(I32AtomicAnd16U, 0xfe2f, i_ii)              \
  V(I64AtomicAnd8U, 0xfe30, l_il)               \
  V(I64AtomicAnd16U, 0xfe31, l_il)              \
  V(I64AtomicAnd32U, 0xfe32, l_il)              \
  V(I32AtomicOr, 0xfe33, i_ii)                  \
  V(I64AtomicOr, 0xfe34, l_il)                  \
  V(I32AtomicOr8U, 0xfe35, i_ii)                \
  V(I32AtomicOr16U, 0xfe36, i_ii)               \
  V(I64AtomicOr8U, 0xfe37, l_il)                \
  V(I64AtomicOr16U, 0xfe38, l_il)               \
  V(I64AtomicOr32U, 0xfe39, l_il)               \
  V(I32AtomicXor, 0xfe3a, i_ii)                 \
  V(I64AtomicXor, 0xfe3b, l_il)                 \
  V(I32AtomicXor8U, 0xfe3c, i_ii)               \
  V(I32AtomicXor16U, 0xfe3d, i_ii)              \
  V(I64AtomicXor8U, 0xfe3e, l_il)               \
  V(I64AtomicXor16U, 0xfe3f, l_il)              \
  V(I64AtomicXor32U, 0xfe40, l_il)              \
  V(I32AtomicExchange, 0xfe41, i_ii)            \
  V(I64AtomicExchange, 0xfe42, l_il)            \
  V(I32AtomicExchange8U, 0xfe43, i_ii)          \
  V(I32AtomicExchange16U, 0xfe44, i_ii)         \
  V(I64AtomicExchange8U, 0xfe45, l_il)          \
  V(I64AtomicExchange16U, 0xfe46, l_il)         \
  V(I64AtomicExchange32U, 0xfe47, l_il)         \
  V(I32AtomicCompareExchange, 0xfe48, i_iii)    \
  V(I64AtomicCompareExchange, 0xfe49, l_ill)    \
  V(I32AtomicCompareExchange8U, 0xfe4a, i_iii)  \
  V(I32AtomicCompareExchange16U, 0xfe4b, i_iii) \
  V(I64AtomicCompareExchange8U, 0xfe4c, l_ill)  \
  V(I64AtomicCompareExchange16U, 0xfe4d, l_ill) \
  V(I64AtomicCompareExchange32U, 0xfe4e, l_ill)

#define FOREACH_GC_OPCODE(V)                                         \
  V(StructNewWithRtt, 0xfb01, _)                                     \
  V(StructNewDefault, 0xfb02, _)                                     \
  V(StructGet, 0xfb03, _)                                            \
  V(StructGetS, 0xfb04, _)                                           \
  V(StructGetU, 0xfb05, _)                                           \
  V(StructSet, 0xfb06, _)                                            \
  V(ArrayNewWithRtt, 0xfb11, _)                                      \
  V(ArrayNewDefault, 0xfb12, _)                                      \
  V(ArrayGet, 0xfb13, _)                                             \
  V(ArrayGetS, 0xfb14, _)                                            \
  V(ArrayGetU, 0xfb15, _)                                            \
  V(ArraySet, 0xfb16, _)                                             \
  V(ArrayLen, 0xfb17, _)                                             \
  V(ArrayCopy, 0xfb18, _) /* not standardized - V8 experimental */   \
  V(ArrayInit, 0xfb19, _) /* not standardized - V8 experimental */   \
  V(I31New, 0xfb20, _)                                               \
  V(I31GetS, 0xfb21, _)                                              \
  V(I31GetU, 0xfb22, _)                                              \
  V(RttCanon, 0xfb30, _)                                             \
  V(RttSub, 0xfb31, _)                                               \
  V(RttFreshSub, 0xfb32, _) /* not standardized - V8 experimental */ \
  V(RefTest, 0xfb40, _)                                              \
  V(RefCast, 0xfb41, _)                                              \
  V(BrOnCast, 0xfb42, _)                                             \
  V(BrOnCastFail, 0xfb43, _)                                         \
  V(RefIsFunc, 0xfb50, _)                                            \
  V(RefIsData, 0xfb51, _)                                            \
  V(RefIsI31, 0xfb52, _)                                             \
  V(RefAsFunc, 0xfb58, _)                                            \
  V(RefAsData, 0xfb59, _)                                            \
  V(RefAsI31, 0xfb5a, _)                                             \
  V(BrOnFunc, 0xfb60, _)                                             \
  V(BrOnData, 0xfb61, _)                                             \
  V(BrOnI31, 0xfb62, _)                                              \
  V(BrOnNonFunc, 0xfb63, _)                                          \
  V(BrOnNonData, 0xfb64, _)                                          \
  V(BrOnNonI31, 0xfb65, _)

#define FOREACH_ATOMIC_0_OPERAND_OPCODE(V)                      \
  /* AtomicFence does not target a particular linear memory. */ \
  V(AtomicFence, 0xfe03, v_v)

// All opcodes.
#define FOREACH_OPCODE(V)            \
  FOREACH_CONTROL_OPCODE(V)          \
  FOREACH_MISC_OPCODE(V)             \
  FOREACH_SIMPLE_OPCODE(V)           \
  FOREACH_SIMPLE_PROTOTYPE_OPCODE(V) \
  FOREACH_STORE_MEM_OPCODE(V)        \
  FOREACH_LOAD_MEM_OPCODE(V)         \
  FOREACH_MISC_MEM_OPCODE(V)         \
  FOREACH_ASMJS_COMPAT_OPCODE(V)     \
  FOREACH_SIMD_OPCODE(V)             \
  FOREACH_ATOMIC_OPCODE(V)           \
  FOREACH_ATOMIC_0_OPERAND_OPCODE(V) \
  FOREACH_NUMERIC_OPCODE(V)          \
  FOREACH_GC_OPCODE(V)

// All signatures.
#define FOREACH_SIGNATURE(V)                        \
  FOREACH_SIMD_SIGNATURE(V)                         \
  V(v_v, kWasmVoid)                                 \
  V(i_ii, kWasmI32, kWasmI32, kWasmI32)             \
  V(i_i, kWasmI32, kWasmI32)                        \
  V(i_v, kWasmI32)                                  \
  V(i_ff, kWasmI32, kWasmF32, kWasmF32)             \
  V(i_f, kWasmI32, kWasmF32)                        \
  V(i_dd, kWasmI32, kWasmF64, kWasmF64)             \
  V(i_d, kWasmI32, kWasmF64)                        \
  V(i_l, kWasmI32, kWasmI64)                        \
  V(l_ll, kWasmI64, kWasmI64, kWasmI64)             \
  V(i_ll, kWasmI32, kWasmI64, kWasmI64)             \
  V(l_l, kWasmI64, kWasmI64)                        \
  V(l_i, kWasmI64, kWasmI32)                        \
  V(l_f, kWasmI64, kWasmF32)                        \
  V(l_d, kWasmI64, kWasmF64)                        \
  V(f_ff, kWasmF32, kWasmF32, kWasmF32)             \
  V(f_f, kWasmF32, kWasmF32)                        \
  V(f_d, kWasmF32, kWasmF64)                        \
  V(f_i, kWasmF32, kWasmI32)                        \
  V(f_l, kWasmF32, kWasmI64)                        \
  V(d_dd, kWasmF64, kWasmF64, kWasmF64)             \
  V(d_d, kWasmF64, kWasmF64)                        \
  V(d_f, kWasmF64, kWasmF32)                        \
  V(d_i, kWasmF64, kWasmI32)                        \
  V(d_l, kWasmF64, kWasmI64)                        \
  V(v_i, kWasmVoid, kWasmI32)                       \
  V(v_ii, kWasmVoid, kWasmI32, kWasmI32)            \
  V(v_id, kWasmVoid, kWasmI32, kWasmF64)            \
  V(d_id, kWasmF64, kWasmI32, kWasmF64)             \
  V(v_if, kWasmVoid, kWasmI32, kWasmF32)            \
  V(f_if, kWasmF32, kWasmI32, kWasmF32)             \
  V(v_il, kWasmVoid, kWasmI32, kWasmI64)            \
  V(l_il, kWasmI64, kWasmI32, kWasmI64)             \
  V(v_iii, kWasmVoid, kWasmI32, kWasmI32, kWasmI32) \
  V(i_iii, kWasmI32, kWasmI32, kWasmI32, kWasmI32)  \
  V(l_ill, kWasmI64, kWasmI32, kWasmI64, kWasmI64)  \
  V(i_iil, kWasmI32, kWasmI32, kWasmI32, kWasmI64)  \
  V(i_ill, kWasmI32, kWasmI32, kWasmI64, kWasmI64)  \
  V(i_e, kWasmI32, kWasmExternRef)                  \
  V(i_ci, kWasmI32, kWasmFuncRef, kWasmI32)         \
  V(i_qq, kWasmI32, kWasmEqRef, kWasmEqRef)

#define FOREACH_SIMD_SIGNATURE(V)                      \
  V(s_s, kWasmS128, kWasmS128)                         \
  V(s_f, kWasmS128, kWasmF32)                          \
  V(s_d, kWasmS128, kWasmF64)                          \
  V(s_ss, kWasmS128, kWasmS128, kWasmS128)             \
  V(s_i, kWasmS128, kWasmI32)                          \
  V(s_l, kWasmS128, kWasmI64)                          \
  V(s_si, kWasmS128, kWasmS128, kWasmI32)              \
  V(i_s, kWasmI32, kWasmS128)                          \
  V(v_is, kWasmVoid, kWasmI32, kWasmS128)              \
  V(s_sss, kWasmS128, kWasmS128, kWasmS128, kWasmS128) \
  V(s_is, kWasmS128, kWasmI32, kWasmS128)

#define FOREACH_PREFIX(V) \
  V(GC, 0xfb)             \
  V(Numeric, 0xfc)        \
  V(Simd, 0xfd)           \
  V(Atomic, 0xfe)

enum WasmOpcode {
// Declare expression opcodes.
#define DECLARE_NAMED_ENUM(name, opcode, sig) kExpr##name = opcode,
  FOREACH_OPCODE(DECLARE_NAMED_ENUM)
#undef DECLARE_NAMED_ENUM
#define DECLARE_PREFIX(name, opcode) k##name##Prefix = opcode,
      FOREACH_PREFIX(DECLARE_PREFIX)
#undef DECLARE_PREFIX
};

enum TrapReason {
#define DECLARE_ENUM(name) k##name,
  FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
  kTrapCount
#undef DECLARE_ENUM
};

// A collection of opcode-related static methods.
class V8_EXPORT_PRIVATE WasmOpcodes {
 public:
  static constexpr const char* OpcodeName(WasmOpcode);
  static constexpr const FunctionSig* Signature(WasmOpcode);
  static constexpr const FunctionSig* AsmjsSignature(WasmOpcode);
  static constexpr bool IsPrefixOpcode(WasmOpcode);
  static constexpr bool IsControlOpcode(WasmOpcode);
  static constexpr bool IsExternRefOpcode(WasmOpcode);
  static constexpr bool IsThrowingOpcode(WasmOpcode);
  static constexpr bool IsRelaxedSimdOpcode(WasmOpcode);
  // Check whether the given opcode always jumps, i.e. all instructions after
  // this one in the current block are dead. Returns false for |end|.
  static constexpr bool IsUnconditionalJump(WasmOpcode);
  static constexpr bool IsBreakable(WasmOpcode);

  static constexpr MessageTemplate TrapReasonToMessageId(TrapReason);
  static inline const char* TrapReasonMessage(TrapReason);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_OPCODES_H_
