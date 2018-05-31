// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/disassembler.h"

#include <memory>
#include <vector>

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/disasm.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/snapshot/serializer-common.h"
#include "src/string-stream.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_DISASSEMBLER

class V8NameConverter: public disasm::NameConverter {
 public:
  explicit V8NameConverter(Code* code) : code_(code) {}
  virtual const char* NameOfAddress(byte* pc) const;
  virtual const char* NameInCode(byte* addr) const;
  Code* code() const { return code_; }
 private:
  Code* code_;

  EmbeddedVector<char, 128> v8_buffer_;
};


const char* V8NameConverter::NameOfAddress(byte* pc) const {
  if (code_ != nullptr) {
    Isolate* isolate = code_->GetIsolate();
    const char* name = isolate->builtins()->Lookup(pc);

    if (name != nullptr) {
      SNPrintF(v8_buffer_, "%p  (%s)", static_cast<void*>(pc), name);
      return v8_buffer_.start();
    }

    int offs = static_cast<int>(pc - code_->raw_instruction_start());
    // print as code offset, if it seems reasonable
    if (0 <= offs && offs < code_->raw_instruction_size()) {
      SNPrintF(v8_buffer_, "%p  <+0x%x>", static_cast<void*>(pc), offs);
      return v8_buffer_.start();
    }

    wasm::WasmCode* wasm_code =
        isolate->wasm_engine()->code_manager()->LookupCode(pc);
    if (wasm_code != nullptr) {
      SNPrintF(v8_buffer_, "%p  (%s)", static_cast<void*>(pc),
               GetWasmCodeKindAsString(wasm_code->kind()));
      return v8_buffer_.start();
    }
  }

  return disasm::NameConverter::NameOfAddress(pc);
}


const char* V8NameConverter::NameInCode(byte* addr) const {
  // The V8NameConverter is used for well known code, so we can "safely"
  // dereference pointers in generated code.
  return (code_ != nullptr) ? reinterpret_cast<const char*>(addr) : "";
}


static void DumpBuffer(std::ostream* os, StringBuilder* out) {
  (*os) << out->Finalize() << std::endl;
  out->Reset();
}


static const int kOutBufferSize = 2048 + String::kMaxShortPrintLength;
static const int kRelocInfoPosition = 57;

static void PrintRelocInfo(StringBuilder* out, Isolate* isolate,
                           const ExternalReferenceEncoder& ref_encoder,
                           std::ostream* os, RelocInfo* relocinfo,
                           bool first_reloc_info = true) {
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
  } else if (rmode == RelocInfo::EMBEDDED_OBJECT) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    relocinfo->target_object()->ShortPrint(&accumulator);
    std::unique_ptr<char[]> obj_name = accumulator.ToCString();
    out->AddFormatted("    ;; object: %s", obj_name.get());
  } else if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
    const char* reference_name = ref_encoder.NameOfAddress(
        isolate, relocinfo->target_external_reference());
    out->AddFormatted("    ;; external reference (%s)", reference_name);
  } else if (RelocInfo::IsCodeTarget(rmode)) {
    out->AddFormatted("    ;; code:");
    Code* code = Code::GetCodeFromTargetAddress(relocinfo->target_address());
    Code::Kind kind = code->kind();
    if (kind == Code::STUB) {
      // Get the STUB key and extract major and minor key.
      uint32_t key = code->stub_key();
      uint32_t minor_key = CodeStub::MinorKeyFromKey(key);
      CodeStub::Major major_key = CodeStub::GetMajorKey(code);
      DCHECK(major_key == CodeStub::MajorKeyFromKey(key));
      out->AddFormatted(" %s, %s, ", Code::Kind2String(kind),
                        CodeStub::MajorName(major_key));
      out->AddFormatted("minor: %d", minor_key);
    } else if (code->is_builtin()) {
      out->AddFormatted(" Builtin::%s", Builtins::name(code->builtin_index()));
    } else {
      out->AddFormatted(" %s", Code::Kind2String(kind));
    }
  } else if (RelocInfo::IsRuntimeEntry(rmode) &&
             isolate->deoptimizer_data() != nullptr) {
    // A runtime entry reloinfo might be a deoptimization bailout->
    Address addr = relocinfo->target_address();
    int id =
        Deoptimizer::GetDeoptimizationId(isolate, addr, Deoptimizer::EAGER);
    if (id == Deoptimizer::kNotDeoptimizationEntry) {
      id = Deoptimizer::GetDeoptimizationId(isolate, addr, Deoptimizer::LAZY);
      if (id == Deoptimizer::kNotDeoptimizationEntry) {
        id = Deoptimizer::GetDeoptimizationId(isolate, addr, Deoptimizer::SOFT);
        if (id == Deoptimizer::kNotDeoptimizationEntry) {
          out->AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
        } else {
          out->AddFormatted("    ;; soft deoptimization bailout %d", id);
        }
      } else {
        out->AddFormatted("    ;; lazy deoptimization bailout %d", id);
      }
    } else {
      out->AddFormatted("    ;; deoptimization bailout %d", id);
    }
  } else {
    out->AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
  }
}

static int DecodeIt(Isolate* isolate, std::ostream* os,
                    const V8NameConverter& converter, byte* begin, byte* end,
                    void* current_pc) {
  SealHandleScope shs(isolate);
  DisallowHeapAllocation no_alloc;
  ExternalReferenceEncoder ref_encoder(isolate);

  v8::internal::EmbeddedVector<char, 128> decode_buffer;
  v8::internal::EmbeddedVector<char, kOutBufferSize> out_buffer;
  StringBuilder out(out_buffer.start(), out_buffer.length());
  byte* pc = begin;
  disasm::Disassembler d(converter);
  RelocIterator* it = nullptr;
  if (converter.code() != nullptr) {
    it = new RelocIterator(converter.code());
  } else {
    // No relocation information when printing code stubs.
  }
  int constants = -1;  // no constants being decoded at the start

  while (pc < end) {
    // First decode instruction so that we know its length.
    byte* prev_pc = pc;
    if (constants > 0) {
      SNPrintF(decode_buffer,
               "%08x       constant",
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
      } else if (it != nullptr && !it->done() && it->rinfo()->pc() == pc &&
                 it->rinfo()->rmode() == RelocInfo::INTERNAL_REFERENCE) {
        // raw pointer embedded in code stream, e.g., jump table
        byte* ptr = *reinterpret_cast<byte**>(pc);
        SNPrintF(
            decode_buffer, "%08" V8PRIxPTR "      jump table entry %4" PRIuS,
            reinterpret_cast<intptr_t>(ptr), static_cast<size_t>(ptr - begin));
        pc += sizeof(ptr);
      } else {
        decode_buffer[0] = '\0';
        pc += d.InstructionDecode(decode_buffer, pc);
      }
    }

    // Collect RelocInfo for this instruction (prev_pc .. pc-1)
    std::vector<const char*> comments;
    std::vector<byte*> pcs;
    std::vector<RelocInfo::Mode> rmodes;
    std::vector<intptr_t> datas;
    if (it != nullptr) {
      while (!it->done() && it->rinfo()->pc() < pc) {
        if (RelocInfo::IsComment(it->rinfo()->rmode())) {
          // For comments just collect the text.
          comments.push_back(
              reinterpret_cast<const char*>(it->rinfo()->data()));
        } else {
          // For other reloc info collect all data.
          pcs.push_back(it->rinfo()->pc());
          rmodes.push_back(it->rinfo()->rmode());
          datas.push_back(it->rinfo()->data());
        }
        it->next();
      }
    }

    // Comments.
    for (size_t i = 0; i < comments.size(); i++) {
      out.AddFormatted("                  %s", comments[i]);
      DumpBuffer(os, &out);
    }

    // Instruction address and instruction offset.
    if (FLAG_log_colour && prev_pc == current_pc) {
      // If this is the given "current" pc, make it yellow and bold.
      out.AddFormatted("\033[33;1m");
    }
    out.AddFormatted("%p  %4" V8PRIxPTRDIFF "  ", static_cast<void*>(prev_pc),
                     prev_pc - begin);

    // Instruction.
    out.AddFormatted("%s", decode_buffer.start());

    // Print all the reloc info for this instruction which are not comments.
    for (size_t i = 0; i < pcs.size(); i++) {
      // Put together the reloc info
      Code* host = converter.code();
      RelocInfo relocinfo(pcs[i], rmodes[i], datas[i], host);
      relocinfo.set_constant_pool(host ? host->constant_pool() : nullptr);

      bool first_reloc_info = (i == 0);
      PrintRelocInfo(&out, isolate, ref_encoder, os, &relocinfo,
                     first_reloc_info);
    }

    // If this is a constant pool load and we haven't found any RelocInfo
    // already, check if we can find some RelocInfo for the target address in
    // the constant pool.
    if (pcs.empty() && converter.code() != nullptr) {
      RelocInfo dummy_rinfo(prev_pc, RelocInfo::NONE, 0, nullptr);
      if (dummy_rinfo.IsInConstantPool()) {
        byte* constant_pool_entry_address =
            dummy_rinfo.constant_pool_entry_address();
        RelocIterator reloc_it(converter.code());
        while (!reloc_it.done()) {
          if (reloc_it.rinfo()->IsInConstantPool() &&
              (reloc_it.rinfo()->constant_pool_entry_address() ==
               constant_pool_entry_address)) {
            PrintRelocInfo(&out, isolate, ref_encoder, os, reloc_it.rinfo());
            break;
          }
          reloc_it.next();
        }
      }
    }

    if (FLAG_log_colour && prev_pc == current_pc) {
      out.AddFormatted("\033[m");
    }

    DumpBuffer(os, &out);
  }

  // Emit comments following the last instruction (if any).
  if (it != nullptr) {
    for ( ; !it->done(); it->next()) {
      if (RelocInfo::IsComment(it->rinfo()->rmode())) {
        out.AddFormatted("                  %s",
                         reinterpret_cast<const char*>(it->rinfo()->data()));
        DumpBuffer(os, &out);
      }
    }
  }

  delete it;
  return static_cast<int>(pc - begin);
}

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, Code* code, void* current_pc) {
  V8NameConverter v8NameConverter(code);
  return DecodeIt(isolate, os, v8NameConverter, begin, end, current_pc);
}

#else  // ENABLE_DISASSEMBLER

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, Code* code, void* current_pc) {
  return 0;
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
