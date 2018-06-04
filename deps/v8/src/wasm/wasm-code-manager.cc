// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-manager.h"

#include <iomanip>

#include "src/assembler-inl.h"
#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/disassembler.h"
#include "src/globals.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"

#define TRACE_HEAP(...)                                   \
  do {                                                    \
    if (FLAG_wasm_trace_native_heap) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// Binary predicate to perform lookups in {NativeModule::owned_code_} with a
// given address into a code object. Use with {std::upper_bound} for example.
struct WasmCodeUniquePtrComparator {
  bool operator()(Address pc, const std::unique_ptr<WasmCode>& code) const {
    DCHECK_NOT_NULL(pc);
    DCHECK_NOT_NULL(code);
    return pc < code->instructions().start();
  }
};

#if V8_TARGET_ARCH_X64
#define __ masm->
constexpr bool kModuleCanAllocateMoreMemory = false;

void GenerateJumpTrampoline(MacroAssembler* masm, Address target) {
  __ movq(kScratchRegister, reinterpret_cast<uint64_t>(target));
  __ jmp(kScratchRegister);
}
#undef __
#elif V8_TARGET_ARCH_S390X
#define __ masm->
constexpr bool kModuleCanAllocateMoreMemory = false;

void GenerateJumpTrampoline(MacroAssembler* masm, Address target) {
  __ mov(ip, Operand(bit_cast<intptr_t, Address>(target)));
  __ b(ip);
}
#undef __
#else
const bool kModuleCanAllocateMoreMemory = true;
#endif

void RelocateCode(WasmCode* code, const WasmCode* orig,
                  WasmCode::FlushICache flush_icache) {
  intptr_t delta = code->instructions().start() - orig->instructions().start();
  for (RelocIterator it(code->instructions(), code->reloc_info(),
                        code->constant_pool(), RelocInfo::kApplyMask);
       !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  if (flush_icache) {
    Assembler::FlushICache(code->instructions().start(),
                           code->instructions().size());
  }
}

}  // namespace

DisjointAllocationPool::DisjointAllocationPool(Address start, Address end) {
  ranges_.push_back({start, end});
}

void DisjointAllocationPool::Merge(DisjointAllocationPool&& other) {
  auto dest_it = ranges_.begin();
  auto dest_end = ranges_.end();

  for (auto src_it = other.ranges_.begin(), src_end = other.ranges_.end();
       src_it != src_end;) {
    if (dest_it == dest_end) {
      // everything else coming from src will be inserted
      // at the back of ranges_ from now on.
      ranges_.push_back(*src_it);
      ++src_it;
      continue;
    }
    // Before or adjacent to dest. Insert or merge, and advance
    // just src.
    if (dest_it->first >= src_it->second) {
      if (dest_it->first == src_it->second) {
        dest_it->first = src_it->first;
      } else {
        ranges_.insert(dest_it, {src_it->first, src_it->second});
      }
      ++src_it;
      continue;
    }
    // Src is strictly after dest. Skip over this dest.
    if (dest_it->second < src_it->first) {
      ++dest_it;
      continue;
    }
    // Src is adjacent from above. Merge and advance
    // just src, because the next src, if any, is bound to be
    // strictly above the newly-formed range.
    DCHECK_EQ(dest_it->second, src_it->first);
    dest_it->second = src_it->second;
    ++src_it;
    // Now that we merged, maybe this new range is adjacent to
    // the next. Since we assume src to have come from the
    // same original memory pool, it follows that the next src
    // must be above or adjacent to the new bubble.
    auto next_dest = dest_it;
    ++next_dest;
    if (next_dest != dest_end && dest_it->second == next_dest->first) {
      dest_it->second = next_dest->second;
      ranges_.erase(next_dest);
    }

    // src_it points now at the next, if any, src
    DCHECK_IMPLIES(src_it != src_end, src_it->first >= dest_it->second);
  }
}

DisjointAllocationPool DisjointAllocationPool::Extract(size_t size,
                                                       ExtractionMode mode) {
  DisjointAllocationPool ret;
  for (auto it = ranges_.begin(), end = ranges_.end(); it != end;) {
    auto current = it;
    ++it;
    DCHECK_LT(current->first, current->second);
    size_t current_size = reinterpret_cast<size_t>(current->second) -
                          reinterpret_cast<size_t>(current->first);
    if (size == current_size) {
      ret.ranges_.push_back(*current);
      ranges_.erase(current);
      return ret;
    }
    if (size < current_size) {
      ret.ranges_.push_back({current->first, current->first + size});
      current->first += size;
      DCHECK(current->first < current->second);
      return ret;
    }
    if (mode != kContiguous) {
      size -= current_size;
      ret.ranges_.push_back(*current);
      ranges_.erase(current);
    }
  }
  if (size > 0) {
    Merge(std::move(ret));
    return {};
  }
  return ret;
}

Address WasmCode::constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    if (constant_pool_offset_ < instructions().size()) {
      return instructions().start() + constant_pool_offset_;
    }
  }
  return nullptr;
}

size_t WasmCode::trap_handler_index() const {
  CHECK(HasTrapHandlerIndex());
  return static_cast<size_t>(trap_handler_index_);
}

void WasmCode::set_trap_handler_index(size_t value) {
  trap_handler_index_ = value;
}

bool WasmCode::HasTrapHandlerIndex() const { return trap_handler_index_ >= 0; }

void WasmCode::ResetTrapHandlerIndex() { trap_handler_index_ = -1; }

bool WasmCode::ShouldBeLogged(Isolate* isolate) {
  return isolate->logger()->is_logging_code_events() ||
         isolate->is_profiling() || FLAG_print_wasm_code || FLAG_print_code;
}

void WasmCode::LogCode(Isolate* isolate) const {
  DCHECK(ShouldBeLogged(isolate));
  if (native_module()->compiled_module()->has_shared() && index_.IsJust()) {
    uint32_t index = this->index();
    Handle<WasmSharedModuleData> shared_handle(
        native_module()->compiled_module()->shared(), isolate);
    int name_length;
    Handle<String> name(
        WasmSharedModuleData::GetFunctionName(isolate, shared_handle, index));
    auto cname =
        name->ToCString(AllowNullsFlag::DISALLOW_NULLS,
                        RobustnessFlag::ROBUST_STRING_TRAVERSAL, &name_length);
    PROFILE(isolate,
            CodeCreateEvent(CodeEventListener::FUNCTION_TAG, this,
                            {cname.get(), static_cast<size_t>(name_length)}));

#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_code || FLAG_print_wasm_code) {
      // TODO(wasm): Use proper log files, here and elsewhere.
      OFStream os(stdout);
      os << "--- Wasm " << (is_liftoff() ? "liftoff" : "turbofan")
         << " code ---\n";
      this->Disassemble(cname.get(), isolate, os);
      os << "--- End code ---\n";
    }
#endif

    if (!source_positions().is_empty()) {
      LOG_CODE_EVENT(isolate, CodeLinePosInfoRecordEvent(instructions().start(),
                                                         source_positions()));
    }
  }
}

void WasmCode::Print(Isolate* isolate) const {
  OFStream os(stdout);
  Disassemble(nullptr, isolate, os);
}

void WasmCode::Disassemble(const char* name, Isolate* isolate,
                           std::ostream& os) const {
  if (name) os << "name: " << name << "\n";
  if (index_.IsJust()) os << "index: " << index_.FromJust() << "\n";
  os << "kind: " << GetWasmCodeKindAsString(kind_) << "\n";
  os << "compiler: " << (is_liftoff() ? "Liftoff" : "TurboFan") << "\n";
  size_t body_size = instructions().size();
  os << "Body (size = " << body_size << ")\n";

#ifdef ENABLE_DISASSEMBLER

  size_t instruction_size = body_size;
  if (constant_pool_offset_ && constant_pool_offset_ < instruction_size) {
    instruction_size = constant_pool_offset_;
  }
  if (safepoint_table_offset_ && safepoint_table_offset_ < instruction_size) {
    instruction_size = safepoint_table_offset_;
  }
  DCHECK_LT(0, instruction_size);
  os << "Instructions (size = " << instruction_size << ")\n";
  // TODO(mtrofin): rework the dependency on isolate and code in
  // Disassembler::Decode.
  Disassembler::Decode(isolate, &os, instructions().start(),
                       instructions().start() + instruction_size, nullptr);
  os << "\n";

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

  os << "RelocInfo (size = " << reloc_size_ << ")\n";
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    it.rinfo()->Print(isolate, os);
  }
  os << "\n";
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
    case WasmCode::kInterpreterStub:
      return "interpreter-entry";
    case WasmCode::kCopiedStub:
      return "copied stub";
    case WasmCode::kTrampoline:
      return "trampoline";
  }
  return "unknown kind";
}

WasmCode::~WasmCode() {
  // Depending on finalizer order, the WasmCompiledModule finalizer may be
  // called first, case in which we release here. If the InstanceFinalizer is
  // called first, the handlers will be cleared in Reset, as-if the NativeModule
  // may be later used again (which would be the case if the WasmCompiledModule
  // were still held by a WasmModuleObject)
  if (HasTrapHandlerIndex()) {
    CHECK_LT(trap_handler_index(),
             static_cast<size_t>(std::numeric_limits<int>::max()));
    trap_handler::ReleaseHandlerData(static_cast<int>(trap_handler_index()));
  }
}

// Helper class to selectively clone and patch code from a
// {source_native_module} into a {cloning_native_module}.
class NativeModule::CloneCodeHelper {
 public:
  explicit CloneCodeHelper(NativeModule* source_native_module,
                           NativeModule* cloning_native_module);

  void SelectForCloning(int32_t code_index);

  void CloneAndPatchCode(bool patch_stub_to_stub_calls);

  void PatchTrampolineAndStubCalls(const WasmCode* original_code,
                                   const WasmCode* new_code,
                                   WasmCode::FlushICache flush_icache);

 private:
  void PatchStubToStubCalls();

  NativeModule* source_native_module_;
  NativeModule* cloning_native_module_;
  std::vector<uint32_t> selection_;
  std::unordered_map<Address, Address, AddressHasher> reverse_lookup_;
};

NativeModule::CloneCodeHelper::CloneCodeHelper(
    NativeModule* source_native_module, NativeModule* cloning_native_module)
    : source_native_module_(source_native_module),
      cloning_native_module_(cloning_native_module) {
  for (auto& pair : source_native_module_->trampolines_) {
    Address old_dest = pair.second;
    auto local = cloning_native_module_->trampolines_.find(pair.first);
    DCHECK(local != cloning_native_module_->trampolines_.end());
    Address new_dest = local->second;
    reverse_lookup_.emplace(old_dest, new_dest);
  }

  for (auto& pair : source_native_module_->stubs_) {
    Address old_dest = pair.second->instructions().start();
    auto local = cloning_native_module_->stubs_.find(pair.first);
    DCHECK(local != cloning_native_module_->stubs_.end());
    Address new_dest = local->second->instructions().start();
    reverse_lookup_.emplace(old_dest, new_dest);
  }
}

void NativeModule::CloneCodeHelper::SelectForCloning(int32_t code_index) {
  selection_.emplace_back(code_index);
}

void NativeModule::CloneCodeHelper::CloneAndPatchCode(
    bool patch_stub_to_stub_calls) {
  if (patch_stub_to_stub_calls) {
    PatchStubToStubCalls();
  }

  WasmCode* anonymous_lazy_builtin = nullptr;
  for (uint32_t index : selection_) {
    const WasmCode* original_code = source_native_module_->GetCode(index);
    switch (original_code->kind()) {
      case WasmCode::kLazyStub: {
        // Use the first anonymous lazy compile stub hit in this loop as the
        // canonical copy for all further ones by remembering it locally via
        // the {anonymous_lazy_builtin} variable.
        if (!original_code->IsAnonymous()) {
          WasmCode* new_code = cloning_native_module_->CloneCode(
              original_code, WasmCode::kNoFlushICache);
          PatchTrampolineAndStubCalls(original_code, new_code,
                                      WasmCode::kFlushICache);
          break;
        }
        if (anonymous_lazy_builtin == nullptr) {
          WasmCode* new_code = cloning_native_module_->CloneCode(
              original_code, WasmCode::kNoFlushICache);
          PatchTrampolineAndStubCalls(original_code, new_code,
                                      WasmCode::kFlushICache);
          anonymous_lazy_builtin = new_code;
        }
        cloning_native_module_->code_table_[index] = anonymous_lazy_builtin;
      } break;
      case WasmCode::kFunction: {
        WasmCode* new_code = cloning_native_module_->CloneCode(
            original_code, WasmCode::kNoFlushICache);
        PatchTrampolineAndStubCalls(original_code, new_code,
                                    WasmCode::kFlushICache);
      } break;
      default:
        UNREACHABLE();
    }
  }
}

void NativeModule::CloneCodeHelper::PatchStubToStubCalls() {
  for (auto& pair : cloning_native_module_->stubs_) {
    WasmCode* new_stub = pair.second;
    WasmCode* old_stub = source_native_module_->stubs_.find(pair.first)->second;
    PatchTrampolineAndStubCalls(old_stub, new_stub, WasmCode::kFlushICache);
  }
}

void NativeModule::CloneCodeHelper::PatchTrampolineAndStubCalls(
    const WasmCode* original_code, const WasmCode* new_code,
    WasmCode::FlushICache flush_icache) {
  // Relocate everything in kApplyMask using this delta, and patch all code
  // targets to call the new trampolines and stubs.
  intptr_t delta =
      new_code->instructions().start() - original_code->instructions().start();
  int mask = RelocInfo::kApplyMask | RelocInfo::kCodeTargetMask;
  RelocIterator orig_it(original_code->instructions(),
                        original_code->reloc_info(),
                        original_code->constant_pool(), mask);
  for (RelocIterator it(new_code->instructions(), new_code->reloc_info(),
                        new_code->constant_pool(), mask);
       !it.done(); it.next(), orig_it.next()) {
    if (RelocInfo::IsCodeTarget(it.rinfo()->rmode())) {
      Address target = orig_it.rinfo()->target_address();
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X
      auto found = reverse_lookup_.find(target);
      DCHECK(found != reverse_lookup_.end());
      target = found->second;
#endif
      it.rinfo()->set_target_address(target, SKIP_WRITE_BARRIER);
    } else {
      it.rinfo()->apply(delta);
    }
  }
  if (flush_icache) {
    Assembler::FlushICache(new_code->instructions().start(),
                           new_code->instructions().size());
  }
}

base::AtomicNumber<size_t> NativeModule::next_id_;

NativeModule::NativeModule(uint32_t num_functions, uint32_t num_imports,
                           bool can_request_more, VirtualMemory* mem,
                           WasmCodeManager* code_manager)
    : instance_id(next_id_.Increment(1)),
      code_table_(num_functions),
      num_imported_functions_(num_imports),
      compilation_state_(NewCompilationState(
          reinterpret_cast<Isolate*>(code_manager->isolate_))),
      free_memory_(reinterpret_cast<Address>(mem->address()),
                   reinterpret_cast<Address>(mem->end())),
      wasm_code_manager_(code_manager),
      can_request_more_memory_(can_request_more) {
  VirtualMemory my_mem;
  owned_memory_.push_back(my_mem);
  owned_memory_.back().TakeControl(mem);
  owned_code_.reserve(num_functions);
}

void NativeModule::ResizeCodeTableForTesting(size_t num_functions,
                                             size_t max_functions) {
  DCHECK_LE(num_functions, max_functions);
  if (num_imported_functions_ == num_functions) {
    // For some tests, the code table might have been initialized to store
    // a number of imported functions on creation. If that is the case,
    // we need to retroactively reserve the space.
    DCHECK_EQ(code_table_.capacity(), num_imported_functions_);
    DCHECK_EQ(code_table_.size(), num_imported_functions_);
    DCHECK_EQ(num_functions, 1);
    code_table_.reserve(max_functions);
  } else {
    DCHECK_GT(num_functions, FunctionCount());
    if (code_table_.capacity() == 0) {
      code_table_.reserve(max_functions);
    }
    DCHECK_EQ(code_table_.capacity(), max_functions);
    code_table_.resize(num_functions);
  }
}

WasmCode* NativeModule::GetCode(uint32_t index) const {
  DCHECK_LT(index, FunctionCount());
  return code_table_[index];
}

void NativeModule::SetCode(uint32_t index, WasmCode* wasm_code) {
  DCHECK_LT(index, FunctionCount());
  code_table_[index] = wasm_code;
}

uint32_t NativeModule::FunctionCount() const {
  DCHECK_LE(code_table_.size(), std::numeric_limits<uint32_t>::max());
  return static_cast<uint32_t>(code_table_.size());
}

WasmCode* NativeModule::AddOwnedCode(
    Vector<const byte> orig_instructions,
    std::unique_ptr<const byte[]> reloc_info, size_t reloc_size,
    std::unique_ptr<const byte[]> source_pos, size_t source_pos_size,
    Maybe<uint32_t> index, WasmCode::Kind kind, size_t constant_pool_offset,
    uint32_t stack_slots, size_t safepoint_table_offset,
    size_t handler_table_offset,
    std::shared_ptr<ProtectedInstructions> protected_instructions,
    WasmCode::Tier tier, WasmCode::FlushICache flush_icache) {
  // both allocation and insertion in owned_code_ happen in the same critical
  // section, thus ensuring owned_code_'s elements are rarely if ever moved.
  base::LockGuard<base::Mutex> lock(&allocation_mutex_);
  Address executable_buffer = AllocateForCode(orig_instructions.size());
  if (executable_buffer == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "NativeModule::AddOwnedCode");
    UNREACHABLE();
  }
  memcpy(executable_buffer, orig_instructions.start(),
         orig_instructions.size());
  std::unique_ptr<WasmCode> code(new WasmCode(
      {executable_buffer, orig_instructions.size()}, std::move(reloc_info),
      reloc_size, std::move(source_pos), source_pos_size, this, index, kind,
      constant_pool_offset, stack_slots, safepoint_table_offset,
      handler_table_offset, std::move(protected_instructions), tier));
  WasmCode* ret = code.get();

  // TODO(mtrofin): We allocate in increasing address order, and
  // even if we end up with segmented memory, we may end up only with a few
  // large moves - if, for example, a new segment is below the current ones.
  auto insert_before = std::upper_bound(owned_code_.begin(), owned_code_.end(),
                                        ret->instructions().start(),
                                        WasmCodeUniquePtrComparator());
  owned_code_.insert(insert_before, std::move(code));
  if (flush_icache) {
    Assembler::FlushICache(ret->instructions().start(),
                           ret->instructions().size());
  }
  return ret;
}

WasmCode* NativeModule::AddCodeCopy(Handle<Code> code, WasmCode::Kind kind,
                                    uint32_t index) {
  WasmCode* ret = AddAnonymousCode(code, kind);
  code_table_[index] = ret;
  ret->index_ = Just(index);
  return ret;
}

WasmCode* NativeModule::AddInterpreterWrapper(Handle<Code> code,
                                              uint32_t index) {
  WasmCode* ret = AddAnonymousCode(code, WasmCode::kInterpreterStub);
  ret->index_ = Just(index);
  return ret;
}

void NativeModule::SetLazyBuiltin(Handle<Code> code) {
  WasmCode* lazy_builtin = AddAnonymousCode(code, WasmCode::kLazyStub);
  for (uint32_t i = num_imported_functions(), e = FunctionCount(); i < e; ++i) {
    code_table_[i] = lazy_builtin;
  }
}

WasmCompiledModule* NativeModule::compiled_module() const {
  DCHECK_NOT_NULL(compiled_module_);
  return *compiled_module_;
}

void NativeModule::SetCompiledModule(
    Handle<WasmCompiledModule> compiled_module) {
  DCHECK_NULL(compiled_module_);
  compiled_module_ = compiled_module->GetIsolate()
                         ->global_handles()
                         ->Create(*compiled_module)
                         .location();
  GlobalHandles::MakeWeak(reinterpret_cast<Object***>(&compiled_module_));
}

WasmCode* NativeModule::AddAnonymousCode(Handle<Code> code,
                                         WasmCode::Kind kind) {
  std::unique_ptr<byte[]> reloc_info;
  if (code->relocation_size() > 0) {
    reloc_info.reset(new byte[code->relocation_size()]);
    memcpy(reloc_info.get(), code->relocation_start(), code->relocation_size());
  }
  std::unique_ptr<byte[]> source_pos;
  Handle<ByteArray> source_pos_table(code->SourcePositionTable());
  if (source_pos_table->length() > 0) {
    source_pos.reset(new byte[source_pos_table->length()]);
    source_pos_table->copy_out(0, source_pos.get(), source_pos_table->length());
  }
  std::shared_ptr<ProtectedInstructions> protected_instructions(
      new ProtectedInstructions(0));
  Vector<const byte> orig_instructions(
      code->raw_instruction_start(),
      static_cast<size_t>(code->raw_instruction_size()));
  int stack_slots = code->has_safepoint_info() ? code->stack_slots() : 0;
  int safepoint_table_offset =
      code->has_safepoint_info() ? code->safepoint_table_offset() : 0;
  WasmCode* ret =
      AddOwnedCode(orig_instructions,      // instructions
                   std::move(reloc_info),  // reloc_info
                   static_cast<size_t>(code->relocation_size()),  // reloc_size
                   std::move(source_pos),  // source positions
                   static_cast<size_t>(source_pos_table->length()),
                   Nothing<uint32_t>(),           // index
                   kind,                          // kind
                   code->constant_pool_offset(),  // constant_pool_offset
                   stack_slots,                   // stack_slots
                   safepoint_table_offset,        // safepoint_table_offset
                   code->handler_table_offset(),  // handler_table_offset
                   protected_instructions,        // protected_instructions
                   WasmCode::kOther,              // kind
                   WasmCode::kNoFlushICache);     // flush_icache
  intptr_t delta = ret->instructions().start() - code->raw_instruction_start();
  int mask = RelocInfo::kApplyMask | RelocInfo::kCodeTargetMask |
             RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);

  RelocIterator orig_it(*code, mask);
  for (RelocIterator it(ret->instructions(), ret->reloc_info(),
                        ret->constant_pool(), mask);
       !it.done(); it.next(), orig_it.next()) {
    if (RelocInfo::IsCodeTarget(it.rinfo()->rmode())) {
      Code* call_target =
          Code::GetCodeFromTargetAddress(orig_it.rinfo()->target_address());
      it.rinfo()->set_target_address(GetLocalAddressFor(handle(call_target)),
                                     SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else {
      if (RelocInfo::IsEmbeddedObject(it.rinfo()->rmode())) {
        DCHECK(Heap::IsImmovable(it.rinfo()->target_object()));
      } else {
        it.rinfo()->apply(delta);
      }
    }
  }
  // Flush the i-cache here instead of in AddOwnedCode, to include the changes
  // made while iterating over the RelocInfo above.
  Assembler::FlushICache(ret->instructions().start(),
                         ret->instructions().size());
  if (FLAG_print_wasm_code) {
    // TODO(mstarzinger): don't need the isolate here.
    ret->Print(code->GetIsolate());
  }
  return ret;
}

WasmCode* NativeModule::AddCode(
    const CodeDesc& desc, uint32_t frame_slots, uint32_t index,
    size_t safepoint_table_offset, size_t handler_table_offset,
    std::unique_ptr<ProtectedInstructions> protected_instructions,
    Handle<ByteArray> source_pos_table, WasmCode::Tier tier) {
  std::unique_ptr<byte[]> reloc_info;
  if (desc.reloc_size) {
    reloc_info.reset(new byte[desc.reloc_size]);
    memcpy(reloc_info.get(), desc.buffer + desc.buffer_size - desc.reloc_size,
           desc.reloc_size);
  }
  std::unique_ptr<byte[]> source_pos;
  if (source_pos_table->length() > 0) {
    source_pos.reset(new byte[source_pos_table->length()]);
    source_pos_table->copy_out(0, source_pos.get(), source_pos_table->length());
  }
  TurboAssembler* origin = reinterpret_cast<TurboAssembler*>(desc.origin);
  WasmCode* ret = AddOwnedCode(
      {desc.buffer, static_cast<size_t>(desc.instr_size)},
      std::move(reloc_info), static_cast<size_t>(desc.reloc_size),
      std::move(source_pos), static_cast<size_t>(source_pos_table->length()),
      Just(index), WasmCode::kFunction,
      desc.instr_size - desc.constant_pool_size, frame_slots,
      safepoint_table_offset, handler_table_offset,
      std::move(protected_instructions), tier, WasmCode::kNoFlushICache);

  code_table_[index] = ret;
  // TODO(mtrofin): this is a copy and paste from Code::CopyFrom.
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                  RelocInfo::kApplyMask;
  // Needed to find target_object and runtime_entry on X64

  AllowDeferredHandleDereference embedding_raw_address;
  for (RelocIterator it(ret->instructions(), ret->reloc_info(),
                        ret->constant_pool(), mode_mask);
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      DCHECK(p->IsUndefined(p->GetIsolate()) || p->IsNull(p->GetIsolate()));
      it.rinfo()->set_target_object(*p, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles to direct pointers to the first instruction in the
      // code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(GetLocalAddressFor(handle(code)),
                                     SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(p, SKIP_WRITE_BARRIER,
                                           SKIP_ICACHE_FLUSH);
    } else {
      intptr_t delta = ret->instructions().start() - desc.buffer;
      it.rinfo()->apply(delta);
    }
  }
  // Flush the i-cache here instead of in AddOwnedCode, to include the changes
  // made while iterating over the RelocInfo above.
  Assembler::FlushICache(ret->instructions().start(),
                         ret->instructions().size());
  return ret;
}

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X
Address NativeModule::CreateTrampolineTo(Handle<Code> code) {
  MacroAssembler masm(code->GetIsolate(), nullptr, 0, CodeObjectRequired::kNo);
  Address dest = code->raw_instruction_start();
  GenerateJumpTrampoline(&masm, dest);
  CodeDesc code_desc;
  masm.GetCode(nullptr, &code_desc);
  Vector<const byte> instructions(code_desc.buffer,
                                  static_cast<size_t>(code_desc.instr_size));
  WasmCode* wasm_code = AddOwnedCode(instructions,           // instructions
                                     nullptr,                // reloc_info
                                     0,                      // reloc_size
                                     nullptr,                // source_pos
                                     0,                      // source_pos_size
                                     Nothing<uint32_t>(),    // index
                                     WasmCode::kTrampoline,  // kind
                                     0,   // constant_pool_offset
                                     0,   // stack_slots
                                     0,   // safepoint_table_offset
                                     0,   // handler_table_offset
                                     {},  // protected_instructions
                                     WasmCode::kOther,         // tier
                                     WasmCode::kFlushICache);  // flush_icache
  Address ret = wasm_code->instructions().start();
  trampolines_.emplace(std::make_pair(dest, ret));
  return ret;
}
#else
Address NativeModule::CreateTrampolineTo(Handle<Code> code) {
  Address ret = code->raw_instruction_start();
  trampolines_.insert(std::make_pair(ret, ret));
  return ret;
}
#endif

Address NativeModule::GetLocalAddressFor(Handle<Code> code) {
  if (!Heap::IsImmovable(*code)) {
    DCHECK(code->kind() == Code::STUB &&
           CodeStub::MajorKeyFromKey(code->stub_key()) == CodeStub::DoubleToI);
    uint32_t key = code->stub_key();
    auto copy = stubs_.find(key);
    if (copy == stubs_.end()) {
      WasmCode* ret = AddAnonymousCode(code, WasmCode::kCopiedStub);
      copy = stubs_.emplace(std::make_pair(key, ret)).first;
    }
    return copy->second->instructions().start();
  } else {
    Address index = code->raw_instruction_start();
    auto trampoline_iter = trampolines_.find(index);
    if (trampoline_iter == trampolines_.end()) {
      return CreateTrampolineTo(code);
    } else {
      return trampoline_iter->second;
    }
  }
}

Address NativeModule::AllocateForCode(size_t size) {
  // this happens under a lock assumed by the caller.
  size = RoundUp(size, kCodeAlignment);
  DisjointAllocationPool mem = free_memory_.Allocate(size);
  if (mem.IsEmpty()) {
    if (!can_request_more_memory_) return nullptr;

    Address hint = owned_memory_.empty()
                       ? nullptr
                       : reinterpret_cast<Address>(owned_memory_.back().end());
    VirtualMemory empty_mem;
    owned_memory_.push_back(empty_mem);
    VirtualMemory& new_mem = owned_memory_.back();
    wasm_code_manager_->TryAllocate(size, &new_mem, hint);
    if (!new_mem.IsReserved()) return nullptr;
    DisjointAllocationPool mem_pool(
        reinterpret_cast<Address>(new_mem.address()),
        reinterpret_cast<Address>(new_mem.end()));
    wasm_code_manager_->AssignRanges(new_mem.address(), new_mem.end(), this);

    free_memory_.Merge(std::move(mem_pool));
    mem = free_memory_.Allocate(size);
    if (mem.IsEmpty()) return nullptr;
  }
  Address ret = mem.ranges().front().first;
  Address end = ret + size;
  Address commit_start = RoundUp(ret, AllocatePageSize());
  Address commit_end = RoundUp(end, AllocatePageSize());
  // {commit_start} will be either ret or the start of the next page.
  // {commit_end} will be the start of the page after the one in which
  // the allocation ends.
  // We start from an aligned start, and we know we allocated vmem in
  // page multiples.
  // We just need to commit what's not committed. The page in which we
  // start is already committed (or we start at the beginning of a page).
  // The end needs to be committed all through the end of the page.
  if (commit_start < commit_end) {
#if V8_OS_WIN
    // On Windows, we cannot commit a range that straddles different
    // reservations of virtual memory. Because we bump-allocate, and because, if
    // we need more memory, we append that memory at the end of the
    // owned_memory_ list, we traverse that list in reverse order to find the
    // reservation(s) that guide how to chunk the region to commit.
    for (auto it = owned_memory_.crbegin(), rend = owned_memory_.crend();
         it != rend && commit_start < commit_end; ++it) {
      if (commit_end > it->end() || it->address() >= commit_end) continue;
      Address start =
          std::max(commit_start, reinterpret_cast<Address>(it->address()));
      size_t commit_size = static_cast<size_t>(commit_end - start);
      DCHECK(IsAligned(commit_size, AllocatePageSize()));
      if (!wasm_code_manager_->Commit(start, commit_size)) {
        return nullptr;
      }
      committed_memory_ += commit_size;
      commit_end = start;
    }
#else
    size_t commit_size = static_cast<size_t>(commit_end - commit_start);
    DCHECK(IsAligned(commit_size, AllocatePageSize()));
    if (!wasm_code_manager_->Commit(commit_start, commit_size)) {
      return nullptr;
    }
    committed_memory_ += commit_size;
#endif
  }
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(ret), kCodeAlignment));
  allocated_memory_.Merge(std::move(mem));
  TRACE_HEAP("ID: %zu. Code alloc: %p,+%zu\n", instance_id,
             reinterpret_cast<void*>(ret), size);
  return ret;
}

WasmCode* NativeModule::Lookup(Address pc) {
  if (owned_code_.empty()) return nullptr;
  auto iter = std::upper_bound(owned_code_.begin(), owned_code_.end(), pc,
                               WasmCodeUniquePtrComparator());
  if (iter == owned_code_.begin()) return nullptr;
  --iter;
  WasmCode* candidate = (*iter).get();
  DCHECK_NOT_NULL(candidate);
  if (candidate->instructions().start() <= pc &&
      pc < candidate->instructions().start() +
               candidate->instructions().size()) {
    return candidate;
  }
  return nullptr;
}

WasmCode* NativeModule::GetIndirectlyCallableCode(uint32_t func_index) {
  WasmCode* code = GetCode(func_index);
  if (!code || code->kind() != WasmCode::kLazyStub) {
    return code;
  }
#if DEBUG
  auto num_imported_functions =
      compiled_module()->shared()->module()->num_imported_functions;
  if (func_index < num_imported_functions) {
    DCHECK(!code->IsAnonymous());
  }
#endif
  if (!code->IsAnonymous()) {
    // If the function wasn't imported, its index should match.
    DCHECK_IMPLIES(func_index >= num_imported_functions,
                   func_index == code->index());
    return code;
  }
  if (!lazy_compile_stubs_.get()) {
    lazy_compile_stubs_ =
        base::make_unique<std::vector<WasmCode*>>(FunctionCount());
  }
  WasmCode* cloned_code = lazy_compile_stubs_.get()->at(func_index);
  if (cloned_code == nullptr) {
    cloned_code = CloneCode(code, WasmCode::kNoFlushICache);
    RelocateCode(cloned_code, code, WasmCode::kFlushICache);
    cloned_code->index_ = Just(func_index);
    lazy_compile_stubs_.get()->at(func_index) = cloned_code;
  }
  DCHECK_EQ(func_index, cloned_code->index());
  return cloned_code;
}

void NativeModule::CloneTrampolinesAndStubs(
    const NativeModule* other, WasmCode::FlushICache flush_icache) {
  for (auto& pair : other->trampolines_) {
    Address key = pair.first;
    Address local =
        GetLocalAddressFor(handle(Code::GetCodeFromTargetAddress(key)));
    DCHECK_NOT_NULL(local);
    trampolines_.emplace(std::make_pair(key, local));
  }
  for (auto& pair : other->stubs_) {
    uint32_t key = pair.first;
    WasmCode* clone = CloneCode(pair.second, flush_icache);
    stubs_.emplace(std::make_pair(key, clone));
  }
}

WasmCode* NativeModule::CloneCode(const WasmCode* original_code,
                                  WasmCode::FlushICache flush_icache) {
  std::unique_ptr<byte[]> reloc_info;
  if (original_code->reloc_info().size() > 0) {
    reloc_info.reset(new byte[original_code->reloc_info().size()]);
    memcpy(reloc_info.get(), original_code->reloc_info().start(),
           original_code->reloc_info().size());
  }
  std::unique_ptr<byte[]> source_pos;
  if (original_code->source_positions().size() > 0) {
    source_pos.reset(new byte[original_code->source_positions().size()]);
    memcpy(source_pos.get(), original_code->source_positions().start(),
           original_code->source_positions().size());
  }
  WasmCode* ret = AddOwnedCode(
      original_code->instructions(), std::move(reloc_info),
      original_code->reloc_info().size(), std::move(source_pos),
      original_code->source_positions().size(), original_code->index_,
      original_code->kind(), original_code->constant_pool_offset_,
      original_code->stack_slots(), original_code->safepoint_table_offset_,
      original_code->handler_table_offset_,
      original_code->protected_instructions_, original_code->tier(),
      flush_icache);
  if (!ret->IsAnonymous()) {
    code_table_[ret->index()] = ret;
  }
  return ret;
}

void NativeModule::UnpackAndRegisterProtectedInstructions() {
  for (uint32_t i = num_imported_functions(), e = FunctionCount(); i < e; ++i) {
    WasmCode* code = GetCode(i);

    if (code == nullptr) continue;
    if (code->kind() != wasm::WasmCode::kFunction) continue;
    if (code->HasTrapHandlerIndex()) continue;

    Address base = code->instructions().start();

    size_t size = code->instructions().size();
    const int index =
        RegisterHandlerData(base, size, code->protected_instructions().size(),
                            code->protected_instructions().data());

    // TODO(eholk): if index is negative, fail.
    CHECK_LE(0, index);
    code->set_trap_handler_index(static_cast<size_t>(index));
  }
}

void NativeModule::ReleaseProtectedInstructions() {
  for (uint32_t i = num_imported_functions(), e = FunctionCount(); i < e; ++i) {
    WasmCode* wasm_code = GetCode(i);
    if (wasm_code->HasTrapHandlerIndex()) {
      CHECK_LT(wasm_code->trap_handler_index(),
               static_cast<size_t>(std::numeric_limits<int>::max()));
      trap_handler::ReleaseHandlerData(
          static_cast<int>(wasm_code->trap_handler_index()));
      wasm_code->ResetTrapHandlerIndex();
    }
  }
}

NativeModule::~NativeModule() {
  TRACE_HEAP("Deleting native module: %p\n", reinterpret_cast<void*>(this));
  // Clear the handle at the beginning of destructor to make it robust against
  // potential GCs in the rest of the desctructor.
  if (compiled_module_ != nullptr) {
    Isolate* isolate = compiled_module()->GetIsolate();
    isolate->global_handles()->Destroy(
        reinterpret_cast<Object**>(compiled_module_));
    compiled_module_ = nullptr;
  }
  wasm_code_manager_->FreeNativeModuleMemories(this);
}

WasmCodeManager::WasmCodeManager(v8::Isolate* isolate, size_t max_committed)
    : isolate_(isolate) {
  DCHECK_LE(max_committed, kMaxWasmCodeMemory);
  remaining_uncommitted_.SetValue(max_committed);
}

bool WasmCodeManager::Commit(Address start, size_t size) {
  DCHECK(IsAligned(reinterpret_cast<size_t>(start), AllocatePageSize()));
  DCHECK(IsAligned(size, AllocatePageSize()));
  if (size > static_cast<size_t>(std::numeric_limits<intptr_t>::max())) {
    return false;
  }
  // reserve the size.
  intptr_t new_value = remaining_uncommitted_.Decrement(size);
  if (new_value < 0) {
    remaining_uncommitted_.Increment(size);
    return false;
  }
  PageAllocator::Permission permission = FLAG_wasm_write_protect_code_memory
                                             ? PageAllocator::kReadWrite
                                             : PageAllocator::kReadWriteExecute;

  bool ret = SetPermissions(start, size, permission);
  TRACE_HEAP("Setting rw permissions for %p:%p\n",
             reinterpret_cast<void*>(start),
             reinterpret_cast<void*>(start + size));
  if (!ret) {
    // Highly unlikely.
    remaining_uncommitted_.Increment(size);
    return false;
  }
  // This API assumes main thread
  isolate_->AdjustAmountOfExternalAllocatedMemory(size);
  if (WouldGCHelp()) {
    // This API does not assume main thread, and would schedule
    // a GC if called from a different thread, instead of synchronously
    // doing one.
    isolate_->MemoryPressureNotification(MemoryPressureLevel::kCritical);
  }
  return ret;
}

bool WasmCodeManager::WouldGCHelp() const {
  // If all we have is one module, or none, no GC would help.
  // GC would help if there's some remaining native modules that
  // would be collected.
  if (active_ <= 1) return false;
  // We have an expectation on the largest size a native function
  // may have.
  constexpr size_t kMaxNativeFunction = 32 * MB;
  intptr_t remaining = remaining_uncommitted_.Value();
  DCHECK_GE(remaining, 0);
  return static_cast<size_t>(remaining) < kMaxNativeFunction;
}

void WasmCodeManager::AssignRanges(void* start, void* end,
                                   NativeModule* native_module) {
  lookup_map_.insert(std::make_pair(
      reinterpret_cast<Address>(start),
      std::make_pair(reinterpret_cast<Address>(end), native_module)));
}

void WasmCodeManager::TryAllocate(size_t size, VirtualMemory* ret, void* hint) {
  DCHECK_GT(size, 0);
  size = RoundUp(size, AllocatePageSize());
  if (hint == nullptr) hint = GetRandomMmapAddr();

  if (!AlignedAllocVirtualMemory(size, static_cast<size_t>(AllocatePageSize()),
                                 hint, ret)) {
    DCHECK(!ret->IsReserved());
  }
  TRACE_HEAP("VMem alloc: %p:%p (%zu)\n", ret->address(), ret->end(),
             ret->size());
}

size_t WasmCodeManager::GetAllocationChunk(const WasmModule& module) {
  // TODO(mtrofin): this should pick up its 'maximal code range size'
  // from something embedder-provided
  if (kRequiresCodeRange) return kMaxWasmCodeMemory;
  DCHECK(kModuleCanAllocateMoreMemory);
  size_t ret = AllocatePageSize();
  // a ballpark guesstimate on native inflation factor.
  constexpr size_t kMultiplier = 4;

  for (auto& function : module.functions) {
    ret += kMultiplier * function.code.length();
  }
  return ret;
}

std::unique_ptr<NativeModule> WasmCodeManager::NewNativeModule(
    const WasmModule& module) {
  size_t code_size = GetAllocationChunk(module);
  return NewNativeModule(
      code_size, static_cast<uint32_t>(module.functions.size()),
      module.num_imported_functions, kModuleCanAllocateMoreMemory);
}

std::unique_ptr<NativeModule> WasmCodeManager::NewNativeModule(
    size_t size_estimate, uint32_t num_functions,
    uint32_t num_imported_functions, bool can_request_more) {
  VirtualMemory mem;
  TryAllocate(size_estimate, &mem);
  if (mem.IsReserved()) {
    void* start = mem.address();
    size_t size = mem.size();
    void* end = mem.end();
    std::unique_ptr<NativeModule> ret(new NativeModule(
        num_functions, num_imported_functions, can_request_more, &mem, this));
    TRACE_HEAP("New Module: ID:%zu. Mem: %p,+%zu\n", ret->instance_id, start,
               size);
    AssignRanges(start, end, ret.get());
    ++active_;
    return ret;
  }

  V8::FatalProcessOutOfMemory(reinterpret_cast<Isolate*>(isolate_),
                              "WasmCodeManager::NewNativeModule");
  return nullptr;
}

bool NativeModule::SetExecutable(bool executable) {
  if (is_executable_ == executable) return true;
  TRACE_HEAP("Setting module %zu as executable: %d.\n", instance_id,
             executable);
  PageAllocator::Permission permission =
      executable ? PageAllocator::kReadExecute : PageAllocator::kReadWrite;

  if (FLAG_wasm_write_protect_code_memory) {
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
      for (auto& vmem : owned_memory_) {
        if (!SetPermissions(vmem.address(), vmem.size(), permission)) {
          return false;
        }
        TRACE_HEAP("Set %p:%p to executable:%d\n", vmem.address(), vmem.end(),
                   executable);
      }
      is_executable_ = executable;
      return true;
    }
#endif
    for (auto& range : allocated_memory_.ranges()) {
      // allocated_memory_ is fine-grained, so we need to
      // page-align it.
      size_t range_size = RoundUp(
          static_cast<size_t>(range.second - range.first), AllocatePageSize());
      if (!SetPermissions(range.first, range_size, permission)) {
        return false;
      }
      TRACE_HEAP("Set %p:%p to executable:%d\n",
                 reinterpret_cast<void*>(range.first),
                 reinterpret_cast<void*>(range.second), executable);
    }
  }
  is_executable_ = executable;
  return true;
}

std::unique_ptr<NativeModule> NativeModule::Clone() {
  std::unique_ptr<NativeModule> ret = wasm_code_manager_->NewNativeModule(
      owned_memory_.front().size(), FunctionCount(), num_imported_functions(),
      can_request_more_memory_);
  TRACE_HEAP("%zu cloned from %zu\n", ret->instance_id, instance_id);
  if (!ret) return ret;

  // Clone trampolines and stubs. They are later patched, so no icache flush
  // needed yet.
  ret->CloneTrampolinesAndStubs(this, WasmCode::kNoFlushICache);

  // Create a helper for cloning and patching code.
  CloneCodeHelper helper(this, ret.get());
  for (uint32_t i = num_imported_functions(), e = FunctionCount(); i < e; ++i) {
    helper.SelectForCloning(i);
  }
  helper.CloneAndPatchCode(true);

  return ret;
}

void WasmCodeManager::FreeNativeModuleMemories(NativeModule* native_module) {
  DCHECK_GE(active_, 1);
  --active_;
  TRACE_HEAP("Freeing %zu\n", native_module->instance_id);
  for (auto& vmem : native_module->owned_memory_) {
    lookup_map_.erase(reinterpret_cast<Address>(vmem.address()));
    Free(&vmem);
    DCHECK(!vmem.IsReserved());
  }
  native_module->owned_memory_.clear();

  // No need to tell the GC anything if we're destroying the heap,
  // which we currently indicate by having the isolate_ as null
  if (isolate_ == nullptr) return;
  size_t freed_mem = native_module->committed_memory_;
  DCHECK(IsAligned(freed_mem, AllocatePageSize()));
  remaining_uncommitted_.Increment(freed_mem);
  isolate_->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<int64_t>(freed_mem));
}

// TODO(wasm): We can make this more efficient if needed. For
// example, we can preface the first instruction with a pointer to
// the WasmCode. In the meantime, we have a separate API so we can
// easily identify those places where we know we have the first
// instruction PC.
WasmCode* WasmCodeManager::GetCodeFromStartAddress(Address pc) const {
  WasmCode* code = LookupCode(pc);
  // This method can only be called for valid instruction start addresses.
  DCHECK_NOT_NULL(code);
  DCHECK_EQ(pc, code->instructions().start());
  return code;
}

WasmCode* WasmCodeManager::LookupCode(Address pc) const {
  if (lookup_map_.empty()) return nullptr;

  auto iter = lookup_map_.upper_bound(pc);
  if (iter == lookup_map_.begin()) return nullptr;
  --iter;
  Address range_start = iter->first;
  Address range_end = iter->second.first;
  NativeModule* candidate = iter->second.second;

  DCHECK_NOT_NULL(candidate);
  if (range_start <= pc && pc < range_end) {
    return candidate->Lookup(pc);
  }
  return nullptr;
}

void WasmCodeManager::Free(VirtualMemory* mem) {
  DCHECK(mem->IsReserved());
  void* start = mem->address();
  void* end = mem->end();
  size_t size = mem->size();
  mem->Free();
  TRACE_HEAP("VMem Release: %p:%p (%zu)\n", start, end, size);
}

intptr_t WasmCodeManager::remaining_uncommitted() const {
  return remaining_uncommitted_.Value();
}

NativeModuleModificationScope::NativeModuleModificationScope(
    NativeModule* native_module)
    : native_module_(native_module) {
  if (native_module_ && (native_module_->modification_scope_depth_++) == 0) {
    bool success = native_module_->SetExecutable(false);
    CHECK(success);
  }
}

NativeModuleModificationScope::~NativeModuleModificationScope() {
  if (native_module_ && (native_module_->modification_scope_depth_--) == 1) {
    bool success = native_module_->SetExecutable(true);
    CHECK(success);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#undef TRACE_HEAP
