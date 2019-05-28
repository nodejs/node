// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-body-decoder.h"

#include "src/flags.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

bool DecodeLocalDecls(const WasmFeatures& enabled, BodyLocalDecls* decls,
                      const byte* start, const byte* end) {
  Decoder decoder(start, end);
  if (WasmDecoder<Decoder::kValidate>::DecodeLocals(enabled, &decoder, nullptr,
                                                    &decls->type_list)) {
    DCHECK(decoder.ok());
    decls->encoded_size = decoder.pc_offset();
    return true;
  }
  return false;
}

BytecodeIterator::BytecodeIterator(const byte* start, const byte* end,
                                   BodyLocalDecls* decls)
    : Decoder(start, end) {
  if (decls != nullptr) {
    if (DecodeLocalDecls(kAllWasmFeatures, decls, start, end)) {
      pc_ += decls->encoded_size;
      if (pc_ > end_) pc_ = end_;
    }
  }
}

DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                            const WasmFeatures& enabled,
                            const WasmModule* module, WasmFeatures* detected,
                            FunctionBody& body) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder<Decoder::kValidate, EmptyInterface> decoder(
      &zone, module, enabled, detected, body);
  decoder.Decode();
  return decoder.toResult(nullptr);
}

unsigned OpcodeLength(const byte* pc, const byte* end) {
  Decoder decoder(pc, end);
  return WasmDecoder<Decoder::kNoValidate>::OpcodeLength(&decoder, pc);
}

std::pair<uint32_t, uint32_t> StackEffect(const WasmModule* module,
                                          FunctionSig* sig, const byte* pc,
                                          const byte* end) {
  WasmFeatures unused_detected_features;
  WasmDecoder<Decoder::kNoValidate> decoder(
      module, kAllWasmFeatures, &unused_detected_features, sig, pc, end);
  return decoder.StackEffect(pc);
}

void PrintRawWasmCode(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  PrintRawWasmCode(&allocator, FunctionBody{nullptr, 0, start, end}, nullptr,
                   kPrintLocals);
}

namespace {
const char* RawOpcodeName(WasmOpcode opcode) {
  switch (opcode) {
#define DECLARE_NAME_CASE(name, opcode, sig) \
  case kExpr##name:                          \
    return "kExpr" #name;
    FOREACH_OPCODE(DECLARE_NAME_CASE)
#undef DECLARE_NAME_CASE
    default:
      break;
  }
  return "Unknown";
}
}  // namespace

bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const WasmModule* module, PrintLocals print_locals) {
  StdoutStream os;
  return PrintRawWasmCode(allocator, body, module, print_locals, os);
}

bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const WasmModule* module, PrintLocals print_locals,
                      std::ostream& os, std::vector<int>* line_numbers) {
  Zone zone(allocator, ZONE_NAME);
  WasmFeatures unused_detected_features;
  WasmDecoder<Decoder::kNoValidate> decoder(module, kAllWasmFeatures,
                                            &unused_detected_features, body.sig,
                                            body.start, body.end);
  int line_nr = 0;
  constexpr int kNoByteCode = -1;

  // Print the function signature.
  if (body.sig) {
    os << "// signature: " << *body.sig << std::endl;
    if (line_numbers) line_numbers->push_back(kNoByteCode);
    ++line_nr;
  }

  // Print the local declarations.
  BodyLocalDecls decls(&zone);
  BytecodeIterator i(body.start, body.end, &decls);
  if (body.start != i.pc() && print_locals == kPrintLocals) {
    os << "// locals:";
    if (!decls.type_list.empty()) {
      ValueType type = decls.type_list[0];
      uint32_t count = 0;
      for (size_t pos = 0; pos < decls.type_list.size(); ++pos) {
        if (decls.type_list[pos] == type) {
          ++count;
        } else {
          os << " " << count << " " << ValueTypes::TypeName(type);
          type = decls.type_list[pos];
          count = 1;
        }
      }
      os << " " << count << " " << ValueTypes::TypeName(type);
    }
    os << std::endl;
    if (line_numbers) line_numbers->push_back(kNoByteCode);
    ++line_nr;

    for (const byte* locals = body.start; locals < i.pc(); locals++) {
      os << (locals == body.start ? "0x" : " 0x") << AsHex(*locals, 2) << ",";
    }
    os << std::endl;
    if (line_numbers) line_numbers->push_back(kNoByteCode);
    ++line_nr;
  }

  os << "// body: " << std::endl;
  if (line_numbers) line_numbers->push_back(kNoByteCode);
  ++line_nr;
  unsigned control_depth = 0;
  for (; i.has_next(); i.next()) {
    unsigned length =
        WasmDecoder<Decoder::kNoValidate>::OpcodeLength(&decoder, i.pc());

    WasmOpcode opcode = i.current();
    if (line_numbers) line_numbers->push_back(i.position());
    if (opcode == kExprElse || opcode == kExprCatch) {
      control_depth--;
    }

    int num_whitespaces = control_depth < 32 ? 2 * control_depth : 64;

    // 64 whitespaces
    const char* padding =
        "                                                                ";
    os.write(padding, num_whitespaces);

    os << RawOpcodeName(opcode) << ",";

    if (opcode == kExprLoop || opcode == kExprIf || opcode == kExprBlock ||
        opcode == kExprTry) {
      DCHECK_EQ(2, length);

      switch (i.pc()[1]) {
#define CASE_LOCAL_TYPE(local_name, type_name) \
  case kLocal##local_name:                     \
    os << " kWasm" #type_name ",";             \
    break;

        CASE_LOCAL_TYPE(I32, I32)
        CASE_LOCAL_TYPE(I64, I64)
        CASE_LOCAL_TYPE(F32, F32)
        CASE_LOCAL_TYPE(F64, F64)
        CASE_LOCAL_TYPE(S128, S128)
        CASE_LOCAL_TYPE(Void, Stmt)
        default:
          os << " 0x" << AsHex(i.pc()[1], 2) << ",";
          break;
      }
#undef CASE_LOCAL_TYPE
    } else {
      for (unsigned j = 1; j < length; ++j) {
        os << " 0x" << AsHex(i.pc()[j], 2) << ",";
      }
    }

    switch (opcode) {
      case kExprElse:
      case kExprCatch:
        os << "   // @" << i.pc_offset();
        control_depth++;
        break;
      case kExprLoop:
      case kExprIf:
      case kExprBlock:
      case kExprTry: {
        BlockTypeImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures, &i,
                                                     i.pc());
        os << "   // @" << i.pc_offset();
        if (decoder.Complete(imm)) {
          for (uint32_t i = 0; i < imm.out_arity(); i++) {
            os << " " << ValueTypes::TypeName(imm.out_type(i));
          }
        }
        control_depth++;
        break;
      }
      case kExprEnd:
        os << "   // @" << i.pc_offset();
        control_depth--;
        break;
      case kExprBr: {
        BranchDepthImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << "   // depth=" << imm.depth;
        break;
      }
      case kExprBrIf: {
        BranchDepthImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << "   // depth=" << imm.depth;
        break;
      }
      case kExprBrTable: {
        BranchTableImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << " // entries=" << imm.table_count;
        break;
      }
      case kExprCallIndirect: {
        CallIndirectImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures, &i,
                                                        i.pc());
        os << "   // sig #" << imm.sig_index;
        if (decoder.Complete(i.pc(), imm)) {
          os << ": " << *imm.sig;
        }
        break;
      }
      case kExprCallFunction: {
        CallFunctionImmediate<Decoder::kNoValidate> imm(&i, i.pc());
        os << " // function #" << imm.index;
        if (decoder.Complete(i.pc(), imm)) {
          os << ": " << *imm.sig;
        }
        break;
      }
      default:
        break;
    }
    os << std::endl;
    ++line_nr;
  }
  DCHECK(!line_numbers || line_numbers->size() == static_cast<size_t>(line_nr));

  return decoder.ok();
}

BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone, size_t num_locals,
                                           const byte* start, const byte* end) {
  Decoder decoder(start, end);
  return WasmDecoder<Decoder::kValidate>::AnalyzeLoopAssignment(
      &decoder, start, static_cast<uint32_t>(num_locals), zone);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
