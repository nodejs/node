// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/disassembler.h"

#include <memory>

#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/disasm.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/snapshot/serializer-common.h"
#include "src/string-stream.h"

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
  const char* name =
      code_ == NULL ? NULL : code_->GetIsolate()->builtins()->Lookup(pc);

  if (name != NULL) {
    SNPrintF(v8_buffer_, "%s  (%p)", name, static_cast<void*>(pc));
    return v8_buffer_.start();
  }

  if (code_ != NULL) {
    int offs = static_cast<int>(pc - code_->instruction_start());
    // print as code offset, if it seems reasonable
    if (0 <= offs && offs < code_->instruction_size()) {
      SNPrintF(v8_buffer_, "%d  (%p)", offs, static_cast<void*>(pc));
      return v8_buffer_.start();
    }
  }

  return disasm::NameConverter::NameOfAddress(pc);
}


const char* V8NameConverter::NameInCode(byte* addr) const {
  // The V8NameConverter is used for well known code, so we can "safely"
  // dereference pointers in generated code.
  return (code_ != NULL) ? reinterpret_cast<const char*>(addr) : "";
}


static void DumpBuffer(std::ostream* os, StringBuilder* out) {
  (*os) << out->Finalize() << std::endl;
  out->Reset();
}


static const int kOutBufferSize = 2048 + String::kMaxShortPrintLength;
static const int kRelocInfoPosition = 57;

static int DecodeIt(Isolate* isolate, std::ostream* os,
                    const V8NameConverter& converter, byte* begin, byte* end) {
  SealHandleScope shs(isolate);
  DisallowHeapAllocation no_alloc;
  ExternalReferenceEncoder ref_encoder(isolate);

  v8::internal::EmbeddedVector<char, 128> decode_buffer;
  v8::internal::EmbeddedVector<char, kOutBufferSize> out_buffer;
  StringBuilder out(out_buffer.start(), out_buffer.length());
  byte* pc = begin;
  disasm::Disassembler d(converter);
  RelocIterator* it = NULL;
  if (converter.code() != NULL) {
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
      } else if (it != NULL && !it->done() && it->rinfo()->pc() == pc &&
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
    List<const char*> comments(4);
    List<byte*> pcs(1);
    List<RelocInfo::Mode> rmodes(1);
    List<intptr_t> datas(1);
    if (it != NULL) {
      while (!it->done() && it->rinfo()->pc() < pc) {
        if (RelocInfo::IsComment(it->rinfo()->rmode())) {
          // For comments just collect the text.
          comments.Add(reinterpret_cast<const char*>(it->rinfo()->data()));
        } else {
          // For other reloc info collect all data.
          pcs.Add(it->rinfo()->pc());
          rmodes.Add(it->rinfo()->rmode());
          datas.Add(it->rinfo()->data());
        }
        it->next();
      }
    }

    // Comments.
    for (int i = 0; i < comments.length(); i++) {
      out.AddFormatted("                  %s", comments[i]);
      DumpBuffer(os, &out);
    }

    // Instruction address and instruction offset.
    out.AddFormatted("%p  %4" V8PRIdPTRDIFF "  ", static_cast<void*>(prev_pc),
                     prev_pc - begin);

    // Instruction.
    out.AddFormatted("%s", decode_buffer.start());

    // Print all the reloc info for this instruction which are not comments.
    for (int i = 0; i < pcs.length(); i++) {
      // Put together the reloc info
      RelocInfo relocinfo(isolate, pcs[i], rmodes[i], datas[i],
                          converter.code());

      // Indent the printing of the reloc info.
      if (i == 0) {
        // The first reloc info is printed after the disassembled instruction.
        out.AddPadding(' ', kRelocInfoPosition - out.position());
      } else {
        // Additional reloc infos are printed on separate lines.
        DumpBuffer(os, &out);
        out.AddPadding(' ', kRelocInfoPosition);
      }

      RelocInfo::Mode rmode = relocinfo.rmode();
      if (rmode == RelocInfo::DEOPT_POSITION) {
        out.AddFormatted("    ;; debug: deopt position '%d'",
                         static_cast<int>(relocinfo.data()));
      } else if (rmode == RelocInfo::DEOPT_REASON) {
        DeoptimizeReason reason =
            static_cast<DeoptimizeReason>(relocinfo.data());
        out.AddFormatted("    ;; debug: deopt reason '%s'",
                         DeoptimizeReasonToString(reason));
      } else if (rmode == RelocInfo::DEOPT_ID) {
        out.AddFormatted("    ;; debug: deopt index %d",
                         static_cast<int>(relocinfo.data()));
      } else if (rmode == RelocInfo::EMBEDDED_OBJECT) {
        HeapStringAllocator allocator;
        StringStream accumulator(&allocator);
        relocinfo.target_object()->ShortPrint(&accumulator);
        std::unique_ptr<char[]> obj_name = accumulator.ToCString();
        out.AddFormatted("    ;; object: %s", obj_name.get());
      } else if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
        const char* reference_name = ref_encoder.NameOfAddress(
            isolate, relocinfo.target_external_reference());
        out.AddFormatted("    ;; external reference (%s)", reference_name);
      } else if (RelocInfo::IsCodeTarget(rmode)) {
        out.AddFormatted("    ;; code:");
        Code* code = Code::GetCodeFromTargetAddress(relocinfo.target_address());
        Code::Kind kind = code->kind();
        if (code->is_inline_cache_stub()) {
          if (kind == Code::LOAD_GLOBAL_IC &&
              LoadGlobalICState::GetTypeofMode(code->extra_ic_state()) ==
                  INSIDE_TYPEOF) {
            out.AddFormatted(" inside typeof,");
          }
          out.AddFormatted(" %s", Code::Kind2String(kind));
          if (!IC::ICUseVector(kind)) {
            InlineCacheState ic_state = IC::StateFromCode(code);
            out.AddFormatted(" %s", Code::ICState2String(ic_state));
          }
        } else if (kind == Code::STUB || kind == Code::HANDLER) {
          // Get the STUB key and extract major and minor key.
          uint32_t key = code->stub_key();
          uint32_t minor_key = CodeStub::MinorKeyFromKey(key);
          CodeStub::Major major_key = CodeStub::GetMajorKey(code);
          DCHECK(major_key == CodeStub::MajorKeyFromKey(key));
          out.AddFormatted(" %s, %s, ", Code::Kind2String(kind),
                           CodeStub::MajorName(major_key));
          out.AddFormatted("minor: %d", minor_key);
        } else {
          out.AddFormatted(" %s", Code::Kind2String(kind));
        }
        if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
          out.AddFormatted(" (id = %d)", static_cast<int>(relocinfo.data()));
        }
      } else if (RelocInfo::IsRuntimeEntry(rmode) &&
                 isolate->deoptimizer_data() != NULL) {
        // A runtime entry reloinfo might be a deoptimization bailout.
        Address addr = relocinfo.target_address();
        int id = Deoptimizer::GetDeoptimizationId(isolate,
                                                  addr,
                                                  Deoptimizer::EAGER);
        if (id == Deoptimizer::kNotDeoptimizationEntry) {
          id = Deoptimizer::GetDeoptimizationId(isolate,
                                                addr,
                                                Deoptimizer::LAZY);
          if (id == Deoptimizer::kNotDeoptimizationEntry) {
            id = Deoptimizer::GetDeoptimizationId(isolate,
                                                  addr,
                                                  Deoptimizer::SOFT);
            if (id == Deoptimizer::kNotDeoptimizationEntry) {
              out.AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
            } else {
              out.AddFormatted("    ;; soft deoptimization bailout %d", id);
            }
          } else {
            out.AddFormatted("    ;; lazy deoptimization bailout %d", id);
          }
        } else {
          out.AddFormatted("    ;; deoptimization bailout %d", id);
        }
      } else {
        out.AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
      }
    }
    DumpBuffer(os, &out);
  }

  // Emit comments following the last instruction (if any).
  if (it != NULL) {
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
                         byte* end, Code* code) {
  V8NameConverter v8NameConverter(code);
  return DecodeIt(isolate, os, v8NameConverter, begin, end);
}

#else  // ENABLE_DISASSEMBLER

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, Code* code) {
  return 0;
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
