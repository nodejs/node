// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OPCODES_H_
#define V8_WASM_OPCODES_H_

#include "src/globals.h"
#include "src/machine-type.h"
#include "src/runtime/runtime.h"
#include "src/signature.h"

namespace v8 {
namespace internal {
namespace wasm {

// Binary encoding of local types.
enum ValueTypeCode {
  kLocalVoid = 0x40,
  kLocalI32 = 0x7f,
  kLocalI64 = 0x7e,
  kLocalF32 = 0x7d,
  kLocalF64 = 0x7c,
  kLocalS128 = 0x7b,
  kLocalS1x4 = 0x7a,
  kLocalS1x8 = 0x79,
  kLocalS1x16 = 0x78
};

// Type code for multi-value block types.
static const uint8_t kMultivalBlock = 0x41;

// We reuse the internal machine type to represent WebAssembly types.
// A typedef improves readability without adding a whole new type system.
using ValueType = MachineRepresentation;
constexpr ValueType kWasmStmt = MachineRepresentation::kNone;
constexpr ValueType kWasmI32 = MachineRepresentation::kWord32;
constexpr ValueType kWasmI64 = MachineRepresentation::kWord64;
constexpr ValueType kWasmF32 = MachineRepresentation::kFloat32;
constexpr ValueType kWasmF64 = MachineRepresentation::kFloat64;
constexpr ValueType kWasmS128 = MachineRepresentation::kSimd128;
constexpr ValueType kWasmS1x4 = MachineRepresentation::kSimd1x4;
constexpr ValueType kWasmS1x8 = MachineRepresentation::kSimd1x8;
constexpr ValueType kWasmS1x16 = MachineRepresentation::kSimd1x16;
constexpr ValueType kWasmVar = MachineRepresentation::kTagged;

using FunctionSig = Signature<ValueType>;
std::ostream& operator<<(std::ostream& os, const FunctionSig& function);
bool IsJSCompatibleSignature(const FunctionSig* sig);

using WasmName = Vector<const char>;

using WasmCodePosition = int;
constexpr WasmCodePosition kNoCodePosition = -1;

// Control expressions and blocks.
#define FOREACH_CONTROL_OPCODE(V)      \
  V(Unreachable, 0x00, _)              \
  V(Nop, 0x01, _)                      \
  V(Block, 0x02, _)                    \
  V(Loop, 0x03, _)                     \
  V(If, 0x004, _)                      \
  V(Else, 0x05, _)                     \
  V(Try, 0x06, _ /* eh_prototype */)   \
  V(Catch, 0x07, _ /* eh_prototype */) \
  V(Throw, 0x08, _ /* eh_prototype */) \
  V(End, 0x0b, _)                      \
  V(Br, 0x0c, _)                       \
  V(BrIf, 0x0d, _)                     \
  V(BrTable, 0x0e, _)                  \
  V(Return, 0x0f, _)

// Constants, locals, globals, and calls.
#define FOREACH_MISC_OPCODE(V) \
  V(CallFunction, 0x10, _)     \
  V(CallIndirect, 0x11, _)     \
  V(Drop, 0x1a, _)             \
  V(Select, 0x1b, _)           \
  V(GetLocal, 0x20, _)         \
  V(SetLocal, 0x21, _)         \
  V(TeeLocal, 0x22, _)         \
  V(GetGlobal, 0x23, _)        \
  V(SetGlobal, 0x24, _)        \
  V(I32Const, 0x41, _)         \
  V(I64Const, 0x42, _)         \
  V(F32Const, 0x43, _)         \
  V(F64Const, 0x44, _)

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
  V(I64LoadMem32U, 0x35, l_i)      \
  V(S128LoadMem, 0xc0, s_i)

// Store memory expressions.
#define FOREACH_STORE_MEM_OPCODE(V) \
  V(I32StoreMem, 0x36, i_ii)        \
  V(I64StoreMem, 0x37, l_il)        \
  V(F32StoreMem, 0x38, f_if)        \
  V(F64StoreMem, 0x39, d_id)        \
  V(I32StoreMem8, 0x3a, i_ii)       \
  V(I32StoreMem16, 0x3b, i_ii)      \
  V(I64StoreMem8, 0x3c, l_il)       \
  V(I64StoreMem16, 0x3d, l_il)      \
  V(I64StoreMem32, 0x3e, l_il)      \
  V(S128StoreMem, 0xc1, s_is)

// Miscellaneous memory expressions
#define FOREACH_MISC_MEM_OPCODE(V) \
  V(MemorySize, 0x3f, i_v)         \
  V(GrowMemory, 0x40, i_i)

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
  V(F64ReinterpretI64, 0xbf, d_l)

// For compatibility with Asm.js.
#define FOREACH_ASMJS_COMPAT_OPCODE(V) \
  V(F64Acos, 0xc2, d_d)                \
  V(F64Asin, 0xc3, d_d)                \
  V(F64Atan, 0xc4, d_d)                \
  V(F64Cos, 0xc5, d_d)                 \
  V(F64Sin, 0xc6, d_d)                 \
  V(F64Tan, 0xc7, d_d)                 \
  V(F64Exp, 0xc8, d_d)                 \
  V(F64Log, 0xc9, d_d)                 \
  V(F64Atan2, 0xca, d_dd)              \
  V(F64Pow, 0xcb, d_dd)                \
  V(F64Mod, 0xcc, d_dd)                \
  V(I32AsmjsDivS, 0xd0, i_ii)          \
  V(I32AsmjsDivU, 0xd1, i_ii)          \
  V(I32AsmjsRemS, 0xd2, i_ii)          \
  V(I32AsmjsRemU, 0xd3, i_ii)          \
  V(I32AsmjsLoadMem8S, 0xd4, i_i)      \
  V(I32AsmjsLoadMem8U, 0xd5, i_i)      \
  V(I32AsmjsLoadMem16S, 0xd6, i_i)     \
  V(I32AsmjsLoadMem16U, 0xd7, i_i)     \
  V(I32AsmjsLoadMem, 0xd8, i_i)        \
  V(F32AsmjsLoadMem, 0xd9, f_i)        \
  V(F64AsmjsLoadMem, 0xda, d_i)        \
  V(I32AsmjsStoreMem8, 0xdb, i_ii)     \
  V(I32AsmjsStoreMem16, 0xdc, i_ii)    \
  V(I32AsmjsStoreMem, 0xdd, i_ii)      \
  V(F32AsmjsStoreMem, 0xde, f_if)      \
  V(F64AsmjsStoreMem, 0xdf, d_id)      \
  V(I32AsmjsSConvertF32, 0xe0, i_f)    \
  V(I32AsmjsUConvertF32, 0xe1, i_f)    \
  V(I32AsmjsSConvertF64, 0xe2, i_d)    \
  V(I32AsmjsUConvertF64, 0xe3, i_d)

#define FOREACH_SIMD_0_OPERAND_OPCODE(V) \
  V(F32x4Splat, 0xe500, s_f)             \
  V(F32x4Abs, 0xe503, s_s)               \
  V(F32x4Neg, 0xe504, s_s)               \
  V(F32x4RecipApprox, 0xe506, s_s)       \
  V(F32x4RecipSqrtApprox, 0xe507, s_s)   \
  V(F32x4Add, 0xe508, s_ss)              \
  V(F32x4AddHoriz, 0xe5b9, s_ss)         \
  V(F32x4Sub, 0xe509, s_ss)              \
  V(F32x4Mul, 0xe50a, s_ss)              \
  V(F32x4Min, 0xe50c, s_ss)              \
  V(F32x4Max, 0xe50d, s_ss)              \
  V(F32x4Eq, 0xe510, s1x4_ss)            \
  V(F32x4Ne, 0xe511, s1x4_ss)            \
  V(F32x4Lt, 0xe512, s1x4_ss)            \
  V(F32x4Le, 0xe513, s1x4_ss)            \
  V(F32x4Gt, 0xe514, s1x4_ss)            \
  V(F32x4Ge, 0xe515, s1x4_ss)            \
  V(F32x4SConvertI32x4, 0xe519, s_s)     \
  V(F32x4UConvertI32x4, 0xe51a, s_s)     \
  V(I32x4Splat, 0xe51b, s_i)             \
  V(I32x4Neg, 0xe51e, s_s)               \
  V(I32x4Add, 0xe51f, s_ss)              \
  V(I32x4AddHoriz, 0xe5ba, s_ss)         \
  V(I32x4Sub, 0xe520, s_ss)              \
  V(I32x4Mul, 0xe521, s_ss)              \
  V(I32x4MinS, 0xe522, s_ss)             \
  V(I32x4MaxS, 0xe523, s_ss)             \
  V(I32x4Eq, 0xe526, s1x4_ss)            \
  V(I32x4Ne, 0xe527, s1x4_ss)            \
  V(I32x4LtS, 0xe528, s1x4_ss)           \
  V(I32x4LeS, 0xe529, s1x4_ss)           \
  V(I32x4GtS, 0xe52a, s1x4_ss)           \
  V(I32x4GeS, 0xe52b, s1x4_ss)           \
  V(I32x4SConvertF32x4, 0xe52f, s_s)     \
  V(I32x4UConvertF32x4, 0xe537, s_s)     \
  V(I32x4SConvertI16x8Low, 0xe594, s_s)  \
  V(I32x4SConvertI16x8High, 0xe595, s_s) \
  V(I32x4UConvertI16x8Low, 0xe596, s_s)  \
  V(I32x4UConvertI16x8High, 0xe597, s_s) \
  V(I32x4MinU, 0xe530, s_ss)             \
  V(I32x4MaxU, 0xe531, s_ss)             \
  V(I32x4LtU, 0xe533, s1x4_ss)           \
  V(I32x4LeU, 0xe534, s1x4_ss)           \
  V(I32x4GtU, 0xe535, s1x4_ss)           \
  V(I32x4GeU, 0xe536, s1x4_ss)           \
  V(I16x8Splat, 0xe538, s_i)             \
  V(I16x8Neg, 0xe53b, s_s)               \
  V(I16x8Add, 0xe53c, s_ss)              \
  V(I16x8AddSaturateS, 0xe53d, s_ss)     \
  V(I16x8AddHoriz, 0xe5bb, s_ss)         \
  V(I16x8Sub, 0xe53e, s_ss)              \
  V(I16x8SubSaturateS, 0xe53f, s_ss)     \
  V(I16x8Mul, 0xe540, s_ss)              \
  V(I16x8MinS, 0xe541, s_ss)             \
  V(I16x8MaxS, 0xe542, s_ss)             \
  V(I16x8Eq, 0xe545, s1x8_ss)            \
  V(I16x8Ne, 0xe546, s1x8_ss)            \
  V(I16x8LtS, 0xe547, s1x8_ss)           \
  V(I16x8LeS, 0xe548, s1x8_ss)           \
  V(I16x8GtS, 0xe549, s1x8_ss)           \
  V(I16x8GeS, 0xe54a, s1x8_ss)           \
  V(I16x8AddSaturateU, 0xe54e, s_ss)     \
  V(I16x8SubSaturateU, 0xe54f, s_ss)     \
  V(I16x8MinU, 0xe550, s_ss)             \
  V(I16x8MaxU, 0xe551, s_ss)             \
  V(I16x8LtU, 0xe553, s1x8_ss)           \
  V(I16x8LeU, 0xe554, s1x8_ss)           \
  V(I16x8GtU, 0xe555, s1x8_ss)           \
  V(I16x8GeU, 0xe556, s1x8_ss)           \
  V(I16x8SConvertI32x4, 0xe598, s_ss)    \
  V(I16x8UConvertI32x4, 0xe599, s_ss)    \
  V(I16x8SConvertI8x16Low, 0xe59a, s_s)  \
  V(I16x8SConvertI8x16High, 0xe59b, s_s) \
  V(I16x8UConvertI8x16Low, 0xe59c, s_s)  \
  V(I16x8UConvertI8x16High, 0xe59d, s_s) \
  V(I8x16Splat, 0xe557, s_i)             \
  V(I8x16Neg, 0xe55a, s_s)               \
  V(I8x16Add, 0xe55b, s_ss)              \
  V(I8x16AddSaturateS, 0xe55c, s_ss)     \
  V(I8x16Sub, 0xe55d, s_ss)              \
  V(I8x16SubSaturateS, 0xe55e, s_ss)     \
  V(I8x16Mul, 0xe55f, s_ss)              \
  V(I8x16MinS, 0xe560, s_ss)             \
  V(I8x16MaxS, 0xe561, s_ss)             \
  V(I8x16Eq, 0xe564, s1x16_ss)           \
  V(I8x16Ne, 0xe565, s1x16_ss)           \
  V(I8x16LtS, 0xe566, s1x16_ss)          \
  V(I8x16LeS, 0xe567, s1x16_ss)          \
  V(I8x16GtS, 0xe568, s1x16_ss)          \
  V(I8x16GeS, 0xe569, s1x16_ss)          \
  V(I8x16AddSaturateU, 0xe56d, s_ss)     \
  V(I8x16SubSaturateU, 0xe56e, s_ss)     \
  V(I8x16MinU, 0xe56f, s_ss)             \
  V(I8x16MaxU, 0xe570, s_ss)             \
  V(I8x16LtU, 0xe572, s1x16_ss)          \
  V(I8x16LeU, 0xe573, s1x16_ss)          \
  V(I8x16GtU, 0xe574, s1x16_ss)          \
  V(I8x16GeU, 0xe575, s1x16_ss)          \
  V(I8x16SConvertI16x8, 0xe59e, s_ss)    \
  V(I8x16UConvertI16x8, 0xe59f, s_ss)    \
  V(S128And, 0xe576, s_ss)               \
  V(S128Or, 0xe577, s_ss)                \
  V(S128Xor, 0xe578, s_ss)               \
  V(S128Not, 0xe579, s_s)                \
  V(S32x4Select, 0xe52c, s_s1x4ss)       \
  V(S16x8Select, 0xe54b, s_s1x8ss)       \
  V(S8x16Select, 0xe56a, s_s1x16ss)      \
  V(S1x4And, 0xe580, s1x4_s1x4s1x4)      \
  V(S1x4Or, 0xe581, s1x4_s1x4s1x4)       \
  V(S1x4Xor, 0xe582, s1x4_s1x4s1x4)      \
  V(S1x4Not, 0xe583, s1x4_s1x4)          \
  V(S1x4AnyTrue, 0xe584, i_s1x4)         \
  V(S1x4AllTrue, 0xe585, i_s1x4)         \
  V(S1x8And, 0xe586, s1x8_s1x8s1x8)      \
  V(S1x8Or, 0xe587, s1x8_s1x8s1x8)       \
  V(S1x8Xor, 0xe588, s1x8_s1x8s1x8)      \
  V(S1x8Not, 0xe589, s1x8_s1x8)          \
  V(S1x8AnyTrue, 0xe58a, i_s1x8)         \
  V(S1x8AllTrue, 0xe58b, i_s1x8)         \
  V(S1x16And, 0xe58c, s1x16_s1x16s1x16)  \
  V(S1x16Or, 0xe58d, s1x16_s1x16s1x16)   \
  V(S1x16Xor, 0xe58e, s1x16_s1x16s1x16)  \
  V(S1x16Not, 0xe58f, s1x16_s1x16)       \
  V(S1x16AnyTrue, 0xe590, i_s1x16)       \
  V(S1x16AllTrue, 0xe591, i_s1x16)

#define FOREACH_SIMD_1_OPERAND_OPCODE(V) \
  V(F32x4ExtractLane, 0xe501, _)         \
  V(F32x4ReplaceLane, 0xe502, _)         \
  V(I32x4ExtractLane, 0xe51c, _)         \
  V(I32x4ReplaceLane, 0xe51d, _)         \
  V(I32x4Shl, 0xe524, _)                 \
  V(I32x4ShrS, 0xe525, _)                \
  V(I32x4ShrU, 0xe532, _)                \
  V(I16x8ExtractLane, 0xe539, _)         \
  V(I16x8ReplaceLane, 0xe53a, _)         \
  V(I16x8Shl, 0xe543, _)                 \
  V(I16x8ShrS, 0xe544, _)                \
  V(I16x8ShrU, 0xe552, _)                \
  V(I8x16ExtractLane, 0xe558, _)         \
  V(I8x16ReplaceLane, 0xe559, _)         \
  V(I8x16Shl, 0xe562, _)                 \
  V(I8x16ShrS, 0xe563, _)                \
  V(I8x16ShrU, 0xe571, _)

#define FOREACH_SIMD_MASK_OPERAND_OPCODE(V) \
  V(S32x4Shuffle, 0xe52d, s_ss)             \
  V(S16x8Shuffle, 0xe54c, s_ss)             \
  V(S8x16Shuffle, 0xe56b, s_ss)

#define FOREACH_ATOMIC_OPCODE(V)               \
  V(I32AtomicAdd8S, 0xe601, i_ii)              \
  V(I32AtomicAdd8U, 0xe602, i_ii)              \
  V(I32AtomicAdd16S, 0xe603, i_ii)             \
  V(I32AtomicAdd16U, 0xe604, i_ii)             \
  V(I32AtomicAdd, 0xe605, i_ii)                \
  V(I32AtomicAnd8S, 0xe606, i_ii)              \
  V(I32AtomicAnd8U, 0xe607, i_ii)              \
  V(I32AtomicAnd16S, 0xe608, i_ii)             \
  V(I32AtomicAnd16U, 0xe609, i_ii)             \
  V(I32AtomicAnd, 0xe60a, i_ii)                \
  V(I32AtomicCompareExchange8S, 0xe60b, i_ii)  \
  V(I32AtomicCompareExchange8U, 0xe60c, i_ii)  \
  V(I32AtomicCompareExchange16S, 0xe60d, i_ii) \
  V(I32AtomicCompareExchange16U, 0xe60e, i_ii) \
  V(I32AtomicCompareExchange, 0xe60f, i_ii)    \
  V(I32AtomicExchange8S, 0xe610, i_ii)         \
  V(I32AtomicExchange8U, 0xe611, i_ii)         \
  V(I32AtomicExchange16S, 0xe612, i_ii)        \
  V(I32AtomicExchange16U, 0xe613, i_ii)        \
  V(I32AtomicExchange, 0xe614, i_ii)           \
  V(I32AtomicOr8S, 0xe615, i_ii)               \
  V(I32AtomicOr8U, 0xe616, i_ii)               \
  V(I32AtomicOr16S, 0xe617, i_ii)              \
  V(I32AtomicOr16U, 0xe618, i_ii)              \
  V(I32AtomicOr, 0xe619, i_ii)                 \
  V(I32AtomicSub8S, 0xe61a, i_ii)              \
  V(I32AtomicSub8U, 0xe61b, i_ii)              \
  V(I32AtomicSub16S, 0xe61c, i_ii)             \
  V(I32AtomicSub16U, 0xe61d, i_ii)             \
  V(I32AtomicSub, 0xe61e, i_ii)                \
  V(I32AtomicXor8S, 0xe61f, i_ii)              \
  V(I32AtomicXor8U, 0xe620, i_ii)              \
  V(I32AtomicXor16S, 0xe621, i_ii)             \
  V(I32AtomicXor16U, 0xe622, i_ii)             \
  V(I32AtomicXor, 0xe623, i_ii)

// All opcodes.
#define FOREACH_OPCODE(V)             \
  FOREACH_CONTROL_OPCODE(V)           \
  FOREACH_MISC_OPCODE(V)              \
  FOREACH_SIMPLE_OPCODE(V)            \
  FOREACH_STORE_MEM_OPCODE(V)         \
  FOREACH_LOAD_MEM_OPCODE(V)          \
  FOREACH_MISC_MEM_OPCODE(V)          \
  FOREACH_ASMJS_COMPAT_OPCODE(V)      \
  FOREACH_SIMD_0_OPERAND_OPCODE(V)    \
  FOREACH_SIMD_1_OPERAND_OPCODE(V)    \
  FOREACH_SIMD_MASK_OPERAND_OPCODE(V) \
  FOREACH_ATOMIC_OPCODE(V)

// All signatures.
#define FOREACH_SIGNATURE(V)            \
  FOREACH_SIMD_SIGNATURE(V)             \
  V(i_ii, kWasmI32, kWasmI32, kWasmI32) \
  V(i_i, kWasmI32, kWasmI32)            \
  V(i_v, kWasmI32)                      \
  V(i_ff, kWasmI32, kWasmF32, kWasmF32) \
  V(i_f, kWasmI32, kWasmF32)            \
  V(i_dd, kWasmI32, kWasmF64, kWasmF64) \
  V(i_d, kWasmI32, kWasmF64)            \
  V(i_l, kWasmI32, kWasmI64)            \
  V(l_ll, kWasmI64, kWasmI64, kWasmI64) \
  V(i_ll, kWasmI32, kWasmI64, kWasmI64) \
  V(l_l, kWasmI64, kWasmI64)            \
  V(l_i, kWasmI64, kWasmI32)            \
  V(l_f, kWasmI64, kWasmF32)            \
  V(l_d, kWasmI64, kWasmF64)            \
  V(f_ff, kWasmF32, kWasmF32, kWasmF32) \
  V(f_f, kWasmF32, kWasmF32)            \
  V(f_d, kWasmF32, kWasmF64)            \
  V(f_i, kWasmF32, kWasmI32)            \
  V(f_l, kWasmF32, kWasmI64)            \
  V(d_dd, kWasmF64, kWasmF64, kWasmF64) \
  V(d_d, kWasmF64, kWasmF64)            \
  V(d_f, kWasmF64, kWasmF32)            \
  V(d_i, kWasmF64, kWasmI32)            \
  V(d_l, kWasmF64, kWasmI64)            \
  V(d_id, kWasmF64, kWasmI32, kWasmF64) \
  V(f_if, kWasmF32, kWasmI32, kWasmF32) \
  V(l_il, kWasmI64, kWasmI32, kWasmI64)

#define FOREACH_SIMD_SIGNATURE(V)                           \
  V(s_s, kWasmS128, kWasmS128)                              \
  V(s_f, kWasmS128, kWasmF32)                               \
  V(s_ss, kWasmS128, kWasmS128, kWasmS128)                  \
  V(s1x4_ss, kWasmS1x4, kWasmS128, kWasmS128)               \
  V(s1x8_ss, kWasmS1x8, kWasmS128, kWasmS128)               \
  V(s1x16_ss, kWasmS1x16, kWasmS128, kWasmS128)             \
  V(s_i, kWasmS128, kWasmI32)                               \
  V(s_si, kWasmS128, kWasmS128, kWasmI32)                   \
  V(i_s, kWasmI32, kWasmS128)                               \
  V(i_s1x4, kWasmI32, kWasmS1x4)                            \
  V(i_s1x8, kWasmI32, kWasmS1x8)                            \
  V(i_s1x16, kWasmI32, kWasmS1x16)                          \
  V(s_s1x4ss, kWasmS128, kWasmS1x4, kWasmS128, kWasmS128)   \
  V(s_s1x8ss, kWasmS128, kWasmS1x8, kWasmS128, kWasmS128)   \
  V(s_s1x16ss, kWasmS128, kWasmS1x16, kWasmS128, kWasmS128) \
  V(s1x4_s1x4, kWasmS1x4, kWasmS1x4)                        \
  V(s1x4_s1x4s1x4, kWasmS1x4, kWasmS1x4, kWasmS1x4)         \
  V(s1x8_s1x8, kWasmS1x8, kWasmS1x8)                        \
  V(s1x8_s1x8s1x8, kWasmS1x8, kWasmS1x8, kWasmS1x8)         \
  V(s1x16_s1x16, kWasmS1x16, kWasmS1x16)                    \
  V(s1x16_s1x16s1x16, kWasmS1x16, kWasmS1x16, kWasmS1x16)

#define FOREACH_PREFIX(V) \
  V(Simd, 0xe5)           \
  V(Atomic, 0xe6)

enum WasmOpcode {
// Declare expression opcodes.
#define DECLARE_NAMED_ENUM(name, opcode, sig) kExpr##name = opcode,
  FOREACH_OPCODE(DECLARE_NAMED_ENUM)
#undef DECLARE_NAMED_ENUM
#define DECLARE_PREFIX(name, opcode) k##name##Prefix = opcode,
      FOREACH_PREFIX(DECLARE_PREFIX)
#undef DECLARE_PREFIX
};

// The reason for a trap.
#define FOREACH_WASM_TRAPREASON(V) \
  V(TrapUnreachable)               \
  V(TrapMemOutOfBounds)            \
  V(TrapDivByZero)                 \
  V(TrapDivUnrepresentable)        \
  V(TrapRemByZero)                 \
  V(TrapFloatUnrepresentable)      \
  V(TrapFuncInvalid)               \
  V(TrapFuncSigMismatch)

enum TrapReason {
#define DECLARE_ENUM(name) k##name,
  FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
  kTrapCount
#undef DECLARE_ENUM
};

// A collection of opcode-related static methods.
class V8_EXPORT_PRIVATE WasmOpcodes {
 public:
  static const char* OpcodeName(WasmOpcode opcode);
  static FunctionSig* Signature(WasmOpcode opcode);
  static FunctionSig* AsmjsSignature(WasmOpcode opcode);
  static FunctionSig* AtomicSignature(WasmOpcode opcode);
  static bool IsPrefixOpcode(WasmOpcode opcode);
  static bool IsControlOpcode(WasmOpcode opcode);
  // Check whether the given opcode always jumps, i.e. all instructions after
  // this one in the current block are dead. Returns false for |end|.
  static bool IsUnconditionalJump(WasmOpcode opcode);

  static int TrapReasonToMessageId(TrapReason reason);
  static const char* TrapReasonMessage(TrapReason reason);

  static byte MemSize(MachineType type) {
    return 1 << ElementSizeLog2Of(type.representation());
  }

  static byte MemSize(ValueType type) { return 1 << ElementSizeLog2Of(type); }

  static ValueTypeCode ValueTypeCodeFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kLocalI32;
      case kWasmI64:
        return kLocalI64;
      case kWasmF32:
        return kLocalF32;
      case kWasmF64:
        return kLocalF64;
      case kWasmS128:
        return kLocalS128;
      case kWasmS1x4:
        return kLocalS1x4;
      case kWasmS1x8:
        return kLocalS1x8;
      case kWasmS1x16:
        return kLocalS1x16;
      case kWasmStmt:
        return kLocalVoid;
      default:
        UNREACHABLE();
        return kLocalVoid;
    }
  }

  static MachineType MachineTypeFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return MachineType::Int32();
      case kWasmI64:
        return MachineType::Int64();
      case kWasmF32:
        return MachineType::Float32();
      case kWasmF64:
        return MachineType::Float64();
      case kWasmS128:
        return MachineType::Simd128();
      case kWasmS1x4:
        return MachineType::Simd1x4();
      case kWasmS1x8:
        return MachineType::Simd1x8();
      case kWasmS1x16:
        return MachineType::Simd1x16();
      case kWasmStmt:
        return MachineType::None();
      default:
        UNREACHABLE();
        return MachineType::None();
    }
  }

  static ValueType ValueTypeFor(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return kWasmI32;
      case MachineRepresentation::kWord64:
        return kWasmI64;
      case MachineRepresentation::kFloat32:
        return kWasmF32;
      case MachineRepresentation::kFloat64:
        return kWasmF64;
      case MachineRepresentation::kSimd128:
        return kWasmS128;
      case MachineRepresentation::kSimd1x4:
        return kWasmS1x4;
      case MachineRepresentation::kSimd1x8:
        return kWasmS1x8;
      case MachineRepresentation::kSimd1x16:
        return kWasmS1x16;
      default:
        UNREACHABLE();
        return kWasmI32;
    }
  }

  static char ShortNameOf(ValueType type) {
    switch (type) {
      case kWasmI32:
        return 'i';
      case kWasmI64:
        return 'l';
      case kWasmF32:
        return 'f';
      case kWasmF64:
        return 'd';
      case kWasmS128:
      case kWasmS1x4:
      case kWasmS1x8:
      case kWasmS1x16:
        return 's';
      case kWasmStmt:
        return 'v';
      case kWasmVar:
        return '*';
      default:
        return '?';
    }
  }

  static const char* TypeName(ValueType type) {
    switch (type) {
      case kWasmI32:
        return "i32";
      case kWasmI64:
        return "i64";
      case kWasmF32:
        return "f32";
      case kWasmF64:
        return "f64";
      case kWasmS128:
        return "s128";
      case kWasmS1x4:
        return "s1x4";
      case kWasmS1x8:
        return "s1x8";
      case kWasmS1x16:
        return "s1x16";
      case kWasmStmt:
        return "<stmt>";
      case kWasmVar:
        return "<var>";
      default:
        return "<unknown>";
    }
  }
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OPCODES_H_
