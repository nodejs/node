// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-manager.h"

#include <iomanip>

#include "src/assembler-inl.h"
#include "src/base/adapters.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/disassembler.h"
#include "src/globals.h"
#include "src/log.h"
#include "src/macro-assembler-inl.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/wasm-import-wrapper-cache-inl.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"

#define TRACE_HEAP(...)                                   \
  do {                                                    \
    if (FLAG_trace_wasm_native_heap) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// Binary predicate to perform lookups in {NativeModule::owned_code_} with a
// given address into a code object. Use with {std::upper_bound} for example.
struct WasmCodeUniquePtrComparator {
  bool operator()(Address pc, const std::unique_ptr<WasmCode>& code) const {
    DCHECK_NE(kNullAddress, pc);
    DCHECK_NOT_NULL(code);
    return pc < code->instruction_start();
  }
};

}  // namespace

void DisjointAllocationPool::Merge(base::AddressRegion region) {
  auto dest_it = regions_.begin();
  auto dest_end = regions_.end();

  // Skip over dest regions strictly before {region}.
  while (dest_it != dest_end && dest_it->end() < region.begin()) ++dest_it;

  // After last dest region: insert and done.
  if (dest_it == dest_end) {
    regions_.push_back(region);
    return;
  }

  // Adjacent (from below) to dest: merge and done.
  if (dest_it->begin() == region.end()) {
    base::AddressRegion merged_region{region.begin(),
                                      region.size() + dest_it->size()};
    DCHECK_EQ(merged_region.end(), dest_it->end());
    *dest_it = merged_region;
    return;
  }

  // Before dest: insert and done.
  if (dest_it->begin() > region.end()) {
    regions_.insert(dest_it, region);
    return;
  }

  // Src is adjacent from above. Merge and check whether the merged region is
  // now adjacent to the next region.
  DCHECK_EQ(dest_it->end(), region.begin());
  dest_it->set_size(dest_it->size() + region.size());
  DCHECK_EQ(dest_it->end(), region.end());
  auto next_dest = dest_it;
  ++next_dest;
  if (next_dest != dest_end && dest_it->end() == next_dest->begin()) {
    dest_it->set_size(dest_it->size() + next_dest->size());
    DCHECK_EQ(dest_it->end(), next_dest->end());
    regions_.erase(next_dest);
  }
}

base::AddressRegion DisjointAllocationPool::Allocate(size_t size) {
  for (auto it = regions_.begin(), end = regions_.end(); it != end; ++it) {
    if (size > it->size()) continue;
    base::AddressRegion ret{it->begin(), size};
    if (size == it->size()) {
      regions_.erase(it);
    } else {
      *it = base::AddressRegion{it->begin() + size, it->size() - size};
    }
    return ret;
  }
  return {};
}

Address WasmCode::constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    if (constant_pool_offset_ < code_comments_offset_) {
      return instruction_start() + constant_pool_offset_;
    }
  }
  return kNullAddress;
}

Address WasmCode::code_comments() const {
  if (code_comments_offset_ < unpadded_binary_size_) {
    return instruction_start() + code_comments_offset_;
  }
  return kNullAddress;
}

size_t WasmCode::trap_handler_index() const {
  CHECK(HasTrapHandlerIndex());
  return static_cast<size_t>(trap_handler_index_);
}

void WasmCode::set_trap_handler_index(size_t value) {
  trap_handler_index_ = value;
}

void WasmCode::RegisterTrapHandlerData() {
  DCHECK(!HasTrapHandlerIndex());
  if (kind() != WasmCode::kFunction) return;

  Address base = instruction_start();

  size_t size = instructions().size();
  const int index =
      RegisterHandlerData(base, size, protected_instructions().size(),
                          protected_instructions().start());

  // TODO(eholk): if index is negative, fail.
  CHECK_LE(0, index);
  set_trap_handler_index(static_cast<size_t>(index));
}

bool WasmCode::HasTrapHandlerIndex() const { return trap_handler_index_ >= 0; }

bool WasmCode::ShouldBeLogged(Isolate* isolate) {
  return isolate->logger()->is_listening_to_code_events() ||
         isolate->is_profiling();
}

void WasmCode::LogCode(Isolate* isolate) const {
  DCHECK(ShouldBeLogged(isolate));
  if (IsAnonymous()) return;

  ModuleWireBytes wire_bytes(native_module()->wire_bytes());
  // TODO(herhut): Allow to log code without on-heap round-trip of the name.
  WireBytesRef name_ref =
      native_module()->module()->LookupFunctionName(wire_bytes, index());
  WasmName name_vec = wire_bytes.GetNameOrNull(name_ref);
  if (!name_vec.is_empty()) {
    HandleScope scope(isolate);
    MaybeHandle<String> maybe_name = isolate->factory()->NewStringFromUtf8(
        Vector<const char>::cast(name_vec));
    Handle<String> name;
    if (!maybe_name.ToHandle(&name)) {
      name = isolate->factory()->NewStringFromAsciiChecked("<name too long>");
    }
    int name_length;
    auto cname =
        name->ToCString(AllowNullsFlag::DISALLOW_NULLS,
                        RobustnessFlag::ROBUST_STRING_TRAVERSAL, &name_length);
    PROFILE(isolate,
            CodeCreateEvent(CodeEventListener::FUNCTION_TAG, this,
                            {cname.get(), static_cast<size_t>(name_length)}));
  } else {
    EmbeddedVector<char, 32> generated_name;
    int length = SNPrintF(generated_name, "wasm-function[%d]", index());
    generated_name.Truncate(length);
    PROFILE(isolate, CodeCreateEvent(CodeEventListener::FUNCTION_TAG, this,
                                     generated_name));
  }

  if (!source_positions().is_empty()) {
    LOG_CODE_EVENT(isolate, CodeLinePosInfoRecordEvent(instruction_start(),
                                                       source_positions()));
  }
}

const char* WasmCode::GetRuntimeStubName() const {
  DCHECK_EQ(WasmCode::kRuntimeStub, kind());
#define RETURN_NAME(Name)                                               \
  if (native_module_->runtime_stub_table_[WasmCode::k##Name] == this) { \
    return #Name;                                                       \
  }
#define RETURN_NAME_TRAP(Name) RETURN_NAME(ThrowWasm##Name)
  WASM_RUNTIME_STUB_LIST(RETURN_NAME, RETURN_NAME_TRAP)
#undef RETURN_NAME_TRAP
#undef RETURN_NAME
  return "<unknown>";
}

void WasmCode::Validate() const {
#ifdef DEBUG
  // We expect certain relocation info modes to never appear in {WasmCode}
  // objects or to be restricted to a small set of valid values. Hence the
  // iteration below does not use a mask, but visits all relocation data.
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        Address target = it.rinfo()->wasm_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
        CHECK_EQ(WasmCode::kJumpTable, code->kind());
        CHECK(code->contains(target));
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        Address target = it.rinfo()->wasm_stub_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
        CHECK_EQ(WasmCode::kRuntimeStub, code->kind());
        CHECK_EQ(target, code->instruction_start());
        break;
      }
      case RelocInfo::INTERNAL_REFERENCE:
      case RelocInfo::INTERNAL_REFERENCE_ENCODED: {
        Address target = it.rinfo()->target_internal_reference();
        CHECK(contains(target));
        break;
      }
      case RelocInfo::EXTERNAL_REFERENCE:
      case RelocInfo::CONST_POOL:
      case RelocInfo::VENEER_POOL:
        // These are OK to appear.
        break;
      default:
        FATAL("Unexpected mode: %d", mode);
    }
  }
#endif
}

void WasmCode::MaybePrint(const char* name) const {
  // Determines whether flags want this code to be printed.
  if ((FLAG_print_wasm_code && kind() == kFunction) ||
      (FLAG_print_wasm_stub_code && kind() != kFunction) || FLAG_print_code) {
    Print(name);
  }
}

void WasmCode::Print(const char* name) const {
  StdoutStream os;
  os << "--- WebAssembly code ---\n";
  Disassemble(name, os);
  os << "--- End code ---\n";
}

void WasmCode::Disassemble(const char* name, std::ostream& os,
                           Address current_pc) const {
  if (name) os << "name: " << name << "\n";
  if (!IsAnonymous()) os << "index: " << index() << "\n";
  os << "kind: " << GetWasmCodeKindAsString(kind_) << "\n";
  os << "compiler: " << (is_liftoff() ? "Liftoff" : "TurboFan") << "\n";
  size_t padding = instructions().size() - unpadded_binary_size_;
  os << "Body (size = " << instructions().size() << " = "
     << unpadded_binary_size_ << " + " << padding << " padding)\n";

#ifdef ENABLE_DISASSEMBLER
  size_t instruction_size = unpadded_binary_size_;
  if (constant_pool_offset_ < instruction_size) {
    instruction_size = constant_pool_offset_;
  }
  if (safepoint_table_offset_ && safepoint_table_offset_ < instruction_size) {
    instruction_size = safepoint_table_offset_;
  }
  if (handler_table_offset_ && handler_table_offset_ < instruction_size) {
    instruction_size = handler_table_offset_;
  }
  DCHECK_LT(0, instruction_size);
  os << "Instructions (size = " << instruction_size << ")\n";
  Disassembler::Decode(nullptr, &os, instructions().start(),
                       instructions().start() + instruction_size,
                       CodeReference(this), current_pc);
  os << "\n";

  if (handler_table_offset_ > 0) {
    HandlerTable table(instruction_start(), handler_table_offset_);
    os << "Exception Handler Table (size = " << table.NumberOfReturnEntries()
       << "):\n";
    table.HandlerTableReturnPrint(os);
    os << "\n";
  }

  if (!protected_instructions_.is_empty()) {
    os << "Protected instructions:\n pc offset  land pad\n";
    for (auto& data : protected_instructions()) {
      os << std::setw(10) << std::hex << data.instr_offset << std::setw(10)
         << std::hex << data.landing_offset << "\n";
    }
    os << "\n";
  }

  if (!source_positions().is_empty()) {
    os << "Source positions:\n pc offset  position\n";
    for (SourcePositionTableIterator it(source_positions()); !it.done();
         it.Advance()) {
      os << std::setw(10) << std::hex << it.code_offset() << std::dec
         << std::setw(10) << it.source_position().ScriptOffset()
         << (it.is_statement() ? "  statement" : "") << "\n";
    }
    os << "\n";
  }

  if (safepoint_table_offset_ > 0) {
    SafepointTable table(instruction_start(), safepoint_table_offset_,
                         stack_slots_);
    os << "Safepoints (size = " << table.size() << ")\n";
    for (uint32_t i = 0; i < table.length(); i++) {
      uintptr_t pc_offset = table.GetPcOffset(i);
      os << reinterpret_cast<const void*>(instruction_start() + pc_offset);
      os << std::setw(6) << std::hex << pc_offset << "  " << std::dec;
      table.PrintEntry(i, os);
      os << " (sp -> fp)";
      SafepointEntry entry = table.GetEntry(i);
      if (entry.trampoline_pc() != -1) {
        os << " trampoline: " << std::hex << entry.trampoline_pc() << std::dec;
      }
      if (entry.has_deoptimization_index()) {
        os << " deopt: " << std::setw(6) << entry.deoptimization_index();
      }
      os << "\n";
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << reloc_info_.size() << ")\n";
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    it.rinfo()->Print(nullptr, os);
  }
  os << "\n";

  if (code_comments_offset() < unpadded_binary_size_) {
    Address code_comments = reinterpret_cast<Address>(instructions().start() +
                                                      code_comments_offset());
    PrintCodeCommentsSection(os, code_comments);
  }
#endif  // ENABLE_DISASSEMBLER
}

const char* GetWasmCodeKindAsString(WasmCode::Kind kind) {
  switch (kind) {
    case WasmCode::kFunction:
      return "wasm function";
    case WasmCode::kWasmToJsWrapper:
      return "wasm-to-js";
    case WasmCode::kLazyStub:
      return "lazy-compile";
    case WasmCode::kRuntimeStub:
      return "runtime-stub";
    case WasmCode::kInterpreterEntry:
      return "interpreter entry";
    case WasmCode::kJumpTable:
      return "jump table";
  }
  return "unknown kind";
}

WasmCode::~WasmCode() {
  if (HasTrapHandlerIndex()) {
    CHECK_LT(trap_handler_index(),
             static_cast<size_t>(std::numeric_limits<int>::max()));
    trap_handler::ReleaseHandlerData(static_cast<int>(trap_handler_index()));
  }
}

NativeModule::NativeModule(Isolate* isolate, const WasmFeatures& enabled,
                           bool can_request_more, VirtualMemory code_space,
                           WasmCodeManager* code_manager,
                           std::shared_ptr<const WasmModule> module)
    : enabled_features_(enabled),
      module_(std::move(module)),
      compilation_state_(CompilationState::New(isolate, this)),
      import_wrapper_cache_(std::unique_ptr<WasmImportWrapperCache>(
          new WasmImportWrapperCache(this))),
      free_code_space_(code_space.region()),
      code_manager_(code_manager),
      can_request_more_memory_(can_request_more),
      use_trap_handler_(trap_handler::IsTrapHandlerEnabled() ? kUseTrapHandler
                                                             : kNoTrapHandler) {
  DCHECK_NOT_NULL(module_);
  owned_code_space_.emplace_back(std::move(code_space));
  owned_code_.reserve(num_functions());

  uint32_t num_wasm_functions = module_->num_declared_functions;
  if (num_wasm_functions > 0) {
    code_table_.reset(new WasmCode*[num_wasm_functions]);
    memset(code_table_.get(), 0, num_wasm_functions * sizeof(WasmCode*));

    jump_table_ = CreateEmptyJumpTable(num_wasm_functions);
  }
}

void NativeModule::ReserveCodeTableForTesting(uint32_t max_functions) {
  DCHECK_LE(num_functions(), max_functions);
  WasmCode** new_table = new WasmCode*[max_functions];
  memset(new_table, 0, max_functions * sizeof(*new_table));
  memcpy(new_table, code_table_.get(),
         module_->num_declared_functions * sizeof(*new_table));
  code_table_.reset(new_table);

  // Re-allocate jump table.
  jump_table_ = CreateEmptyJumpTable(max_functions);
}

void NativeModule::LogWasmCodes(Isolate* isolate) {
  if (!WasmCode::ShouldBeLogged(isolate)) return;

  // TODO(titzer): we skip the logging of the import wrappers
  // here, but they should be included somehow.
  for (WasmCode* code : code_table()) {
    if (code != nullptr) code->LogCode(isolate);
  }
}

CompilationEnv NativeModule::CreateCompilationEnv() const {
  return {module(), use_trap_handler_, kRuntimeExceptionSupport,
          enabled_features_};
}

WasmCode* NativeModule::AddOwnedCode(
    uint32_t index, Vector<const byte> instructions, uint32_t stack_slots,
    size_t safepoint_table_offset, size_t handler_table_offset,
    size_t constant_pool_offset, size_t code_comments_offset,
    size_t unpadded_binary_size,
    OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions,
    OwnedVector<const byte> reloc_info,
    OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
    WasmCode::Tier tier) {
  WasmCode* code;
  {
    // Both allocation and insertion in owned_code_ happen in the same critical
    // section, thus ensuring owned_code_'s elements are rarely if ever moved.
    base::MutexGuard lock(&allocation_mutex_);
    Vector<byte> executable_buffer = AllocateForCode(instructions.size());
    // Ownership will be transferred to {owned_code_} below.
    code = new WasmCode(
        this, index, executable_buffer, stack_slots, safepoint_table_offset,
        handler_table_offset, constant_pool_offset, code_comments_offset,
        unpadded_binary_size, std::move(protected_instructions),
        std::move(reloc_info), std::move(source_position_table), kind, tier);

    if (owned_code_.empty() ||
        code->instruction_start() > owned_code_.back()->instruction_start()) {
      // Common case.
      owned_code_.emplace_back(code);
    } else {
      // Slow but unlikely case.
      // TODO(mtrofin): We allocate in increasing address order, and
      // even if we end up with segmented memory, we may end up only with a few
      // large moves - if, for example, a new segment is below the current ones.
      auto insert_before = std::upper_bound(
          owned_code_.begin(), owned_code_.end(), code->instruction_start(),
          WasmCodeUniquePtrComparator{});
      owned_code_.emplace(insert_before, code);
    }
  }
  memcpy(reinterpret_cast<void*>(code->instruction_start()),
         instructions.start(), instructions.size());

  return code;
}

WasmCode* NativeModule::AddCodeForTesting(Handle<Code> code) {
  WasmCode* ret = AddAnonymousCode(code, WasmCode::kFunction);
  return ret;
}

void NativeModule::SetLazyBuiltin(Handle<Code> code) {
  uint32_t num_wasm_functions = module_->num_declared_functions;
  if (num_wasm_functions == 0) return;
  WasmCode* lazy_builtin = AddAnonymousCode(code, WasmCode::kLazyStub);
  // Fill the jump table with jumps to the lazy compile stub.
  Address lazy_compile_target = lazy_builtin->instruction_start();
  for (uint32_t i = 0; i < num_wasm_functions; ++i) {
    JumpTableAssembler::EmitLazyCompileJumpSlot(
        jump_table_->instruction_start(), i,
        i + module_->num_imported_functions, lazy_compile_target,
        WasmCode::kNoFlushICache);
  }
  Assembler::FlushICache(jump_table_->instructions().start(),
                         jump_table_->instructions().size());
}

void NativeModule::SetRuntimeStubs(Isolate* isolate) {
  HandleScope scope(isolate);
  DCHECK_NULL(runtime_stub_table_[0]);  // Only called once.
#define COPY_BUILTIN(Name)                                                     \
  runtime_stub_table_[WasmCode::k##Name] =                                     \
      AddAnonymousCode(isolate->builtins()->builtin_handle(Builtins::k##Name), \
                       WasmCode::kRuntimeStub, #Name);
#define COPY_BUILTIN_TRAP(Name) COPY_BUILTIN(ThrowWasm##Name)
  WASM_RUNTIME_STUB_LIST(COPY_BUILTIN, COPY_BUILTIN_TRAP)
#undef COPY_BUILTIN_TRAP
#undef COPY_BUILTIN
}

WasmCode* NativeModule::AddAnonymousCode(Handle<Code> code, WasmCode::Kind kind,
                                         const char* name) {
  // For off-heap builtins, we create a copy of the off-heap instruction stream
  // instead of the on-heap code object containing the trampoline. Ensure that
  // we do not apply the on-heap reloc info to the off-heap instructions.
  const size_t relocation_size =
      code->is_off_heap_trampoline() ? 0 : code->relocation_size();
  OwnedVector<byte> reloc_info = OwnedVector<byte>::New(relocation_size);
  memcpy(reloc_info.start(), code->relocation_start(), relocation_size);
  Handle<ByteArray> source_pos_table(code->SourcePositionTable(),
                                     code->GetIsolate());
  OwnedVector<byte> source_pos =
      OwnedVector<byte>::New(source_pos_table->length());
  source_pos_table->copy_out(0, source_pos.start(), source_pos_table->length());
  Vector<const byte> instructions(
      reinterpret_cast<byte*>(code->InstructionStart()),
      static_cast<size_t>(code->InstructionSize()));
  int stack_slots = code->has_safepoint_info() ? code->stack_slots() : 0;
  int safepoint_table_offset =
      code->has_safepoint_info() ? code->safepoint_table_offset() : 0;
  WasmCode* ret =
      AddOwnedCode(WasmCode::kAnonymousFuncIndex,  // index
                   instructions,                   // instructions
                   stack_slots,                    // stack_slots
                   safepoint_table_offset,         // safepoint_table_offset
                   code->handler_table_offset(),   // handler_table_offset
                   code->constant_pool_offset(),   // constant_pool_offset
                   code->code_comments_offset(),   // code_comments_offset
                   instructions.size(),            // unpadded_binary_size
                   {},                             // protected_instructions
                   std::move(reloc_info),          // reloc_info
                   std::move(source_pos),          // source positions
                   kind,                           // kind
                   WasmCode::kOther);              // tier

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = ret->instruction_start() - code->InstructionStart();
  int mode_mask = RelocInfo::kApplyMask |
                  RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  RelocIterator orig_it(*code, mode_mask);
  for (RelocIterator it(ret->instructions(), ret->reloc_info(),
                        ret->constant_pool(), mode_mask);
       !it.done(); it.next(), orig_it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = orig_it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      WasmCode* code =
          runtime_stub(static_cast<WasmCode::RuntimeStubId>(stub_call_tag));
      it.rinfo()->set_wasm_stub_call_address(code->instruction_start(),
                                             SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache here instead of in AddOwnedCode, to include the changes
  // made while iterating over the RelocInfo above.
  Assembler::FlushICache(ret->instructions().start(),
                         ret->instructions().size());
  ret->MaybePrint(name);
  ret->Validate();
  return ret;
}

WasmCode* NativeModule::AddCode(
    uint32_t index, const CodeDesc& desc, uint32_t stack_slots,
    size_t safepoint_table_offset, size_t handler_table_offset,
    OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions,
    OwnedVector<const byte> source_pos_table, WasmCode::Kind kind,
    WasmCode::Tier tier) {
  OwnedVector<byte> reloc_info = OwnedVector<byte>::New(desc.reloc_size);
  memcpy(reloc_info.start(), desc.buffer + desc.buffer_size - desc.reloc_size,
         desc.reloc_size);

  WasmCode* ret = AddOwnedCode(
      index, {desc.buffer, static_cast<size_t>(desc.instr_size)}, stack_slots,
      safepoint_table_offset, handler_table_offset, desc.constant_pool_offset(),
      desc.code_comments_offset(), desc.instr_size,
      std::move(protected_instructions), std::move(reloc_info),
      std::move(source_pos_table), kind, tier);

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = ret->instructions().start() - desc.buffer;
  int mode_mask = RelocInfo::kApplyMask |
                  RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                  RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  for (RelocIterator it(ret->instructions(), ret->reloc_info(),
                        ret->constant_pool(), mode_mask);
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmCall(mode)) {
      uint32_t call_tag = it.rinfo()->wasm_call_tag();
      Address target = GetCallTargetForFunction(call_tag);
      it.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      WasmCode* code =
          runtime_stub(static_cast<WasmCode::RuntimeStubId>(stub_call_tag));
      it.rinfo()->set_wasm_stub_call_address(code->instruction_start(),
                                             SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache here instead of in AddOwnedCode, to include the changes
  // made while iterating over the RelocInfo above.
  Assembler::FlushICache(ret->instructions().start(),
                         ret->instructions().size());
  ret->MaybePrint();
  ret->Validate();
  return ret;
}

WasmCode* NativeModule::AddDeserializedCode(
    uint32_t index, Vector<const byte> instructions, uint32_t stack_slots,
    size_t safepoint_table_offset, size_t handler_table_offset,
    size_t constant_pool_offset, size_t code_comments_offset,
    size_t unpadded_binary_size,
    OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions,
    OwnedVector<const byte> reloc_info,
    OwnedVector<const byte> source_position_table, WasmCode::Tier tier) {
  WasmCode* code =
      AddOwnedCode(index, instructions, stack_slots, safepoint_table_offset,
                   handler_table_offset, constant_pool_offset,
                   code_comments_offset, unpadded_binary_size,
                   std::move(protected_instructions), std::move(reloc_info),
                   std::move(source_position_table), WasmCode::kFunction, tier);

  if (!code->protected_instructions_.is_empty()) {
    code->RegisterTrapHandlerData();
  }
  base::MutexGuard lock(&allocation_mutex_);
  InstallCode(code);
  // Note: we do not flush the i-cache here, since the code needs to be
  // relocated anyway. The caller is responsible for flushing the i-cache later.
  return code;
}

void NativeModule::PublishCode(WasmCode* code) {
  base::MutexGuard lock(&allocation_mutex_);
  // Skip publishing code if there is an active redirection to the interpreter
  // for the given function index, in order to preserve the redirection.
  if (has_interpreter_redirection(code->index())) return;

  if (!code->protected_instructions_.is_empty()) {
    code->RegisterTrapHandlerData();
  }
  InstallCode(code);
}

void NativeModule::PublishInterpreterEntry(WasmCode* code,
                                           uint32_t func_index) {
  code->index_ = func_index;
  base::MutexGuard lock(&allocation_mutex_);
  InstallCode(code);
  SetInterpreterRedirection(func_index);
}

std::vector<WasmCode*> NativeModule::SnapshotCodeTable() const {
  base::MutexGuard lock(&allocation_mutex_);
  std::vector<WasmCode*> result;
  result.reserve(code_table().size());
  for (WasmCode* code : code_table()) result.push_back(code);
  return result;
}

WasmCode* NativeModule::CreateEmptyJumpTable(uint32_t num_wasm_functions) {
  // Only call this if we really need a jump table.
  DCHECK_LT(0, num_wasm_functions);
  OwnedVector<byte> instructions = OwnedVector<byte>::New(
      JumpTableAssembler::SizeForNumberOfSlots(num_wasm_functions));
  memset(instructions.start(), 0, instructions.size());
  return AddOwnedCode(WasmCode::kAnonymousFuncIndex,  // index
                      instructions.as_vector(),       // instructions
                      0,                              // stack_slots
                      instructions.size(),            // safepoint_table_offset
                      instructions.size(),            // handler_table_offset
                      instructions.size(),            // constant_pool_offset
                      instructions.size(),            // code_comments_offset
                      instructions.size(),            // unpadded_binary_size
                      {},                             // protected_instructions
                      {},                             // reloc_info
                      {},                             // source_pos
                      WasmCode::kJumpTable,           // kind
                      WasmCode::kOther);              // tier
}

void NativeModule::InstallCode(WasmCode* code) {
  DCHECK_LT(code->index(), num_functions());
  DCHECK_LE(module_->num_imported_functions, code->index());

  // Update code table, except for interpreter entries.
  if (code->kind() != WasmCode::kInterpreterEntry) {
    code_table_[code->index() - module_->num_imported_functions] = code;
  }

  // Patch jump table.
  uint32_t slot_idx = code->index() - module_->num_imported_functions;
  JumpTableAssembler::PatchJumpTableSlot(jump_table_->instruction_start(),
                                         slot_idx, code->instruction_start(),
                                         WasmCode::kFlushICache);
}

Vector<byte> NativeModule::AllocateForCode(size_t size) {
  DCHECK_LT(0, size);
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  // This happens under a lock assumed by the caller.
  size = RoundUp(size, kCodeAlignment);
  base::AddressRegion code_space = free_code_space_.Allocate(size);
  if (code_space.is_empty()) {
    if (!can_request_more_memory_) {
      V8::FatalProcessOutOfMemory(nullptr,
                                  "NativeModule::AllocateForCode reservation");
      UNREACHABLE();
    }

    Address hint = owned_code_space_.empty() ? kNullAddress
                                             : owned_code_space_.back().end();

    VirtualMemory new_mem =
        code_manager_->TryAllocate(size, reinterpret_cast<void*>(hint));
    if (!new_mem.IsReserved()) {
      V8::FatalProcessOutOfMemory(nullptr,
                                  "NativeModule::AllocateForCode reservation");
      UNREACHABLE();
    }
    code_manager_->AssignRanges(new_mem.address(), new_mem.end(), this);

    free_code_space_.Merge(new_mem.region());
    owned_code_space_.emplace_back(std::move(new_mem));
    code_space = free_code_space_.Allocate(size);
    DCHECK(!code_space.is_empty());
  }
  const Address page_size = page_allocator->AllocatePageSize();
  Address commit_start = RoundUp(code_space.begin(), page_size);
  Address commit_end = RoundUp(code_space.end(), page_size);
  // {commit_start} will be either code_space.start or the start of the next
  // page. {commit_end} will be the start of the page after the one in which
  // the allocation ends.
  // We start from an aligned start, and we know we allocated vmem in
  // page multiples.
  // We just need to commit what's not committed. The page in which we
  // start is already committed (or we start at the beginning of a page).
  // The end needs to be committed all through the end of the page.
  if (commit_start < commit_end) {
    committed_code_space_.fetch_add(commit_end - commit_start);
    // Committed code cannot grow bigger than maximum code space size.
    DCHECK_LE(committed_code_space_.load(), kMaxWasmCodeMemory);
#if V8_OS_WIN
    // On Windows, we cannot commit a region that straddles different
    // reservations of virtual memory. Because we bump-allocate, and because, if
    // we need more memory, we append that memory at the end of the
    // owned_code_space_ list, we traverse that list in reverse order to find
    // the reservation(s) that guide how to chunk the region to commit.
    for (auto& vmem : base::Reversed(owned_code_space_)) {
      if (commit_end <= vmem.address() || vmem.end() <= commit_start) continue;
      Address start = std::max(commit_start, vmem.address());
      Address end = std::min(commit_end, vmem.end());
      size_t commit_size = static_cast<size_t>(end - start);
      if (!code_manager_->Commit(start, commit_size)) {
        V8::FatalProcessOutOfMemory(nullptr,
                                    "NativeModule::AllocateForCode commit");
        UNREACHABLE();
      }
      // Opportunistically reduce the commit range. This might terminate the
      // loop early.
      if (commit_start == start) commit_start = end;
      if (commit_end == end) commit_end = start;
      if (commit_start >= commit_end) break;
    }
#else
    if (!code_manager_->Commit(commit_start, commit_end - commit_start)) {
      V8::FatalProcessOutOfMemory(nullptr,
                                  "NativeModule::AllocateForCode commit");
      UNREACHABLE();
    }
#endif
  }
  DCHECK(IsAligned(code_space.begin(), kCodeAlignment));
  allocated_code_space_.Merge(code_space);
  TRACE_HEAP("Code alloc for %p: %" PRIxPTR ",+%zu\n", this, code_space.begin(),
             size);
  return {reinterpret_cast<byte*>(code_space.begin()), code_space.size()};
}

namespace {
class NativeModuleWireBytesStorage final : public WireBytesStorage {
 public:
  explicit NativeModuleWireBytesStorage(
      std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes)
      : wire_bytes_(std::move(wire_bytes)) {}

  Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
    return wire_bytes_->as_vector().SubVector(ref.offset(), ref.end_offset());
  }

 private:
  const std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes_;
};
}  // namespace

void NativeModule::SetWireBytes(OwnedVector<const uint8_t> wire_bytes) {
  auto shared_wire_bytes =
      std::make_shared<OwnedVector<const uint8_t>>(std::move(wire_bytes));
  wire_bytes_ = shared_wire_bytes;
  if (!shared_wire_bytes->is_empty()) {
    compilation_state_->SetWireBytesStorage(
        std::make_shared<NativeModuleWireBytesStorage>(
            std::move(shared_wire_bytes)));
  }
}

WasmCode* NativeModule::Lookup(Address pc) const {
  base::MutexGuard lock(&allocation_mutex_);
  if (owned_code_.empty()) return nullptr;
  auto iter = std::upper_bound(owned_code_.begin(), owned_code_.end(), pc,
                               WasmCodeUniquePtrComparator());
  if (iter == owned_code_.begin()) return nullptr;
  --iter;
  WasmCode* candidate = iter->get();
  DCHECK_NOT_NULL(candidate);
  return candidate->contains(pc) ? candidate : nullptr;
}

Address NativeModule::GetCallTargetForFunction(uint32_t func_index) const {
  // TODO(clemensh): Measure performance win of returning instruction start
  // directly if we have turbofan code. Downside: Redirecting functions (e.g.
  // for debugging) gets much harder.

  // Return the jump table slot for that function index.
  DCHECK_NOT_NULL(jump_table_);
  uint32_t slot_idx = func_index - module_->num_imported_functions;
  uint32_t slot_offset = JumpTableAssembler::SlotIndexToOffset(slot_idx);
  DCHECK_LT(slot_offset, jump_table_->instructions().size());
  return jump_table_->instruction_start() + slot_offset;
}

uint32_t NativeModule::GetFunctionIndexFromJumpTableSlot(
    Address slot_address) const {
  DCHECK(is_jump_table_slot(slot_address));
  uint32_t slot_offset =
      static_cast<uint32_t>(slot_address - jump_table_->instruction_start());
  uint32_t slot_idx = JumpTableAssembler::SlotOffsetToIndex(slot_offset);
  DCHECK_LT(slot_idx, module_->num_declared_functions);
  return module_->num_imported_functions + slot_idx;
}

void NativeModule::DisableTrapHandler() {
  // Switch {use_trap_handler_} from true to false.
  DCHECK(use_trap_handler_);
  use_trap_handler_ = kNoTrapHandler;

  // Clear the code table (just to increase the chances to hit an error if we
  // forget to re-add all code).
  uint32_t num_wasm_functions = module_->num_declared_functions;
  memset(code_table_.get(), 0, num_wasm_functions * sizeof(WasmCode*));

  // TODO(clemensh): Actually free the owned code, such that the memory can be
  // recycled.
}

NativeModule::~NativeModule() {
  TRACE_HEAP("Deleting native module: %p\n", reinterpret_cast<void*>(this));
  // Cancel all background compilation before resetting any field of the
  // NativeModule or freeing anything.
  compilation_state_->CancelAndWait();
  code_manager_->FreeNativeModule(this);
}

WasmCodeManager::WasmCodeManager(WasmMemoryTracker* memory_tracker,
                                 size_t max_committed)
    : memory_tracker_(memory_tracker),
      remaining_uncommitted_code_space_(max_committed),
      critical_uncommitted_code_space_(max_committed / 2) {
  DCHECK_LE(max_committed, kMaxWasmCodeMemory);
}

bool WasmCodeManager::Commit(Address start, size_t size) {
  // TODO(v8:8462) Remove eager commit once perf supports remapping.
  if (FLAG_perf_prof) return true;
  DCHECK(IsAligned(start, AllocatePageSize()));
  DCHECK(IsAligned(size, AllocatePageSize()));
  // Reserve the size. Use CAS loop to avoid underflow on
  // {remaining_uncommitted_}. Temporary underflow would allow concurrent
  // threads to over-commit.
  size_t old_value = remaining_uncommitted_code_space_.load();
  while (true) {
    if (old_value < size) return false;
    if (remaining_uncommitted_code_space_.compare_exchange_weak(
            old_value, old_value - size)) {
      break;
    }
  }
  PageAllocator::Permission permission = FLAG_wasm_write_protect_code_memory
                                             ? PageAllocator::kReadWrite
                                             : PageAllocator::kReadWriteExecute;

  bool ret =
      SetPermissions(GetPlatformPageAllocator(), start, size, permission);
  TRACE_HEAP("Setting rw permissions for %p:%p\n",
             reinterpret_cast<void*>(start),
             reinterpret_cast<void*>(start + size));

  if (!ret) {
    // Highly unlikely.
    remaining_uncommitted_code_space_.fetch_add(size);
    return false;
  }
  return ret;
}

void WasmCodeManager::AssignRanges(Address start, Address end,
                                   NativeModule* native_module) {
  base::MutexGuard lock(&native_modules_mutex_);
  lookup_map_.insert(std::make_pair(start, std::make_pair(end, native_module)));
}

void WasmCodeManager::AssignRangesAndAddModule(Address start, Address end,
                                               NativeModule* native_module) {
  base::MutexGuard lock(&native_modules_mutex_);
  lookup_map_.insert(std::make_pair(start, std::make_pair(end, native_module)));
  native_modules_.emplace(native_module);
}

VirtualMemory WasmCodeManager::TryAllocate(size_t size, void* hint) {
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  DCHECK_GT(size, 0);
  size = RoundUp(size, page_allocator->AllocatePageSize());
  if (!memory_tracker_->ReserveAddressSpace(size,
                                            WasmMemoryTracker::kHardLimit)) {
    return {};
  }
  if (hint == nullptr) hint = page_allocator->GetRandomMmapAddr();

  VirtualMemory mem(page_allocator, size, hint,
                    page_allocator->AllocatePageSize());
  if (!mem.IsReserved()) {
    memory_tracker_->ReleaseReservation(size);
    return {};
  }
  TRACE_HEAP("VMem alloc: %p:%p (%zu)\n",
             reinterpret_cast<void*>(mem.address()),
             reinterpret_cast<void*>(mem.end()), mem.size());

  // TODO(v8:8462) Remove eager commit once perf supports remapping.
  if (FLAG_perf_prof) {
    SetPermissions(GetPlatformPageAllocator(), mem.address(), mem.size(),
                   PageAllocator::kReadWriteExecute);
  }
  return mem;
}

void WasmCodeManager::SampleModuleSizes(Isolate* isolate) const {
  base::MutexGuard lock(&native_modules_mutex_);
  for (NativeModule* native_module : native_modules_) {
    int code_size =
        static_cast<int>(native_module->committed_code_space_.load() / MB);
    isolate->counters()->wasm_module_code_size_mb()->AddSample(code_size);
  }
}

void WasmCodeManager::SetMaxCommittedMemoryForTesting(size_t limit) {
  remaining_uncommitted_code_space_.store(limit);
  critical_uncommitted_code_space_.store(limit / 2);
}

namespace {

void ModuleSamplingCallback(v8::Isolate* v8_isolate, v8::GCType type,
                            v8::GCCallbackFlags flags, void* data) {
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  isolate->wasm_engine()->code_manager()->SampleModuleSizes(isolate);
}

}  // namespace

// static
void WasmCodeManager::InstallSamplingGCCallback(Isolate* isolate) {
  isolate->heap()->AddGCEpilogueCallback(ModuleSamplingCallback,
                                         v8::kGCTypeMarkSweepCompact, nullptr);
}

// static
size_t WasmCodeManager::EstimateNativeModuleCodeSize(const WasmModule* module) {
  constexpr size_t kCodeSizeMultiplier = 4;
  constexpr size_t kCodeOverhead = 32;     // for prologue, stack check, ...
  constexpr size_t kStaticCodeSize = 512;  // runtime stubs, ...
  constexpr size_t kImportSize = 64 * kSystemPointerSize;

  size_t estimate = kStaticCodeSize;
  for (auto& function : module->functions) {
    estimate += kCodeOverhead + kCodeSizeMultiplier * function.code.length();
  }
  estimate +=
      JumpTableAssembler::SizeForNumberOfSlots(module->num_declared_functions);
  estimate += kImportSize * module->num_imported_functions;

  return estimate;
}

// static
size_t WasmCodeManager::EstimateNativeModuleNonCodeSize(
    const WasmModule* module) {
  size_t wasm_module_estimate = EstimateStoredSize(module);

  uint32_t num_wasm_functions = module->num_declared_functions;

  // TODO(wasm): Include wire bytes size.
  size_t native_module_estimate =
      sizeof(NativeModule) +                     /* NativeModule struct */
      (sizeof(WasmCode*) * num_wasm_functions) + /* code table size */
      (sizeof(WasmCode) * num_wasm_functions);   /* code object size */

  return wasm_module_estimate + native_module_estimate;
}

std::unique_ptr<NativeModule> WasmCodeManager::NewNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, size_t code_size_estimate,
    bool can_request_more, std::shared_ptr<const WasmModule> module) {
  DCHECK_EQ(this, isolate->wasm_engine()->code_manager());
  if (remaining_uncommitted_code_space_.load() <
      critical_uncommitted_code_space_.load()) {
    (reinterpret_cast<v8::Isolate*>(isolate))
        ->MemoryPressureNotification(MemoryPressureLevel::kCritical);
    critical_uncommitted_code_space_.store(
        remaining_uncommitted_code_space_.load() / 2);
  }

  // If the code must be contiguous, reserve enough address space up front.
  size_t code_vmem_size =
      kRequiresCodeRange ? kMaxWasmCodeMemory : code_size_estimate;
  // Try up to two times; getting rid of dead JSArrayBuffer allocations might
  // require two GCs because the first GC maybe incremental and may have
  // floating garbage.
  static constexpr int kAllocationRetries = 2;
  VirtualMemory code_space;
  for (int retries = 0;; ++retries) {
    code_space = TryAllocate(code_vmem_size);
    if (code_space.IsReserved()) break;
    if (retries == kAllocationRetries) {
      V8::FatalProcessOutOfMemory(isolate, "WasmCodeManager::NewNativeModule");
      UNREACHABLE();
    }
    // Run one GC, then try the allocation again.
    isolate->heap()->MemoryPressureNotification(MemoryPressureLevel::kCritical,
                                                true);
  }

  Address start = code_space.address();
  size_t size = code_space.size();
  Address end = code_space.end();
  std::unique_ptr<NativeModule> ret(new NativeModule(
      isolate, enabled, can_request_more, std::move(code_space),
      isolate->wasm_engine()->code_manager(), std::move(module)));
  TRACE_HEAP("New NativeModule %p: Mem: %" PRIuPTR ",+%zu\n", ret.get(), start,
             size);
  AssignRangesAndAddModule(start, end, ret.get());
  return ret;
}

bool NativeModule::SetExecutable(bool executable) {
  if (is_executable_ == executable) return true;
  TRACE_HEAP("Setting module %p as executable: %d.\n", this, executable);

  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();

  if (FLAG_wasm_write_protect_code_memory) {
    PageAllocator::Permission permission =
        executable ? PageAllocator::kReadExecute : PageAllocator::kReadWrite;
#if V8_OS_WIN
    // On windows, we need to switch permissions per separate virtual memory
    // reservation. This is really just a problem when the NativeModule is
    // growable (meaning can_request_more_memory_). That's 32-bit in production,
    // or unittests.
    // For now, in that case, we commit at reserved memory granularity.
    // Technically, that may be a waste, because we may reserve more than we
    // use. On 32-bit though, the scarce resource is the address space -
    // committed or not.
    if (can_request_more_memory_) {
      for (auto& vmem : owned_code_space_) {
        if (!SetPermissions(page_allocator, vmem.address(), vmem.size(),
                            permission)) {
          return false;
        }
        TRACE_HEAP("Set %p:%p to executable:%d\n", vmem.address(), vmem.end(),
                   executable);
      }
      is_executable_ = executable;
      return true;
    }
#endif
    for (auto& region : allocated_code_space_.regions()) {
      // allocated_code_space_ is fine-grained, so we need to
      // page-align it.
      size_t region_size =
          RoundUp(region.size(), page_allocator->AllocatePageSize());
      if (!SetPermissions(page_allocator, region.begin(), region_size,
                          permission)) {
        return false;
      }
      TRACE_HEAP("Set %p:%p to executable:%d\n",
                 reinterpret_cast<void*>(region.begin()),
                 reinterpret_cast<void*>(region.end()), executable);
    }
  }
  is_executable_ = executable;
  return true;
}

void WasmCodeManager::FreeNativeModule(NativeModule* native_module) {
  base::MutexGuard lock(&native_modules_mutex_);
  DCHECK_EQ(1, native_modules_.count(native_module));
  native_modules_.erase(native_module);
  TRACE_HEAP("Freeing NativeModule %p\n", native_module);
  for (auto& code_space : native_module->owned_code_space_) {
    DCHECK(code_space.IsReserved());
    TRACE_HEAP("VMem Release: %" PRIxPTR ":%" PRIxPTR " (%zu)\n",
               code_space.address(), code_space.end(), code_space.size());
    lookup_map_.erase(code_space.address());
    memory_tracker_->ReleaseReservation(code_space.size());
    code_space.Free();
    DCHECK(!code_space.IsReserved());
  }
  native_module->owned_code_space_.clear();

  size_t code_size = native_module->committed_code_space_.load();
  DCHECK(IsAligned(code_size, AllocatePageSize()));
  remaining_uncommitted_code_space_.fetch_add(code_size);
  // Remaining code space cannot grow bigger than maximum code space size.
  DCHECK_LE(remaining_uncommitted_code_space_.load(), kMaxWasmCodeMemory);
}

NativeModule* WasmCodeManager::LookupNativeModule(Address pc) const {
  base::MutexGuard lock(&native_modules_mutex_);
  if (lookup_map_.empty()) return nullptr;

  auto iter = lookup_map_.upper_bound(pc);
  if (iter == lookup_map_.begin()) return nullptr;
  --iter;
  Address region_start = iter->first;
  Address region_end = iter->second.first;
  NativeModule* candidate = iter->second.second;

  DCHECK_NOT_NULL(candidate);
  return region_start <= pc && pc < region_end ? candidate : nullptr;
}

WasmCode* WasmCodeManager::LookupCode(Address pc) const {
  NativeModule* candidate = LookupNativeModule(pc);
  return candidate ? candidate->Lookup(pc) : nullptr;
}

size_t WasmCodeManager::remaining_uncommitted_code_space() const {
  return remaining_uncommitted_code_space_.load();
}

// TODO(v8:7424): Code protection scopes are not yet supported with shared code
// enabled and need to be revisited to work with --wasm-shared-code as well.
NativeModuleModificationScope::NativeModuleModificationScope(
    NativeModule* native_module)
    : native_module_(native_module) {
  if (FLAG_wasm_write_protect_code_memory && native_module_ &&
      (native_module_->modification_scope_depth_++) == 0) {
    bool success = native_module_->SetExecutable(false);
    CHECK(success);
  }
}

NativeModuleModificationScope::~NativeModuleModificationScope() {
  if (FLAG_wasm_write_protect_code_memory && native_module_ &&
      (native_module_->modification_scope_depth_--) == 1) {
    bool success = native_module_->SetExecutable(true);
    CHECK(success);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#undef TRACE_HEAP
