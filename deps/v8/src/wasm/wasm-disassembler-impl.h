// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_DISASSEMBLER_IMPL_H_
#define V8_WASM_WASM_DISASSEMBLER_IMPL_H_

#include <iomanip>

#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/names-provider.h"
#include "src/wasm/string-builder-multiline.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

template <typename ValidationTag>
class ImmediatesPrinter;

using IndexAsComment = NamesProvider::IndexAsComment;

////////////////////////////////////////////////////////////////////////////////
// Configuration flags for aspects of behavior where we might want to change
// our minds. {true} is the legacy DevTools behavior.
constexpr bool kSkipFunctionTypesInTypeSection = true;
constexpr IndexAsComment kIndicesAsComments = NamesProvider::kIndexAsComment;
constexpr bool kSkipDataSegmentNames = true;

////////////////////////////////////////////////////////////////////////////////
// Helpers.

class Indentation {
 public:
  Indentation(int current, int delta) : current_(current), delta_(delta) {
    DCHECK_GE(current, 0);
    DCHECK_GE(delta, 0);
  }

  Indentation Extra(int extra) { return {current_ + extra, delta_}; }

  void increase() { current_ += delta_; }
  void decrease() {
    DCHECK_GE(current_, delta_);
    current_ -= delta_;
  }
  int current() { return current_; }

 private:
  int current_;
  int delta_;
};

inline StringBuilder& operator<<(StringBuilder& sb, Indentation indentation) {
  char* ptr = sb.allocate(indentation.current());
  memset(ptr, ' ', indentation.current());
  return sb;
}

V8_EXPORT_PRIVATE void PrintSignatureOneLine(
    StringBuilder& out, const FunctionSig* sig, uint32_t func_index,
    NamesProvider* names, bool param_names,
    IndexAsComment indices_as_comments = NamesProvider::kDontPrintIndex);

class OffsetsProvider;

////////////////////////////////////////////////////////////////////////////////
// FunctionBodyDisassembler.

class V8_EXPORT_PRIVATE FunctionBodyDisassembler
    : public WasmDecoder<Decoder::FullValidationTag> {
 public:
  using ValidationTag = Decoder::FullValidationTag;
  enum FunctionHeader : bool { kSkipHeader = false, kPrintHeader = true };

  FunctionBodyDisassembler(Zone* zone, const WasmModule* module,
                           uint32_t func_index, WasmFeatures* detected,
                           const FunctionSig* sig, const uint8_t* start,
                           const uint8_t* end, uint32_t offset,
                           const ModuleWireBytes wire_bytes,
                           NamesProvider* names)
      : WasmDecoder<ValidationTag>(zone, module, WasmFeatures::All(), detected,
                                   sig, start, end, offset),
        func_index_(func_index),
        wire_bytes_(wire_bytes),
        names_(names) {}

  void DecodeAsWat(MultiLineStringBuilder& out, Indentation indentation,
                   FunctionHeader include_header = kPrintHeader,
                   uint32_t* first_instruction_offset = nullptr);

  void DecodeGlobalInitializer(StringBuilder& out);

  std::set<uint32_t>& used_types() { return used_types_; }

 protected:
  WasmOpcode GetOpcode();

  uint32_t PrintImmediatesAndGetLength(StringBuilder& out);

  void PrintHexNumber(StringBuilder& out, uint64_t number);

  LabelInfo& label_info(int depth) {
    return label_stack_[label_stack_.size() - 1 - depth];
  }

  friend class ImmediatesPrinter<ValidationTag>;
  uint32_t func_index_;
  WasmOpcode current_opcode_ = kExprUnreachable;
  const ModuleWireBytes wire_bytes_;
  NamesProvider* names_;
  std::set<uint32_t> used_types_;
  std::vector<LabelInfo> label_stack_;
  MultiLineStringBuilder* out_;
  // Labels use two different indexing systems: for looking them up in the
  // name section, they're indexed by order of occurrence; for generating names
  // like "$label0", the order in which they show up as targets of branch
  // instructions is used for generating consecutive names.
  // (This is legacy wasmparser behavior; we could change it.)
  uint32_t label_occurrence_index_ = 0;
  uint32_t label_generation_index_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
// ModuleDisassembler.

class ModuleDisassembler {
 public:
  V8_EXPORT_PRIVATE ModuleDisassembler(
      MultiLineStringBuilder& out, const WasmModule* module,
      NamesProvider* names, const ModuleWireBytes wire_bytes,
      AccountingAllocator* allocator, bool collect_offsets,
      std::vector<int>* function_body_offsets = nullptr);
  V8_EXPORT_PRIVATE ~ModuleDisassembler();

  V8_EXPORT_PRIVATE void PrintTypeDefinition(uint32_t type_index,
                                             Indentation indendation,
                                             IndexAsComment index_as_comment);
  V8_EXPORT_PRIVATE void PrintModule(Indentation indentation, size_t max_mb);

 private:
  void PrintImportName(const WasmImport& import);
  void PrintExportName(ImportExportKindCode kind, uint32_t index);
  void PrintMutableType(bool mutability, ValueType type);
  void PrintTable(const WasmTable& table);
  void PrintMemory(const WasmMemory& memory);
  void PrintGlobal(const WasmGlobal& global);
  void PrintInitExpression(const ConstantExpression& init,
                           ValueType expected_type);
  void PrintTagSignature(const FunctionSig* sig);
  void PrintString(WireBytesRef ref);
  void PrintStringAsJSON(WireBytesRef ref);
  void LineBreakOrSpace(bool break_lines, Indentation indentation,
                        uint32_t byte_offset);

  MultiLineStringBuilder& out_;
  const WasmModule* module_;
  NamesProvider* names_;
  const ModuleWireBytes wire_bytes_;
  const uint8_t* start_;
  Zone zone_;
  std::unique_ptr<OffsetsProvider> offsets_;
  std::vector<int>* function_body_offsets_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_DISASSEMBLER_IMPL_H_
