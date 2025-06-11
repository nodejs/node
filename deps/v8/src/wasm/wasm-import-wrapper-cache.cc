// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-import-wrapper-cache.h"

#include <vector>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/common/code-memory-access-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/std-object-sizes.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-code-pointer-table-inl.h"
#include "src/wasm/wasm-engine.h"

namespace v8::internal::wasm {

WasmImportWrapperHandle::WasmImportWrapperHandle(WasmCode* code, Address addr,
                                                 uint64_t signature_hash)
    : code_pointer_(
          GetProcessWideWasmCodePointerTable()->AllocateAndInitializeEntry(
              addr, signature_hash)),
      code_(code) {
  if (code) code->IncRef();
}

WasmImportWrapperHandle::WasmImportWrapperHandle(Address addr,
                                                 uint64_t signature_hash)
    : code_pointer_(
          GetProcessWideWasmCodePointerTable()->AllocateAndInitializeEntry(
              addr, signature_hash)) {}

WasmImportWrapperHandle::~WasmImportWrapperHandle() {
  WasmCode* wasm_code = code_.load(std::memory_order_relaxed);
  if (wasm_code) {
    bool should_free = wasm_code->DecRef();
    USE(should_free);
    DCHECK(!should_free);
  }
  GetProcessWideWasmCodePointerTable()->FreeEntry(code_pointer_);
}

void WasmImportWrapperHandle::set_code(WasmCode* code) {
  GetWasmImportWrapperCache()->mutex_.AssertHeld();
  // We're taking ownership of a WasmCode object that has just been allocated
  // and should have a refcount of 1.
  code->DcheckRefCountIsOne();
  DCHECK(!has_code());
  code_.store(code, std::memory_order_relaxed);
}

// The wrapper cache is shared per-process; but it is initialized on demand, and
// this action is triggered by some isolate; so we use this isolate for error
// reporting and running GCs if required.
void WasmImportWrapperCache::LazyInitialize(Isolate* triggering_isolate) {
  base::MutexGuard lock(&mutex_);
  if (code_allocator_.get() != nullptr) return;  // Already initialized.
  // Most wrappers are small (200-300 bytes), most modules don't need many.
  // 32K is enough for ~100 wrappers.
  static constexpr size_t kInitialReservationSize = 1 << 15;
  // See {NewNativeModule} for reasoning.
  static constexpr int kAllocationRetries = 2;
  VirtualMemory code_space;
  for (int retries = 0;; ++retries) {
    code_space = GetWasmCodeManager()->TryAllocate(kInitialReservationSize);
    if (code_space.IsReserved()) break;
    if (retries == kAllocationRetries) {
      V8::FatalProcessOutOfMemory(
          triggering_isolate,
          "Failed to allocate code space for import wrappers");
      UNREACHABLE();
    }
    triggering_isolate->heap()->MemoryPressureNotification(
        MemoryPressureLevel::kCritical, true);
  }
  code_allocator_.reset(
      new WasmCodeAllocator(triggering_isolate->async_counters()));
  base::AddressRegion initial_region = code_space.region();
  code_allocator_->Init(std::move(code_space));
  code_allocator_->InitializeCodeRange(nullptr, initial_region);
}

WasmCode* WasmImportWrapperCache::ModificationScope::AddWrapper(
    WasmCompilationResult result, WasmCode::Kind kind, uint64_t signature_hash,
    std::shared_ptr<wasm::WasmImportWrapperHandle> wrapper_handle) {
  cache_->mutex_.AssertHeld();
  DCHECK_NOT_NULL(wrapper_handle);
  // Equivalent of NativeModule::AddCode().
  const CodeDesc& desc = result.code_desc;
  base::Vector<uint8_t> code_space =
      cache_->code_allocator_->AllocateForWrapper(desc.instr_size);

  // Equivalent of NativeModule::AddCodeWithCodeSpace().
  base::Vector<uint8_t> reloc_info{
      desc.buffer + desc.buffer_size - desc.reloc_size,
      static_cast<size_t>(desc.reloc_size)};
  // Contrary to the NativeModule, we don't track code size here, because we
  // have no source to attribute it to.
  const int safepoint_table_offset =
      desc.safepoint_table_size == 0 ? 0 : desc.safepoint_table_offset;
  const int handler_table_offset = desc.handler_table_offset;
  const int constant_pool_offset = desc.constant_pool_offset;
  const int code_comments_offset = desc.code_comments_offset;
  const int jump_table_info_offset = desc.jump_table_info_offset;
  const int instr_size = desc.instr_size;
  {
    WritableJitAllocation jit_allocation =
        ThreadIsolation::RegisterJitAllocation(
            reinterpret_cast<Address>(code_space.begin()), code_space.size(),
            ThreadIsolation::JitAllocationType::kWasmCode, true);
    jit_allocation.CopyCode(0, desc.buffer, desc.instr_size);

    intptr_t delta = code_space.begin() - desc.buffer;
    Address code_start = reinterpret_cast<Address>(code_space.begin());
    Address constant_pool_start = code_start + constant_pool_offset;
    for (WritableRelocIterator it(jit_allocation, code_space, reloc_info,
                                  constant_pool_start, RelocInfo::kApplyMask);
         !it.done(); it.next()) {
      // Wrappers should contain no direct calls to Wasm functions.
      DCHECK(!RelocInfo::IsWasmCall(it.rinfo()->rmode()));
      // Wrappers should not call builtins via a Wasm jump table.
      DCHECK(!RelocInfo::IsWasmStubCall(it.rinfo()->rmode()));
      it.rinfo()->apply(delta);
    }
    jit_allocation.UpdateWasmCodePointer(wrapper_handle->code_pointer(),
                                         signature_hash);
  }
  FlushInstructionCache(code_space.begin(), code_space.size());
  const int frame_slot_count = result.frame_slot_count;
  const int ool_spill_count = result.ool_spill_count;
  constexpr bool frame_has_feedback_slot = false;
  WasmCode* code = new WasmCode{nullptr /* no NativeModule */,
                                kAnonymousFuncIndex,
                                code_space,
                                frame_slot_count,
                                ool_spill_count,
                                result.tagged_parameter_slots,
                                safepoint_table_offset,
                                handler_table_offset,
                                constant_pool_offset,
                                code_comments_offset,
                                jump_table_info_offset,
                                instr_size,
                                result.protected_instructions_data.as_vector(),
                                reloc_info,
                                result.source_positions.as_vector(),
                                result.inlining_positions.as_vector(),
                                result.deopt_data.as_vector(),
                                kind,
                                ExecutionTier::kNone,
                                wasm::kNotForDebugging,
                                signature_hash,
                                frame_has_feedback_slot};

  code->Validate();

  wrapper_handle->set_code(code);

  // As an optimization, we assume that wrappers are allocated in increasing
  // memory locations.
  std::map<Address, WasmCode*>& codes = cache_->codes_;
  codes.emplace_hint(codes.end(), code->instruction_start(), code);
  return code;
}

void WasmImportWrapperCache::LogForIsolate(Isolate* isolate) {
  base::MutexGuard lock(&mutex_);
  for (const auto& entry : codes_) {
    entry.second->LogCode(isolate, "", -1);  // No source URL, no ScriptId.
  }
}

void WasmImportWrapperCache::Free(std::vector<WasmCode*>& wrappers) {
  base::MutexGuard lock(&mutex_);
  if (codes_.empty() || wrappers.empty()) return;
  // {WasmCodeAllocator::FreeCode()} wants code objects to be sorted.
  std::sort(wrappers.begin(), wrappers.end(), [](WasmCode* a, WasmCode* b) {
    return a->instruction_start() < b->instruction_start();
  });
  for (WasmCode* wrapper : wrappers) {
    codes_.erase(wrapper->instruction_start());
  }
  for (auto it = entry_map_.begin(); it != entry_map_.end();) {
    if (it->second.expired()) {
      it = entry_map_.erase(it);
    } else {
      it++;
    }
  }
  code_allocator_->FreeCode(base::VectorOf(wrappers));
  for (WasmCode* wrapper : wrappers) {
    // TODO(407003348): Drop this check if it doesn't trigger in the wild.
    CHECK_EQ(wrapper->ref_count_bitfield_.load(std::memory_order_acquire),
             WasmCode::kIsDyingMask);
    delete wrapper;
  }
  // Make sure nobody tries to access stale pointers.
  wrappers.clear();
}

std::optional<Builtin> WasmImportWrapperCache::BuiltinForWrapper(
    ImportCallKind kind, const wasm::CanonicalSig* sig, Suspend suspend) {
  if (kind != ImportCallKind::kJSFunction) return {};

  if (!IsJSCompatibleSignature(sig)) {
    return Builtin::kWasmToJsWrapperInvalidSig;
  }

  if (UseGenericWasmToJSWrapper(kind, sig, suspend)) {
    return Builtin::kWasmToJsWrapperAsm;
  }

  return {};
}

std::shared_ptr<WasmImportWrapperHandle> WasmImportWrapperCache::Get(
    Isolate* isolate, ImportCallKind kind, CanonicalTypeIndex type_index,
    int expected_arity, Suspend suspend, const wasm::CanonicalSig* sig) {
  CacheKey cache_key(kind, type_index, expected_arity, suspend);

  {
    base::MutexGuard lock(&mutex_);

    // If we have an entry already, just return that.
    auto it = entry_map_.find(cache_key);
    if (it != entry_map_.end()) {
      std::shared_ptr<WasmImportWrapperHandle> handle = it->second.lock();
      if (handle) {
        return handle;
      }
    }

    // Check if we should use a builtin.
    std::optional<Builtin> builtin = BuiltinForWrapper(kind, sig, suspend);
    if (builtin) {
      Address call_target = Builtins::EmbeddedEntryOf(*builtin);
      static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
      std::shared_ptr<WasmImportWrapperHandle> wrapper_handle =
          std::make_shared<WasmImportWrapperHandle>(call_target,
                                                    sig->signature_hash());
      entry_map_[cache_key] = wrapper_handle;
      return wrapper_handle;
    }
  }

  // Fall back to compile a new wrapper.
  return CompileWrapper(isolate, cache_key, sig, nullptr);
}

#ifdef DEBUG
bool WasmImportWrapperCache::IsCompiledWrapper(WasmCodePointer code_pointer) {
  base::MutexGuard lock(&mutex_);
  for (auto& [cache_key, weak_handle] : entry_map_) {
    std::shared_ptr<WasmImportWrapperHandle> handle = weak_handle.lock();
    if (!handle) continue;

    if (handle->code_pointer() == code_pointer) {
      return handle->has_code();
    }
  }
  UNREACHABLE();
}
#endif

bool WasmImportWrapperCache::HasCodeForTesting(ImportCallKind kind,
                                               CanonicalTypeIndex type_index,
                                               int expected_arity,
                                               Suspend suspend) {
  base::MutexGuard lock(&mutex_);

  CacheKey cache_key(kind, type_index, expected_arity, suspend);
  auto it = entry_map_.find(cache_key);
  if (it == entry_map_.end()) return false;
  auto handle = it->second.lock();
  if (!handle) return false;
  return handle->has_code();
}

std::shared_ptr<WasmImportWrapperHandle> WasmImportWrapperCache::GetCompiled(
    Isolate* isolate, ImportCallKind kind, CanonicalTypeIndex type_index,
    int expected_arity, Suspend suspend, const CanonicalSig* sig) {
  CHECK_NE(kind, ImportCallKind::kWasmToJSFastApi);
  DCHECK(!v8_flags.wasm_jitless);

  CacheKey cache_key{kind, type_index, expected_arity, suspend};

  std::shared_ptr<wasm::WasmImportWrapperHandle> wrapper_handle;
  {
    base::MutexGuard lock(&mutex_);

    auto it = entry_map_.find(cache_key);
    if (it != entry_map_.end()) {
      wrapper_handle = it->second.lock();
      if (wrapper_handle && wrapper_handle->has_code()) {
        return wrapper_handle;
      }
    }
  }

  return CompileWrapper(isolate, cache_key, sig, wrapper_handle);
}

std::shared_ptr<WasmImportWrapperHandle> WasmImportWrapperCache::CompileWrapper(
    Isolate* isolate, const CacheKey& cache_key, const CanonicalSig* sig,
    std::shared_ptr<WasmImportWrapperHandle> wrapper_handle) {
  wasm::WasmCompilationResult result;
  wasm::WasmCode::Kind code_kind;
  switch (cache_key.kind) {
    case ImportCallKind::kLinkError:
    case ImportCallKind::kWasmToWasm:
    case ImportCallKind::kWasmToJSFastApi:
      UNREACHABLE();
    case ImportCallKind::kJSFunction:
    case ImportCallKind::kUseCallBuiltin:
    case ImportCallKind::kRuntimeTypeError:
      result = compiler::CompileWasmImportCallWrapper(
          cache_key.kind, sig, cache_key.expected_arity, cache_key.suspend);
      code_kind = WasmCode::Kind::kWasmToJsWrapper;
      break;
    case ImportCallKind::kWasmToCapi:
      result = compiler::CompileWasmCapiCallWrapper(sig);
      code_kind = wasm::WasmCode::Kind::kWasmToCapiWrapper;
      break;
  }

  WasmCode* wasm_code;
  {
    wasm::WasmImportWrapperCache::ModificationScope cache_scope(this);

    if (!wrapper_handle) {
      auto it = entry_map_.find(cache_key);
      if (it != entry_map_.end()) {
        wrapper_handle = it->second.lock();
      }
    }

    if (wrapper_handle && wrapper_handle->has_code()) {
      // There was a race and we have code now.
      return wrapper_handle;
    }

    if (!wrapper_handle) {
      wrapper_handle = std::make_shared<WasmImportWrapperHandle>(
          kNullAddress, sig->signature_hash());
      entry_map_[cache_key] = wrapper_handle;
    }

    wasm_code = cache_scope.AddWrapper(std::move(result), code_kind,
                                       sig->signature_hash(), wrapper_handle);
  }

  // To avoid lock order inversion, code printing must happen after the
  // end of the {cache_scope}.
  wasm_code->MaybePrint();
  if (isolate) {
    isolate->counters()->wasm_generated_code_size()->Increment(
        wasm_code->instructions().length());
    isolate->counters()->wasm_reloc_size()->Increment(
        wasm_code->reloc_info().length());
  }
  if (GetWasmEngine()->LogWrapperCode(wasm_code) && isolate) {
    // Log the code immediately in the current isolate.
    GetWasmEngine()->LogOutstandingCodesForIsolate(isolate);
  }

  return wrapper_handle;
}

std::shared_ptr<WasmImportWrapperHandle>
WasmImportWrapperCache::CompileWasmJsFastCallWrapper(
    Isolate* isolate, DirectHandle<JSReceiver> callable,
    const wasm::CanonicalSig* sig) {
  // Note: the wrapper we're about to compile is specific to this
  // instantiation, so it cannot be shared.
  WasmCompilationResult result =
      compiler::CompileWasmJSFastCallWrapper(sig, callable);

  std::shared_ptr<WasmImportWrapperHandle> wrapper_handle =
      std::make_shared<WasmImportWrapperHandle>(kNullAddress,
                                                sig->signature_hash());
  WasmCode* wasm_code;
  {
    ModificationScope cache_scope(this);
    wasm_code = cache_scope.AddWrapper(std::move(result),
                                       WasmCode::Kind::kWasmToJsWrapper,
                                       sig->signature_hash(), wrapper_handle);
  }
  // To avoid lock order inversion, code printing must happen after the
  // end of the {cache_scope}.
  wasm_code->MaybePrint();
  isolate->counters()->wasm_generated_code_size()->Increment(
      wasm_code->instructions().length());
  isolate->counters()->wasm_reloc_size()->Increment(
      wasm_code->reloc_info().length());
  if (GetWasmEngine()->LogWrapperCode(wasm_code)) {
    // Log the code immediately in the current isolate.
    GetWasmEngine()->LogOutstandingCodesForIsolate(isolate);
  }

  return wrapper_handle;
}

WasmCode* WasmImportWrapperCache::Lookup(Address pc) const {
  // This can be called from the disassembler via `code->MaybePrint()` in
  // `AddWrapper()` above, so we need a recursive mutex.
  base::MutexGuard lock(&mutex_);
  auto iter = codes_.upper_bound(pc);
  if (iter == codes_.begin()) return nullptr;
  --iter;
  WasmCode* candidate = iter->second;
  DCHECK_EQ(candidate->instruction_start(), iter->first);
  if (!candidate->contains(pc)) return nullptr;
  WasmCodeRefScope::AddRef(candidate);
  // Note: this function is used for iterating the stack, where dying
  // code objects can still have their last few activations, so we
  // must return {candidate} even if {candidate->is_dying()}.
  return candidate;
}

size_t WasmImportWrapperCache::EstimateCurrentMemoryConsumption() const {
  UPDATE_WHEN_CLASS_CHANGES(WasmImportWrapperCache, 88);
  base::MutexGuard lock(&mutex_);
  return sizeof(WasmImportWrapperCache) + ContentSize(entry_map_) +
         ContentSize(codes_);
}

}  // namespace v8::internal::wasm
