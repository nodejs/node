// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MACRO_GEN_H_
#define V8_WASM_MACRO_GEN_H_

#include "src/wasm/wasm-opcodes.h"

#include "src/zone/zone-containers.h"

#define U32_LE(v)                                    \
  static_cast<byte>(v), static_cast<byte>((v) >> 8), \
      static_cast<byte>((v) >> 16), static_cast<byte>((v) >> 24)

#define U16_LE(v) static_cast<byte>(v), static_cast<byte>((v) >> 8)

#define WASM_MODULE_HEADER U32_LE(kWasmMagic), U32_LE(kWasmVersion)

#define SIG_INDEX(v) U32V_1(v)
#define FUNC_INDEX(v) U32V_1(v)
#define EXCEPTION_INDEX(v) U32V_1(v)
#define NO_NAME U32V_1(0)
#define ENTRY_COUNT(v) U32V_1(v)

// Segment flags
#define ACTIVE_NO_INDEX 0
#define PASSIVE 1
#define ACTIVE_WITH_INDEX 2
#define DECLARATIVE 3
#define PASSIVE_WITH_ELEMENTS 5
#define ACTIVE_WITH_ELEMENTS 6
#define DECLARATIVE_WITH_ELEMENTS 7

// The table index field in an element segment was repurposed as a flags field.
// To specify a table index, we have to set the flag value to 2, followed by
// the table index.
#define TABLE_INDEX0 static_cast<byte>(ACTIVE_NO_INDEX)
#define TABLE_INDEX(v) static_cast<byte>(ACTIVE_WITH_INDEX), U32V_1(v)

#define ZERO_ALIGNMENT 0
#define ZERO_OFFSET 0

#define BR_TARGET(v) U32V_1(v)

#define MASK_7 ((1 << 7) - 1)
#define MASK_14 ((1 << 14) - 1)
#define MASK_21 ((1 << 21) - 1)
#define MASK_28 ((1 << 28) - 1)

#define U32V_1(x) static_cast<byte>((x)&MASK_7)
#define U32V_2(x) \
  static_cast<byte>(((x)&MASK_7) | 0x80), static_cast<byte>(((x) >> 7) & MASK_7)
#define U32V_3(x)                                      \
  static_cast<byte>((((x)) & MASK_7) | 0x80),          \
      static_cast<byte>((((x) >> 7) & MASK_7) | 0x80), \
      static_cast<byte>(((x) >> 14) & MASK_7)
#define U32V_4(x)                                       \
  static_cast<byte>(((x)&MASK_7) | 0x80),               \
      static_cast<byte>((((x) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>((((x) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((x) >> 21) & MASK_7)
#define U32V_5(x)                                       \
  static_cast<byte>(((x)&MASK_7) | 0x80),               \
      static_cast<byte>((((x) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>((((x) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>((((x) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>((((x) >> 28) & MASK_7))

// Convenience macros for building Wasm bytecode directly into a byte array.

//------------------------------------------------------------------------------
// Control.
//------------------------------------------------------------------------------
#define WASM_NOP kExprNop
#define WASM_END kExprEnd

#define ARITY_0 0
#define ARITY_1 1
#define ARITY_2 2
#define DEPTH_0 0
#define DEPTH_1 1
#define DEPTH_2 2
#define ARITY_2 2

#define WASM_BLOCK(...) kExprBlock, kLocalVoid, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_I(...) kExprBlock, kLocalI32, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_L(...) kExprBlock, kLocalI64, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_F(...) kExprBlock, kLocalF32, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_D(...) kExprBlock, kLocalF64, __VA_ARGS__, kExprEnd

#define WASM_BLOCK_T(t, ...) \
  kExprBlock, static_cast<byte>((t).value_type_code()), __VA_ARGS__, kExprEnd

#define WASM_BLOCK_X(index, ...)                                  \
  kExprBlock, static_cast<byte>(index), __VA_ARGS__, kExprEnd

#define WASM_INFINITE_LOOP kExprLoop, kLocalVoid, kExprBr, DEPTH_0, kExprEnd

#define WASM_LOOP(...) kExprLoop, kLocalVoid, __VA_ARGS__, kExprEnd
#define WASM_LOOP_I(...) kExprLoop, kLocalI32, __VA_ARGS__, kExprEnd
#define WASM_LOOP_L(...) kExprLoop, kLocalI64, __VA_ARGS__, kExprEnd
#define WASM_LOOP_F(...) kExprLoop, kLocalF32, __VA_ARGS__, kExprEnd
#define WASM_LOOP_D(...) kExprLoop, kLocalF64, __VA_ARGS__, kExprEnd

#define WASM_LOOP_T(t, ...) \
  kExprLoop, static_cast<byte>((t).value_type_code()), __VA_ARGS__, kExprEnd

#define WASM_LOOP_X(index, ...)                                   \
  kExprLoop, static_cast<byte>(index), __VA_ARGS__, kExprEnd

#define WASM_IF(cond, ...) cond, kExprIf, kLocalVoid, __VA_ARGS__, kExprEnd

#define WASM_IF_T(t, cond, ...) \
  cond, kExprIf, static_cast<byte>((t).value_type_code()), __VA_ARGS__, kExprEnd

#define WASM_IF_X(index, cond, ...)                                   \
  cond, kExprIf, static_cast<byte>(index), __VA_ARGS__, kExprEnd

#define WASM_IF_ELSE(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalVoid, tstmt, kExprElse, fstmt, kExprEnd

#define WASM_IF_ELSE_I(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalI32, tstmt, kExprElse, fstmt, kExprEnd
#define WASM_IF_ELSE_L(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalI64, tstmt, kExprElse, fstmt, kExprEnd
#define WASM_IF_ELSE_F(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalF32, tstmt, kExprElse, fstmt, kExprEnd
#define WASM_IF_ELSE_D(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalF64, tstmt, kExprElse, fstmt, kExprEnd

#define WASM_IF_ELSE_T(t, cond, tstmt, fstmt)                                \
  cond, kExprIf, static_cast<byte>((t).value_type_code()), tstmt, kExprElse, \
      fstmt, kExprEnd

#define WASM_IF_ELSE_X(index, cond, tstmt, fstmt)                            \
  cond, kExprIf, static_cast<byte>(index), tstmt, kExprElse, fstmt, kExprEnd

#define WASM_TRY_CATCH_T(t, trystmt, catchstmt)                            \
  kExprTry, static_cast<byte>((t).value_type_code()), trystmt, kExprCatch, \
      catchstmt, kExprEnd

#define WASM_SELECT(tval, fval, cond) tval, fval, cond, kExprSelect
#define WASM_SELECT_I(tval, fval, cond) \
  tval, fval, cond, kExprSelectWithType, U32V_1(1), kLocalI32
#define WASM_SELECT_L(tval, fval, cond) \
  tval, fval, cond, kExprSelectWithType, U32V_1(1), kLocalI64
#define WASM_SELECT_F(tval, fval, cond) \
  tval, fval, cond, kExprSelectWithType, U32V_1(1), kLocalF32
#define WASM_SELECT_D(tval, fval, cond) \
  tval, fval, cond, kExprSelectWithType, U32V_1(1), kLocalF64
#define WASM_SELECT_R(tval, fval, cond) \
  tval, fval, cond, kExprSelectWithType, U32V_1(1), kLocalAnyRef
#define WASM_SELECT_A(tval, fval, cond) \
  tval, fval, cond, kExprSelectWithType, U32V_1(1), kLocalFuncRef

#define WASM_RETURN0 kExprReturn
#define WASM_RETURN1(val) val, kExprReturn
#define WASM_RETURNN(count, ...) __VA_ARGS__, kExprReturn

#define WASM_BR(depth) kExprBr, static_cast<byte>(depth)
#define WASM_BR_IF(depth, cond) cond, kExprBrIf, static_cast<byte>(depth)
#define WASM_BR_IFD(depth, val, cond) \
  val, cond, kExprBrIf, static_cast<byte>(depth), kExprDrop
#define WASM_CONTINUE(depth) kExprBr, static_cast<byte>(depth)
#define WASM_UNREACHABLE kExprUnreachable

#define WASM_BR_TABLE(key, count, ...) \
  key, kExprBrTable, U32V_1(count), __VA_ARGS__

#define WASM_CASE(x) static_cast<byte>(x), static_cast<byte>(x >> 8)
#define WASM_CASE_BR(x) static_cast<byte>(x), static_cast<byte>(0x80 | (x) >> 8)

#define WASM_THROW(index) kExprThrow, static_cast<byte>(index)

//------------------------------------------------------------------------------
// Misc expressions.
//------------------------------------------------------------------------------
#define WASM_STMTS(...) __VA_ARGS__
#define WASM_ZERO kExprI32Const, 0
#define WASM_ONE kExprI32Const, 1

#define I32V_MIN(length) -(1 << (6 + (7 * ((length)-1))))
#define I32V_MAX(length) ((1 << (6 + (7 * ((length)-1)))) - 1)
#define I64V_MIN(length) -(1LL << (6 + (7 * ((length)-1))))
#define I64V_MAX(length) ((1LL << (6 + 7 * ((length)-1))) - 1)

#define I32V_IN_RANGE(value, length) \
  ((value) >= I32V_MIN(length) && (value) <= I32V_MAX(length))
#define I64V_IN_RANGE(value, length) \
  ((value) >= I64V_MIN(length) && (value) <= I64V_MAX(length))

#define WASM_NO_LOCALS 0

namespace v8 {
namespace internal {
namespace wasm {

inline void CheckI32v(int32_t value, int length) {
  DCHECK(length >= 1 && length <= 5);
  DCHECK(length == 5 || I32V_IN_RANGE(value, length));
}

inline void CheckI64v(int64_t value, int length) {
  DCHECK(length >= 1 && length <= 10);
  DCHECK(length == 10 || I64V_IN_RANGE(value, length));
}

inline WasmOpcode LoadStoreOpcodeOf(MachineType type, bool store) {
  switch (type.representation()) {
    case MachineRepresentation::kWord8:
      return store ? kExprI32StoreMem8
                   : type.IsSigned() ? kExprI32LoadMem8S : kExprI32LoadMem8U;
    case MachineRepresentation::kWord16:
      return store ? kExprI32StoreMem16
                   : type.IsSigned() ? kExprI32LoadMem16S : kExprI32LoadMem16U;
    case MachineRepresentation::kWord32:
      return store ? kExprI32StoreMem : kExprI32LoadMem;
    case MachineRepresentation::kWord64:
      return store ? kExprI64StoreMem : kExprI64LoadMem;
    case MachineRepresentation::kFloat32:
      return store ? kExprF32StoreMem : kExprF32LoadMem;
    case MachineRepresentation::kFloat64:
      return store ? kExprF64StoreMem : kExprF64LoadMem;
    case MachineRepresentation::kSimd128:
      return store ? kExprS128StoreMem : kExprS128LoadMem;
    default:
      UNREACHABLE();
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

//------------------------------------------------------------------------------
// Int32 Const operations
//------------------------------------------------------------------------------
#define WASM_I32V(val) kExprI32Const, U32V_5(val)

#define WASM_I32V_1(val) \
  static_cast<byte>(CheckI32v((val), 1), kExprI32Const), U32V_1(val)
#define WASM_I32V_2(val) \
  static_cast<byte>(CheckI32v((val), 2), kExprI32Const), U32V_2(val)
#define WASM_I32V_3(val) \
  static_cast<byte>(CheckI32v((val), 3), kExprI32Const), U32V_3(val)
#define WASM_I32V_4(val) \
  static_cast<byte>(CheckI32v((val), 4), kExprI32Const), U32V_4(val)
#define WASM_I32V_5(val) \
  static_cast<byte>(CheckI32v((val), 5), kExprI32Const), U32V_5(val)

//------------------------------------------------------------------------------
// Int64 Const operations
//------------------------------------------------------------------------------
#define WASM_I64V(val)                                                        \
  kExprI64Const,                                                              \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 49) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 56) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 63) & MASK_7)

#define WASM_I64V_1(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 1), kExprI64Const), \
      static_cast<byte>(static_cast<int64_t>(val) & MASK_7)
#define WASM_I64V_2(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 2), kExprI64Const), \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),        \
      static_cast<byte>((static_cast<int64_t>(val) >> 7) & MASK_7)
#define WASM_I64V_3(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 3), kExprI64Const), \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),        \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 14) & MASK_7)
#define WASM_I64V_4(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 4), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 21) & MASK_7)
#define WASM_I64V_5(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 5), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 28) & MASK_7)
#define WASM_I64V_6(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 6), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 35) & MASK_7)
#define WASM_I64V_7(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 7), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 42) & MASK_7)
#define WASM_I64V_8(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 8), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 49) & MASK_7)
#define WASM_I64V_9(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 9), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 49) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 56) & MASK_7)
#define WASM_I64V_10(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 10), kExprI64Const), \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 49) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 56) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 63) & MASK_7)

#define WASM_F32(val)                                                       \
  kExprF32Const,                                                            \
      static_cast<byte>(bit_cast<int32_t>(static_cast<float>(val))),        \
      static_cast<byte>(bit_cast<uint32_t>(static_cast<float>(val)) >> 8),  \
      static_cast<byte>(bit_cast<uint32_t>(static_cast<float>(val)) >> 16), \
      static_cast<byte>(bit_cast<uint32_t>(static_cast<float>(val)) >> 24)
#define WASM_F64(val)                                                        \
  kExprF64Const,                                                             \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val))),       \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 8),  \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 16), \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 24), \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 32), \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 40), \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 48), \
      static_cast<byte>(bit_cast<uint64_t>(static_cast<double>(val)) >> 56)

#define WASM_REF_NULL kExprRefNull
#define WASM_REF_FUNC(val) kExprRefFunc, val
#define WASM_REF_IS_NULL(val) val, kExprRefIsNull

#define WASM_GET_LOCAL(index) kExprLocalGet, static_cast<byte>(index)
#define WASM_SET_LOCAL(index, val) val, kExprLocalSet, static_cast<byte>(index)
#define WASM_TEE_LOCAL(index, val) val, kExprLocalTee, static_cast<byte>(index)
#define WASM_DROP kExprDrop
#define WASM_GET_GLOBAL(index) kExprGlobalGet, static_cast<byte>(index)
#define WASM_SET_GLOBAL(index, val) \
  val, kExprGlobalSet, static_cast<byte>(index)
#define WASM_TABLE_GET(table_index, index) \
  index, kExprTableGet, static_cast<byte>(table_index)
#define WASM_TABLE_SET(table_index, index, val) \
  index, val, kExprTableSet, static_cast<byte>(table_index)
#define WASM_LOAD_MEM(type, index)                                           \
  index,                                                                     \
      static_cast<byte>(v8::internal::wasm::LoadStoreOpcodeOf(type, false)), \
      ZERO_ALIGNMENT, ZERO_OFFSET
#define WASM_STORE_MEM(type, index, val)                                    \
  index, val,                                                               \
      static_cast<byte>(v8::internal::wasm::LoadStoreOpcodeOf(type, true)), \
      ZERO_ALIGNMENT, ZERO_OFFSET
#define WASM_LOAD_MEM_OFFSET(type, offset, index)                            \
  index,                                                                     \
      static_cast<byte>(v8::internal::wasm::LoadStoreOpcodeOf(type, false)), \
      ZERO_ALIGNMENT, offset
#define WASM_STORE_MEM_OFFSET(type, offset, index, val)                     \
  index, val,                                                               \
      static_cast<byte>(v8::internal::wasm::LoadStoreOpcodeOf(type, true)), \
      ZERO_ALIGNMENT, offset
#define WASM_LOAD_MEM_ALIGNMENT(type, index, alignment)                      \
  index,                                                                     \
      static_cast<byte>(v8::internal::wasm::LoadStoreOpcodeOf(type, false)), \
      alignment, ZERO_OFFSET
#define WASM_STORE_MEM_ALIGNMENT(type, index, alignment, val)               \
  index, val,                                                               \
      static_cast<byte>(v8::internal::wasm::LoadStoreOpcodeOf(type, true)), \
      alignment, ZERO_OFFSET

#define WASM_CALL_FUNCTION0(index) kExprCallFunction, static_cast<byte>(index)
#define WASM_CALL_FUNCTION(index, ...) \
  __VA_ARGS__, kExprCallFunction, static_cast<byte>(index)

#define WASM_RETURN_CALL_FUNCTION0(index) \
  kExprReturnCall, static_cast<byte>(index)
#define WASM_RETURN_CALL_FUNCTION(index, ...) \
  __VA_ARGS__, kExprReturnCall, static_cast<byte>(index)

#define TABLE_ZERO 0

// Pass: sig_index, ...args, func_index
#define WASM_CALL_INDIRECT(sig_index, ...) \
  __VA_ARGS__, kExprCallIndirect, static_cast<byte>(sig_index), TABLE_ZERO
#define WASM_CALL_INDIRECT_TABLE(table, sig_index, ...)         \
  __VA_ARGS__, kExprCallIndirect, static_cast<byte>(sig_index), \
      static_cast<byte>(table)
#define WASM_RETURN_CALL_INDIRECT(sig_index, ...) \
  __VA_ARGS__, kExprReturnCallIndirect, static_cast<byte>(sig_index), TABLE_ZERO

#define WASM_NOT(x) x, kExprI32Eqz
#define WASM_SEQ(...) __VA_ARGS__

//------------------------------------------------------------------------------
// Constructs that are composed of multiple bytecodes.
//------------------------------------------------------------------------------
#define WASM_WHILE(x, y)                                              \
  kExprLoop, kLocalVoid, x, kExprIf, kLocalVoid, y, kExprBr, DEPTH_1, \
      kExprEnd, kExprEnd
#define WASM_INC_LOCAL(index)                                             \
  kExprLocalGet, static_cast<byte>(index), kExprI32Const, 1, kExprI32Add, \
      kExprLocalTee, static_cast<byte>(index)
#define WASM_INC_LOCAL_BYV(index, count)                    \
  kExprLocalGet, static_cast<byte>(index), kExprI32Const,   \
      static_cast<byte>(count), kExprI32Add, kExprLocalTee, \
      static_cast<byte>(index)
#define WASM_INC_LOCAL_BY(index, count)                     \
  kExprLocalGet, static_cast<byte>(index), kExprI32Const,   \
      static_cast<byte>(count), kExprI32Add, kExprLocalSet, \
      static_cast<byte>(index)
#define WASM_UNOP(opcode, x) x, static_cast<byte>(opcode)
#define WASM_BINOP(opcode, x, y) x, y, static_cast<byte>(opcode)

//------------------------------------------------------------------------------
// Int32 operations
//------------------------------------------------------------------------------
#define WASM_I32_ADD(x, y) x, y, kExprI32Add
#define WASM_I32_SUB(x, y) x, y, kExprI32Sub
#define WASM_I32_MUL(x, y) x, y, kExprI32Mul
#define WASM_I32_DIVS(x, y) x, y, kExprI32DivS
#define WASM_I32_DIVU(x, y) x, y, kExprI32DivU
#define WASM_I32_REMS(x, y) x, y, kExprI32RemS
#define WASM_I32_REMU(x, y) x, y, kExprI32RemU
#define WASM_I32_AND(x, y) x, y, kExprI32And
#define WASM_I32_IOR(x, y) x, y, kExprI32Ior
#define WASM_I32_XOR(x, y) x, y, kExprI32Xor
#define WASM_I32_SHL(x, y) x, y, kExprI32Shl
#define WASM_I32_SHR(x, y) x, y, kExprI32ShrU
#define WASM_I32_SAR(x, y) x, y, kExprI32ShrS
#define WASM_I32_ROR(x, y) x, y, kExprI32Ror
#define WASM_I32_ROL(x, y) x, y, kExprI32Rol
#define WASM_I32_EQ(x, y) x, y, kExprI32Eq
#define WASM_I32_NE(x, y) x, y, kExprI32Ne
#define WASM_I32_LTS(x, y) x, y, kExprI32LtS
#define WASM_I32_LES(x, y) x, y, kExprI32LeS
#define WASM_I32_LTU(x, y) x, y, kExprI32LtU
#define WASM_I32_LEU(x, y) x, y, kExprI32LeU
#define WASM_I32_GTS(x, y) x, y, kExprI32GtS
#define WASM_I32_GES(x, y) x, y, kExprI32GeS
#define WASM_I32_GTU(x, y) x, y, kExprI32GtU
#define WASM_I32_GEU(x, y) x, y, kExprI32GeU
#define WASM_I32_CLZ(x) x, kExprI32Clz
#define WASM_I32_CTZ(x) x, kExprI32Ctz
#define WASM_I32_POPCNT(x) x, kExprI32Popcnt
#define WASM_I32_EQZ(x) x, kExprI32Eqz

//------------------------------------------------------------------------------
// Asmjs Int32 operations
//------------------------------------------------------------------------------
#define WASM_I32_ASMJS_DIVS(x, y) x, y, kExprI32AsmjsDivS
#define WASM_I32_ASMJS_REMS(x, y) x, y, kExprI32AsmjsRemS
#define WASM_I32_ASMJS_DIVU(x, y) x, y, kExprI32AsmjsDivU
#define WASM_I32_ASMJS_REMU(x, y) x, y, kExprI32AsmjsRemU

//------------------------------------------------------------------------------
// Int64 operations
//------------------------------------------------------------------------------
#define WASM_I64_ADD(x, y) x, y, kExprI64Add
#define WASM_I64_SUB(x, y) x, y, kExprI64Sub
#define WASM_I64_MUL(x, y) x, y, kExprI64Mul
#define WASM_I64_DIVS(x, y) x, y, kExprI64DivS
#define WASM_I64_DIVU(x, y) x, y, kExprI64DivU
#define WASM_I64_REMS(x, y) x, y, kExprI64RemS
#define WASM_I64_REMU(x, y) x, y, kExprI64RemU
#define WASM_I64_AND(x, y) x, y, kExprI64And
#define WASM_I64_IOR(x, y) x, y, kExprI64Ior
#define WASM_I64_XOR(x, y) x, y, kExprI64Xor
#define WASM_I64_SHL(x, y) x, y, kExprI64Shl
#define WASM_I64_SHR(x, y) x, y, kExprI64ShrU
#define WASM_I64_SAR(x, y) x, y, kExprI64ShrS
#define WASM_I64_ROR(x, y) x, y, kExprI64Ror
#define WASM_I64_ROL(x, y) x, y, kExprI64Rol
#define WASM_I64_EQ(x, y) x, y, kExprI64Eq
#define WASM_I64_NE(x, y) x, y, kExprI64Ne
#define WASM_I64_LTS(x, y) x, y, kExprI64LtS
#define WASM_I64_LES(x, y) x, y, kExprI64LeS
#define WASM_I64_LTU(x, y) x, y, kExprI64LtU
#define WASM_I64_LEU(x, y) x, y, kExprI64LeU
#define WASM_I64_GTS(x, y) x, y, kExprI64GtS
#define WASM_I64_GES(x, y) x, y, kExprI64GeS
#define WASM_I64_GTU(x, y) x, y, kExprI64GtU
#define WASM_I64_GEU(x, y) x, y, kExprI64GeU
#define WASM_I64_CLZ(x) x, kExprI64Clz
#define WASM_I64_CTZ(x) x, kExprI64Ctz
#define WASM_I64_POPCNT(x) x, kExprI64Popcnt
#define WASM_I64_EQZ(x) x, kExprI64Eqz

//------------------------------------------------------------------------------
// Float32 operations
//------------------------------------------------------------------------------
#define WASM_F32_ADD(x, y) x, y, kExprF32Add
#define WASM_F32_SUB(x, y) x, y, kExprF32Sub
#define WASM_F32_MUL(x, y) x, y, kExprF32Mul
#define WASM_F32_DIV(x, y) x, y, kExprF32Div
#define WASM_F32_MIN(x, y) x, y, kExprF32Min
#define WASM_F32_MAX(x, y) x, y, kExprF32Max
#define WASM_F32_ABS(x) x, kExprF32Abs
#define WASM_F32_NEG(x) x, kExprF32Neg
#define WASM_F32_COPYSIGN(x, y) x, y, kExprF32CopySign
#define WASM_F32_CEIL(x) x, kExprF32Ceil
#define WASM_F32_FLOOR(x) x, kExprF32Floor
#define WASM_F32_TRUNC(x) x, kExprF32Trunc
#define WASM_F32_NEARESTINT(x) x, kExprF32NearestInt
#define WASM_F32_SQRT(x) x, kExprF32Sqrt
#define WASM_F32_EQ(x, y) x, y, kExprF32Eq
#define WASM_F32_NE(x, y) x, y, kExprF32Ne
#define WASM_F32_LT(x, y) x, y, kExprF32Lt
#define WASM_F32_LE(x, y) x, y, kExprF32Le
#define WASM_F32_GT(x, y) x, y, kExprF32Gt
#define WASM_F32_GE(x, y) x, y, kExprF32Ge

//------------------------------------------------------------------------------
// Float64 operations
//------------------------------------------------------------------------------
#define WASM_F64_ADD(x, y) x, y, kExprF64Add
#define WASM_F64_SUB(x, y) x, y, kExprF64Sub
#define WASM_F64_MUL(x, y) x, y, kExprF64Mul
#define WASM_F64_DIV(x, y) x, y, kExprF64Div
#define WASM_F64_MIN(x, y) x, y, kExprF64Min
#define WASM_F64_MAX(x, y) x, y, kExprF64Max
#define WASM_F64_ABS(x) x, kExprF64Abs
#define WASM_F64_NEG(x) x, kExprF64Neg
#define WASM_F64_COPYSIGN(x, y) x, y, kExprF64CopySign
#define WASM_F64_CEIL(x) x, kExprF64Ceil
#define WASM_F64_FLOOR(x) x, kExprF64Floor
#define WASM_F64_TRUNC(x) x, kExprF64Trunc
#define WASM_F64_NEARESTINT(x) x, kExprF64NearestInt
#define WASM_F64_SQRT(x) x, kExprF64Sqrt
#define WASM_F64_EQ(x, y) x, y, kExprF64Eq
#define WASM_F64_NE(x, y) x, y, kExprF64Ne
#define WASM_F64_LT(x, y) x, y, kExprF64Lt
#define WASM_F64_LE(x, y) x, y, kExprF64Le
#define WASM_F64_GT(x, y) x, y, kExprF64Gt
#define WASM_F64_GE(x, y) x, y, kExprF64Ge

//------------------------------------------------------------------------------
// Type conversions.
//------------------------------------------------------------------------------
#define WASM_I32_SCONVERT_F32(x) x, kExprI32SConvertF32
#define WASM_I32_SCONVERT_F64(x) x, kExprI32SConvertF64
#define WASM_I32_UCONVERT_F32(x) x, kExprI32UConvertF32
#define WASM_I32_UCONVERT_F64(x) x, kExprI32UConvertF64
#define WASM_I32_CONVERT_I64(x) x, kExprI32ConvertI64
#define WASM_I64_SCONVERT_F32(x) x, kExprI64SConvertF32
#define WASM_I64_SCONVERT_F64(x) x, kExprI64SConvertF64
#define WASM_I64_UCONVERT_F32(x) x, kExprI64UConvertF32
#define WASM_I64_UCONVERT_F64(x) x, kExprI64UConvertF64
#define WASM_I64_SCONVERT_I32(x) x, kExprI64SConvertI32
#define WASM_I64_UCONVERT_I32(x) x, kExprI64UConvertI32
#define WASM_F32_SCONVERT_I32(x) x, kExprF32SConvertI32
#define WASM_F32_UCONVERT_I32(x) x, kExprF32UConvertI32
#define WASM_F32_SCONVERT_I64(x) x, kExprF32SConvertI64
#define WASM_F32_UCONVERT_I64(x) x, kExprF32UConvertI64
#define WASM_F32_CONVERT_F64(x) x, kExprF32ConvertF64
#define WASM_F32_REINTERPRET_I32(x) x, kExprF32ReinterpretI32
#define WASM_F64_SCONVERT_I32(x) x, kExprF64SConvertI32
#define WASM_F64_UCONVERT_I32(x) x, kExprF64UConvertI32
#define WASM_F64_SCONVERT_I64(x) x, kExprF64SConvertI64
#define WASM_F64_UCONVERT_I64(x) x, kExprF64UConvertI64
#define WASM_F64_CONVERT_F32(x) x, kExprF64ConvertF32
#define WASM_F64_REINTERPRET_I64(x) x, kExprF64ReinterpretI64
#define WASM_I32_REINTERPRET_F32(x) x, kExprI32ReinterpretF32
#define WASM_I64_REINTERPRET_F64(x) x, kExprI64ReinterpretF64

//------------------------------------------------------------------------------
// Numeric operations
//------------------------------------------------------------------------------
#define WASM_NUMERIC_OP(op) kNumericPrefix, static_cast<byte>(op)
#define WASM_I32_SCONVERT_SAT_F32(x) x, WASM_NUMERIC_OP(kExprI32SConvertSatF32)
#define WASM_I32_UCONVERT_SAT_F32(x) x, WASM_NUMERIC_OP(kExprI32UConvertSatF32)
#define WASM_I32_SCONVERT_SAT_F64(x) x, WASM_NUMERIC_OP(kExprI32SConvertSatF64)
#define WASM_I32_UCONVERT_SAT_F64(x) x, WASM_NUMERIC_OP(kExprI32UConvertSatF64)
#define WASM_I64_SCONVERT_SAT_F32(x) x, WASM_NUMERIC_OP(kExprI64SConvertSatF32)
#define WASM_I64_UCONVERT_SAT_F32(x) x, WASM_NUMERIC_OP(kExprI64UConvertSatF32)
#define WASM_I64_SCONVERT_SAT_F64(x) x, WASM_NUMERIC_OP(kExprI64SConvertSatF64)
#define WASM_I64_UCONVERT_SAT_F64(x) x, WASM_NUMERIC_OP(kExprI64UConvertSatF64)

#define MEMORY_ZERO 0

#define WASM_MEMORY_INIT(seg, dst, src, size) \
  dst, src, size, WASM_NUMERIC_OP(kExprMemoryInit), U32V_1(seg), MEMORY_ZERO
#define WASM_DATA_DROP(seg) WASM_NUMERIC_OP(kExprDataDrop), U32V_1(seg)
#define WASM_MEMORY_COPY(dst, src, size) \
  dst, src, size, WASM_NUMERIC_OP(kExprMemoryCopy), MEMORY_ZERO, MEMORY_ZERO
#define WASM_MEMORY_FILL(dst, val, size) \
  dst, val, size, WASM_NUMERIC_OP(kExprMemoryFill), MEMORY_ZERO
#define WASM_TABLE_INIT(table, seg, dst, src, size)             \
  dst, src, size, WASM_NUMERIC_OP(kExprTableInit), U32V_1(seg), \
      static_cast<byte>(table)
#define WASM_ELEM_DROP(seg) WASM_NUMERIC_OP(kExprElemDrop), U32V_1(seg)
#define WASM_TABLE_COPY(table_dst, table_src, dst, src, size) \
  dst, src, size, WASM_NUMERIC_OP(kExprTableCopy),            \
      static_cast<byte>(table_dst), static_cast<byte>(table_src)
#define WASM_TABLE_GROW(table, initial_value, delta)     \
  initial_value, delta, WASM_NUMERIC_OP(kExprTableGrow), \
      static_cast<byte>(table)
#define WASM_TABLE_SIZE(table) \
  WASM_NUMERIC_OP(kExprTableSize), static_cast<byte>(table)
#define WASM_TABLE_FILL(table, times, value, start) \
  times, value, start, WASM_NUMERIC_OP(kExprTableFill), static_cast<byte>(table)

//------------------------------------------------------------------------------
// Memory Operations.
//------------------------------------------------------------------------------
#define WASM_GROW_MEMORY(x) x, kExprMemoryGrow, 0
#define WASM_MEMORY_SIZE kExprMemorySize, 0

#define SIG_ENTRY_v_v kWasmFunctionTypeCode, 0, 0
#define SIZEOF_SIG_ENTRY_v_v 3

#define SIG_ENTRY_v_x(a) kWasmFunctionTypeCode, 1, a, 0
#define SIG_ENTRY_v_xx(a, b) kWasmFunctionTypeCode, 2, a, b, 0
#define SIG_ENTRY_v_xxx(a, b, c) kWasmFunctionTypeCode, 3, a, b, c, 0
#define SIZEOF_SIG_ENTRY_v_x 4
#define SIZEOF_SIG_ENTRY_v_xx 5
#define SIZEOF_SIG_ENTRY_v_xxx 6

#define SIG_ENTRY_x(r) kWasmFunctionTypeCode, 0, 1, r
#define SIG_ENTRY_x_x(r, a) kWasmFunctionTypeCode, 1, a, 1, r
#define SIG_ENTRY_x_xx(r, a, b) kWasmFunctionTypeCode, 2, a, b, 1, r
#define SIG_ENTRY_xx_xx(r, s, a, b) kWasmFunctionTypeCode, 2, a, b, 2, r, s
#define SIG_ENTRY_x_xxx(r, a, b, c) kWasmFunctionTypeCode, 3, a, b, c, 1, r
#define SIZEOF_SIG_ENTRY_x 4
#define SIZEOF_SIG_ENTRY_x_x 5
#define SIZEOF_SIG_ENTRY_x_xx 6
#define SIZEOF_SIG_ENTRY_xx_xx 7
#define SIZEOF_SIG_ENTRY_x_xxx 7

#define WASM_BRV(depth, ...) __VA_ARGS__, kExprBr, static_cast<byte>(depth)
#define WASM_BRV_IF(depth, val, cond) \
  val, cond, kExprBrIf, static_cast<byte>(depth)
#define WASM_BRV_IFD(depth, val, cond) \
  val, cond, kExprBrIf, static_cast<byte>(depth), kExprDrop
#define WASM_IFB(cond, ...) cond, kExprIf, kLocalVoid, __VA_ARGS__, kExprEnd
#define WASM_BR_TABLEV(val, key, count, ...) \
  val, key, kExprBrTable, U32V_1(count), __VA_ARGS__

//------------------------------------------------------------------------------
// Atomic Operations.
//------------------------------------------------------------------------------
#define WASM_ATOMICS_OP(op) kAtomicPrefix, static_cast<byte>(op)
#define WASM_ATOMICS_BINOP(op, x, y, representation) \
  x, y, WASM_ATOMICS_OP(op),                         \
      static_cast<byte>(ElementSizeLog2Of(representation)), ZERO_OFFSET
#define WASM_ATOMICS_TERNARY_OP(op, x, y, z, representation) \
  x, y, z, WASM_ATOMICS_OP(op),                              \
      static_cast<byte>(ElementSizeLog2Of(representation)), ZERO_OFFSET
#define WASM_ATOMICS_LOAD_OP(op, x, representation) \
  x, WASM_ATOMICS_OP(op),                           \
      static_cast<byte>(ElementSizeLog2Of(representation)), ZERO_OFFSET
#define WASM_ATOMICS_STORE_OP(op, x, y, representation) \
  x, y, WASM_ATOMICS_OP(op),                            \
      static_cast<byte>(ElementSizeLog2Of(representation)), ZERO_OFFSET
#define WASM_ATOMICS_WAIT(op, index, value, timeout, offset) \
  index, value, timeout, WASM_ATOMICS_OP(op), ZERO_ALIGNMENT, offset
#define WASM_ATOMICS_FENCE WASM_ATOMICS_OP(kExprAtomicFence), ZERO_OFFSET

//------------------------------------------------------------------------------
// Sign Externsion Operations.
//------------------------------------------------------------------------------
#define WASM_I32_SIGN_EXT_I8(x) x, kExprI32SExtendI8
#define WASM_I32_SIGN_EXT_I16(x) x, kExprI32SExtendI16
#define WASM_I64_SIGN_EXT_I8(x) x, kExprI64SExtendI8
#define WASM_I64_SIGN_EXT_I16(x) x, kExprI64SExtendI16
#define WASM_I64_SIGN_EXT_I32(x) x, kExprI64SExtendI32

//------------------------------------------------------------------------------
// Compilation Hints.
//------------------------------------------------------------------------------
#define COMPILE_STRATEGY_DEFAULT (0x00)
#define COMPILE_STRATEGY_LAZY (0x01)
#define COMPILE_STRATEGY_EAGER (0x02)
#define BASELINE_TIER_DEFAULT (0x00 << 2)
#define BASELINE_TIER_INTERPRETER (0x01 << 2)
#define BASELINE_TIER_BASELINE (0x02 << 2)
#define BASELINE_TIER_OPTIMIZED (0x03 << 2)
#define TOP_TIER_DEFAULT (0x00 << 4)
#define TOP_TIER_INTERPRETER (0x01 << 4)
#define TOP_TIER_BASELINE (0x02 << 4)
#define TOP_TIER_OPTIMIZED (0x03 << 4)

#endif  // V8_WASM_MACRO_GEN_H_
