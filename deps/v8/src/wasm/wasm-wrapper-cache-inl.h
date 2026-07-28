// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_WRAPPER_CACHE_INL_H_
#define V8_WASM_WASM_WRAPPER_CACHE_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/wasm/wasm-wrapper-cache.h"
// Include the non-inl header before the rest of the headers.

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

struct ImportWrapperCacheKey;

template <typename CacheKey>
WasmCode* WasmWrapperCache<CacheKey>::ModificationScope::AddWrapper(
    WasmCompilationResult result, WasmCode::Kind kind, uint64_t signature_hash,
    std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle) {
  cache_->mutex_.AssertHeld();
  DCHECK_NOT_NULL(wrapper_handle);
  // Equivalent of NativeModule::AddCode().
  const CodeDesc& desc = result.code_desc;
  base::Vector<uint8_t> code_space =
      cache_->code_allocator_.AllocateForWrapper(desc.instr_size);

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
                                {},  // effect handlers
                                frame_has_feedback_slot};

  code->Validate();

  wrapper_handle->set_code(code);

  // As an optimization, we assume that wrappers are allocated in increasing
  // memory locations.
  std::map<Address, WasmCode*>& codes = cache_->codes_;
  codes.emplace_hint(codes.end(), code->instruction_start(), code);
  return code;
}

template <typename CacheKey>
void WasmWrapperCache<CacheKey>::LogForIsolate(Isolate* isolate) {
  base::MutexGuard lock(&mutex_);
  for (const auto& entry : codes_) {
    entry.second->LogCode(isolate, "", -1);  // No source URL, no ScriptId.
  }
}

template <typename CacheKey>
void WasmWrapperCache<CacheKey>::Free(std::vector<WasmCode*>& wrappers) {
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
  code_allocator_.FreeCode(base::VectorOf(wrappers));
  for (WasmCode* wrapper : wrappers) {
    // TODO(407003348): Drop this check if it doesn't trigger in the wild.
    CHECK_EQ(wrapper->ref_count_bitfield_.load(std::memory_order_acquire),
             WasmCode::kIsDyingMask);
    delete wrapper;
  }
  // Make sure nobody tries to access stale pointers.
  wrappers.clear();
}

template <typename CacheKey>
V8_EXPORT_PRIVATE std::shared_ptr<WasmWrapperHandle>
WasmWrapperCache<CacheKey>::GetCompiled(Isolate* isolate, CacheKey cache_key) {
  if constexpr (std::is_same_v<CacheKey, ImportWrapperCacheKey>) {
    CHECK_NE(cache_key.kind, ImportCallKind::kWasmToJSFastApi);
  }
  DCHECK(!v8_flags.wasm_jitless);

  std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle;
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

  auto [result, code_kind] = CompileWrapper(isolate, cache_key);
  return CacheCompiledWrapper(isolate, std::move(result), code_kind, cache_key,
                              wrapper_handle);
}

template <typename CacheKey>
std::shared_ptr<WasmWrapperHandle>
WasmWrapperCache<CacheKey>::CacheCompiledWrapper(
    Isolate* isolate, WasmCompilationResult result, WasmCode::Kind code_kind,
    CacheKey cache_key, std::shared_ptr<WasmWrapperHandle> wrapper_handle) {
  WasmCode* wasm_code;
  {
    wasm::WasmWrapperCache<CacheKey>::ModificationScope cache_scope(this);

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
      wrapper_handle = std::make_shared<WasmWrapperHandle>(
          kNullAddress, cache_key.sig->signature_hash());
      entry_map_[cache_key] = wrapper_handle;
    }

    wasm_code =
        cache_scope.AddWrapper(std::move(result), code_kind,
                               cache_key.sig->signature_hash(), wrapper_handle);
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

template <typename CacheKey>
WasmCode* WasmWrapperCache<CacheKey>::Lookup(Address pc) const {
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

template <typename CacheKey>
size_t WasmWrapperCache<CacheKey>::EstimateCurrentMemoryConsumption() const {
  UPDATE_WHEN_CLASS_CHANGES(WasmWrapperCache<CacheKey>, 240);
  base::MutexGuard lock(&mutex_);
  return sizeof(WasmWrapperCache) + ContentSize(entry_map_) +
         ContentSize(codes_) +
         (counter_updates_.EstimateCurrentMemoryConsumption() -
          sizeof(counter_updates_));
}

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_WRAPPER_CACHE_INL_H_
