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

#define IMPORT_SIG_INDEX(v) U32V_1(v)
#define FUNC_INDEX(v) U32V_1(v)
#define TABLE_INDEX(v) U32V_1(v)
#define NO_NAME U32V_1(0)
#define NAME_LENGTH(v) U32V_1(v)
#define ENTRY_COUNT(v) U32V_1(v)

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

#define ARITY_0 0
#define ARITY_1 1
#define ARITY_2 2
#define DEPTH_0 0
#define DEPTH_1 1
#define DEPTH_2 2
#define ARITY_2 2

#define WASM_BLOCK(...) kExprBlock, kLocalVoid, __VA_ARGS__, kExprEnd

#define WASM_BLOCK_T(t, ...)                                       \
  kExprBlock, static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(t)), \
      __VA_ARGS__, kExprEnd

#define WASM_BLOCK_TT(t1, t2, ...)                                       \
  kExprBlock, kMultivalBlock, 0,                                         \
      static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(t1)),              \
      static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(t2)), __VA_ARGS__, \
      kExprEnd

#define WASM_BLOCK_I(...) kExprBlock, kLocalI32, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_L(...) kExprBlock, kLocalI64, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_F(...) kExprBlock, kLocalF32, __VA_ARGS__, kExprEnd
#define WASM_BLOCK_D(...) kExprBlock, kLocalF64, __VA_ARGS__, kExprEnd

#define WASM_INFINITE_LOOP kExprLoop, kLocalVoid, kExprBr, DEPTH_0, kExprEnd

#define WASM_LOOP(...) kExprLoop, kLocalVoid, __VA_ARGS__, kExprEnd
#define WASM_LOOP_I(...) kExprLoop, kLocalI32, __VA_ARGS__, kExprEnd
#define WASM_LOOP_L(...) kExprLoop, kLocalI64, __VA_ARGS__, kExprEnd
#define WASM_LOOP_F(...) kExprLoop, kLocalF32, __VA_ARGS__, kExprEnd
#define WASM_LOOP_D(...) kExprLoop, kLocalF64, __VA_ARGS__, kExprEnd

#define WASM_IF(cond, tstmt) cond, kExprIf, kLocalVoid, tstmt, kExprEnd

#define WASM_IF_ELSE(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalVoid, tstmt, kExprElse, fstmt, kExprEnd

#define WASM_IF_ELSE_T(t, cond, tstmt, fstmt)                                \
  cond, kExprIf, static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(t)), tstmt, \
      kExprElse, fstmt, kExprEnd

#define WASM_IF_ELSE_TT(t1, t2, cond, tstmt, fstmt)                           \
  cond, kExprIf, kMultivalBlock, 0,                                           \
      static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(t1)),                   \
      static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(t2)), tstmt, kExprElse, \
      fstmt, kExprEnd

#define WASM_IF_ELSE_I(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalI32, tstmt, kExprElse, fstmt, kExprEnd
#define WASM_IF_ELSE_L(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalI64, tstmt, kExprElse, fstmt, kExprEnd
#define WASM_IF_ELSE_F(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalF32, tstmt, kExprElse, fstmt, kExprEnd
#define WASM_IF_ELSE_D(cond, tstmt, fstmt) \
  cond, kExprIf, kLocalF64, tstmt, kExprElse, fstmt, kExprEnd

#define WASM_SELECT(tval, fval, cond) tval, fval, cond, kExprSelect

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

//------------------------------------------------------------------------------
// Misc expressions.
//------------------------------------------------------------------------------
#define WASM_ID(...) __VA_ARGS__
#define WASM_ZERO kExprI8Const, 0
#define WASM_ONE kExprI8Const, 1
#define WASM_I8(val) kExprI8Const, static_cast<byte>(val)

#define I32V_MIN(length) -(1 << (6 + (7 * ((length) - 1))))
#define I32V_MAX(length) ((1 << (6 + (7 * ((length) - 1)))) - 1)
#define I64V_MIN(length) -(1LL << (6 + (7 * ((length) - 1))))
#define I64V_MAX(length) ((1LL << (6 + 7 * ((length) - 1))) - 1)

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

// A helper for encoding local declarations prepended to the body of a
// function.
// TODO(titzer): move this to an appropriate header.
class LocalDeclEncoder {
 public:
  explicit LocalDeclEncoder(Zone* zone, FunctionSig* s = nullptr)
      : sig(s), local_decls(zone), total(0) {}

  // Prepend local declarations by creating a new buffer and copying data
  // over. The new buffer must be delete[]'d by the caller.
  void Prepend(Zone* zone, const byte** start, const byte** end) const {
    size_t size = (*end - *start);
    byte* buffer = reinterpret_cast<byte*>(zone->New(Size() + size));
    size_t pos = Emit(buffer);
    memcpy(buffer + pos, *start, size);
    pos += size;
    *start = buffer;
    *end = buffer + pos;
  }

  size_t Emit(byte* buffer) const {
    size_t pos = 0;
    pos = WriteUint32v(buffer, pos, static_cast<uint32_t>(local_decls.size()));
    for (size_t i = 0; i < local_decls.size(); ++i) {
      pos = WriteUint32v(buffer, pos, local_decls[i].first);
      buffer[pos++] = WasmOpcodes::LocalTypeCodeFor(local_decls[i].second);
    }
    DCHECK_EQ(Size(), pos);
    return pos;
  }

  // Add locals declarations to this helper. Return the index of the newly added
  // local(s), with an optional adjustment for the parameters.
  uint32_t AddLocals(uint32_t count, LocalType type) {
    uint32_t result =
        static_cast<uint32_t>(total + (sig ? sig->parameter_count() : 0));
    total += count;
    if (local_decls.size() > 0 && local_decls.back().second == type) {
      count += local_decls.back().first;
      local_decls.pop_back();
    }
    local_decls.push_back(std::pair<uint32_t, LocalType>(count, type));
    return result;
  }

  size_t Size() const {
    size_t size = SizeofUint32v(static_cast<uint32_t>(local_decls.size()));
    for (auto p : local_decls) size += 1 + SizeofUint32v(p.first);
    return size;
  }

  bool has_sig() const { return sig != nullptr; }
  FunctionSig* get_sig() const { return sig; }
  void set_sig(FunctionSig* s) { sig = s; }

 private:
  FunctionSig* sig;
  ZoneVector<std::pair<uint32_t, LocalType>> local_decls;
  size_t total;

  size_t SizeofUint32v(uint32_t val) const {
    size_t size = 1;
    while (true) {
      byte b = val & MASK_7;
      if (b == val) return size;
      size++;
      val = val >> 7;
    }
  }

  // TODO(titzer): lift encoding of u32v to a common place.
  size_t WriteUint32v(byte* buffer, size_t pos, uint32_t val) const {
    while (true) {
      byte b = val & MASK_7;
      if (b == val) {
        buffer[pos++] = b;
        break;
      }
      buffer[pos++] = 0x80 | b;
      val = val >> 7;
    }
    return pos;
  }
};
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
#define WASM_F64(val)                                        \
  kExprF64Const, static_cast<byte>(bit_cast<uint64_t>(val)), \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 8),       \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 16),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 24),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 32),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 40),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 48),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 56)
#define WASM_GET_LOCAL(index) kExprGetLocal, static_cast<byte>(index)
#define WASM_SET_LOCAL(index, val) val, kExprSetLocal, static_cast<byte>(index)
#define WASM_TEE_LOCAL(index, val) val, kExprTeeLocal, static_cast<byte>(index)
#define WASM_DROP kExprDrop
#define WASM_GET_GLOBAL(index) kExprGetGlobal, static_cast<byte>(index)
#define WASM_SET_GLOBAL(index, val) \
  val, kExprSetGlobal, static_cast<byte>(index)
#define WASM_LOAD_MEM(type, index)                                             \
  index, static_cast<byte>(                                                    \
             v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, false)), \
      ZERO_ALIGNMENT, ZERO_OFFSET
#define WASM_STORE_MEM(type, index, val)                                   \
  index, val,                                                              \
      static_cast<byte>(                                                   \
          v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, true)), \
      ZERO_ALIGNMENT, ZERO_OFFSET
#define WASM_LOAD_MEM_OFFSET(type, offset, index)                              \
  index, static_cast<byte>(                                                    \
             v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, false)), \
      ZERO_ALIGNMENT, static_cast<byte>(offset)
#define WASM_STORE_MEM_OFFSET(type, offset, index, val)                    \
  index, val,                                                              \
      static_cast<byte>(                                                   \
          v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, true)), \
      ZERO_ALIGNMENT, static_cast<byte>(offset)
#define WASM_LOAD_MEM_ALIGNMENT(type, index, alignment)                        \
  index, static_cast<byte>(                                                    \
             v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, false)), \
      alignment, ZERO_OFFSET
#define WASM_STORE_MEM_ALIGNMENT(type, index, alignment, val)              \
  index, val,                                                              \
      static_cast<byte>(                                                   \
          v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, true)), \
      alignment, ZERO_OFFSET

#define WASM_CALL_FUNCTION0(index) kExprCallFunction, static_cast<byte>(index)
#define WASM_CALL_FUNCTION(index, ...) \
  __VA_ARGS__, kExprCallFunction, static_cast<byte>(index)

// TODO(titzer): change usages of these macros to put func last.
#define WASM_CALL_INDIRECT0(index, func) \
  func, kExprCallIndirect, static_cast<byte>(index)
#define WASM_CALL_INDIRECT1(index, func, a) \
  a, func, kExprCallIndirect, static_cast<byte>(index)
#define WASM_CALL_INDIRECT2(index, func, a, b) \
  a, b, func, kExprCallIndirect, static_cast<byte>(index)
#define WASM_CALL_INDIRECT3(index, func, a, b, c) \
  a, b, c, func, kExprCallIndirect, static_cast<byte>(index)
#define WASM_CALL_INDIRECT4(index, func, a, b, c, d) \
  a, b, c, d, func, kExprCallIndirect, static_cast<byte>(index)
#define WASM_CALL_INDIRECT5(index, func, a, b, c, d, e) \
  a, b, c, d, e, func, kExprCallIndirect, static_cast<byte>(index)
#define WASM_CALL_INDIRECTN(arity, index, func, ...) \
  __VA_ARGS__, func, kExprCallIndirect, static_cast<byte>(index)

#define WASM_NOT(x) x, kExprI32Eqz
#define WASM_SEQ(...) __VA_ARGS__

//------------------------------------------------------------------------------
// Constructs that are composed of multiple bytecodes.
//------------------------------------------------------------------------------
#define WASM_WHILE(x, y)                                              \
  kExprLoop, kLocalVoid, x, kExprIf, kLocalVoid, y, kExprBr, DEPTH_1, \
      kExprEnd, kExprEnd
#define WASM_INC_LOCAL(index)                                            \
  kExprGetLocal, static_cast<byte>(index), kExprI8Const, 1, kExprI32Add, \
      kExprTeeLocal, static_cast<byte>(index)
#define WASM_INC_LOCAL_BYV(index, count)                    \
  kExprGetLocal, static_cast<byte>(index), kExprI8Const,    \
      static_cast<byte>(count), kExprI32Add, kExprTeeLocal, \
      static_cast<byte>(index)
#define WASM_INC_LOCAL_BY(index, count)                     \
  kExprGetLocal, static_cast<byte>(index), kExprI8Const,    \
      static_cast<byte>(count), kExprI32Add, kExprSetLocal, \
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
// Memory Operations.
//------------------------------------------------------------------------------
#define WASM_GROW_MEMORY(x) x, kExprGrowMemory
#define WASM_MEMORY_SIZE kExprMemorySize

//------------------------------------------------------------------------------
// Simd Operations.
//------------------------------------------------------------------------------
#define WASM_SIMD_I32x4_SPLAT(x) x, kSimdPrefix, kExprI32x4Splat & 0xff
#define WASM_SIMD_I32x4_EXTRACT_LANE(lane, x) \
  x, kSimdPrefix, kExprI32x4ExtractLane & 0xff, static_cast<byte>(lane)

#define SIG_ENTRY_v_v kWasmFunctionTypeForm, 0, 0
#define SIZEOF_SIG_ENTRY_v_v 3

#define SIG_ENTRY_v_x(a) kWasmFunctionTypeForm, 1, a, 0
#define SIG_ENTRY_v_xx(a, b) kWasmFunctionTypeForm, 2, a, b, 0
#define SIG_ENTRY_v_xxx(a, b, c) kWasmFunctionTypeForm, 3, a, b, c, 0
#define SIZEOF_SIG_ENTRY_v_x 4
#define SIZEOF_SIG_ENTRY_v_xx 5
#define SIZEOF_SIG_ENTRY_v_xxx 6

#define SIG_ENTRY_x(r) kWasmFunctionTypeForm, 0, 1, r
#define SIG_ENTRY_x_x(r, a) kWasmFunctionTypeForm, 1, a, 1, r
#define SIG_ENTRY_x_xx(r, a, b) kWasmFunctionTypeForm, 2, a, b, 1, r
#define SIG_ENTRY_x_xxx(r, a, b, c) kWasmFunctionTypeForm, 3, a, b, c, 1, r
#define SIZEOF_SIG_ENTRY_x 4
#define SIZEOF_SIG_ENTRY_x_x 5
#define SIZEOF_SIG_ENTRY_x_xx 6
#define SIZEOF_SIG_ENTRY_x_xxx 7

#define WASM_BRV(depth, val) val, kExprBr, static_cast<byte>(depth)
#define WASM_BRV_IF(depth, val, cond) \
  val, cond, kExprBrIf, static_cast<byte>(depth)
#define WASM_BRV_IFD(depth, val, cond) \
  val, cond, kExprBrIf, static_cast<byte>(depth), kExprDrop
#define WASM_IFB(cond, ...) cond, kExprIf, kLocalVoid, __VA_ARGS__, kExprEnd
#define WASM_BR_TABLEV(val, key, count, ...) \
  val, key, kExprBrTable, U32V_1(count), __VA_ARGS__

#endif  // V8_WASM_MACRO_GEN_H_
