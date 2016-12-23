// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OPCODES_H_
#define V8_WASM_OPCODES_H_

#include "src/machine-type.h"
#include "src/signature.h"

namespace v8 {
namespace internal {
namespace wasm {

// Binary encoding of local types.
enum LocalTypeCode {
  kLocalVoid = 0,
  kLocalI32 = 1,
  kLocalI64 = 2,
  kLocalF32 = 3,
  kLocalF64 = 4,
  kLocalS128 = 5
};

// Type code for multi-value block types.
static const uint8_t kMultivalBlock = 0x41;

// We reuse the internal machine type to represent WebAssembly AST types.
// A typedef improves readability without adding a whole new type system.
typedef MachineRepresentation LocalType;
const LocalType kAstStmt = MachineRepresentation::kNone;
const LocalType kAstI32 = MachineRepresentation::kWord32;
const LocalType kAstI64 = MachineRepresentation::kWord64;
const LocalType kAstF32 = MachineRepresentation::kFloat32;
const LocalType kAstF64 = MachineRepresentation::kFloat64;
const LocalType kAstS128 = MachineRepresentation::kSimd128;
// We use kTagged here because kNone is already used by kAstStmt.
const LocalType kAstEnd = MachineRepresentation::kTagged;

typedef Signature<LocalType> FunctionSig;
std::ostream& operator<<(std::ostream& os, const FunctionSig& function);

typedef Vector<const char> WasmName;

typedef int WasmCodePosition;
const WasmCodePosition kNoCodePosition = -1;

// Control expressions and blocks.
#define FOREACH_CONTROL_OPCODE(V) \
  V(Unreachable, 0x00, _)         \
  V(Block, 0x01, _)               \
  V(Loop, 0x02, _)                \
  V(If, 0x03, _)                  \
  V(Else, 0x04, _)                \
  V(Select, 0x05, _)              \
  V(Br, 0x06, _)                  \
  V(BrIf, 0x07, _)                \
  V(BrTable, 0x08, _)             \
  V(Return, 0x09, _)              \
  V(Nop, 0x0a, _)                 \
  V(Throw, 0xfa, _)               \
  V(Try, 0xfb, _)                 \
  V(Catch, 0xfe, _)               \
  V(End, 0x0F, _)

// Constants, locals, globals, and calls.
#define FOREACH_MISC_OPCODE(V) \
  V(I32Const, 0x10, _)         \
  V(I64Const, 0x11, _)         \
  V(F64Const, 0x12, _)         \
  V(F32Const, 0x13, _)         \
  V(GetLocal, 0x14, _)         \
  V(SetLocal, 0x15, _)         \
  V(TeeLocal, 0x19, _)         \
  V(Drop, 0x0b, _)             \
  V(CallFunction, 0x16, _)     \
  V(CallIndirect, 0x17, _)     \
  V(I8Const, 0xcb, _)          \
  V(GetGlobal, 0xbb, _)        \
  V(SetGlobal, 0xbc, _)

// Load memory expressions.
#define FOREACH_LOAD_MEM_OPCODE(V) \
  V(I32LoadMem8S, 0x20, i_i)       \
  V(I32LoadMem8U, 0x21, i_i)       \
  V(I32LoadMem16S, 0x22, i_i)      \
  V(I32LoadMem16U, 0x23, i_i)      \
  V(I64LoadMem8S, 0x24, l_i)       \
  V(I64LoadMem8U, 0x25, l_i)       \
  V(I64LoadMem16S, 0x26, l_i)      \
  V(I64LoadMem16U, 0x27, l_i)      \
  V(I64LoadMem32S, 0x28, l_i)      \
  V(I64LoadMem32U, 0x29, l_i)      \
  V(I32LoadMem, 0x2a, i_i)         \
  V(I64LoadMem, 0x2b, l_i)         \
  V(F32LoadMem, 0x2c, f_i)         \
  V(F64LoadMem, 0x2d, d_i)

// Store memory expressions.
#define FOREACH_STORE_MEM_OPCODE(V) \
  V(I32StoreMem8, 0x2e, i_ii)       \
  V(I32StoreMem16, 0x2f, i_ii)      \
  V(I64StoreMem8, 0x30, l_il)       \
  V(I64StoreMem16, 0x31, l_il)      \
  V(I64StoreMem32, 0x32, l_il)      \
  V(I32StoreMem, 0x33, i_ii)        \
  V(I64StoreMem, 0x34, l_il)        \
  V(F32StoreMem, 0x35, f_if)        \
  V(F64StoreMem, 0x36, d_id)

#define FOREACH_SIMPLE_MEM_OPCODE(V) V(GrowMemory, 0x39, i_i)

// Load memory expressions.
#define FOREACH_MISC_MEM_OPCODE(V) \
  V(MemorySize, 0x3b, i_v)

// Expressions with signatures.
#define FOREACH_SIMPLE_OPCODE(V)  \
  V(I32Add, 0x40, i_ii)           \
  V(I32Sub, 0x41, i_ii)           \
  V(I32Mul, 0x42, i_ii)           \
  V(I32DivS, 0x43, i_ii)          \
  V(I32DivU, 0x44, i_ii)          \
  V(I32RemS, 0x45, i_ii)          \
  V(I32RemU, 0x46, i_ii)          \
  V(I32And, 0x47, i_ii)           \
  V(I32Ior, 0x48, i_ii)           \
  V(I32Xor, 0x49, i_ii)           \
  V(I32Shl, 0x4a, i_ii)           \
  V(I32ShrU, 0x4b, i_ii)          \
  V(I32ShrS, 0x4c, i_ii)          \
  V(I32Eq, 0x4d, i_ii)            \
  V(I32Ne, 0x4e, i_ii)            \
  V(I32LtS, 0x4f, i_ii)           \
  V(I32LeS, 0x50, i_ii)           \
  V(I32LtU, 0x51, i_ii)           \
  V(I32LeU, 0x52, i_ii)           \
  V(I32GtS, 0x53, i_ii)           \
  V(I32GeS, 0x54, i_ii)           \
  V(I32GtU, 0x55, i_ii)           \
  V(I32GeU, 0x56, i_ii)           \
  V(I32Clz, 0x57, i_i)            \
  V(I32Ctz, 0x58, i_i)            \
  V(I32Popcnt, 0x59, i_i)         \
  V(I32Eqz, 0x5a, i_i)            \
  V(I64Add, 0x5b, l_ll)           \
  V(I64Sub, 0x5c, l_ll)           \
  V(I64Mul, 0x5d, l_ll)           \
  V(I64DivS, 0x5e, l_ll)          \
  V(I64DivU, 0x5f, l_ll)          \
  V(I64RemS, 0x60, l_ll)          \
  V(I64RemU, 0x61, l_ll)          \
  V(I64And, 0x62, l_ll)           \
  V(I64Ior, 0x63, l_ll)           \
  V(I64Xor, 0x64, l_ll)           \
  V(I64Shl, 0x65, l_ll)           \
  V(I64ShrU, 0x66, l_ll)          \
  V(I64ShrS, 0x67, l_ll)          \
  V(I64Eq, 0x68, i_ll)            \
  V(I64Ne, 0x69, i_ll)            \
  V(I64LtS, 0x6a, i_ll)           \
  V(I64LeS, 0x6b, i_ll)           \
  V(I64LtU, 0x6c, i_ll)           \
  V(I64LeU, 0x6d, i_ll)           \
  V(I64GtS, 0x6e, i_ll)           \
  V(I64GeS, 0x6f, i_ll)           \
  V(I64GtU, 0x70, i_ll)           \
  V(I64GeU, 0x71, i_ll)           \
  V(I64Clz, 0x72, l_l)            \
  V(I64Ctz, 0x73, l_l)            \
  V(I64Popcnt, 0x74, l_l)         \
  V(I64Eqz, 0xba, i_l)            \
  V(F32Add, 0x75, f_ff)           \
  V(F32Sub, 0x76, f_ff)           \
  V(F32Mul, 0x77, f_ff)           \
  V(F32Div, 0x78, f_ff)           \
  V(F32Min, 0x79, f_ff)           \
  V(F32Max, 0x7a, f_ff)           \
  V(F32Abs, 0x7b, f_f)            \
  V(F32Neg, 0x7c, f_f)            \
  V(F32CopySign, 0x7d, f_ff)      \
  V(F32Ceil, 0x7e, f_f)           \
  V(F32Floor, 0x7f, f_f)          \
  V(F32Trunc, 0x80, f_f)          \
  V(F32NearestInt, 0x81, f_f)     \
  V(F32Sqrt, 0x82, f_f)           \
  V(F32Eq, 0x83, i_ff)            \
  V(F32Ne, 0x84, i_ff)            \
  V(F32Lt, 0x85, i_ff)            \
  V(F32Le, 0x86, i_ff)            \
  V(F32Gt, 0x87, i_ff)            \
  V(F32Ge, 0x88, i_ff)            \
  V(F64Add, 0x89, d_dd)           \
  V(F64Sub, 0x8a, d_dd)           \
  V(F64Mul, 0x8b, d_dd)           \
  V(F64Div, 0x8c, d_dd)           \
  V(F64Min, 0x8d, d_dd)           \
  V(F64Max, 0x8e, d_dd)           \
  V(F64Abs, 0x8f, d_d)            \
  V(F64Neg, 0x90, d_d)            \
  V(F64CopySign, 0x91, d_dd)      \
  V(F64Ceil, 0x92, d_d)           \
  V(F64Floor, 0x93, d_d)          \
  V(F64Trunc, 0x94, d_d)          \
  V(F64NearestInt, 0x95, d_d)     \
  V(F64Sqrt, 0x96, d_d)           \
  V(F64Eq, 0x97, i_dd)            \
  V(F64Ne, 0x98, i_dd)            \
  V(F64Lt, 0x99, i_dd)            \
  V(F64Le, 0x9a, i_dd)            \
  V(F64Gt, 0x9b, i_dd)            \
  V(F64Ge, 0x9c, i_dd)            \
  V(I32SConvertF32, 0x9d, i_f)    \
  V(I32SConvertF64, 0x9e, i_d)    \
  V(I32UConvertF32, 0x9f, i_f)    \
  V(I32UConvertF64, 0xa0, i_d)    \
  V(I32ConvertI64, 0xa1, i_l)     \
  V(I64SConvertF32, 0xa2, l_f)    \
  V(I64SConvertF64, 0xa3, l_d)    \
  V(I64UConvertF32, 0xa4, l_f)    \
  V(I64UConvertF64, 0xa5, l_d)    \
  V(I64SConvertI32, 0xa6, l_i)    \
  V(I64UConvertI32, 0xa7, l_i)    \
  V(F32SConvertI32, 0xa8, f_i)    \
  V(F32UConvertI32, 0xa9, f_i)    \
  V(F32SConvertI64, 0xaa, f_l)    \
  V(F32UConvertI64, 0xab, f_l)    \
  V(F32ConvertF64, 0xac, f_d)     \
  V(F32ReinterpretI32, 0xad, f_i) \
  V(F64SConvertI32, 0xae, d_i)    \
  V(F64UConvertI32, 0xaf, d_i)    \
  V(F64SConvertI64, 0xb0, d_l)    \
  V(F64UConvertI64, 0xb1, d_l)    \
  V(F64ConvertF32, 0xb2, d_f)     \
  V(F64ReinterpretI64, 0xb3, d_l) \
  V(I32ReinterpretF32, 0xb4, i_f) \
  V(I64ReinterpretF64, 0xb5, l_d) \
  V(I32Ror, 0xb6, i_ii)           \
  V(I32Rol, 0xb7, i_ii)           \
  V(I64Ror, 0xb8, l_ll)           \
  V(I64Rol, 0xb9, l_ll)

// For compatibility with Asm.js.
#define FOREACH_ASMJS_COMPAT_OPCODE(V) \
  V(F64Acos, 0xc0, d_d)                \
  V(F64Asin, 0xc1, d_d)                \
  V(F64Atan, 0xc2, d_d)                \
  V(F64Cos, 0xc3, d_d)                 \
  V(F64Sin, 0xc4, d_d)                 \
  V(F64Tan, 0xc5, d_d)                 \
  V(F64Exp, 0xc6, d_d)                 \
  V(F64Log, 0xc7, d_d)                 \
  V(F64Atan2, 0xc8, d_dd)              \
  V(F64Pow, 0xc9, d_dd)                \
  V(F64Mod, 0xca, d_dd)                \
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
  V(F32x4ReplaceLane, 0xe502, s_sif)     \
  V(F32x4Abs, 0xe503, s_s)               \
  V(F32x4Neg, 0xe504, s_s)               \
  V(F32x4Sqrt, 0xe505, s_s)              \
  V(F32x4RecipApprox, 0xe506, s_s)       \
  V(F32x4SqrtApprox, 0xe507, s_s)        \
  V(F32x4Add, 0xe508, s_ss)              \
  V(F32x4Sub, 0xe509, s_ss)              \
  V(F32x4Mul, 0xe50a, s_ss)              \
  V(F32x4Div, 0xe50b, s_ss)              \
  V(F32x4Min, 0xe50c, s_ss)              \
  V(F32x4Max, 0xe50d, s_ss)              \
  V(F32x4MinNum, 0xe50e, s_ss)           \
  V(F32x4MaxNum, 0xe50f, s_ss)           \
  V(F32x4Eq, 0xe510, s_ss)               \
  V(F32x4Ne, 0xe511, s_ss)               \
  V(F32x4Lt, 0xe512, s_ss)               \
  V(F32x4Le, 0xe513, s_ss)               \
  V(F32x4Gt, 0xe514, s_ss)               \
  V(F32x4Ge, 0xe515, s_ss)               \
  V(F32x4Select, 0xe516, s_sss)          \
  V(F32x4Swizzle, 0xe517, s_s)           \
  V(F32x4Shuffle, 0xe518, s_ss)          \
  V(F32x4FromInt32x4, 0xe519, s_s)       \
  V(F32x4FromUint32x4, 0xe51a, s_s)      \
  V(I32x4Splat, 0xe51b, s_i)             \
  V(I32x4ReplaceLane, 0xe51d, s_sii)     \
  V(I32x4Neg, 0xe51e, s_s)               \
  V(I32x4Add, 0xe51f, s_ss)              \
  V(I32x4Sub, 0xe520, s_ss)              \
  V(I32x4Mul, 0xe521, s_ss)              \
  V(I32x4Min_s, 0xe522, s_ss)            \
  V(I32x4Max_s, 0xe523, s_ss)            \
  V(I32x4Shl, 0xe524, s_si)              \
  V(I32x4Shr_s, 0xe525, s_si)            \
  V(I32x4Eq, 0xe526, s_ss)               \
  V(I32x4Ne, 0xe527, s_ss)               \
  V(I32x4Lt_s, 0xe528, s_ss)             \
  V(I32x4Le_s, 0xe529, s_ss)             \
  V(I32x4Gt_s, 0xe52a, s_ss)             \
  V(I32x4Ge_s, 0xe52b, s_ss)             \
  V(I32x4Select, 0xe52c, s_sss)          \
  V(I32x4Swizzle, 0xe52d, s_s)           \
  V(I32x4Shuffle, 0xe52e, s_ss)          \
  V(I32x4FromFloat32x4, 0xe52f, s_s)     \
  V(I32x4Min_u, 0xe530, s_ss)            \
  V(I32x4Max_u, 0xe531, s_ss)            \
  V(I32x4Shr_u, 0xe532, s_ss)            \
  V(I32x4Lt_u, 0xe533, s_ss)             \
  V(I32x4Le_u, 0xe534, s_ss)             \
  V(I32x4Gt_u, 0xe535, s_ss)             \
  V(I32x4Ge_u, 0xe536, s_ss)             \
  V(Ui32x4FromFloat32x4, 0xe537, s_s)    \
  V(I16x8Splat, 0xe538, s_i)             \
  V(I16x8ReplaceLane, 0xe53a, s_sii)     \
  V(I16x8Neg, 0xe53b, s_s)               \
  V(I16x8Add, 0xe53c, s_ss)              \
  V(I16x8AddSaturate_s, 0xe53d, s_ss)    \
  V(I16x8Sub, 0xe53e, s_ss)              \
  V(I16x8SubSaturate_s, 0xe53f, s_ss)    \
  V(I16x8Mul, 0xe540, s_ss)              \
  V(I16x8Min_s, 0xe541, s_ss)            \
  V(I16x8Max_s, 0xe542, s_ss)            \
  V(I16x8Shl, 0xe543, s_si)              \
  V(I16x8Shr_s, 0xe544, s_si)            \
  V(I16x8Eq, 0xe545, s_ss)               \
  V(I16x8Ne, 0xe546, s_ss)               \
  V(I16x8Lt_s, 0xe547, s_ss)             \
  V(I16x8Le_s, 0xe548, s_ss)             \
  V(I16x8Gt_s, 0xe549, s_ss)             \
  V(I16x8Ge_s, 0xe54a, s_ss)             \
  V(I16x8Select, 0xe54b, s_sss)          \
  V(I16x8Swizzle, 0xe54c, s_s)           \
  V(I16x8Shuffle, 0xe54d, s_ss)          \
  V(I16x8AddSaturate_u, 0xe54e, s_ss)    \
  V(I16x8SubSaturate_u, 0xe54f, s_ss)    \
  V(I16x8Min_u, 0xe550, s_ss)            \
  V(I16x8Max_u, 0xe551, s_ss)            \
  V(I16x8Shr_u, 0xe552, s_si)            \
  V(I16x8Lt_u, 0xe553, s_ss)             \
  V(I16x8Le_u, 0xe554, s_ss)             \
  V(I16x8Gt_u, 0xe555, s_ss)             \
  V(I16x8Ge_u, 0xe556, s_ss)             \
  V(I8x16Splat, 0xe557, s_i)             \
  V(I8x16ReplaceLane, 0xe559, s_sii)     \
  V(I8x16Neg, 0xe55a, s_s)               \
  V(I8x16Add, 0xe55b, s_ss)              \
  V(I8x16AddSaturate_s, 0xe55c, s_ss)    \
  V(I8x16Sub, 0xe55d, s_ss)              \
  V(I8x16SubSaturate_s, 0xe55e, s_ss)    \
  V(I8x16Mul, 0xe55f, s_ss)              \
  V(I8x16Min_s, 0xe560, s_ss)            \
  V(I8x16Max_s, 0xe561, s_ss)            \
  V(I8x16Shl, 0xe562, s_si)              \
  V(I8x16Shr_s, 0xe563, s_si)            \
  V(I8x16Eq, 0xe564, s_ss)               \
  V(I8x16Neq, 0xe565, s_ss)              \
  V(I8x16Lt_s, 0xe566, s_ss)             \
  V(I8x16Le_s, 0xe567, s_ss)             \
  V(I8x16Gt_s, 0xe568, s_ss)             \
  V(I8x16Ge_s, 0xe569, s_ss)             \
  V(I8x16Select, 0xe56a, s_sss)          \
  V(I8x16Swizzle, 0xe56b, s_s)           \
  V(I8x16Shuffle, 0xe56c, s_ss)          \
  V(I8x16AddSaturate_u, 0xe56d, s_ss)    \
  V(I8x16Sub_saturate_u, 0xe56e, s_ss)   \
  V(I8x16Min_u, 0xe56f, s_ss)            \
  V(I8x16Max_u, 0xe570, s_ss)            \
  V(I8x16Shr_u, 0xe571, s_ss)            \
  V(I8x16Lt_u, 0xe572, s_ss)             \
  V(I8x16Le_u, 0xe573, s_ss)             \
  V(I8x16Gt_u, 0xe574, s_ss)             \
  V(I8x16Ge_u, 0xe575, s_ss)             \
  V(S128And, 0xe576, s_ss)               \
  V(S128Ior, 0xe577, s_ss)               \
  V(S128Xor, 0xe578, s_ss)               \
  V(S128Not, 0xe579, s_s)

#define FOREACH_SIMD_1_OPERAND_OPCODE(V) \
  V(F32x4ExtractLane, 0xe501, _)         \
  V(I32x4ExtractLane, 0xe51c, _)         \
  V(I16x8ExtractLane, 0xe539, _)         \
  V(I8x16ExtractLane, 0xe558, _)

// All opcodes.
#define FOREACH_OPCODE(V)          \
  FOREACH_CONTROL_OPCODE(V)        \
  FOREACH_MISC_OPCODE(V)           \
  FOREACH_SIMPLE_OPCODE(V)         \
  FOREACH_SIMPLE_MEM_OPCODE(V)     \
  FOREACH_STORE_MEM_OPCODE(V)      \
  FOREACH_LOAD_MEM_OPCODE(V)       \
  FOREACH_MISC_MEM_OPCODE(V)       \
  FOREACH_ASMJS_COMPAT_OPCODE(V)   \
  FOREACH_SIMD_0_OPERAND_OPCODE(V) \
  FOREACH_SIMD_1_OPERAND_OPCODE(V)

// All signatures.
#define FOREACH_SIGNATURE(V)         \
  FOREACH_SIMD_SIGNATURE(V)          \
  V(i_ii, kAstI32, kAstI32, kAstI32) \
  V(i_i, kAstI32, kAstI32)           \
  V(i_v, kAstI32)                    \
  V(i_ff, kAstI32, kAstF32, kAstF32) \
  V(i_f, kAstI32, kAstF32)           \
  V(i_dd, kAstI32, kAstF64, kAstF64) \
  V(i_d, kAstI32, kAstF64)           \
  V(i_l, kAstI32, kAstI64)           \
  V(l_ll, kAstI64, kAstI64, kAstI64) \
  V(i_ll, kAstI32, kAstI64, kAstI64) \
  V(l_l, kAstI64, kAstI64)           \
  V(l_i, kAstI64, kAstI32)           \
  V(l_f, kAstI64, kAstF32)           \
  V(l_d, kAstI64, kAstF64)           \
  V(f_ff, kAstF32, kAstF32, kAstF32) \
  V(f_f, kAstF32, kAstF32)           \
  V(f_d, kAstF32, kAstF64)           \
  V(f_i, kAstF32, kAstI32)           \
  V(f_l, kAstF32, kAstI64)           \
  V(d_dd, kAstF64, kAstF64, kAstF64) \
  V(d_d, kAstF64, kAstF64)           \
  V(d_f, kAstF64, kAstF32)           \
  V(d_i, kAstF64, kAstI32)           \
  V(d_l, kAstF64, kAstI64)           \
  V(d_id, kAstF64, kAstI32, kAstF64) \
  V(f_if, kAstF32, kAstI32, kAstF32) \
  V(l_il, kAstI64, kAstI32, kAstI64)

#define FOREACH_SIMD_SIGNATURE(V)                  \
  V(s_s, kAstS128, kAstS128)                       \
  V(s_f, kAstS128, kAstF32)                        \
  V(s_sif, kAstS128, kAstS128, kAstI32, kAstF32)   \
  V(s_ss, kAstS128, kAstS128, kAstS128)            \
  V(s_sss, kAstS128, kAstS128, kAstS128, kAstS128) \
  V(s_i, kAstS128, kAstI32)                        \
  V(s_sii, kAstS128, kAstS128, kAstI32, kAstI32)   \
  V(s_si, kAstS128, kAstS128, kAstI32)

#define FOREACH_PREFIX(V) V(Simd, 0xe5)

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
  V(TrapFuncSigMismatch)           \
  V(TrapInvalidIndex)

enum TrapReason {
#define DECLARE_ENUM(name) k##name,
  FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
  kTrapCount
#undef DECLARE_ENUM
};

// A collection of opcode-related static methods.
class WasmOpcodes {
 public:
  static const char* OpcodeName(WasmOpcode opcode);
  static const char* ShortOpcodeName(WasmOpcode opcode);
  static FunctionSig* Signature(WasmOpcode opcode);
  static FunctionSig* AsmjsSignature(WasmOpcode opcode);
  static bool IsPrefixOpcode(WasmOpcode opcode);

  static int TrapReasonToMessageId(TrapReason reason);
  static const char* TrapReasonMessage(TrapReason reason);

  static byte MemSize(MachineType type) {
    return 1 << ElementSizeLog2Of(type.representation());
  }

  static byte MemSize(LocalType type) { return 1 << ElementSizeLog2Of(type); }

  static LocalTypeCode LocalTypeCodeFor(LocalType type) {
    switch (type) {
      case kAstI32:
        return kLocalI32;
      case kAstI64:
        return kLocalI64;
      case kAstF32:
        return kLocalF32;
      case kAstF64:
        return kLocalF64;
      case kAstS128:
        return kLocalS128;
      case kAstStmt:
        return kLocalVoid;
      default:
        UNREACHABLE();
        return kLocalVoid;
    }
  }

  static MachineType MachineTypeFor(LocalType type) {
    switch (type) {
      case kAstI32:
        return MachineType::Int32();
      case kAstI64:
        return MachineType::Int64();
      case kAstF32:
        return MachineType::Float32();
      case kAstF64:
        return MachineType::Float64();
      case kAstS128:
        return MachineType::Simd128();
      case kAstStmt:
        return MachineType::None();
      default:
        UNREACHABLE();
        return MachineType::None();
    }
  }

  static LocalType LocalTypeFor(MachineType type) {
    if (type == MachineType::Int8()) {
      return kAstI32;
    } else if (type == MachineType::Uint8()) {
      return kAstI32;
    } else if (type == MachineType::Int16()) {
      return kAstI32;
    } else if (type == MachineType::Uint16()) {
      return kAstI32;
    } else if (type == MachineType::Int32()) {
      return kAstI32;
    } else if (type == MachineType::Uint32()) {
      return kAstI32;
    } else if (type == MachineType::Int64()) {
      return kAstI64;
    } else if (type == MachineType::Uint64()) {
      return kAstI64;
    } else if (type == MachineType::Float32()) {
      return kAstF32;
    } else if (type == MachineType::Float64()) {
      return kAstF64;
    } else if (type == MachineType::Simd128()) {
      return kAstS128;
    } else {
      UNREACHABLE();
      return kAstI32;
    }
  }

  static WasmOpcode LoadStoreOpcodeOf(MachineType type, bool store) {
    if (type == MachineType::Int8()) {
      return store ? kExprI32StoreMem8 : kExprI32LoadMem8S;
    } else if (type == MachineType::Uint8()) {
      return store ? kExprI32StoreMem8 : kExprI32LoadMem8U;
    } else if (type == MachineType::Int16()) {
      return store ? kExprI32StoreMem16 : kExprI32LoadMem16S;
    } else if (type == MachineType::Uint16()) {
      return store ? kExprI32StoreMem16 : kExprI32LoadMem16U;
    } else if (type == MachineType::Int32()) {
      return store ? kExprI32StoreMem : kExprI32LoadMem;
    } else if (type == MachineType::Uint32()) {
      return store ? kExprI32StoreMem : kExprI32LoadMem;
    } else if (type == MachineType::Int64()) {
      return store ? kExprI64StoreMem : kExprI64LoadMem;
    } else if (type == MachineType::Uint64()) {
      return store ? kExprI64StoreMem : kExprI64LoadMem;
    } else if (type == MachineType::Float32()) {
      return store ? kExprF32StoreMem : kExprF32LoadMem;
    } else if (type == MachineType::Float64()) {
      return store ? kExprF64StoreMem : kExprF64LoadMem;
    } else {
      UNREACHABLE();
      return kExprNop;
    }
  }

  static char ShortNameOf(LocalType type) {
    switch (type) {
      case kAstI32:
        return 'i';
      case kAstI64:
        return 'l';
      case kAstF32:
        return 'f';
      case kAstF64:
        return 'd';
      case kAstS128:
        return 's';
      case kAstStmt:
        return 'v';
      case kAstEnd:
        return 'x';
      default:
        UNREACHABLE();
        return '?';
    }
  }

  static const char* TypeName(LocalType type) {
    switch (type) {
      case kAstI32:
        return "i32";
      case kAstI64:
        return "i64";
      case kAstF32:
        return "f32";
      case kAstF64:
        return "f64";
      case kAstS128:
        return "s128";
      case kAstStmt:
        return "<stmt>";
      case kAstEnd:
        return "<end>";
      default:
        return "<unknown>";
    }
  }
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OPCODES_H_
