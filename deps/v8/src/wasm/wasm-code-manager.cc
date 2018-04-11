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
size_t native_module_ids = 0;

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

void PatchTrampolineAndStubCalls(
    const WasmCode* original_code, const WasmCode* new_code,
    const std::unordered_map<Address, Address, AddressHasher>& reverse_lookup) {
  RelocIterator orig_it(
      original_code->instructions(), original_code->reloc_info(),
      original_code->constant_pool(), RelocInfo::kCodeTargetMask);
  for (RelocIterator it(new_code->instructions(), new_code->reloc_info(),
                        new_code->constant_pool(), RelocInfo::kCodeTargetMask);
       !it.done(); it.next(), orig_it.next()) {
    Address old_target = orig_it.rinfo()->target_address();
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X
    auto found = reverse_lookup.find(old_target);
    DCHECK(found != reverse_lookup.end());
    Address new_target = found->second;
#else
    Address new_target = old_target;
#endif
    it.rinfo()->set_target_address(nullptr, new_target, SKIP_WRITE_BARRIER,
                                   SKIP_ICACHE_FLUSH);
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

  size_t instruction_size =
      std::min(constant_pool_offset_, safepoint_table_offset_);
  os << "Instructions (size = " << instruction_size << ")\n";
  // TODO(mtrofin): rework the dependency on isolate and code in
  // Disassembler::Decode.
  Disassembler::Decode(isolate, &os, instructions().start(),
                       instructions().start() + instruction_size, nullptr);
  os << "\n";

  Object* source_positions_or_undef =
      owner_->compiled_module()->source_positions()->get(index());
  if (!source_positions_or_undef->IsUndefined(isolate)) {
    os << "Source positions:\n pc offset  position\n";
    for (SourcePositionTableIterator it(
             ByteArray::cast(source_positions_or_undef));
         !it.done(); it.Advance()) {
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
    case WasmCode::kWasmToWasmWrapper:
      return "wasm-to-wasm";
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

NativeModule::NativeModule(uint32_t num_functions, uint32_t num_imports,
                           bool can_request_more, VirtualMemory* mem,
                           WasmCodeManager* code_manager)
    : instance_id(native_module_ids++),
      code_table_(num_functions),
      num_imported_functions_(num_imports),
      free_memory_(reinterpret_cast<Address>(mem->address()),
                   reinterpret_cast<Address>(mem->end())),
      wasm_code_manager_(code_manager),
      can_request_more_memory_(can_request_more) {
  VirtualMemory my_mem;
  owned_memory_.push_back(my_mem);
  owned_memory_.back().TakeControl(mem);
  owned_code_.reserve(num_functions);
}

void NativeModule::ResizeCodeTableForTest(size_t last_index) {
  size_t new_size = last_index + 1;
  if (new_size > FunctionCount()) {
    Isolate* isolate = compiled_module()->GetIsolate();
    code_table_.resize(new_size);
    int grow_by = static_cast<int>(new_size) -
                  compiled_module()->source_positions()->length();
    Handle<FixedArray> source_positions(compiled_module()->source_positions(),
                                        isolate);
    source_positions = isolate->factory()->CopyFixedArrayAndGrow(
        source_positions, grow_by, TENURED);
    compiled_module()->set_source_positions(*source_positions);
    Handle<FixedArray> handler_table(compiled_module()->handler_table(),
                                     isolate);
    handler_table = isolate->factory()->CopyFixedArrayAndGrow(handler_table,
                                                              grow_by, TENURED);
    compiled_module()->set_handler_table(*handler_table);
  }
}

WasmCode* NativeModule::GetCode(uint32_t index) const {
  return code_table_[index];
}

uint32_t NativeModule::FunctionCount() const {
  DCHECK_LE(code_table_.size(), std::numeric_limits<uint32_t>::max());
  return static_cast<uint32_t>(code_table_.size());
}

WasmCode* NativeModule::AddOwnedCode(
    Vector<const byte> orig_instructions,
    std::unique_ptr<const byte[]> reloc_info, size_t reloc_size,
    Maybe<uint32_t> index, WasmCode::Kind kind, size_t constant_pool_offset,
    uint32_t stack_slots, size_t safepoint_table_offset,
    std::shared_ptr<ProtectedInstructions> protected_instructions,
    bool is_liftoff) {
  // both allocation and insertion in owned_code_ happen in the same critical
  // section, thus ensuring owned_code_'s elements are rarely if ever moved.
  base::LockGuard<base::Mutex> lock(&allocation_mutex_);
  Address executable_buffer = AllocateForCode(orig_instructions.size());
  if (executable_buffer == nullptr) return nullptr;
  memcpy(executable_buffer, orig_instructions.start(),
         orig_instructions.size());
  std::unique_ptr<WasmCode> code(new WasmCode(
      {executable_buffer, orig_instructions.size()}, std::move(reloc_info),
      reloc_size, this, index, kind, constant_pool_offset, stack_slots,
      safepoint_table_offset, std::move(protected_instructions), is_liftoff));
  WasmCode* ret = code.get();

  // TODO(mtrofin): We allocate in increasing address order, and
  // even if we end up with segmented memory, we may end up only with a few
  // large moves - if, for example, a new segment is below the current ones.
  auto insert_before = std::upper_bound(owned_code_.begin(), owned_code_.end(),
                                        code, owned_code_comparer_);
  owned_code_.insert(insert_before, std::move(code));
  wasm_code_manager_->FlushICache(ret->instructions().start(),
                                  ret->instructions().size());

  return ret;
}

WasmCode* NativeModule::AddCodeCopy(Handle<Code> code, WasmCode::Kind kind,
                                    uint32_t index) {
  WasmCode* ret = AddAnonymousCode(code, kind);
  SetCodeTable(index, ret);
  ret->index_ = Just(index);
  compiled_module()->source_positions()->set(static_cast<int>(index),
                                             code->source_position_table());
  compiled_module()->handler_table()->set(static_cast<int>(index),
                                          code->handler_table());
  return ret;
}

WasmCode* NativeModule::AddInterpreterWrapper(Handle<Code> code,
                                              uint32_t index) {
  WasmCode* ret = AddAnonymousCode(code, WasmCode::kInterpreterStub);
  ret->index_ = Just(index);
  return ret;
}

WasmCode* NativeModule::SetLazyBuiltin(Handle<Code> code) {
  DCHECK_NULL(lazy_builtin_);
  lazy_builtin_ = AddAnonymousCode(code, WasmCode::kLazyStub);

  for (uint32_t i = num_imported_functions(), e = FunctionCount(); i < e; ++i) {
    SetCodeTable(i, lazy_builtin_);
  }

  return lazy_builtin_;
}

WasmCompiledModule* NativeModule::compiled_module() const {
  return *compiled_module_;
}

void NativeModule::SetCompiledModule(
    Handle<WasmCompiledModule> compiled_module) {
  DCHECK(compiled_module_.is_null());
  compiled_module_ = compiled_module;
}

WasmCode* NativeModule::AddAnonymousCode(Handle<Code> code,
                                         WasmCode::Kind kind) {
  std::unique_ptr<byte[]> reloc_info;
  if (code->relocation_size() > 0) {
    reloc_info.reset(new byte[code->relocation_size()]);
    memcpy(reloc_info.get(), code->relocation_start(), code->relocation_size());
  }
  WasmCode* ret = AddOwnedCode(
      {code->instruction_start(),
       static_cast<size_t>(code->instruction_size())},
      std::move(reloc_info), static_cast<size_t>(code->relocation_size()),
      Nothing<uint32_t>(), kind, code->constant_pool_offset(),
      (code->has_safepoint_info() ? code->stack_slots() : 0),
      (code->has_safepoint_info() ? code->safepoint_table_offset() : 0), {});
  if (ret == nullptr) return nullptr;
  intptr_t delta = ret->instructions().start() - code->instruction_start();
  int mask = RelocInfo::kApplyMask | RelocInfo::kCodeTargetMask |
             RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);

  RelocIterator orig_it(*code, mask);
  for (RelocIterator it(ret->instructions(), ret->reloc_info(),
                        ret->constant_pool(), mask);
       !it.done(); it.next(), orig_it.next()) {
    if (RelocInfo::IsCodeTarget(it.rinfo()->rmode())) {
      Code* call_target =
          Code::GetCodeFromTargetAddress(orig_it.rinfo()->target_address());
      it.rinfo()->set_target_address(nullptr,
                                     GetLocalAddressFor(handle(call_target)),
                                     SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else {
      if (RelocInfo::IsEmbeddedObject(it.rinfo()->rmode())) {
        DCHECK(Heap::IsImmovable(it.rinfo()->target_object()));
      } else {
        it.rinfo()->apply(delta);
      }
    }
  }
  return ret;
}

WasmCode* NativeModule::AddCode(
    const CodeDesc& desc, uint32_t frame_slots, uint32_t index,
    size_t safepoint_table_offset,
    std::unique_ptr<ProtectedInstructions> protected_instructions,
    bool is_liftoff) {
  std::unique_ptr<byte[]> reloc_info;
  if (desc.reloc_size) {
    reloc_info.reset(new byte[desc.reloc_size]);
    memcpy(reloc_info.get(), desc.buffer + desc.buffer_size - desc.reloc_size,
           desc.reloc_size);
  }
  TurboAssembler* origin = reinterpret_cast<TurboAssembler*>(desc.origin);
  WasmCode* ret = AddOwnedCode(
      {desc.buffer, static_cast<size_t>(desc.instr_size)},
      std::move(reloc_info), static_cast<size_t>(desc.reloc_size), Just(index),
      WasmCode::kFunction, desc.instr_size - desc.constant_pool_size,
      frame_slots, safepoint_table_offset, std::move(protected_instructions),
      is_liftoff);
  if (ret == nullptr) return nullptr;

  SetCodeTable(index, ret);
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
      DCHECK_EQ(*p, p->GetIsolate()->heap()->undefined_value());
      it.rinfo()->set_target_object(*p, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles to direct pointers to the first instruction in the
      // code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(nullptr, GetLocalAddressFor(handle(code)),
                                     SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(
          origin->isolate(), p, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else {
      intptr_t delta = ret->instructions().start() - desc.buffer;
      it.rinfo()->apply(delta);
    }
  }
  return ret;
}

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X
Address NativeModule::CreateTrampolineTo(Handle<Code> code) {
  MacroAssembler masm(code->GetIsolate(), nullptr, 0, CodeObjectRequired::kNo);
  Address dest = code->instruction_start();
  GenerateJumpTrampoline(&masm, dest);
  CodeDesc code_desc;
  masm.GetCode(nullptr, &code_desc);
  WasmCode* wasm_code = AddOwnedCode(
      {code_desc.buffer, static_cast<size_t>(code_desc.instr_size)}, nullptr, 0,
      Nothing<uint32_t>(), WasmCode::kTrampoline, 0, 0, 0, {});
  if (wasm_code == nullptr) return nullptr;
  Address ret = wasm_code->instructions().start();
  trampolines_.emplace(std::make_pair(dest, ret));
  return ret;
}
#else
Address NativeModule::CreateTrampolineTo(Handle<Code> code) {
  Address ret = code->instruction_start();
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
    Address index = code->instruction_start();
    auto trampoline_iter = trampolines_.find(index);
    if (trampoline_iter == trampolines_.end()) {
      return CreateTrampolineTo(code);
    } else {
      return trampoline_iter->second;
    }
  }
}

WasmCode* NativeModule::GetExportedWrapper(uint32_t index) {
  auto found = exported_wasm_to_wasm_wrappers_.find(index);
  if (found != exported_wasm_to_wasm_wrappers_.end()) {
    return found->second;
  }
  return nullptr;
}

WasmCode* NativeModule::AddExportedWrapper(Handle<Code> code, uint32_t index) {
  WasmCode* ret = AddAnonymousCode(code, WasmCode::kWasmToWasmWrapper);
  ret->index_ = Just(index);
  exported_wasm_to_wasm_wrappers_.insert(std::make_pair(index, ret));
  return ret;
}

void NativeModule::LinkAll() {
  for (uint32_t index = 0; index < code_table_.size(); ++index) {
    Link(index);
  }
}

void NativeModule::Link(uint32_t index) {
  WasmCode* code = code_table_[index];
  // skip imports
  if (!code) return;
  int mode_mask = RelocInfo::ModeMask(RelocInfo::WASM_CALL);
  for (RelocIterator it(code->instructions(), code->reloc_info(),
                        code->constant_pool(), mode_mask);
       !it.done(); it.next()) {
    uint32_t index = GetWasmCalleeTag(it.rinfo());
    const WasmCode* target = GetCode(index);
    if (target == nullptr) continue;
    Address target_addr = target->instructions().start();
    DCHECK_NOT_NULL(target);
    it.rinfo()->set_wasm_call_address(nullptr, target_addr,
                                      ICacheFlushMode::SKIP_ICACHE_FLUSH);
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
  // Make a fake WasmCode temp, to look into owned_code_
  std::unique_ptr<WasmCode> temp(new WasmCode(pc));
  auto iter = std::upper_bound(owned_code_.begin(), owned_code_.end(), temp,
                               owned_code_comparer_);
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

WasmCode* NativeModule::CloneLazyBuiltinInto(uint32_t index) {
  DCHECK_NOT_NULL(lazy_builtin());
  WasmCode* ret = CloneCode(lazy_builtin());
  SetCodeTable(index, ret);
  ret->index_ = Just(index);
  return ret;
}

bool NativeModule::CloneTrampolinesAndStubs(const NativeModule* other) {
  for (auto& pair : other->trampolines_) {
    Address key = pair.first;
    Address local =
        GetLocalAddressFor(handle(Code::GetCodeFromTargetAddress(key)));
    if (local == nullptr) return false;
    trampolines_.emplace(std::make_pair(key, local));
  }
  for (auto& pair : other->stubs_) {
    uint32_t key = pair.first;
    WasmCode* clone = CloneCode(pair.second);
    if (!clone) return false;
    stubs_.emplace(std::make_pair(key, clone));
  }
  return true;
}

WasmCode* NativeModule::CloneCode(const WasmCode* original_code) {
  std::unique_ptr<byte[]> reloc_info;
  if (original_code->reloc_info().size() > 0) {
    reloc_info.reset(new byte[original_code->reloc_info().size()]);
    memcpy(reloc_info.get(), original_code->reloc_info().start(),
           original_code->reloc_info().size());
  }
  WasmCode* ret = AddOwnedCode(
      original_code->instructions(), std::move(reloc_info),
      original_code->reloc_info().size(), original_code->index_,
      original_code->kind(), original_code->constant_pool_offset_,
      original_code->stack_slots(), original_code->safepoint_table_offset_,
      original_code->protected_instructions_);
  if (ret == nullptr) return nullptr;
  if (!ret->IsAnonymous()) {
    SetCodeTable(ret->index(), ret);
  }
  intptr_t delta =
      ret->instructions().start() - original_code->instructions().start();
  for (RelocIterator it(ret->instructions(), ret->reloc_info(),
                        ret->constant_pool(), RelocInfo::kApplyMask);
       !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  return ret;
}

void NativeModule::SetCodeTable(uint32_t index, wasm::WasmCode* code) {
  code_table_[index] = code;
}

NativeModule::~NativeModule() {
  TRACE_HEAP("Deleting native module: %p\n", reinterpret_cast<void*>(this));
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

  V8::FatalProcessOutOfMemory("WasmCodeManager::NewNativeModule");
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

  if (lazy_builtin() != nullptr) {
    ret->lazy_builtin_ = ret->CloneCode(lazy_builtin());
  }

  if (!ret->CloneTrampolinesAndStubs(this)) return nullptr;

  std::unordered_map<Address, Address, AddressHasher> reverse_lookup;
  for (auto& pair : trampolines_) {
    Address old_dest = pair.second;
    auto local = ret->trampolines_.find(pair.first);
    DCHECK(local != ret->trampolines_.end());
    Address new_dest = local->second;
    reverse_lookup.emplace(old_dest, new_dest);
  }

  for (auto& pair : stubs_) {
    Address old_dest = pair.second->instructions().start();
    auto local = ret->stubs_.find(pair.first);
    DCHECK(local != ret->stubs_.end());
    Address new_dest = local->second->instructions().start();
    reverse_lookup.emplace(old_dest, new_dest);
  }

  for (auto& pair : ret->stubs_) {
    WasmCode* new_stub = pair.second;
    WasmCode* old_stub = stubs_.find(pair.first)->second;
    PatchTrampolineAndStubCalls(old_stub, new_stub, reverse_lookup);
  }
  if (lazy_builtin_ != nullptr) {
    PatchTrampolineAndStubCalls(lazy_builtin_, ret->lazy_builtin_,
                                reverse_lookup);
  }

  for (uint32_t i = num_imported_functions(), e = FunctionCount(); i < e; ++i) {
    const WasmCode* original_code = GetCode(i);
    switch (original_code->kind()) {
      case WasmCode::kLazyStub: {
        if (original_code->IsAnonymous()) {
          ret->SetCodeTable(i, ret->lazy_builtin());
        } else {
          if (!ret->CloneLazyBuiltinInto(i)) return nullptr;
        }
      } break;
      case WasmCode::kFunction: {
        WasmCode* new_code = ret->CloneCode(original_code);
        if (new_code == nullptr) return nullptr;
        PatchTrampolineAndStubCalls(original_code, new_code, reverse_lookup);
      } break;
      default:
        UNREACHABLE();
    }
  }
  ret->specialization_data_ = specialization_data_;
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

void WasmCodeManager::FlushICache(Address start, size_t size) {
  Assembler::FlushICache(reinterpret_cast<internal::Isolate*>(isolate_), start,
                         size);
}

NativeModuleModificationScope::NativeModuleModificationScope(
    NativeModule* native_module)
    : native_module_(native_module) {
  if (native_module_) {
    bool success = native_module_->SetExecutable(false);
    CHECK(success);
  }
}

NativeModuleModificationScope::~NativeModuleModificationScope() {
  if (native_module_) {
    bool success = native_module_->SetExecutable(true);
    CHECK(success);
  }
}

// On Intel, call sites are encoded as a displacement. For linking
// and for serialization/deserialization, we want to store/retrieve
// a tag (the function index). On Intel, that means accessing the
// raw displacement. Everywhere else, that simply means accessing
// the target address.
void SetWasmCalleeTag(RelocInfo* rinfo, uint32_t tag) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  *(reinterpret_cast<uint32_t*>(rinfo->target_address_address())) = tag;
#else
  rinfo->set_target_address(nullptr, reinterpret_cast<Address>(tag),
                            SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
#endif
}

uint32_t GetWasmCalleeTag(RelocInfo* rinfo) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  return *(reinterpret_cast<uint32_t*>(rinfo->target_address_address()));
#else
  return static_cast<uint32_t>(
      reinterpret_cast<size_t>(rinfo->target_address()));
#endif
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#undef TRACE_HEAP
