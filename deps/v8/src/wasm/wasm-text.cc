// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-text.h"

#include "src/debug/interface-types.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/vector.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
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

void PrintWasmText(const WasmModule* module, const ModuleWireBytes& wire_bytes,
                   uint32_t func_index, std::ostream& os,
                   debug::WasmDisassembly::OffsetTable* offset_table) {
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
  if (fun->sig->parameter_count()) {
    os << " (param";
    for (auto param : fun->sig->parameters())
      os << ' ' << WasmOpcodes::TypeName(param);
    os << ')';
  }
  if (fun->sig->return_count()) {
    os << " (result";
    for (auto ret : fun->sig->returns())
      os << ' ' << WasmOpcodes::TypeName(ret);
    os << ')';
  }
  os << "\n";
  ++line_nr;

  // Print the local declarations.
  BodyLocalDecls decls(&zone);
  Vector<const byte> func_bytes = wire_bytes.GetFunctionBytes(fun);
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
      offset_table->emplace_back(i.pc_offset(), line_nr, indentation);
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
        BlockTypeOperand<Decoder::kNoValidate> operand(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode);
        if (operand.type == kWasmVar) {
          os << " (type " << operand.sig_index << ")";
        } else if (operand.out_arity() > 0) {
          os << " " << WasmOpcodes::TypeName(operand.out_type(0));
        }
        control_depth++;
        break;
      }
      case kExprBr:
      case kExprBrIf: {
        BreakDepthOperand<Decoder::kNoValidate> operand(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << operand.depth;
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
        BranchTableOperand<Decoder::kNoValidate> operand(&i, i.pc());
        BranchTableIterator<Decoder::kNoValidate> iterator(&i, operand);
        os << "br_table";
        while (iterator.has_next()) os << ' ' << iterator.next();
        break;
      }
      case kExprCallIndirect: {
        CallIndirectOperand<Decoder::kNoValidate> operand(&i, i.pc());
        DCHECK_EQ(0, operand.table_index);
        os << "call_indirect " << operand.index;
        break;
      }
      case kExprCallFunction: {
        CallFunctionOperand<Decoder::kNoValidate> operand(&i, i.pc());
        os << "call " << operand.index;
        break;
      }
      case kExprGetLocal:
      case kExprSetLocal:
      case kExprTeeLocal: {
        LocalIndexOperand<Decoder::kNoValidate> operand(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << operand.index;
        break;
      }
      case kExprThrow:
      case kExprCatch: {
        ExceptionIndexOperand<Decoder::kNoValidate> operand(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << operand.index;
        break;
      }
      case kExprGetGlobal:
      case kExprSetGlobal: {
        GlobalIndexOperand<Decoder::kNoValidate> operand(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << operand.index;
        break;
      }
#define CASE_CONST(type, str, cast_type)                           \
  case kExpr##type##Const: {                                       \
    Imm##type##Operand<Decoder::kNoValidate> operand(&i, i.pc());  \
    os << #str ".const " << static_cast<cast_type>(operand.value); \
    break;                                                         \
  }
        CASE_CONST(I32, i32, int32_t)
        CASE_CONST(I64, i64, int64_t)
        CASE_CONST(F32, f32, float)
        CASE_CONST(F64, f64, double)
#undef CASE_CONST

#define CASE_OPCODE(opcode, _, __) case kExpr##opcode:
        FOREACH_LOAD_MEM_OPCODE(CASE_OPCODE)
        FOREACH_STORE_MEM_OPCODE(CASE_OPCODE) {
          MemoryAccessOperand<Decoder::kNoValidate> operand(&i, i.pc(),
                                                            kMaxUInt32);
          os << WasmOpcodes::OpcodeName(opcode) << " offset=" << operand.offset
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
        os << WasmOpcodes::OpcodeName(opcode);
        break;
      case kAtomicPrefix: {
        WasmOpcode atomic_opcode = i.prefixed_opcode();
        switch (atomic_opcode) {
          FOREACH_ATOMIC_OPCODE(CASE_OPCODE) {
            MemoryAccessOperand<Decoder::kNoValidate> operand(&i, i.pc(),
                                                              kMaxUInt32);
            os << WasmOpcodes::OpcodeName(atomic_opcode)
               << " offset=" << operand.offset
               << " align=" << (1ULL << operand.alignment);
            break;
          }
          default:
            UNREACHABLE();
            break;
        }
      }

        // This group is just printed by their internal opcode name, as they
        // should never be shown to end-users.
        FOREACH_ASMJS_COMPAT_OPCODE(CASE_OPCODE)
        // TODO(wasm): Add correct printing for SIMD and atomic opcodes once
        // they are publicly available.
        FOREACH_SIMD_0_OPERAND_OPCODE(CASE_OPCODE)
        FOREACH_SIMD_1_OPERAND_OPCODE(CASE_OPCODE)
        FOREACH_SIMD_MASK_OPERAND_OPCODE(CASE_OPCODE)
        FOREACH_SIMD_MEM_OPCODE(CASE_OPCODE)
        os << WasmOpcodes::OpcodeName(opcode);
        break;
#undef CASE_OPCODE

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

}  // namespace wasm
}  // namespace internal
}  // namespace v8
