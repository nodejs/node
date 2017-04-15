// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-text.h"

#include "src/debug/interface-types.h"
#include "src/ostreams.h"
#include "src/vector.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone.h"

using namespace v8;
using namespace v8::internal;
using namespace v8::internal::wasm;

namespace {
const char *GetOpName(WasmOpcode opcode) {
#define CASE_OP(name, str) \
  case kExpr##name:        \
    return str;
#define CASE_I32_OP(name, str) CASE_OP(I32##name, "i32." str)
#define CASE_I64_OP(name, str) CASE_OP(I64##name, "i64." str)
#define CASE_F32_OP(name, str) CASE_OP(F32##name, "f32." str)
#define CASE_F64_OP(name, str) CASE_OP(F64##name, "f64." str)
#define CASE_INT_OP(name, str) CASE_I32_OP(name, str) CASE_I64_OP(name, str)
#define CASE_FLOAT_OP(name, str) CASE_F32_OP(name, str) CASE_F64_OP(name, str)
#define CASE_ALL_OP(name, str) CASE_FLOAT_OP(name, str) CASE_INT_OP(name, str)
#define CASE_SIGN_OP(TYPE, name, str) \
  CASE_##TYPE##_OP(name##S, str "_s") CASE_##TYPE##_OP(name##U, str "_u")
#define CASE_ALL_SIGN_OP(name, str) \
  CASE_FLOAT_OP(name, str) CASE_SIGN_OP(INT, name, str)
#define CASE_CONVERT_OP(name, RES, SRC, src_suffix, str) \
  CASE_##RES##_OP(U##name##SRC, str "_u/" src_suffix)    \
      CASE_##RES##_OP(S##name##SRC, str "_s/" src_suffix)

  switch (opcode) {
    CASE_INT_OP(Eqz, "eqz")
    CASE_ALL_OP(Eq, "eq")
    CASE_ALL_OP(Ne, "ne")
    CASE_ALL_OP(Add, "add")
    CASE_ALL_OP(Sub, "sub")
    CASE_ALL_OP(Mul, "mul")
    CASE_ALL_SIGN_OP(Lt, "lt")
    CASE_ALL_SIGN_OP(Gt, "gt")
    CASE_ALL_SIGN_OP(Le, "le")
    CASE_ALL_SIGN_OP(Ge, "ge")
    CASE_INT_OP(Clz, "clz")
    CASE_INT_OP(Ctz, "ctz")
    CASE_INT_OP(Popcnt, "popcnt")
    CASE_ALL_SIGN_OP(Div, "div")
    CASE_SIGN_OP(INT, Rem, "rem")
    CASE_INT_OP(And, "and")
    CASE_INT_OP(Ior, "or")
    CASE_INT_OP(Xor, "xor")
    CASE_INT_OP(Shl, "shl")
    CASE_SIGN_OP(INT, Shr, "shr")
    CASE_INT_OP(Rol, "rol")
    CASE_INT_OP(Ror, "ror")
    CASE_FLOAT_OP(Abs, "abs")
    CASE_FLOAT_OP(Neg, "neg")
    CASE_FLOAT_OP(Ceil, "ceil")
    CASE_FLOAT_OP(Floor, "floor")
    CASE_FLOAT_OP(Trunc, "trunc")
    CASE_FLOAT_OP(NearestInt, "nearest")
    CASE_FLOAT_OP(Sqrt, "sqrt")
    CASE_FLOAT_OP(Min, "min")
    CASE_FLOAT_OP(Max, "max")
    CASE_FLOAT_OP(CopySign, "copysign")
    CASE_I32_OP(ConvertI64, "wrap/i64")
    CASE_CONVERT_OP(Convert, INT, F32, "f32", "trunc")
    CASE_CONVERT_OP(Convert, INT, F64, "f64", "trunc")
    CASE_CONVERT_OP(Convert, I64, I32, "i32", "extend")
    CASE_CONVERT_OP(Convert, F32, I32, "i32", "convert")
    CASE_CONVERT_OP(Convert, F32, I64, "i64", "convert")
    CASE_F32_OP(ConvertF64, "demote/f64")
    CASE_CONVERT_OP(Convert, F64, I32, "i32", "convert")
    CASE_CONVERT_OP(Convert, F64, I64, "i64", "convert")
    CASE_F64_OP(ConvertF32, "promote/f32")
    CASE_I32_OP(ReinterpretF32, "reinterpret/f32")
    CASE_I64_OP(ReinterpretF64, "reinterpret/f64")
    CASE_F32_OP(ReinterpretI32, "reinterpret/i32")
    CASE_F64_OP(ReinterpretI64, "reinterpret/i64")
    CASE_OP(Unreachable, "unreachable")
    CASE_OP(Nop, "nop")
    CASE_OP(Return, "return")
    CASE_OP(MemorySize, "current_memory")
    CASE_OP(GrowMemory, "grow_memory")
    CASE_OP(Loop, "loop")
    CASE_OP(If, "if")
    CASE_OP(Block, "block")
    CASE_OP(Try, "try")
    CASE_OP(Throw, "throw")
    CASE_OP(Catch, "catch")
    CASE_OP(Drop, "drop")
    CASE_OP(Select, "select")
    CASE_ALL_OP(LoadMem, "load")
    CASE_SIGN_OP(INT, LoadMem8, "load8")
    CASE_SIGN_OP(INT, LoadMem16, "load16")
    CASE_SIGN_OP(I64, LoadMem32, "load32")
    CASE_ALL_OP(StoreMem, "store")
    CASE_INT_OP(StoreMem8, "store8")
    CASE_INT_OP(StoreMem16, "store16")
    CASE_I64_OP(StoreMem32, "store32")
    CASE_OP(SetLocal, "set_local")
    CASE_OP(GetLocal, "get_local")
    CASE_OP(TeeLocal, "tee_local")
    CASE_OP(GetGlobal, "get_global")
    CASE_OP(SetGlobal, "set_global")
    CASE_OP(Br, "br")
    CASE_OP(BrIf, "br_if")
    default:
      UNREACHABLE();
      return "";
  }
}

bool IsValidFunctionName(const Vector<const char> &name) {
  if (name.is_empty()) return false;
  const char *special_chars = "_.+-*/\\^~=<>!?@#$%&|:'`";
  for (char c : name) {
    bool valid_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') || strchr(special_chars, c);
    if (!valid_char) return false;
  }
  return true;
}

}  // namespace

void wasm::PrintWasmText(const WasmModule *module,
                         const ModuleWireBytes &wire_bytes, uint32_t func_index,
                         std::ostream &os,
                         debug::WasmDisassembly::OffsetTable *offset_table) {
  DCHECK_NOT_NULL(module);
  DCHECK_GT(module->functions.size(), func_index);
  const WasmFunction *fun = &module->functions[func_index];

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  int line_nr = 0;
  int control_depth = 1;

  // Print the function signature.
  os << "func";
  WasmName fun_name = wire_bytes.GetNameOrNull(fun);
  if (IsValidFunctionName(fun_name)) {
    os << " $";
    os.write(fun_name.start(), fun_name.length());
  }
  size_t param_count = fun->sig->parameter_count();
  if (param_count) {
    os << " (param";
    for (size_t i = 0; i < param_count; ++i)
      os << ' ' << WasmOpcodes::TypeName(fun->sig->GetParam(i));
    os << ')';
  }
  size_t return_count = fun->sig->return_count();
  if (return_count) {
    os << " (result";
    for (size_t i = 0; i < return_count; ++i)
      os << ' ' << WasmOpcodes::TypeName(fun->sig->GetReturn(i));
    os << ')';
  }
  os << "\n";
  ++line_nr;

  // Print the local declarations.
  BodyLocalDecls decls(&zone);
  Vector<const byte> func_bytes = wire_bytes.module_bytes.SubVector(
      fun->code_start_offset, fun->code_end_offset);
  BytecodeIterator i(func_bytes.begin(), func_bytes.end(), &decls);
  DCHECK_LT(func_bytes.begin(), i.pc());
  if (!decls.type_list.empty()) {
    os << "(local";
    for (const ValueType &v : decls.type_list) {
      os << ' ' << WasmOpcodes::TypeName(v);
    }
    os << ")\n";
    ++line_nr;
  }

  for (; i.has_next(); i.next()) {
    WasmOpcode opcode = i.current();
    if (opcode == kExprElse || opcode == kExprEnd) --control_depth;

    DCHECK_LE(0, control_depth);
    const int kMaxIndentation = 64;
    int indentation = std::min(kMaxIndentation, 2 * control_depth);
    if (offset_table) {
      offset_table->push_back(debug::WasmDisassemblyOffsetTableEntry(
          i.pc_offset(), line_nr, indentation));
    }

    // 64 whitespaces
    const char padding[kMaxIndentation + 1] =
        "                                                                ";
    os.write(padding, indentation);

    switch (opcode) {
      case kExprLoop:
      case kExprIf:
      case kExprBlock:
      case kExprTry: {
        BlockTypeOperand operand(&i, i.pc());
        os << GetOpName(opcode);
        for (unsigned i = 0; i < operand.arity; i++) {
          os << " " << WasmOpcodes::TypeName(operand.read_entry(i));
        }
        control_depth++;
        break;
      }
      case kExprBr:
      case kExprBrIf: {
        BreakDepthOperand operand(&i, i.pc());
        os << GetOpName(opcode) << ' ' << operand.depth;
        break;
      }
      case kExprElse:
        os << "else";
        control_depth++;
        break;
      case kExprEnd:
        os << "end";
        break;
      case kExprBrTable: {
        BranchTableOperand operand(&i, i.pc());
        BranchTableIterator iterator(&i, operand);
        os << "br_table";
        while (iterator.has_next()) os << ' ' << iterator.next();
        break;
      }
      case kExprCallIndirect: {
        CallIndirectOperand operand(&i, i.pc());
        DCHECK_EQ(0, operand.table_index);
        os << "call_indirect " << operand.index;
        break;
      }
      case kExprCallFunction: {
        CallFunctionOperand operand(&i, i.pc());
        os << "call " << operand.index;
        break;
      }
      case kExprGetLocal:
      case kExprSetLocal:
      case kExprTeeLocal:
      case kExprCatch: {
        LocalIndexOperand operand(&i, i.pc());
        os << GetOpName(opcode) << ' ' << operand.index;
        break;
      }
      case kExprGetGlobal:
      case kExprSetGlobal: {
        GlobalIndexOperand operand(&i, i.pc());
        os << GetOpName(opcode) << ' ' << operand.index;
        break;
      }
#define CASE_CONST(type, str, cast_type)                           \
  case kExpr##type##Const: {                                       \
    Imm##type##Operand operand(&i, i.pc());                        \
    os << #str ".const " << static_cast<cast_type>(operand.value); \
    break;                                                         \
  }
        CASE_CONST(I32, i32, int32_t)
        CASE_CONST(I64, i64, int64_t)
        CASE_CONST(F32, f32, float)
        CASE_CONST(F64, f64, double)

#define CASE_OPCODE(opcode, _, __) case kExpr##opcode:
        FOREACH_LOAD_MEM_OPCODE(CASE_OPCODE)
        FOREACH_STORE_MEM_OPCODE(CASE_OPCODE) {
          MemoryAccessOperand operand(&i, i.pc(), kMaxUInt32);
          os << GetOpName(opcode) << " offset=" << operand.offset
             << " align=" << (1ULL << operand.alignment);
          break;
        }

        FOREACH_SIMPLE_OPCODE(CASE_OPCODE)
      case kExprUnreachable:
      case kExprNop:
      case kExprReturn:
      case kExprMemorySize:
      case kExprGrowMemory:
      case kExprDrop:
      case kExprSelect:
      case kExprThrow:
        os << GetOpName(opcode);
        break;

        // This group is just printed by their internal opcode name, as they
        // should never be shown to end-users.
        FOREACH_ASMJS_COMPAT_OPCODE(CASE_OPCODE)
        // TODO(wasm): Add correct printing for SIMD and atomic opcodes once
        // they are publicly available.
        FOREACH_SIMD_0_OPERAND_OPCODE(CASE_OPCODE)
        FOREACH_SIMD_1_OPERAND_OPCODE(CASE_OPCODE)
        FOREACH_ATOMIC_OPCODE(CASE_OPCODE)
        os << WasmOpcodes::OpcodeName(opcode);
        break;

      default:
        UNREACHABLE();
        break;
    }
    os << '\n';
    ++line_nr;
  }
  DCHECK_EQ(0, control_depth);
  DCHECK(i.ok());
}
