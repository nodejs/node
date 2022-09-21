// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-disassembler.h"

#include "src/debug/debug-interface.h"
#include "src/numbers/conversions.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/names-provider.h"
#include "src/wasm/wasm-disassembler-impl.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

////////////////////////////////////////////////////////////////////////////////
// Public interface.

void Disassemble(const WasmModule* module, ModuleWireBytes wire_bytes,
                 NamesProvider* names,
                 v8::debug::DisassemblyCollector* collector,
                 std::vector<int>* function_body_offsets) {
  MultiLineStringBuilder out;
  AccountingAllocator allocator;
  ModuleDisassembler md(out, module, names, wire_bytes, &allocator,
                        function_body_offsets);
  md.PrintModule({0, 2});
  out.ToDisassemblyCollector(collector);
}

void MultiLineStringBuilder::ToDisassemblyCollector(
    v8::debug::DisassemblyCollector* collector) {
  if (length() != 0) NextLine(0);  // Finalize last line.
  collector->ReserveLineCount(lines_.size());
  for (const Line& l : lines_) {
    // Don't include trailing '\n'.
    collector->AddLine(l.data, l.len - 1, l.bytecode_offset);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Helpers.

static constexpr char kHexChars[] = "0123456789abcdef";
static constexpr char kUpperHexChars[] = "0123456789ABCDEF";

// Returns the log2 of the alignment, e.g. "4" means 2<<4 == 16 bytes.
// This is the same format as used in .wasm binary modules.
uint32_t GetDefaultAlignment(WasmOpcode opcode) {
  switch (opcode) {
    case kExprS128LoadMem:
    case kExprS128StoreMem:
      return 4;
    case kExprS128Load8x8S:
    case kExprS128Load8x8U:
    case kExprS128Load16x4S:
    case kExprS128Load16x4U:
    case kExprS128Load32x2S:
    case kExprS128Load32x2U:
    case kExprS128Load64Splat:
    case kExprS128Load64Zero:
    case kExprS128Load64Lane:
    case kExprS128Store64Lane:
      return 3;
    case kExprS128Load32Splat:
    case kExprS128Load32Zero:
    case kExprS128Load32Lane:
    case kExprS128Store32Lane:
      return 2;
    case kExprS128Load16Splat:
    case kExprS128Load16Lane:
    case kExprS128Store16Lane:
      return 1;
    case kExprS128Load8Splat:
    case kExprS128Load8Lane:
    case kExprS128Store8Lane:
      return 0;

#define CASE(Opcode, ...) \
  case kExpr##Opcode:     \
    return GetLoadType(kExpr##Opcode).size_log_2();
      FOREACH_LOAD_MEM_OPCODE(CASE)
#undef CASE
#define CASE(Opcode, ...) \
  case kExpr##Opcode:     \
    return GetStoreType(kExpr##Opcode).size_log_2();
      FOREACH_STORE_MEM_OPCODE(CASE)
#undef CASE

#define CASE(Opcode, Type) \
  case kExpr##Opcode:      \
    return ElementSizeLog2Of(MachineType::Type().representation());
      ATOMIC_OP_LIST(CASE)
      ATOMIC_STORE_OP_LIST(CASE)
#undef CASE

    default:
      UNREACHABLE();
  }
}

StringBuilder& operator<<(StringBuilder& sb, uint64_t n) {
  if (n == 0) {
    *sb.allocate(1) = '0';
    return sb;
  }
  static constexpr size_t kBufferSize = 20;  // Just enough for a uint64.
  char buffer[kBufferSize];
  char* end = buffer + kBufferSize;
  char* out = end;
  while (n != 0) {
    *(--out) = '0' + (n % 10);
    n /= 10;
  }
  sb.write(out, static_cast<size_t>(end - out));
  return sb;
}

void PrintSignatureOneLine(StringBuilder& out, const FunctionSig* sig,
                           uint32_t func_index, NamesProvider* names,
                           bool param_names,
                           IndexAsComment indices_as_comments) {
  if (param_names) {
    for (uint32_t i = 0; i < sig->parameter_count(); i++) {
      out << " (param ";
      names->PrintLocalName(out, func_index, i, indices_as_comments);
      out << ' ';
      names->PrintValueType(out, sig->GetParam(i));
      out << ")";
    }
  } else if (sig->parameter_count() > 0) {
    out << " (param";
    for (uint32_t i = 0; i < sig->parameter_count(); i++) {
      out << " ";
      names->PrintValueType(out, sig->GetParam(i));
    }
    out << ")";
  }
  for (size_t i = 0; i < sig->return_count(); i++) {
    out << " (result ";
    names->PrintValueType(out, sig->GetReturn(i));
    out << ")";
  }
}

////////////////////////////////////////////////////////////////////////////////
// FunctionBodyDisassembler.

void FunctionBodyDisassembler::DecodeAsWat(MultiLineStringBuilder& out,
                                           Indentation indentation,
                                           FunctionHeader include_header,
                                           uint32_t* first_instruction_offset) {
  out_ = &out;
  int base_indentation = indentation.current();
  // Print header.
  if (include_header == kPrintHeader) {
    out << indentation << "(func ";
    names_->PrintFunctionName(out, func_index_, NamesProvider::kDevTools);
    PrintSignatureOneLine(out, sig_, func_index_, names_, true,
                          kIndicesAsComments);
    out.NextLine(pc_offset());
  } else {
    out.set_current_line_bytecode_offset(pc_offset());
  }
  indentation.increase();

  // Decode and print locals.
  uint32_t locals_length;
  InitializeLocalsFromSig();
  DecodeLocals(pc_, &locals_length);
  if (failed()) {
    // TODO(jkummerow): Improve error handling.
    out << "Failed to decode locals\n";
    return;
  }
  for (uint32_t i = static_cast<uint32_t>(sig_->parameter_count());
       i < num_locals_; i++) {
    out << indentation << "(local ";
    names_->PrintLocalName(out, func_index_, i);
    out << " ";
    names_->PrintValueType(out, local_type(i));
    out << ")";
    out.NextLine(pc_offset());
  }
  consume_bytes(locals_length);
  out.set_current_line_bytecode_offset(pc_offset());
  if (first_instruction_offset) *first_instruction_offset = pc_offset();

  // Main loop.
  while (pc_ < end_) {
    WasmOpcode opcode = GetOpcode();
    current_opcode_ = opcode;  // Some immediates need to know this.

    // Deal with indentation.
    if (opcode == kExprEnd || opcode == kExprElse || opcode == kExprCatch ||
        opcode == kExprCatchAll || opcode == kExprDelegate) {
      indentation.decrease();
    }
    out << indentation;
    if (opcode == kExprElse || opcode == kExprCatch ||
        opcode == kExprCatchAll || opcode == kExprBlock || opcode == kExprIf ||
        opcode == kExprLoop || opcode == kExprTry) {
      indentation.increase();
    }

    // Print the opcode and its immediates.
    if (opcode == kExprEnd) {
      if (indentation.current() == base_indentation) {
        out << ")";  // End of the function.
      } else {
        out << "end";
        const LabelInfo& label = label_stack_.back();
        if (label.start != nullptr) {
          out << " ";
          out.write(label.start, label.length);
        }
        label_stack_.pop_back();
      }
    } else {
      out << WasmOpcodes::OpcodeName(opcode);
    }
    if (opcode == kExprBlock || opcode == kExprIf || opcode == kExprLoop ||
        opcode == kExprTry) {
      label_stack_.emplace_back(out.line_number(), out.length(),
                                label_occurrence_index_++);
    }
    uint32_t length = PrintImmediatesAndGetLength(out);

    pc_ += length;
    out.NextLine(pc_offset());
  }

  if (pc_ != end_) {
    // TODO(jkummerow): Improve error handling.
    out << "Beyond end of code";
  }
}

void FunctionBodyDisassembler::DecodeGlobalInitializer(StringBuilder& out) {
  while (pc_ < end_) {
    WasmOpcode opcode = GetOpcode();
    current_opcode_ = opcode;  // Some immediates need to know this.
    // Don't print the final "end".
    if (opcode == kExprEnd && pc_ + 1 == end_) break;
    uint32_t length;
    out << " (" << WasmOpcodes::OpcodeName(opcode);
    length = PrintImmediatesAndGetLength(out);
    out << ")";
    pc_ += length;
  }
}

WasmOpcode FunctionBodyDisassembler::GetOpcode() {
  WasmOpcode opcode = static_cast<WasmOpcode>(*pc_);
  if (!WasmOpcodes::IsPrefixOpcode(opcode)) return opcode;
  uint32_t opcode_length;
  return read_prefixed_opcode<validate>(pc_, &opcode_length);
}

void FunctionBodyDisassembler::PrintHexNumber(StringBuilder& out,
                                              uint64_t number) {
  constexpr size_t kBufferSize = sizeof(number) * 2 + 2;  // +2 for "0x".
  char buffer[kBufferSize];
  char* end = buffer + kBufferSize;
  char* ptr = end;
  do {
    *(--ptr) = kHexChars[number & 0xF];
    number >>= 4;
  } while (number > 0);
  *(--ptr) = 'x';
  *(--ptr) = '0';
  size_t length = static_cast<size_t>(end - ptr);
  char* output = out.allocate(length);
  memcpy(output, ptr, length);
}

////////////////////////////////////////////////////////////////////////////////
// ImmediatesPrinter.

template <Decoder::ValidateFlag validate>
class ImmediatesPrinter {
 public:
  ImmediatesPrinter(StringBuilder& out, FunctionBodyDisassembler* owner)
      : out_(out), owner_(owner) {}

  void PrintDepthAsLabel(int imm_depth) {
    out_ << " ";
    size_t label_start_position = out_.length();
    int depth = imm_depth;
    if (owner_->current_opcode_ == kExprDelegate) depth++;
    // Be robust: if the module is invalid, print what we got.
    if (depth >= static_cast<int>(owner_->label_stack_.size())) {
      out_ << imm_depth;
      return;
    }
    // If the label's name has already been determined and backpatched, just
    // copy it here.
    LabelInfo& label_info = owner_->label_info(depth);
    if (label_info.start != nullptr) {
      out_.write(label_info.start, label_info.length);
      return;
    }
    // Determine the label's name and backpatch the line that opened the block.
    names()->PrintLabelName(out_, owner_->func_index_,
                            label_info.name_section_index,
                            owner_->label_generation_index_++);
    label_info.length = out_.length() - label_start_position;
    owner_->out_->PatchLabel(label_info, out_.start() + label_start_position);
  }

  void BlockType(BlockTypeImmediate<validate>& imm) {
    if (imm.type == kWasmBottom) {
      const FunctionSig* sig = owner_->module_->signature(imm.sig_index);
      PrintSignatureOneLine(out_, sig, 0 /* ignored */, names(), false);
    } else if (imm.type == kWasmVoid) {
      // Just be silent.
    } else {
      out_ << " (result ";
      names()->PrintValueType(out_, imm.type);
      out_ << ")";
    }
  }

  void HeapType(HeapTypeImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintHeapType(out_, imm.type);
    if (imm.type.is_index()) use_type(imm.type.ref_index());
  }

  void BranchDepth(BranchDepthImmediate<validate>& imm) {
    PrintDepthAsLabel(imm.depth);
  }

  void BranchTable(BranchTableImmediate<validate>& imm) {
    const byte* pc = imm.table;
    for (uint32_t i = 0; i <= imm.table_count; i++) {
      uint32_t length;
      uint32_t target = owner_->read_u32v<validate>(pc, &length);
      PrintDepthAsLabel(target);
      pc += length;
    }
  }

  void CallIndirect(CallIndirectImmediate<validate>& imm) {
    const FunctionSig* sig = owner_->module_->signature(imm.sig_imm.index);
    PrintSignatureOneLine(out_, sig, 0 /* ignored */, names(), false);
    if (imm.table_imm.index != 0) TableIndex(imm.table_imm);
  }

  void SelectType(SelectTypeImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintValueType(out_, imm.type);
  }

  void MemoryAccess(MemoryAccessImmediate<validate>& imm) {
    if (imm.offset != 0) out_ << " offset=" << imm.offset;
    if (imm.alignment != GetDefaultAlignment(owner_->current_opcode_)) {
      out_ << " align=" << (1u << imm.alignment);
    }
  }

  void SimdLane(SimdLaneImmediate<validate>& imm) {
    out_ << " " << uint32_t{imm.lane};
  }

  void Field(FieldImmediate<validate>& imm) {
    TypeIndex(imm.struct_imm);
    out_ << " ";
    names()->PrintFieldName(out_, imm.struct_imm.index, imm.field_imm.index);
  }

  void Length(IndexImmediate<validate>& imm) {
    out_ << " " << imm.index;  // --
  }

  void TagIndex(TagIndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintTagName(out_, imm.index);
  }

  void FunctionIndex(IndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintFunctionName(out_, imm.index, NamesProvider::kDevTools);
  }

  void TypeIndex(IndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintTypeName(out_, imm.index);
    use_type(imm.index);
  }

  void LocalIndex(IndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintLocalName(out_, func_index(), imm.index);
  }

  void GlobalIndex(IndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintGlobalName(out_, imm.index);
  }

  void TableIndex(IndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintTableName(out_, imm.index);
  }

  void MemoryIndex(MemoryIndexImmediate<validate>& imm) {
    if (imm.index == 0) return;
    out_ << " " << imm.index;
  }

  void DataSegmentIndex(IndexImmediate<validate>& imm) {
    if (kSkipDataSegmentNames) {
      out_ << " " << imm.index;
    } else {
      out_ << " ";
      names()->PrintDataSegmentName(out_, imm.index);
    }
  }

  void ElemSegmentIndex(IndexImmediate<validate>& imm) {
    out_ << " ";
    names()->PrintElementSegmentName(out_, imm.index);
  }

  void I32Const(ImmI32Immediate<validate>& imm) {
    out_ << " " << imm.value;  // --
  }

  void I64Const(ImmI64Immediate<validate>& imm) {
    if (imm.value >= 0) {
      out_ << " " << static_cast<uint64_t>(imm.value);
    } else {
      out_ << " -" << ((~static_cast<uint64_t>(imm.value)) + 1);
    }
  }

  void F32Const(ImmF32Immediate<validate>& imm) {
    float f = imm.value;
    if (f == 0) {
      out_ << (1 / f < 0 ? " -0.0" : " 0.0");
    } else if (std::isnan(f)) {
      uint32_t bits = base::bit_cast<uint32_t>(f);
      uint32_t payload = bits & 0x7F'FFFFu;
      uint32_t signbit = bits >> 31;
      if (payload == 0x40'0000u) {
        out_ << (signbit == 1 ? " -nan" : " nan");
      } else {
        out_ << (signbit == 1 ? " -nan:" : " +nan:");
        owner_->PrintHexNumber(out_, payload);
      }
    } else {
      std::ostringstream o;
      o << std::setprecision(std::numeric_limits<float>::digits10 + 1) << f;
      out_ << " " << o.str();
    }
  }

  void F64Const(ImmF64Immediate<validate>& imm) {
    double d = imm.value;
    if (d == 0) {
      out_ << (1 / d < 0 ? " -0.0" : " 0.0");
    } else if (std::isinf(d)) {
      out_ << (d > 0 ? " inf" : " -inf");
    } else if (std::isnan(d)) {
      uint64_t bits = base::bit_cast<uint64_t>(d);
      uint64_t payload = bits & 0xF'FFFF'FFFF'FFFFull;
      uint64_t signbit = bits >> 63;
      if (payload == 0x8'0000'0000'0000ull) {
        out_ << (signbit == 1 ? " -nan" : " nan");
      } else {
        out_ << (signbit == 1 ? " -nan:" : " +nan:");
        owner_->PrintHexNumber(out_, payload);
      }
    } else {
      char buffer[100];
      const char* str = DoubleToCString(d, base::VectorOf(buffer, 100u));
      out_ << " " << str;
    }
  }

  void S128Const(Simd128Immediate<validate>& imm) {
    if (owner_->current_opcode_ == kExprI8x16Shuffle) {
      for (int i = 0; i < 16; i++) {
        out_ << " " << uint32_t{imm.value[i]};
      }
    } else {
      DCHECK_EQ(owner_->current_opcode_, kExprS128Const);
      out_ << " i32x4";
      for (int i = 0; i < 4; i++) {
        out_ << " 0x";
        for (int j = 3; j >= 0; j--) {  // Little endian.
          uint8_t b = imm.value[i * 4 + j];
          out_ << kUpperHexChars[b >> 4];
          out_ << kUpperHexChars[b & 0xF];
        }
      }
    }
  }

  void StringConst(StringConstImmediate<validate>& imm) {
    // TODO(jkummerow): Print (a prefix of) the string?
    out_ << " " << imm.index;
  }

  void MemoryInit(MemoryInitImmediate<validate>& imm) {
    DataSegmentIndex(imm.data_segment);
    if (imm.memory.index != 0) out_ << " " << uint32_t{imm.memory.index};
  }

  void MemoryCopy(MemoryCopyImmediate<validate>& imm) {
    if (imm.memory_dst.index == 0 && imm.memory_src.index == 0) return;
    out_ << " " << uint32_t{imm.memory_dst.index};
    out_ << " " << uint32_t{imm.memory_src.index};
  }

  void TableInit(TableInitImmediate<validate>& imm) {
    if (imm.table.index != 0) TableIndex(imm.table);
    ElemSegmentIndex(imm.element_segment);
  }

  void TableCopy(TableCopyImmediate<validate>& imm) {
    if (imm.table_dst.index == 0 && imm.table_src.index == 0) return;
    out_ << " ";
    names()->PrintTableName(out_, imm.table_dst.index);
    out_ << " ";
    names()->PrintTableName(out_, imm.table_src.index);
  }

  void ArrayCopy(IndexImmediate<validate>& dst, IndexImmediate<validate>& src) {
    out_ << " ";
    names()->PrintTypeName(out_, dst.index);
    out_ << " ";
    names()->PrintTypeName(out_, src.index);
    use_type(dst.index);
    use_type(src.index);
  }

 private:
  void use_type(uint32_t type_index) { owner_->used_types_.insert(type_index); }

  NamesProvider* names() { return owner_->names_; }

  uint32_t func_index() { return owner_->func_index_; }

  StringBuilder& out_;
  FunctionBodyDisassembler* owner_;
};

uint32_t FunctionBodyDisassembler::PrintImmediatesAndGetLength(
    StringBuilder& out) {
  using Printer = ImmediatesPrinter<validate>;
  Printer imm_printer(out, this);
  return WasmDecoder::OpcodeLength<Printer>(this, this->pc_, &imm_printer);
}

////////////////////////////////////////////////////////////////////////////////
// OffsetsProvider.

class OffsetsProvider {
 public:
  OffsetsProvider() = default;

  void CollectOffsets(const WasmModule* module, const byte* start,
                      const byte* end, AccountingAllocator* allocator) {
    type_offsets_.reserve(module->types.size());
    import_offsets_.reserve(module->import_table.size());
    table_offsets_.reserve(module->tables.size());
    tag_offsets_.reserve(module->tags.size());
    global_offsets_.reserve(module->globals.size());
    element_offsets_.reserve(module->elem_segments.size());
    data_offsets_.reserve(module->data_segments.size());

    using OffsetsCollectingDecoder = ModuleDecoderTemplate<OffsetsProvider>;
    OffsetsCollectingDecoder decoder(WasmFeatures::All(), start, end,
                                     kWasmOrigin, *this);
    constexpr bool verify_functions = false;
    decoder.DecodeModule(nullptr, allocator, verify_functions);

    enabled_ = true;
  }

  void TypeOffset(uint32_t offset) { type_offsets_.push_back(offset); }

  void ImportOffset(uint32_t offset) { import_offsets_.push_back(offset); }

  void TableOffset(uint32_t offset) { table_offsets_.push_back(offset); }

  void MemoryOffset(uint32_t offset) { memory_offset_ = offset; }

  void TagOffset(uint32_t offset) { tag_offsets_.push_back(offset); }

  void GlobalOffset(uint32_t offset) { global_offsets_.push_back(offset); }

  void StartOffset(uint32_t offset) { start_offset_ = offset; }

  void ElementOffset(uint32_t offset) { element_offsets_.push_back(offset); }

  void DataOffset(uint32_t offset) { data_offsets_.push_back(offset); }

  // Unused by this tracer:
  void ImportsDone() {}
  void Bytes(const byte* start, uint32_t count) {}
  void Description(const char* desc) {}
  void Description(const char* desc, size_t length) {}
  void Description(uint32_t number) {}
  void Description(ValueType type) {}
  void Description(HeapType type) {}
  void Description(const FunctionSig* sig) {}
  void NextLine() {}
  void NextLineIfFull() {}
  void NextLineIfNonEmpty() {}
  void InitializerExpression(const byte* start, const byte* end,
                             ValueType expected_type) {}
  void FunctionBody(const WasmFunction* func, const byte* start) {}
  void FunctionName(uint32_t func_index) {}
  void NameSection(const byte* start, const byte* end, uint32_t offset) {}

#define GETTER(name)                       \
  uint32_t name##_offset(uint32_t index) { \
    if (!enabled_) return 0;               \
    return name##_offsets_[index];         \
  }
  GETTER(type)
  GETTER(import)
  GETTER(table)
  GETTER(tag)
  GETTER(global)
  GETTER(element)
  GETTER(data)
#undef GETTER

  uint32_t memory_offset() { return memory_offset_; }

  uint32_t start_offset() { return start_offset_; }

 private:
  bool enabled_{false};
  std::vector<uint32_t> type_offsets_;
  std::vector<uint32_t> import_offsets_;
  std::vector<uint32_t> table_offsets_;
  std::vector<uint32_t> tag_offsets_;
  std::vector<uint32_t> global_offsets_;
  std::vector<uint32_t> element_offsets_;
  std::vector<uint32_t> data_offsets_;
  uint32_t memory_offset_{0};
  uint32_t start_offset_{0};
};

////////////////////////////////////////////////////////////////////////////////
// ModuleDisassembler.

ModuleDisassembler::ModuleDisassembler(MultiLineStringBuilder& out,
                                       const WasmModule* module,
                                       NamesProvider* names,
                                       const ModuleWireBytes wire_bytes,
                                       AccountingAllocator* allocator,
                                       std::vector<int>* function_body_offsets)
    : out_(out),
      module_(module),
      names_(names),
      wire_bytes_(wire_bytes),
      start_(wire_bytes_.start()),
      zone_(allocator, "disassembler zone"),
      offsets_(new OffsetsProvider()),
      function_body_offsets_(function_body_offsets) {
  if (function_body_offsets != nullptr) {
    offsets_->CollectOffsets(module, wire_bytes_.start(), wire_bytes_.end(),
                             allocator);
  }
}

ModuleDisassembler::~ModuleDisassembler() = default;

void ModuleDisassembler::PrintTypeDefinition(uint32_t type_index,
                                             Indentation indentation,
                                             IndexAsComment index_as_comment) {
  uint32_t offset = offsets_->type_offset(type_index);
  out_.NextLine(offset);
  out_ << indentation << "(type ";
  names_->PrintTypeName(out_, type_index, index_as_comment);
  bool has_super = module_->has_supertype(type_index);
  if (module_->has_array(type_index)) {
    const ArrayType* type = module_->array_type(type_index);
    // TODO(jkummerow): "_subtype" is the naming convention used for nominal
    // types; update this for isorecursive hybrid types.
    out_ << (has_super ? " (array_subtype (field " : " (array (field ");
    PrintMutableType(type->mutability(), type->element_type());
    out_ << ")";
    if (has_super) {
      out_ << " ";
      names_->PrintHeapType(out_, HeapType(module_->supertype(type_index)));
    }
    out_ << ")";
  } else if (module_->has_struct(type_index)) {
    const StructType* type = module_->struct_type(type_index);
    out_ << (has_super ? " (struct_subtype" : " (struct");
    bool break_lines = type->field_count() > 2;
    for (uint32_t i = 0; i < type->field_count(); i++) {
      LineBreakOrSpace(break_lines, indentation, offset);
      out_ << "(field ";
      names_->PrintFieldName(out_, type_index, i);
      out_ << " ";
      PrintMutableType(type->mutability(i), type->field(i));
      out_ << ")";
    }
    if (has_super) {
      LineBreakOrSpace(break_lines, indentation, offset);
      names_->PrintHeapType(out_, HeapType(module_->supertype(type_index)));
    }
    out_ << ")";
  } else if (module_->has_signature(type_index)) {
    const FunctionSig* sig = module_->signature(type_index);
    out_ << (has_super ? " (func_subtype" : " (func");
    bool break_lines = sig->parameter_count() + sig->return_count() > 2;
    for (uint32_t i = 0; i < sig->parameter_count(); i++) {
      LineBreakOrSpace(break_lines, indentation, offset);
      out_ << "(param ";
      names_->PrintLocalName(out_, type_index, i);
      out_ << " ";
      names_->PrintValueType(out_, sig->GetParam(i));
      out_ << ")";
    }
    for (uint32_t i = 0; i < sig->return_count(); i++) {
      LineBreakOrSpace(break_lines, indentation, offset);
      out_ << "(result ";
      names_->PrintValueType(out_, sig->GetReturn(i));
      out_ << ")";
    }
    if (has_super) {
      LineBreakOrSpace(break_lines, indentation, offset);
      names_->PrintHeapType(out_, HeapType(module_->supertype(type_index)));
    }
    out_ << ")";
  }
}

void ModuleDisassembler::PrintModule(Indentation indentation) {
  // 0. General infrastructure.
  // We don't store import/export information on {WasmTag} currently.
  size_t num_tags = module_->tags.size();
  std::vector<bool> exported_tags(num_tags, false);
  std::vector<bool> imported_tags(num_tags, false);
  for (const WasmExport& ex : module_->export_table) {
    if (ex.kind == kExternalTag) exported_tags[ex.index] = true;
  }

  // I. Module name.
  out_ << indentation << "(module";
  if (module_->name.is_set()) {
    out_ << " $";
    const byte* name_start = start_ + module_->name.offset();
    out_.write(name_start, module_->name.length());
  }
  indentation.increase();

  // II. Types
  // TODO(jkummerow): If we want to support binary -> WAT -> binary round
  // trips, then we need to print rec groups.
  for (uint32_t i = 0; i < module_->types.size(); i++) {
    if (kSkipFunctionTypesInTypeSection && module_->has_signature(i)) {
      continue;
    }
    PrintTypeDefinition(i, indentation, kIndicesAsComments);
  }

  // III. Imports
  bool memory_imported = false;
  for (uint32_t i = 0; i < module_->import_table.size(); i++) {
    const WasmImport& import = module_->import_table[i];
    out_.NextLine(offsets_->import_offset(i));
    out_ << indentation;
    switch (import.kind) {
      case kExternalTable: {
        out_ << "(table ";
        names_->PrintTableName(out_, import.index, kIndicesAsComments);
        const WasmTable& table = module_->tables[import.index];
        if (table.exported) PrintExportName(kExternalTable, import.index);
        PrintImportName(import);
        PrintTable(table);
        break;
      }
      case kExternalFunction: {
        out_ << "(func ";
        names_->PrintFunctionName(out_, import.index, NamesProvider::kDevTools,
                                  kIndicesAsComments);
        const WasmFunction& func = module_->functions[import.index];
        if (func.exported) PrintExportName(kExternalFunction, import.index);
        PrintImportName(import);
        PrintSignatureOneLine(out_, func.sig, import.index, names_, false);
        break;
      }
      case kExternalGlobal: {
        out_ << "(global ";
        names_->PrintGlobalName(out_, import.index, kIndicesAsComments);
        const WasmGlobal& global = module_->globals[import.index];
        if (global.exported) PrintExportName(kExternalGlobal, import.index);
        PrintImportName(import);
        PrintGlobal(global);
        break;
      }
      case kExternalMemory:
        memory_imported = true;
        out_ << "(memory ";
        names_->PrintMemoryName(out_, import.index, kIndicesAsComments);
        if (module_->mem_export) PrintExportName(kExternalMemory, 0);
        PrintImportName(import);
        PrintMemory();
        break;
      case kExternalTag:
        out_ << "(tag ";
        names_->PrintTagName(out_, import.index, kIndicesAsComments);
        PrintImportName(import);
        if (exported_tags[import.index]) {
          PrintExportName(kExternalTag, import.index);
        }
        PrintTagSignature(module_->tags[import.index].sig);
        imported_tags[import.index] = true;
        break;
    }
    out_ << ")";
  }

  // IV. Tables
  for (uint32_t i = 0; i < module_->tables.size(); i++) {
    const WasmTable& table = module_->tables[i];
    if (table.imported) continue;
    out_.NextLine(offsets_->table_offset(i));
    out_ << indentation << "(table ";
    names_->PrintTableName(out_, i, kIndicesAsComments);
    if (table.exported) PrintExportName(kExternalTable, i);
    PrintTable(table);
    out_ << ")";
  }

  // V. Memories
  static_assert(kV8MaxWasmMemories == 1,
                "Code below needs updating for multi-memory");
  uint32_t num_memories = module_->has_memory ? 1 : 0;
  for (uint32_t i = 0; i < num_memories; i++) {
    if (memory_imported) continue;
    out_.NextLine(offsets_->memory_offset());
    out_ << indentation << "(memory ";
    names_->PrintMemoryName(out_, 0, kIndicesAsComments);
    if (module_->mem_export) PrintExportName(kExternalMemory, 0);
    PrintMemory();
    out_ << ")";
  }

  // VI.Tags
  for (uint32_t i = 0; i < module_->tags.size(); i++) {
    if (imported_tags[i]) continue;
    const WasmTag& tag = module_->tags[i];
    out_.NextLine(offsets_->tag_offset(i));
    out_ << indentation << "(tag ";
    names_->PrintTagName(out_, i, kIndicesAsComments);
    if (exported_tags[i]) PrintExportName(kExternalTag, i);
    PrintTagSignature(tag.sig);
    out_ << ")";
  }

  // VII. String literals
  // TODO(jkummerow/12868): Implement.

  // VIII. Globals
  for (uint32_t i = 0; i < module_->globals.size(); i++) {
    const WasmGlobal& global = module_->globals[i];
    if (global.imported) continue;
    out_.NextLine(offsets_->global_offset(i));
    out_ << indentation << "(global ";
    names_->PrintGlobalName(out_, i, kIndicesAsComments);
    if (global.exported) PrintExportName(kExternalGlobal, i);
    PrintGlobal(global);
    PrintInitExpression(global.init, global.type);
    out_ << ")";
  }

  // IX. Start
  if (module_->start_function_index >= 0) {
    out_.NextLine(offsets_->start_offset());
    out_ << indentation << "(start ";
    names_->PrintFunctionName(out_, module_->start_function_index,
                              NamesProvider::kDevTools);
    out_ << ")";
  }

  // X. Elements
  for (uint32_t i = 0; i < module_->elem_segments.size(); i++) {
    const WasmElemSegment& elem = module_->elem_segments[i];
    out_.NextLine(offsets_->element_offset(i));
    out_ << indentation << "(elem ";
    names_->PrintElementSegmentName(out_, i);
    if (elem.status == WasmElemSegment::kStatusDeclarative) {
      out_ << " declare";
    } else if (elem.status == WasmElemSegment::kStatusActive) {
      if (elem.table_index != 0) {
        out_ << " (table ";
        names_->PrintTableName(out_, elem.table_index);
        out_ << ")";
      }
      PrintInitExpression(elem.offset, kWasmI32);
    }
    out_ << " ";
    names_->PrintValueType(out_, elem.type);
    for (const ConstantExpression& entry : elem.entries) {
      PrintInitExpression(entry, elem.type);
    }
    out_ << ")";
  }

  // For the FunctionBodyDisassembler, we flip the convention: {NextLine} is
  // now called *after* printing something, instead of before.
  if (out_.length() != 0) out_.NextLine(0);

  // XI. Code / function bodies.
  if (function_body_offsets_ != nullptr) {
    size_t num_defined_functions =
        module_->functions.size() - module_->num_imported_functions;
    function_body_offsets_->reserve(num_defined_functions * 2);
  }
  for (uint32_t i = module_->num_imported_functions;
       i < module_->functions.size(); i++) {
    const WasmFunction* func = &module_->functions[i];
    out_.set_current_line_bytecode_offset(func->code.offset());
    out_ << indentation << "(func ";
    names_->PrintFunctionName(out_, i, NamesProvider::kDevTools,
                              kIndicesAsComments);
    if (func->exported) PrintExportName(kExternalFunction, i);
    PrintSignatureOneLine(out_, func->sig, i, names_, true, kIndicesAsComments);
    out_.NextLine(func->code.offset());
    WasmFeatures detected;
    base::Vector<const byte> code = wire_bytes_.GetFunctionBytes(func);
    FunctionBodyDisassembler d(&zone_, module_, i, &detected, func->sig,
                               code.begin(), code.end(), func->code.offset(),
                               names_);
    uint32_t first_instruction_offset;
    d.DecodeAsWat(out_, indentation, FunctionBodyDisassembler::kSkipHeader,
                  &first_instruction_offset);
    if (function_body_offsets_ != nullptr) {
      function_body_offsets_->push_back(first_instruction_offset);
      function_body_offsets_->push_back(d.pc_offset());
    }
  }

  // XII. Data
  for (uint32_t i = 0; i < module_->data_segments.size(); i++) {
    const WasmDataSegment& data = module_->data_segments[i];
    out_.set_current_line_bytecode_offset(offsets_->data_offset(i));
    out_ << indentation << "(data";
    if (!kSkipDataSegmentNames) {
      out_ << " ";
      names_->PrintDataSegmentName(out_, i);
    }
    if (data.active) {
      ValueType type = module_->is_memory64 ? kWasmI64 : kWasmI32;
      PrintInitExpression(data.dest_addr, type);
    }
    out_ << " \"";
    PrintString(data.source);
    out_ << "\")";
    out_.NextLine(0);
  }

  indentation.decrease();
  out_.set_current_line_bytecode_offset(
      static_cast<uint32_t>(wire_bytes_.length()));
  out_ << indentation << ")";  // End of the module.
  out_.NextLine(0);
}

void ModuleDisassembler::PrintImportName(const WasmImport& import) {
  out_ << " (import \"";
  PrintString(import.module_name);
  out_ << "\" \"";
  PrintString(import.field_name);
  out_ << "\")";
}

void ModuleDisassembler::PrintExportName(ImportExportKindCode kind,
                                         uint32_t index) {
  for (const WasmExport& ex : module_->export_table) {
    if (ex.kind != kind || ex.index != index) continue;
    out_ << " (export \"";
    PrintStringAsJSON(ex.name);
    out_ << "\")";
  }
}

void ModuleDisassembler::PrintMutableType(bool mutability, ValueType type) {
  if (mutability) out_ << "(mut ";
  names_->PrintValueType(out_, type);
  if (mutability) out_ << ")";
}

void ModuleDisassembler::PrintTable(const WasmTable& table) {
  out_ << " " << table.initial_size << " ";
  if (table.has_maximum_size) out_ << table.maximum_size << " ";
  names_->PrintValueType(out_, table.type);
}

void ModuleDisassembler::PrintMemory() {
  out_ << " " << module_->initial_pages;
  if (module_->has_maximum_pages) out_ << " " << module_->maximum_pages;
  if (module_->has_shared_memory) out_ << " shared";
}

void ModuleDisassembler::PrintGlobal(const WasmGlobal& global) {
  out_ << " ";
  PrintMutableType(global.mutability, global.type);
}

void ModuleDisassembler::PrintInitExpression(const ConstantExpression& init,
                                             ValueType expected_type) {
  switch (init.kind()) {
    case ConstantExpression::kEmpty:
      break;
    case ConstantExpression::kI32Const:
      out_ << " (i32.const " << init.i32_value() << ")";
      break;
    case ConstantExpression::kRefNull:
      out_ << " (ref.null ";
      names_->PrintHeapType(out_, HeapType(init.repr()));
      out_ << ")";
      break;
    case ConstantExpression::kRefFunc:
      out_ << " (ref.func ";
      names_->PrintFunctionName(out_, init.index(), NamesProvider::kDevTools);
      out_ << ")";
      break;
    case ConstantExpression::kWireBytesRef:
      WireBytesRef ref = init.wire_bytes_ref();
      const byte* start = start_ + ref.offset();
      const byte* end = start_ + ref.end_offset();

      auto sig = FixedSizeSignature<ValueType>::Returns(expected_type);
      WasmFeatures detected;
      FunctionBodyDisassembler d(&zone_, module_, 0, &detected, &sig, start,
                                 end, ref.offset(), names_);
      d.DecodeGlobalInitializer(out_);
      break;
  }
}

void ModuleDisassembler::PrintTagSignature(const FunctionSig* sig) {
  for (uint32_t i = 0; i < sig->parameter_count(); i++) {
    out_ << " (param ";
    names_->PrintValueType(out_, sig->GetParam(i));
    out_ << ")";
  }
}

void ModuleDisassembler::PrintString(WireBytesRef ref) {
  for (const byte* ptr = start_ + ref.offset(); ptr < start_ + ref.end_offset();
       ptr++) {
    byte b = *ptr;
    if (b < 32 || b >= 127 || b == '"' || b == '\\') {
      out_ << '\\' << kHexChars[b >> 4] << kHexChars[b & 0xF];
    } else {
      out_ << static_cast<char>(b);
    }
  }
}

// This mimics legacy wasmparser behavior. It might be a questionable choice,
// but we'll follow suit for now.
void ModuleDisassembler::PrintStringAsJSON(WireBytesRef ref) {
  for (const byte* ptr = start_ + ref.offset(); ptr < start_ + ref.end_offset();
       ptr++) {
    byte b = *ptr;
    if (b <= 34) {
      switch (b) {
        // clang-format off
        case '\b': out_ << "\\b";  break;
        case '\t': out_ << "\\t";  break;
        case '\n': out_ << "\\n";  break;
        case '\f': out_ << "\\f";  break;
        case '\r': out_ << "\\r";  break;
        case ' ':  out_ << ' ';    break;
        case '!':  out_ << '!';    break;
        case '"':  out_ << "\\\""; break;
        // clang-format on
        default:
          out_ << "\\u00" << kHexChars[b >> 4] << kHexChars[b & 0xF];
          break;
      }
    } else if (b != 127 && b != '\\') {
      out_ << static_cast<char>(b);
    } else if (b == '\\') {
      out_ << "\\\\";
    } else {
      out_ << "\\x7F";
    }
  }
}

void ModuleDisassembler::LineBreakOrSpace(bool break_lines,
                                          Indentation indentation,
                                          uint32_t byte_offset) {
  if (break_lines) {
    out_.NextLine(byte_offset);
    out_ << indentation.Extra(2);
  } else {
    out_ << " ";
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
