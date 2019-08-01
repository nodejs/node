// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/diagnostics/disassembler.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/code-comments.h"
#include "src/codegen/code-reference.h"
#include "src/codegen/macro-assembler.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/disasm.h"
#include "src/execution/isolate-data.h"
#include "src/ic/ic.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/serializer-common.h"
#include "src/strings/string-stream.h"
#include "src/utils/vector.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_DISASSEMBLER

class V8NameConverter : public disasm::NameConverter {
 public:
  explicit V8NameConverter(Isolate* isolate, CodeReference code = {})
      : isolate_(isolate), code_(code) {}
  const char* NameOfAddress(byte* pc) const override;
  const char* NameInCode(byte* addr) const override;
  const char* RootRelativeName(int offset) const override;

  const CodeReference& code() const { return code_; }

 private:
  void InitExternalRefsCache() const;

  Isolate* isolate_;
  CodeReference code_;

  EmbeddedVector<char, 128> v8_buffer_;

  // Map from root-register relative offset of the external reference value to
  // the external reference name (stored in the external reference table).
  // This cache is used to recognize [root_reg + offs] patterns as direct
  // access to certain external reference's value.
  mutable std::unordered_map<int, const char*> directly_accessed_external_refs_;
};

void V8NameConverter::InitExternalRefsCache() const {
  ExternalReferenceTable* external_reference_table =
      isolate_->external_reference_table();
  if (!external_reference_table->is_initialized()) return;

  base::AddressRegion addressable_region =
      isolate_->root_register_addressable_region();
  Address isolate_root = isolate_->isolate_root();

  for (uint32_t i = 0; i < ExternalReferenceTable::kSize; i++) {
    Address address = external_reference_table->address(i);
    if (addressable_region.contains(address)) {
      int offset = static_cast<int>(address - isolate_root);
      const char* name = external_reference_table->name(i);
      directly_accessed_external_refs_.insert({offset, name});
    }
  }
}

const char* V8NameConverter::NameOfAddress(byte* pc) const {
  if (!code_.is_null()) {
    const char* name =
        isolate_ ? isolate_->builtins()->Lookup(reinterpret_cast<Address>(pc))
                 : nullptr;

    if (name != nullptr) {
      SNPrintF(v8_buffer_, "%p  (%s)", static_cast<void*>(pc), name);
      return v8_buffer_.begin();
    }

    int offs = static_cast<int>(reinterpret_cast<Address>(pc) -
                                code_.instruction_start());
    // print as code offset, if it seems reasonable
    if (0 <= offs && offs < code_.instruction_size()) {
      SNPrintF(v8_buffer_, "%p  <+0x%x>", static_cast<void*>(pc), offs);
      return v8_buffer_.begin();
    }

    wasm::WasmCodeRefScope wasm_code_ref_scope;
    wasm::WasmCode* wasm_code =
        isolate_ ? isolate_->wasm_engine()->code_manager()->LookupCode(
                       reinterpret_cast<Address>(pc))
                 : nullptr;
    if (wasm_code != nullptr) {
      SNPrintF(v8_buffer_, "%p  (%s)", static_cast<void*>(pc),
               wasm::GetWasmCodeKindAsString(wasm_code->kind()));
      return v8_buffer_.begin();
    }
  }

  return disasm::NameConverter::NameOfAddress(pc);
}

const char* V8NameConverter::NameInCode(byte* addr) const {
  // The V8NameConverter is used for well known code, so we can "safely"
  // dereference pointers in generated code.
  return code_.is_null() ? "" : reinterpret_cast<const char*>(addr);
}

const char* V8NameConverter::RootRelativeName(int offset) const {
  if (isolate_ == nullptr) return nullptr;

  const int kRootsTableStart = IsolateData::roots_table_offset();
  const unsigned kRootsTableSize = sizeof(RootsTable);
  const int kExtRefsTableStart = IsolateData::external_reference_table_offset();
  const unsigned kExtRefsTableSize = ExternalReferenceTable::kSizeInBytes;
  const int kBuiltinsTableStart = IsolateData::builtins_table_offset();
  const unsigned kBuiltinsTableSize =
      Builtins::builtin_count * kSystemPointerSize;

  if (static_cast<unsigned>(offset - kRootsTableStart) < kRootsTableSize) {
    uint32_t offset_in_roots_table = offset - kRootsTableStart;

    // Fail safe in the unlikely case of an arbitrary root-relative offset.
    if (offset_in_roots_table % kSystemPointerSize != 0) return nullptr;

    RootIndex root_index =
        static_cast<RootIndex>(offset_in_roots_table / kSystemPointerSize);

    SNPrintF(v8_buffer_, "root (%s)", RootsTable::name(root_index));
    return v8_buffer_.begin();

  } else if (static_cast<unsigned>(offset - kExtRefsTableStart) <
             kExtRefsTableSize) {
    uint32_t offset_in_extref_table = offset - kExtRefsTableStart;

    // Fail safe in the unlikely case of an arbitrary root-relative offset.
    if (offset_in_extref_table % ExternalReferenceTable::kEntrySize != 0) {
      return nullptr;
    }

    // Likewise if the external reference table is uninitialized.
    if (!isolate_->external_reference_table()->is_initialized()) {
      return nullptr;
    }

    SNPrintF(v8_buffer_, "external reference (%s)",
             isolate_->external_reference_table()->NameFromOffset(
                 offset_in_extref_table));
    return v8_buffer_.begin();

  } else if (static_cast<unsigned>(offset - kBuiltinsTableStart) <
             kBuiltinsTableSize) {
    uint32_t offset_in_builtins_table = (offset - kBuiltinsTableStart);

    Builtins::Name builtin_id = static_cast<Builtins::Name>(
        offset_in_builtins_table / kSystemPointerSize);

    const char* name = Builtins::name(builtin_id);
    SNPrintF(v8_buffer_, "builtin (%s)", name);
    return v8_buffer_.begin();

  } else {
    // It must be a direct access to one of the external values.
    if (directly_accessed_external_refs_.empty()) {
      InitExternalRefsCache();
    }

    auto iter = directly_accessed_external_refs_.find(offset);
    if (iter != directly_accessed_external_refs_.end()) {
      SNPrintF(v8_buffer_, "external value (%s)", iter->second);
      return v8_buffer_.begin();
    }
    return nullptr;
  }
}

static void DumpBuffer(std::ostream* os, StringBuilder* out) {
  (*os) << out->Finalize() << std::endl;
  out->Reset();
}

static const int kOutBufferSize = 2048 + String::kMaxShortPrintLength;
static const int kRelocInfoPosition = 57;

static void PrintRelocInfo(StringBuilder* out, Isolate* isolate,
                           const ExternalReferenceEncoder* ref_encoder,
                           std::ostream* os, CodeReference host,
                           RelocInfo* relocinfo, bool first_reloc_info = true) {
  // Indent the printing of the reloc info.
  if (first_reloc_info) {
    // The first reloc info is printed after the disassembled instruction.
    out->AddPadding(' ', kRelocInfoPosition - out->position());
  } else {
    // Additional reloc infos are printed on separate lines.
    DumpBuffer(os, out);
    out->AddPadding(' ', kRelocInfoPosition);
  }

  RelocInfo::Mode rmode = relocinfo->rmode();
  if (rmode == RelocInfo::DEOPT_SCRIPT_OFFSET) {
    out->AddFormatted("    ;; debug: deopt position, script offset '%d'",
                      static_cast<int>(relocinfo->data()));
  } else if (rmode == RelocInfo::DEOPT_INLINING_ID) {
    out->AddFormatted("    ;; debug: deopt position, inlining id '%d'",
                      static_cast<int>(relocinfo->data()));
  } else if (rmode == RelocInfo::DEOPT_REASON) {
    DeoptimizeReason reason = static_cast<DeoptimizeReason>(relocinfo->data());
    out->AddFormatted("    ;; debug: deopt reason '%s'",
                      DeoptimizeReasonToString(reason));
  } else if (rmode == RelocInfo::DEOPT_ID) {
    out->AddFormatted("    ;; debug: deopt index %d",
                      static_cast<int>(relocinfo->data()));
  } else if (RelocInfo::IsEmbeddedObjectMode(rmode)) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    relocinfo->target_object().ShortPrint(&accumulator);
    std::unique_ptr<char[]> obj_name = accumulator.ToCString();
    const bool is_compressed = RelocInfo::IsCompressedEmbeddedObject(rmode);
    out->AddFormatted("    ;; %sobject: %s",
                      is_compressed ? "(compressed) " : "", obj_name.get());
  } else if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
    const char* reference_name =
        ref_encoder ? ref_encoder->NameOfAddress(
                          isolate, relocinfo->target_external_reference())
                    : "unknown";
    out->AddFormatted("    ;; external reference (%s)", reference_name);
  } else if (RelocInfo::IsCodeTargetMode(rmode)) {
    out->AddFormatted("    ;; code:");
    Code code = isolate->heap()->GcSafeFindCodeForInnerPointer(
        relocinfo->target_address());
    Code::Kind kind = code.kind();
    if (code.is_builtin()) {
      out->AddFormatted(" Builtin::%s", Builtins::name(code.builtin_index()));
    } else {
      out->AddFormatted(" %s", Code::Kind2String(kind));
    }
  } else if (RelocInfo::IsWasmStubCall(rmode) && host.is_wasm_code()) {
    // Host is isolate-independent, try wasm native module instead.
    const char* runtime_stub_name =
        host.as_wasm_code()->native_module()->GetRuntimeStubName(
            relocinfo->wasm_stub_call_address());
    out->AddFormatted("    ;; wasm stub: %s", runtime_stub_name);
  } else if (RelocInfo::IsRuntimeEntry(rmode) && isolate &&
             isolate->deoptimizer_data() != nullptr) {
    // A runtime entry relocinfo might be a deoptimization bailout.
    Address addr = relocinfo->target_address();
    DeoptimizeKind type;
    if (Deoptimizer::IsDeoptimizationEntry(isolate, addr, &type)) {
      out->AddFormatted("    ;; %s deoptimization bailout",
                        Deoptimizer::MessageFor(type));
    } else {
      out->AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
    }
  } else {
    out->AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
  }
}

static int DecodeIt(Isolate* isolate, ExternalReferenceEncoder* ref_encoder,
                    std::ostream* os, CodeReference code,
                    const V8NameConverter& converter, byte* begin, byte* end,
                    Address current_pc) {
  CHECK(!code.is_null());
  v8::internal::EmbeddedVector<char, 128> decode_buffer;
  v8::internal::EmbeddedVector<char, kOutBufferSize> out_buffer;
  StringBuilder out(out_buffer.begin(), out_buffer.length());
  byte* pc = begin;
  disasm::Disassembler d(converter,
                         disasm::Disassembler::kContinueOnUnimplementedOpcode);
  RelocIterator* it = nullptr;
  CodeCommentsIterator cit(code.code_comments(), code.code_comments_size());
  // Relocation exists if we either have no isolate (wasm code),
  // or we have an isolate and it is not an off-heap instruction stream.
  if (!isolate ||
      !InstructionStream::PcIsOffHeap(isolate, bit_cast<Address>(begin))) {
    it = new RelocIterator(code);
  } else {
    // No relocation information when printing code stubs.
  }
  int constants = -1;  // no constants being decoded at the start

  while (pc < end) {
    // First decode instruction so that we know its length.
    byte* prev_pc = pc;
    if (constants > 0) {
      SNPrintF(decode_buffer, "%08x       constant",
               *reinterpret_cast<int32_t*>(pc));
      constants--;
      pc += 4;
    } else {
      int num_const = d.ConstantPoolSizeAt(pc);
      if (num_const >= 0) {
        SNPrintF(decode_buffer,
                 "%08x       constant pool begin (num_const = %d)",
                 *reinterpret_cast<int32_t*>(pc), num_const);
        constants = num_const;
        pc += 4;
      } else if (it != nullptr && !it->done() &&
                 it->rinfo()->pc() == reinterpret_cast<Address>(pc) &&
                 it->rinfo()->rmode() == RelocInfo::INTERNAL_REFERENCE) {
        // raw pointer embedded in code stream, e.g., jump table
        byte* ptr = *reinterpret_cast<byte**>(pc);
        SNPrintF(decode_buffer, "%08" V8PRIxPTR "      jump table entry %4zu",
                 reinterpret_cast<intptr_t>(ptr),
                 static_cast<size_t>(ptr - begin));
        pc += sizeof(ptr);
      } else {
        decode_buffer[0] = '\0';
        pc += d.InstructionDecode(decode_buffer, pc);
      }
    }

    // Collect RelocInfo for this instruction (prev_pc .. pc-1)
    std::vector<const char*> comments;
    std::vector<Address> pcs;
    std::vector<RelocInfo::Mode> rmodes;
    std::vector<intptr_t> datas;
    if (it != nullptr) {
      while (!it->done() && it->rinfo()->pc() < reinterpret_cast<Address>(pc)) {
        // Collect all data.
        pcs.push_back(it->rinfo()->pc());
        rmodes.push_back(it->rinfo()->rmode());
        datas.push_back(it->rinfo()->data());
        it->next();
      }
    }
    while (cit.HasCurrent() &&
           cit.GetPCOffset() < static_cast<Address>(pc - begin)) {
      comments.push_back(cit.GetComment());
      cit.Next();
    }

    // Comments.
    for (size_t i = 0; i < comments.size(); i++) {
      out.AddFormatted("                  %s", comments[i]);
      DumpBuffer(os, &out);
    }

    // Instruction address and instruction offset.
    if (FLAG_log_colour && reinterpret_cast<Address>(prev_pc) == current_pc) {
      // If this is the given "current" pc, make it yellow and bold.
      out.AddFormatted("\033[33;1m");
    }
    out.AddFormatted("%p  %4" V8PRIxPTRDIFF "  ", static_cast<void*>(prev_pc),
                     prev_pc - begin);

    // Instruction.
    out.AddFormatted("%s", decode_buffer.begin());

    // Print all the reloc info for this instruction which are not comments.
    for (size_t i = 0; i < pcs.size(); i++) {
      // Put together the reloc info
      const CodeReference& host = code;
      Address constant_pool =
          host.is_null() ? kNullAddress : host.constant_pool();
      Code code_pointer;
      if (!host.is_null() && host.is_js()) {
        code_pointer = *host.as_js_code();
      }

      RelocInfo relocinfo(pcs[i], rmodes[i], datas[i], code_pointer,
                          constant_pool);

      bool first_reloc_info = (i == 0);
      PrintRelocInfo(&out, isolate, ref_encoder, os, code, &relocinfo,
                     first_reloc_info);
    }

    // If this is a constant pool load and we haven't found any RelocInfo
    // already, check if we can find some RelocInfo for the target address in
    // the constant pool.
    if (pcs.empty() && !code.is_null()) {
      RelocInfo dummy_rinfo(reinterpret_cast<Address>(prev_pc), RelocInfo::NONE,
                            0, Code());
      if (dummy_rinfo.IsInConstantPool()) {
        Address constant_pool_entry_address =
            dummy_rinfo.constant_pool_entry_address();
        RelocIterator reloc_it(code);
        while (!reloc_it.done()) {
          if (reloc_it.rinfo()->IsInConstantPool() &&
              (reloc_it.rinfo()->constant_pool_entry_address() ==
               constant_pool_entry_address)) {
            PrintRelocInfo(&out, isolate, ref_encoder, os, code,
                           reloc_it.rinfo());
            break;
          }
          reloc_it.next();
        }
      }
    }

    if (FLAG_log_colour && reinterpret_cast<Address>(prev_pc) == current_pc) {
      out.AddFormatted("\033[m");
    }

    DumpBuffer(os, &out);
  }

  // Emit comments following the last instruction (if any).
  while (cit.HasCurrent() &&
         cit.GetPCOffset() < static_cast<Address>(pc - begin)) {
    out.AddFormatted("                  %s", cit.GetComment());
    DumpBuffer(os, &out);
    cit.Next();
  }

  delete it;
  return static_cast<int>(pc - begin);
}

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, CodeReference code, Address current_pc) {
  V8NameConverter v8NameConverter(isolate, code);
  if (isolate) {
    // We have an isolate, so support external reference names.
    SealHandleScope shs(isolate);
    DisallowHeapAllocation no_alloc;
    ExternalReferenceEncoder ref_encoder(isolate);
    return DecodeIt(isolate, &ref_encoder, os, code, v8NameConverter, begin,
                    end, current_pc);
  } else {
    // No isolate => isolate-independent code. No external reference names.
    return DecodeIt(nullptr, nullptr, os, code, v8NameConverter, begin, end,
                    current_pc);
  }
}

#else  // ENABLE_DISASSEMBLER

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, CodeReference code, Address current_pc) {
  return 0;
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
