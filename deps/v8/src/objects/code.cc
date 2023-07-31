// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/code.h"

#include <iomanip>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/codegen/reloc-info-inl.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/objects/code-inl.h"

#ifdef ENABLE_DISASSEMBLER
#include "src/diagnostics/disassembler.h"
#include "src/diagnostics/eh-frame.h"
#endif

namespace v8 {
namespace internal {

ByteArray Code::raw_position_table() const {
  return TaggedField<ByteArray, kPositionTableOffset>::load(*this);
}

HeapObject Code::raw_deoptimization_data_or_interpreter_data() const {
  return TaggedField<HeapObject,
                     kDeoptimizationDataOrInterpreterDataOffset>::load(*this);
}

void Code::ClearEmbeddedObjects(Heap* heap) {
  DisallowGarbageCollection no_gc;
  HeapObject undefined = ReadOnlyRoots(heap).undefined_value();
  InstructionStream istream = unchecked_instruction_stream();
  int mode_mask = RelocInfo::EmbeddedObjectModeMask();
  {
    CodePageMemoryModificationScope memory_modification_scope(istream);
    for (RelocIterator it(*this, mode_mask); !it.done(); it.next()) {
      DCHECK(RelocInfo::IsEmbeddedObjectMode(it.rinfo()->rmode()));
      it.rinfo()->set_target_object(istream, undefined, SKIP_WRITE_BARRIER);
    }
  }
  set_embedded_objects_cleared(true);
}

void Code::FlushICache() const {
  FlushInstructionCache(instruction_start(), instruction_size());
}

void Code::CopyFromNoFlush(ByteArray reloc_info, Heap* heap,
                           const CodeDesc& desc) {
  // Copy from compilation artifacts stored in CodeDesc to the target on-heap
  // objects.
  //
  // Note this is quite convoluted for historical reasons. The CodeDesc buffer
  // contains instructions, a part of inline metadata, and the relocation info.
  // Additionally, the unwinding_info is stored in a separate buffer
  // `desc.unwinding_info`. In this method, we copy all these parts into the
  // final on-heap representation.
  //
  // The off-heap representation:
  //
  // CodeDesc.buffer:
  //
  // +-------------------
  // | instructions
  // +-------------------
  // | inline metadata
  // | .. safepoint table
  // | .. handler table
  // | .. constant pool
  // | .. code comments
  // +-------------------
  // | reloc info
  // +-------------------
  //
  // CodeDesc.unwinding_info:  .. the unwinding info.
  //
  // This is transformed into the on-heap representation, where
  // InstructionStream contains all instructions and inline metadata, and a
  // pointer to the relocation info byte array.

  // Copy code and inline metadata.
  static_assert(InstructionStream::kOnHeapBodyIsContiguous);
  CopyBytes(reinterpret_cast<uint8_t*>(instruction_start()), desc.buffer,
            static_cast<size_t>(desc.instr_size));
  CopyBytes(reinterpret_cast<uint8_t*>(instruction_start() + desc.instr_size),
            desc.unwinding_info, static_cast<size_t>(desc.unwinding_info_size));
  DCHECK_EQ(desc.body_size(), desc.instr_size + desc.unwinding_info_size);
  DCHECK_EQ(body_size(), instruction_size() + metadata_size());

  // Copy the relocation info.
  DCHECK_EQ(reloc_info.length(), desc.reloc_size);
  CopyBytes(reloc_info.GetDataStartAddress(), desc.buffer + desc.reloc_offset,
            static_cast<size_t>(desc.reloc_size));

  RelocateFromDesc(heap, desc);
}

void Code::RelocateFromDesc(Heap* heap, const CodeDesc& desc) {
  DisallowGarbageCollection no_gc;
  Assembler* origin = desc.origin;
  InstructionStream istream = instruction_stream();
  const int mode_mask = RelocInfo::PostCodegenRelocationMask();
  for (RelocIterator it(*this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsEmbeddedObjectMode(mode)) {
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(istream, *p, UPDATE_WRITE_BARRIER,
                                    SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTargetMode(mode)) {
      // Rewrite code handles to direct pointers to the first instruction in the
      // code object.
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      DCHECK(p->IsCode());
      InstructionStream target_istream = Code::cast(*p).instruction_stream();
      it.rinfo()->set_target_address(istream,
                                     target_istream.instruction_start(),
                                     UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsNearBuiltinEntry(mode)) {
      // Rewrite builtin IDs to PC-relative offset to the builtin entry point.
      Builtin builtin = it.rinfo()->target_builtin_at(origin);
      Address p =
          heap->isolate()->builtin_entry_table()[Builtins::ToInt(builtin)];
      it.rinfo()->set_target_address(istream, p, UPDATE_WRITE_BARRIER,
                                     SKIP_ICACHE_FLUSH);
      DCHECK_EQ(p, it.rinfo()->target_address());
    } else if (RelocInfo::IsWasmStubCall(mode)) {
#if V8_ENABLE_WEBASSEMBLY
      // Map wasm stub id to builtin.
      uint32_t stub_call_tag = it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, wasm::WasmCode::kRuntimeStubCount);
      Builtin builtin = wasm::RuntimeStubIdToBuiltinName(
          static_cast<wasm::WasmCode::RuntimeStubId>(stub_call_tag));
      // Store the builtin address in relocation info.
      Address entry =
          heap->isolate()->builtin_entry_table()[Builtins::ToInt(builtin)];
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
#else
      UNREACHABLE();
#endif
    } else {
      intptr_t delta =
          instruction_start() - reinterpret_cast<Address>(desc.buffer);
      it.rinfo()->apply(delta);
    }
  }
}

SafepointEntry Code::GetSafepointEntry(Isolate* isolate, Address pc) {
  DCHECK(!is_maglevved());
  SafepointTable table(isolate, pc, *this);
  return table.FindEntry(pc);
}

MaglevSafepointEntry Code::GetMaglevSafepointEntry(Isolate* isolate,
                                                   Address pc) {
  DCHECK(is_maglevved());
  MaglevSafepointTable table(isolate, pc, *this);
  return table.FindEntry(pc);
}

bool Code::IsIsolateIndependent(Isolate* isolate) {
  static constexpr int kModeMask =
      RelocInfo::AllRealModesMask() &
      ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
      ~RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) &
      ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  static_assert(kModeMask ==
                (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
                 RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET) |
                 RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
                 RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
                 RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                 RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                 RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                 RelocInfo::ModeMask(RelocInfo::NEAR_BUILTIN_ENTRY) |
                 RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                 RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL)));

#if defined(V8_TARGET_ARCH_PPC) || defined(V8_TARGET_ARCH_PPC64) || \
    defined(V8_TARGET_ARCH_MIPS64)
  return RelocIterator(*this, kModeMask).done();
#elif defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64) ||  \
    defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_S390) ||     \
    defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_RISCV64) || \
    defined(V8_TARGET_ARCH_LOONG64) || defined(V8_TARGET_ARCH_RISCV32)
  for (RelocIterator it(*this, kModeMask); !it.done(); it.next()) {
    // On these platforms we emit relative builtin-to-builtin
    // jumps for isolate independent builtins in the snapshot. They are later
    // rewritten as pc-relative jumps to the off-heap instruction stream and are
    // thus process-independent. See also: FinalizeEmbeddedCodeTargets.
    if (RelocInfo::IsCodeTargetMode(it.rinfo()->rmode())) {
      Address target_address = it.rinfo()->target_address();
      if (OffHeapInstructionStream::PcIsOffHeap(isolate, target_address))
        continue;

      Code target = Code::FromTargetAddress(target_address);
      if (Builtins::IsIsolateIndependentBuiltin(target)) {
        continue;
      }
    }
    return false;
  }
  return true;
#else
#error Unsupported architecture.
#endif
}

bool Code::Inlines(SharedFunctionInfo sfi) {
  // We can only check for inlining for optimized code.
  DCHECK(is_optimized_code());
  DisallowGarbageCollection no_gc;
  DeoptimizationData const data =
      DeoptimizationData::cast(deoptimization_data());
  if (data.length() == 0) return false;
  if (data.SharedFunctionInfo() == sfi) return true;
  DeoptimizationLiteralArray const literals = data.LiteralArray();
  int const inlined_count = data.InlinedFunctionCount().value();
  for (int i = 0; i < inlined_count; ++i) {
    if (SharedFunctionInfo::cast(literals.get(i)) == sfi) return true;
  }
  return false;
}

#ifdef ENABLE_DISASSEMBLER

namespace {

void DisassembleCodeRange(Isolate* isolate, std::ostream& os, Code code,
                          Address begin, size_t size, Address current_pc,
                          size_t range_limit = 0) {
  Address end = begin + size;
  AllowHandleAllocation allow_handles;
  DisallowGarbageCollection no_gc;
  HandleScope handle_scope(isolate);
  Disassembler::Decode(isolate, os, reinterpret_cast<uint8_t*>(begin),
                       reinterpret_cast<uint8_t*>(end),
                       CodeReference(handle(code, isolate)), current_pc,
                       range_limit);
}

void DisassembleOnlyCode(const char* name, std::ostream& os, Isolate* isolate,
                         Code code, Address current_pc, size_t range_limit) {
  int code_size = code.instruction_size();
  DisassembleCodeRange(isolate, os, code, code.instruction_start(), code_size,
                       current_pc, range_limit);
}

void Disassemble(const char* name, std::ostream& os, Isolate* isolate,
                 Code code, Address current_pc) {
  CodeKind kind = code.kind();
  os << "kind = " << CodeKindToString(kind) << "\n";
  if (name == nullptr && code.is_builtin()) {
    name = Builtins::name(code.builtin_id());
  }
  if ((name != nullptr) && (name[0] != '\0')) {
    os << "name = " << name << "\n";
  }
  if (CodeKindIsOptimizedJSFunction(kind) && kind != CodeKind::BASELINE) {
    os << "stack_slots = " << code.stack_slots() << "\n";
  }
  os << "compiler = "
     << (code.is_turbofanned()        ? "turbofan"
         : code.is_maglevved()        ? "maglev"
         : kind == CodeKind::BASELINE ? "baseline"
                                      : "unknown")
     << "\n";
  os << "address = " << reinterpret_cast<void*>(code.ptr()) << "\n\n";

  {
    int code_size = code.instruction_size();
    os << "Instructions (size = " << code_size << ")\n";
    DisassembleCodeRange(isolate, os, code, code.instruction_start(), code_size,
                         current_pc);

    if (int pool_size = code.constant_pool_size()) {
      DCHECK_EQ(pool_size & kPointerAlignmentMask, 0);
      os << "\nConstant Pool (size = " << pool_size << ")\n";
      base::Vector<char> buf = base::Vector<char>::New(50);
      intptr_t* ptr = reinterpret_cast<intptr_t*>(code.constant_pool());
      for (int i = 0; i < pool_size; i += kSystemPointerSize, ptr++) {
        SNPrintF(buf, "%4d %08" V8PRIxPTR, i, *ptr);
        os << static_cast<const void*>(ptr) << "  " << buf.begin() << "\n";
      }
    }
  }
  os << "\n";

  // TODO(cbruni): add support for baseline code.
  if (kind != CodeKind::BASELINE) {
    {
      SourcePositionTableIterator it(
          code.source_position_table(),
          SourcePositionTableIterator::kJavaScriptOnly);
      if (!it.done()) {
        os << "Source positions:\n pc offset  position\n";
        for (; !it.done(); it.Advance()) {
          os << std::setw(10) << std::hex << it.code_offset() << std::dec
             << std::setw(10) << it.source_position().ScriptOffset()
             << (it.is_statement() ? "  statement" : "") << "\n";
        }
        os << "\n";
      }
    }

    {
      SourcePositionTableIterator it(
          code.source_position_table(),
          SourcePositionTableIterator::kExternalOnly);
      if (!it.done()) {
        os << "External Source positions:\n pc offset  fileid  line\n";
        for (; !it.done(); it.Advance()) {
          DCHECK(it.source_position().IsExternal());
          os << std::setw(10) << std::hex << it.code_offset() << std::dec
             << std::setw(10) << it.source_position().ExternalFileId()
             << std::setw(10) << it.source_position().ExternalLine() << "\n";
        }
        os << "\n";
      }
    }
  }

  if (CodeKindCanDeoptimize(kind)) {
    DeoptimizationData data =
        DeoptimizationData::cast(code.deoptimization_data());
    data.DeoptimizationDataPrint(os);
  }
  os << "\n";

  if (code.uses_safepoint_table()) {
    if (code.is_maglevved()) {
      MaglevSafepointTable table(isolate, current_pc, code);
      table.Print(os);
    } else {
      SafepointTable table(isolate, current_pc, code);
      table.Print(os);
    }
    os << "\n";
  }

  if (code.has_handler_table()) {
    HandlerTable table(code);
    os << "Handler Table (size = " << table.NumberOfReturnEntries() << ")\n";
    if (CodeKindIsOptimizedJSFunction(kind)) {
      table.HandlerTableReturnPrint(os);
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << code.relocation_size() << ")\n";
  if (code.has_instruction_stream()) {
    for (RelocIterator it(code); !it.done(); it.next()) {
      it.rinfo()->Print(isolate, os);
    }
  }
  os << "\n";

  if (code.has_unwinding_info()) {
    os << "UnwindingInfo (size = " << code.unwinding_info_size() << ")\n";
    EhFrameDisassembler eh_frame_disassembler(
        reinterpret_cast<uint8_t*>(code.unwinding_info_start()),
        reinterpret_cast<uint8_t*>(code.unwinding_info_end()));
    eh_frame_disassembler.DisassembleToStream(os);
    os << "\n";
  }
}

}  // namespace

void Code::Disassemble(const char* name, std::ostream& os, Isolate* isolate,
                       Address current_pc) {
  i::Disassemble(name, os, isolate, *this, current_pc);
}

void Code::DisassembleOnlyCode(const char* name, std::ostream& os,
                               Isolate* isolate, Address current_pc,
                               size_t range_limit) {
  i::DisassembleOnlyCode(name, os, isolate, *this, current_pc, range_limit);
}

#endif  // ENABLE_DISASSEMBLER

void Code::SetMarkedForDeoptimization(Isolate* isolate, const char* reason) {
  set_marked_for_deoptimization(true);
  Deoptimizer::TraceMarkForDeoptimization(isolate, *this, reason);
}

}  // namespace internal
}  // namespace v8
