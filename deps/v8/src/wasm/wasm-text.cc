// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-text.h"

#include "src/debug/interface-types.h"
#include "src/utils/ostreams.h"
#include "src/utils/vector.h"
#include "src/objects/objects-inl.h"
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
  if (name.empty()) return false;
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
  WasmName fun_name = wire_bytes.GetNameOrNull(fun, module);
  if (IsValidFunctionName(fun_name)) {
    os << " $";
    os.write(fun_name.begin(), fun_name.length());
  }
  if (fun->sig->parameter_count()) {
    os << " (param";
    for (auto param : fun->sig->parameters())
      os << ' ' << ValueTypes::TypeName(param);
    os << ')';
  }
  if (fun->sig->return_count()) {
    os << " (result";
    for (auto ret : fun->sig->returns()) os << ' ' << ValueTypes::TypeName(ret);
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
      os << ' ' << ValueTypes::TypeName(v);
    }
    os << ")\n";
    ++line_nr;
  }

  for (; i.has_next(); i.next()) {
    WasmOpcode opcode = i.current();
    if (opcode == kExprElse || opcode == kExprCatch || opcode == kExprEnd) {
      --control_depth;
    }

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
        BlockTypeImmediate<Decoder::kNoValidate> imm(WasmFeatures::All(), &i,
                                                     i.pc());
        os << WasmOpcodes::OpcodeName(opcode);
        if (imm.type == kWasmBottom) {
          os << " (type " << imm.sig_index << ")";
        } else if (imm.out_arity() > 0) {
          os << " " << ValueTypes::TypeName(imm.out_type(0));
        }
        control_depth++;
        break;
      }
      case kExprBr:
      case kExprBrIf: {
        BranchDepthImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.depth;
        break;
      }
      case kExprBrOnExn: {
        BranchOnExceptionImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.depth.depth << ' '
           << imm.index.index;
        break;
      }
      case kExprElse:
      case kExprCatch:
        os << WasmOpcodes::OpcodeName(opcode);
        control_depth++;
        break;
      case kExprEnd:
        os << "end";
        break;
      case kExprBrTable: {
        BranchTableImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        BranchTableIterator<Decoder::kNoValidate> iterator(&i, imm);
        os << "br_table";
        while (iterator.has_next()) os << ' ' << iterator.next();
        break;
      }
      case kExprCallIndirect:
      case kExprReturnCallIndirect: {
        CallIndirectImmediate<Decoder::kNoValidate> imm(WasmFeatures::All(), &i,
                                                        i.pc());
        DCHECK_EQ(0, imm.table_index);
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.sig_index;
        break;
      }
      case kExprCallFunction:
      case kExprReturnCall: {
        CallFunctionImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
        break;
      }
      case kExprLocalGet:
      case kExprLocalSet:
      case kExprLocalTee: {
        LocalIndexImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
        break;
      }
      case kExprThrow: {
        ExceptionIndexImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
        break;
      }
      case kExprGlobalGet:
      case kExprGlobalSet: {
        GlobalIndexImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
        break;
      }
      case kExprTableGet:
      case kExprTableSet: {
        TableIndexImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
        break;
      }
      case kExprSelectWithType: {
        SelectTypeImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' '
           << ValueTypes::TypeName(imm.type);
        break;
      }
#define CASE_CONST(type, str, cast_type)                        \
  case kExpr##type##Const: {                                    \
    Imm##type##Immediate<Decoder::kNoValidate> imm(&i, i.pc()); \
    os << #str ".const " << static_cast<cast_type>(imm.value);  \
    break;                                                      \
  }
        CASE_CONST(I32, i32, int32_t)
        CASE_CONST(I64, i64, int64_t)
        CASE_CONST(F32, f32, float)
        CASE_CONST(F64, f64, double)
#undef CASE_CONST

      case kExprRefFunc: {
        FunctionIndexImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
        break;
      }

#define CASE_OPCODE(opcode, _, __) case kExpr##opcode:
        FOREACH_LOAD_MEM_OPCODE(CASE_OPCODE)
        FOREACH_STORE_MEM_OPCODE(CASE_OPCODE) {
          MemoryAccessImmediate<Decoder::kNoValidate> imm(&i, i.pc(),
                                                          kMaxUInt32);
          os << WasmOpcodes::OpcodeName(opcode) << " offset=" << imm.offset
             << " align=" << (1ULL << imm.alignment);
          break;
        }

        FOREACH_SIMPLE_OPCODE(CASE_OPCODE)
        FOREACH_SIMPLE_PROTOTYPE_OPCODE(CASE_OPCODE)
      case kExprUnreachable:
      case kExprNop:
      case kExprReturn:
      case kExprMemorySize:
      case kExprMemoryGrow:
      case kExprDrop:
      case kExprSelect:
      case kExprRethrow:
      case kExprRefNull:
        os << WasmOpcodes::OpcodeName(opcode);
        break;

      case kNumericPrefix: {
        WasmOpcode numeric_opcode = i.prefixed_opcode();
        switch (numeric_opcode) {
          case kExprI32SConvertSatF32:
          case kExprI32UConvertSatF32:
          case kExprI32SConvertSatF64:
          case kExprI32UConvertSatF64:
          case kExprI64SConvertSatF32:
          case kExprI64UConvertSatF32:
          case kExprI64SConvertSatF64:
          case kExprI64UConvertSatF64:
          case kExprMemoryCopy:
          case kExprMemoryFill:
            os << WasmOpcodes::OpcodeName(opcode);
            break;
          case kExprMemoryInit: {
            MemoryInitImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode) << ' '
               << imm.data_segment_index;
            break;
          }
          case kExprDataDrop: {
            DataDropImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
            break;
          }
          case kExprTableInit: {
            TableInitImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode) << ' '
               << imm.elem_segment_index << ' ' << imm.table.index;
            break;
          }
          case kExprElemDrop: {
            ElemDropImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
            break;
          }
          case kExprTableCopy: {
            TableCopyImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.table_src.index
               << ' ' << imm.table_dst.index;
            break;
          }
          case kExprTableGrow:
          case kExprTableSize:
          case kExprTableFill: {
            TableIndexImmediate<Decoder::kNoValidate> imm(&i, i.pc() + 1);
            os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.index;
            break;
          }
          default:
            UNREACHABLE();
            break;
        }
        break;
      }

      case kSimdPrefix: {
        WasmOpcode simd_opcode = i.prefixed_opcode();
        switch (simd_opcode) {
          case kExprS128LoadMem:
          case kExprS128StoreMem: {
            MemoryAccessImmediate<Decoder::kNoValidate> imm(&i, i.pc(),
                                                            kMaxUInt32);
            os << WasmOpcodes::OpcodeName(opcode) << " offset=" << imm.offset
               << " align=" << (1ULL << imm.alignment);
            break;
          }

          case kExprS8x16Shuffle: {
            Simd8x16ShuffleImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode);
            for (uint8_t v : imm.shuffle) {
              os << ' ' << v;
            }
            break;
          }

          case kExprI8x16ExtractLaneS:
          case kExprI8x16ExtractLaneU:
          case kExprI16x8ExtractLaneS:
          case kExprI16x8ExtractLaneU:
          case kExprI32x4ExtractLane:
          case kExprI64x2ExtractLane:
          case kExprF32x4ExtractLane:
          case kExprF64x2ExtractLane:
          case kExprI8x16ReplaceLane:
          case kExprI16x8ReplaceLane:
          case kExprI32x4ReplaceLane:
          case kExprI64x2ReplaceLane:
          case kExprF32x4ReplaceLane:
          case kExprF64x2ReplaceLane: {
            SimdLaneImmediate<Decoder::kNoValidate> imm(&i, i.pc());
            os << WasmOpcodes::OpcodeName(opcode) << ' ' << imm.lane;
            break;
          }

            FOREACH_SIMD_0_OPERAND_OPCODE(CASE_OPCODE) {
              os << WasmOpcodes::OpcodeName(opcode);
              break;
            }

          default:
            UNREACHABLE();
            break;
        }
        break;
      }

      case kAtomicPrefix: {
        WasmOpcode atomic_opcode = i.prefixed_opcode();
        switch (atomic_opcode) {
          FOREACH_ATOMIC_OPCODE(CASE_OPCODE) {
            MemoryAccessImmediate<Decoder::kNoValidate> imm(&i, i.pc() + 1,
                                                            kMaxUInt32);
            os << WasmOpcodes::OpcodeName(atomic_opcode)
               << " offset=" << imm.offset
               << " align=" << (1ULL << imm.alignment);
            break;
          }
          FOREACH_ATOMIC_0_OPERAND_OPCODE(CASE_OPCODE) {
            os << WasmOpcodes::OpcodeName(atomic_opcode);
            break;
          }
          default:
            UNREACHABLE();
            break;
        }
        break;
      }

        // This group is just printed by their internal opcode name, as they
        // should never be shown to end-users.
        FOREACH_ASMJS_COMPAT_OPCODE(CASE_OPCODE) {
          os << WasmOpcodes::OpcodeName(opcode);
        }
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
