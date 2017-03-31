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
  kLocalS128 = 0x7b
};

// Type code for multi-value block types.
static const uint8_t kMultivalBlock = 0x41;

// We reuse the internal machine type to represent WebAssembly types.
// A typedef improves readability without adding a whole new type system.
typedef MachineRepresentation ValueType;
const ValueType kWasmStmt = MachineRepresentation::kNone;
const ValueType kWasmI32 = MachineRepresentation::kWord32;
const ValueType kWasmI64 = MachineRepresentation::kWord64;
const ValueType kWasmF32 = MachineRepresentation::kFloat32;
const ValueType kWasmF64 = MachineRepresentation::kFloat64;
const ValueType kWasmS128 = MachineRepresentation::kSimd128;
const ValueType kWasmVar = MachineRepresentation::kTagged;

typedef Signature<ValueType> FunctionSig;
std::ostream& operator<<(std::ostream& os, const FunctionSig& function);

typedef Vector<const char> WasmName;

typedef int WasmCodePosition;
const WasmCodePosition kNoCodePosition = -1;

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
  V(I64LoadMem32U, 0x35, l_i)

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
  V(I64StoreMem32, 0x3e, l_il)

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
  V(F32x4FromInt32x4, 0xe519, s_s)       \
  V(F32x4FromUint32x4, 0xe51a, s_s)      \
  V(I32x4Splat, 0xe51b, s_i)             \
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
  V(S128Not, 0xe579, s_s)                \
  V(S32x4Select, 0xe580, s_sss)          \
  V(S32x4Swizzle, 0xe581, s_s)           \
  V(S32x4Shuffle, 0xe582, s_ss)

#define FOREACH_SIMD_1_OPERAND_OPCODE(V) \
  V(F32x4ExtractLane, 0xe501, _)         \
  V(F32x4ReplaceLane, 0xe502, _)         \
  V(I32x4ExtractLane, 0xe51c, _)         \
  V(I32x4ReplaceLane, 0xe51d, _)         \
  V(I16x8ExtractLane, 0xe539, _)         \
  V(I16x8ReplaceLane, 0xe53a, _)         \
  V(I8x16ExtractLane, 0xe558, _)         \
  V(I8x16ReplaceLane, 0xe559, _)

#define FOREACH_ATOMIC_OPCODE(V)               \
  V(I32AtomicAdd8S, 0xe601, i_ii)              \
  V(I32AtomicAdd8U, 0xe602, i_ii)              \
  V(I32AtomicAdd16S, 0xe603, i_ii)             \
  V(I32AtomicAdd16U, 0xe604, i_ii)             \
  V(I32AtomicAdd32, 0xe605, i_ii)              \
  V(I32AtomicAnd8S, 0xe606, i_ii)              \
  V(I32AtomicAnd8U, 0xe607, i_ii)              \
  V(I32AtomicAnd16S, 0xe608, i_ii)             \
  V(I32AtomicAnd16U, 0xe609, i_ii)             \
  V(I32AtomicAnd32, 0xe60a, i_ii)              \
  V(I32AtomicCompareExchange8S, 0xe60b, i_ii)  \
  V(I32AtomicCompareExchange8U, 0xe60c, i_ii)  \
  V(I32AtomicCompareExchange16S, 0xe60d, i_ii) \
  V(I32AtomicCompareExchange16U, 0xe60e, i_ii) \
  V(I32AtomicCompareExchange32, 0xe60f, i_ii)  \
  V(I32AtomicExchange8S, 0xe610, i_ii)         \
  V(I32AtomicExchange8U, 0xe611, i_ii)         \
  V(I32AtomicExchange16S, 0xe612, i_ii)        \
  V(I32AtomicExchange16U, 0xe613, i_ii)        \
  V(I32AtomicExchange32, 0xe614, i_ii)         \
  V(I32AtomicOr8S, 0xe615, i_ii)               \
  V(I32AtomicOr8U, 0xe616, i_ii)               \
  V(I32AtomicOr16S, 0xe617, i_ii)              \
  V(I32AtomicOr16U, 0xe618, i_ii)              \
  V(I32AtomicOr32, 0xe619, i_ii)               \
  V(I32AtomicSub8S, 0xe61a, i_ii)              \
  V(I32AtomicSub8U, 0xe61b, i_ii)              \
  V(I32AtomicSub16S, 0xe61c, i_ii)             \
  V(I32AtomicSub16U, 0xe61d, i_ii)             \
  V(I32AtomicSub32, 0xe61e, i_ii)              \
  V(I32AtomicXor8S, 0xe61f, i_ii)              \
  V(I32AtomicXor8U, 0xe620, i_ii)              \
  V(I32AtomicXor16S, 0xe621, i_ii)             \
  V(I32AtomicXor16U, 0xe622, i_ii)             \
  V(I32AtomicXor32, 0xe623, i_ii)

// All opcodes.
#define FOREACH_OPCODE(V)          \
  FOREACH_CONTROL_OPCODE(V)        \
  FOREACH_MISC_OPCODE(V)           \
  FOREACH_SIMPLE_OPCODE(V)         \
  FOREACH_STORE_MEM_OPCODE(V)      \
  FOREACH_LOAD_MEM_OPCODE(V)       \
  FOREACH_MISC_MEM_OPCODE(V)       \
  FOREACH_ASMJS_COMPAT_OPCODE(V)   \
  FOREACH_SIMD_0_OPERAND_OPCODE(V) \
  FOREACH_SIMD_1_OPERAND_OPCODE(V) \
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

#define FOREACH_SIMD_SIGNATURE(V)                      \
  V(s_s, kWasmS128, kWasmS128)                         \
  V(s_f, kWasmS128, kWasmF32)                          \
  V(s_ss, kWasmS128, kWasmS128, kWasmS128)             \
  V(s_sss, kWasmS128, kWasmS128, kWasmS128, kWasmS128) \
  V(s_i, kWasmS128, kWasmI32)                          \
  V(s_si, kWasmS128, kWasmS128, kWasmI32)

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
  static const char* ShortOpcodeName(WasmOpcode opcode);
  static FunctionSig* Signature(WasmOpcode opcode);
  static FunctionSig* AsmjsSignature(WasmOpcode opcode);
  static FunctionSig* AtomicSignature(WasmOpcode opcode);
  static bool IsPrefixOpcode(WasmOpcode opcode);

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
      case kWasmStmt:
        return MachineType::None();
      default:
        UNREACHABLE();
        return MachineType::None();
    }
  }

  static ValueType ValueTypeFor(MachineType type) {
    if (type == MachineType::Int8()) {
      return kWasmI32;
    } else if (type == MachineType::Uint8()) {
      return kWasmI32;
    } else if (type == MachineType::Int16()) {
      return kWasmI32;
    } else if (type == MachineType::Uint16()) {
      return kWasmI32;
    } else if (type == MachineType::Int32()) {
      return kWasmI32;
    } else if (type == MachineType::Uint32()) {
      return kWasmI32;
    } else if (type == MachineType::Int64()) {
      return kWasmI64;
    } else if (type == MachineType::Uint64()) {
      return kWasmI64;
    } else if (type == MachineType::Float32()) {
      return kWasmF32;
    } else if (type == MachineType::Float64()) {
      return kWasmF64;
    } else if (type == MachineType::Simd128()) {
      return kWasmS128;
    } else {
      UNREACHABLE();
      return kWasmI32;
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
