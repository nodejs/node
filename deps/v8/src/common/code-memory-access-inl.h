// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_CODE_MEMORY_ACCESS_INL_H_
#define V8_COMMON_CODE_MEMORY_ACCESS_INL_H_

#include "src/common/code-memory-access.h"
#include "src/flags/flags.h"
#if V8_HAS_PKU_JIT_WRITE_PROTECT
#include "src/base/platform/memory-protection-key.h"
#endif
#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT
#include "src/base/platform/platform.h"
#endif

namespace v8 {
namespace internal {

RwxMemoryWriteScope::RwxMemoryWriteScope(const char* comment) {
  if (!v8_flags.jitless) {
    SetWritable();
  }
}

RwxMemoryWriteScope::~RwxMemoryWriteScope() {
  if (!v8_flags.jitless) {
    SetExecutable();
  }
}

ThreadIsolation::WritableJitAllocation::WritableJitAllocation(
    Address addr, size_t size, JitAllocationType type,
    JitAllocationSource source)
    : address_(addr),
      // The order of these is important. We need to create the write scope
      // before we lookup the Jit page, since the latter will take a mutex in
      // protected memory.
      write_scope_("WritableJitAllocation"),
      page_ref_(ThreadIsolation::LookupJitPage(addr, size)),
      allocation_(source == JitAllocationSource::kRegister
                      ? page_ref_.RegisterAllocation(addr, size, type)
                      : page_ref_.LookupAllocation(addr, size, type)) {}

template <typename T, size_t offset>
void ThreadIsolation::WritableJitAllocation::WriteHeaderSlot(T value) {
  // These asserts are no strict requirements, they just guard against
  // non-implemented functionality.
  static_assert(!std::is_convertible_v<T, Object>);
  static_assert(offset != HeapObject::kMapOffset);

  WriteMaybeUnalignedValue<T>(address_ + offset, value);
}

template <typename T, size_t offset>
void ThreadIsolation::WritableJitAllocation::WriteHeaderSlot(T value,
                                                             ReleaseStoreTag) {
  // These asserts are no strict requirements, they just guard against
  // non-implemented functionality.
  static_assert(std::is_convertible_v<T, Object>);
  static_assert(offset != HeapObject::kMapOffset);

  TaggedField<T, offset>::Release_Store(HeapObject::FromAddress(address_),
                                        value);
}

template <typename T, size_t offset>
void ThreadIsolation::WritableJitAllocation::WriteHeaderSlot(T value,
                                                             RelaxedStoreTag) {
  // These asserts are no strict requirements, they just guard against
  // non-implemented functionality.
  static_assert(std::is_convertible_v<T, Object>);

  if constexpr (offset == HeapObject::kMapOffset) {
    TaggedField<T, offset>::Relaxed_Store_Map_Word(
        HeapObject::FromAddress(address_), value);
  } else {
    TaggedField<T, offset>::Relaxed_Store(HeapObject::FromAddress(address_),
                                          value);
  }
}

void ThreadIsolation::WritableJitAllocation::CopyCode(size_t dst_offset,
                                                      const uint8_t* src,
                                                      size_t num_bytes) {
  CopyBytes(reinterpret_cast<uint8_t*>(address_ + dst_offset), src, num_bytes);
}

void ThreadIsolation::WritableJitAllocation::CopyData(size_t dst_offset,
                                                      const uint8_t* src,
                                                      size_t num_bytes) {
  CopyBytes(reinterpret_cast<uint8_t*>(address_ + dst_offset), src, num_bytes);
}

void ThreadIsolation::WritableJitAllocation::ClearBytes(size_t offset,
                                                        size_t len) {
  memset(reinterpret_cast<void*>(address_ + offset), 0, len);
}

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// static
bool RwxMemoryWriteScope::IsSupported() { return true; }

// static
bool RwxMemoryWriteScope::IsSupportedUntrusted() { return true; }

// static
void RwxMemoryWriteScope::SetWritable() {
  if (code_space_write_nesting_level_ == 0) {
    base::SetJitWriteProtected(0);
  }
  code_space_write_nesting_level_++;
}

// static
void RwxMemoryWriteScope::SetExecutable() {
  code_space_write_nesting_level_--;
  if (code_space_write_nesting_level_ == 0) {
    base::SetJitWriteProtected(1);
  }
}

#elif V8_HAS_PKU_JIT_WRITE_PROTECT
// static
bool RwxMemoryWriteScope::IsSupported() {
  static_assert(base::MemoryProtectionKey::kNoMemoryProtectionKey == -1);
  DCHECK(ThreadIsolation::initialized());
  // TODO(sroettger): can we check this at initialization time instead? The
  // tests won't be able to run with/without pkey support anymore in the same
  // process.
  return v8_flags.memory_protection_keys && ThreadIsolation::pkey() >= 0;
}

// static
bool RwxMemoryWriteScope::IsSupportedUntrusted() {
  DCHECK(ThreadIsolation::initialized());
  return v8_flags.memory_protection_keys &&
         ThreadIsolation::untrusted_pkey() >= 0;
}

// static
void RwxMemoryWriteScope::SetWritable() {
  DCHECK(ThreadIsolation::initialized());
  if (!IsSupported()) return;
  if (code_space_write_nesting_level_ == 0) {
    DCHECK_NE(
        base::MemoryProtectionKey::GetKeyPermission(ThreadIsolation::pkey()),
        base::MemoryProtectionKey::kNoRestrictions);
    base::MemoryProtectionKey::SetPermissionsForKey(
        ThreadIsolation::pkey(), base::MemoryProtectionKey::kNoRestrictions);
  }
  code_space_write_nesting_level_++;
}

// static
void RwxMemoryWriteScope::SetExecutable() {
  DCHECK(ThreadIsolation::initialized());
  if (!IsSupported()) return;
  code_space_write_nesting_level_--;
  if (code_space_write_nesting_level_ == 0) {
    DCHECK_EQ(
        base::MemoryProtectionKey::GetKeyPermission(ThreadIsolation::pkey()),
        base::MemoryProtectionKey::kNoRestrictions);
    base::MemoryProtectionKey::SetPermissionsForKey(
        ThreadIsolation::pkey(), base::MemoryProtectionKey::kDisableWrite);
  }
}

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT && !V8_TRY_USE_PKU_JIT_WRITE_PROTECT

// static
bool RwxMemoryWriteScope::IsSupported() { return false; }

// static
bool RwxMemoryWriteScope::IsSupportedUntrusted() { return false; }

// static
void RwxMemoryWriteScope::SetWritable() {}

// static
void RwxMemoryWriteScope::SetExecutable() {}

#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_CODE_MEMORY_ACCESS_INL_H_
