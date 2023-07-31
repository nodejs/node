// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/code-memory-access-inl.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

ThreadIsolation::TrustedData ThreadIsolation::trusted_data_;
ThreadIsolation::UntrustedData ThreadIsolation::untrusted_data_;

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT
thread_local int RwxMemoryWriteScope::code_space_write_nesting_level_ = 0;
#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT

#if V8_HAS_PKU_JIT_WRITE_PROTECT

bool RwxMemoryWriteScope::IsPKUWritable() {
  DCHECK(ThreadIsolation::initialized());
  return base::MemoryProtectionKey::GetKeyPermission(ThreadIsolation::pkey()) ==
         base::MemoryProtectionKey::kNoRestrictions;
}

void RwxMemoryWriteScope::SetDefaultPermissionsForSignalHandler() {
  DCHECK(ThreadIsolation::initialized());
  if (!RwxMemoryWriteScope::IsSupportedUntrusted()) return;
  base::MemoryProtectionKey::SetPermissionsForKey(
      ThreadIsolation::untrusted_pkey(),
      base::MemoryProtectionKey::kDisableWrite);
}

#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT

RwxMemoryWriteScopeForTesting::RwxMemoryWriteScopeForTesting()
    : RwxMemoryWriteScope("For Testing") {}

RwxMemoryWriteScopeForTesting::~RwxMemoryWriteScopeForTesting() {}

// static
bool ThreadIsolation::Enabled() {
#if V8_HEAP_USE_PKU_JIT_WRITE_PROTECT
  return allocator() != nullptr;
#else
  return false;
#endif
}

// static
template <typename T, typename... Args>
void ThreadIsolation::ConstructNew(T** ptr, Args&&... args) {
  *ptr = reinterpret_cast<T*>(trusted_data_.allocator->Allocate(sizeof(T)));
  if (!*ptr) return;
  new (*ptr) T(std::forward<Args>(args)...);
}

// static
template <typename T>
void ThreadIsolation::Delete(T* ptr) {
  ptr->~T();
  trusted_data_.allocator->Free(ptr);
}

// static
void ThreadIsolation::Initialize(
    ThreadIsolatedAllocator* thread_isolated_allocator) {
#if DEBUG
  untrusted_data_.initialized = true;
#endif

  if (!thread_isolated_allocator) {
    return;
  }

  if (v8_flags.jitless) {
    return;
  }

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  if (!base::MemoryProtectionKey::InitializeMemoryProtectionKeySupport()) {
    return;
  }
#endif

  // Check that our compile time assumed page size that we use for padding was
  // large enough.
  CHECK_GE(THREAD_ISOLATION_ALIGN_SZ,
           GetPlatformPageAllocator()->CommitPageSize());

  trusted_data_.allocator = thread_isolated_allocator;

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  trusted_data_.pkey = trusted_data_.allocator->Pkey();
  untrusted_data_.pkey = trusted_data_.pkey;

  {
    RwxMemoryWriteScope write_scope("Initialize thread isolation.");
    ConstructNew(&trusted_data_.jit_pages_mutex_);
    ConstructNew(&trusted_data_.jit_pages_);
  }

  base::MemoryProtectionKey::SetPermissionsAndKey(
      {reinterpret_cast<Address>(&trusted_data_), sizeof(trusted_data_)},
      v8::PageAllocator::Permission::kRead, trusted_data_.pkey);
#endif
}

// static
ThreadIsolation::JitPageReference ThreadIsolation::LookupJitPage(Address addr,
                                                                 size_t size) {
  base::MutexGuard guard(trusted_data_.jit_pages_mutex_);
  return LookupJitPageLocked(addr, size);
}

// static
ThreadIsolation::JitPageReference ThreadIsolation::LookupJitPageLocked(
    Address addr, size_t size) {
  trusted_data_.jit_pages_mutex_->AssertHeld();

  // upper_bound gives us an iterator to the position after address.
  auto it = trusted_data_.jit_pages_->upper_bound(addr);

  // The previous page should be the one we're looking for.
  CHECK_NE(it, trusted_data_.jit_pages_->begin());
  it--;

  JitPageReference jit_page(it->second, it->first);
  size_t start_offset = addr - it->first;
  size_t end_offset = start_offset + size;

  // Check that the page includes the whole range.
  CHECK_GT(end_offset, start_offset);
  CHECK_GT(jit_page.Size(), start_offset);
  CHECK_GE(jit_page.Size(), end_offset);

  return jit_page;
}

namespace {

template <class T>
void CheckForRegionOverlap(const T& map, Address addr, size_t size) {
  // The data is untrusted from the pov of CFI, so we check that there's no
  // overlaps with existing regions etc.
  CHECK_GE(addr + size, addr);

  // Find an entry in the map with key > addr
  auto it = map.upper_bound(addr);
  bool is_begin = it == map.begin();
  bool is_end = it == map.end();

  // Check for overlap with the next entry
  if (!is_end) {
    Address next_addr = it->first;
    Address offset = next_addr - addr;
    CHECK_LE(size, offset);
  }

  // Check the previous entry for overlap
  if (!is_begin) {
    it--;
    Address prev_addr = it->first;
    const typename T::value_type::second_type& prev_entry = it->second;
    Address offset = addr - prev_addr;
    CHECK_LE(prev_entry.Size(), offset);
  }
}

}  // namespace

ThreadIsolation::JitPageReference::JitPageReference(class JitPage* jit_page,
                                                    base::Address address)
    : page_lock_(&jit_page->mutex_), jit_page_(jit_page), address_(address) {}

ThreadIsolation::JitPage::~JitPage() {
  // TODO(sroettger): check that the page is not in use (scan shadow stacks).
}

size_t ThreadIsolation::JitPageReference::Size() const {
  return jit_page_->size_;
}

bool ThreadIsolation::JitPageReference::Empty() const {
  return jit_page_->allocations_.empty();
}

void ThreadIsolation::JitPageReference::Shrink(class JitPage* tail) {
  jit_page_->size_ -= tail->size_;
  // Move all allocations that are out of bounds.
  auto it = jit_page_->allocations_.lower_bound(End());
  tail->allocations_.insert(it, jit_page_->allocations_.end());
  jit_page_->allocations_.erase(it, jit_page_->allocations_.end());
}

void ThreadIsolation::JitPageReference::Expand(size_t offset) {
  jit_page_->size_ += offset;
}

void ThreadIsolation::JitPageReference::Merge(JitPageReference& next) {
  DCHECK_EQ(End(), next.Address());
  jit_page_->size_ += next.jit_page_->size_;
  next.jit_page_->size_ = 0;
  jit_page_->allocations_.merge(next.jit_page_->allocations_);
  DCHECK(next.jit_page_->allocations_.empty());
}

void ThreadIsolation::JitPageReference::RegisterAllocation(base::Address addr,
                                                           size_t size) {
  // The data is untrusted from the pov of CFI, so the checks are security
  // sensitive.
  CHECK_GE(addr, address_);
  base::Address offset = addr - address_;
  base::Address end_offset = offset + size;
  CHECK_GT(end_offset, offset);
  CHECK_GT(jit_page_->size_, offset);
  CHECK_GE(jit_page_->size_, end_offset);

  CheckForRegionOverlap(jit_page_->allocations_, addr, size);
  jit_page_->allocations_.emplace(addr, JitAllocation(size));
}

void ThreadIsolation::JitPageReference::UnregisterAllocation(
    base::Address addr) {
  // TODO(sroettger): check that the memory is not in use (scan shadow stacks).
  CHECK_EQ(jit_page_->allocations_.erase(addr), 1);
}

void ThreadIsolation::JitPageReference::UnregisterAllocationsExcept(
    base::Address start, size_t size, const std::vector<base::Address>& keep) {
  // TODO(sroettger): check that the page is not in use (scan shadow stacks).
  JitPage::AllocationMap keep_allocations;

  auto keep_before = jit_page_->allocations_.lower_bound(start);
  auto keep_after = jit_page_->allocations_.lower_bound(start + size);

  // keep all allocations before the start address.
  if (keep_before != jit_page_->allocations_.begin()) {
    keep_before--;
    keep_allocations.insert(jit_page_->allocations_.begin(), keep_before);
  }

  // from the start address, keep only allocations passed in the vector
  auto keep_iterator = keep.begin();
  for (auto it = keep_before; it != keep_after; it++) {
    if (keep_iterator == keep.end()) break;
    if (it->first == *keep_iterator) {
      keep_allocations.emplace_hint(keep_allocations.end(), it->first,
                                    it->second);
      keep_iterator++;
    }
  }
  CHECK_EQ(keep_iterator, keep.end());

  // keep all allocations after the region
  keep_allocations.insert(keep_after, jit_page_->allocations_.end());

  jit_page_->allocations_.swap(keep_allocations);
}

// static
void ThreadIsolation::RegisterJitPage(Address address, size_t size) {
  if (!Enabled()) return;

  RwxMemoryWriteScope write_scope("Adding new executable memory.");

  {
    // Wasm code can allocate memory that spans jit page registrations, so we
    // merge them here too.
    base::MutexGuard guard(trusted_data_.jit_pages_mutex_);
    auto it = trusted_data_.jit_pages_->upper_bound(address);
    base::Optional<JitPageReference> jit_page, next_page;
    bool is_begin = it == trusted_data_.jit_pages_->begin();
    bool is_end = it == trusted_data_.jit_pages_->end();

    Address end = address + size;
    CHECK_GT(end, address);

    // Check for overlap first since we might extend the previous page below.
    if (!is_end) {
      next_page.emplace(it->second, it->first);
      CHECK_LE(end, next_page->Address());
    }

    // Check if we can expand the previous page instead of creating a new one.
    bool expanded_previous = false;
    if (!is_begin) {
      it--;
      jit_page.emplace(it->second, it->first);
      CHECK_LE(jit_page->End(), address);
      if (jit_page->End() == address) {
        jit_page->Expand(size);
        expanded_previous = true;
      }
    }

    // Expanding didn't work, create a new region.
    if (!expanded_previous) {
      JitPage* new_jit_page;
      ConstructNew(&new_jit_page, size);
      trusted_data_.jit_pages_->emplace(address, new_jit_page);
      jit_page.emplace(new_jit_page, address);
    }
    DCHECK(jit_page.has_value());

    // Check if we should merge with the next page.
    if (next_page && jit_page->End() == next_page->Address()) {
      jit_page->Merge(*next_page);
      trusted_data_.jit_pages_->erase(next_page->Address());
      JitPage* to_delete = next_page->JitPage();
      next_page.reset();
      Delete(to_delete);
    }
  }
}

void ThreadIsolation::UnregisterJitPage(Address address, size_t size) {
  if (!Enabled()) return;

  RwxMemoryWriteScope write_scope("Removing executable memory.");

  JitPage* to_delete;

  {
    base::MutexGuard guard(trusted_data_.jit_pages_mutex_);
    JitPageReference jit_page = LookupJitPageLocked(address, size);

    // We're merging jit pages together, so potentially split them back up
    // if we're only freeing a subrange.

    Address to_free_end = address + size;
    Address jit_page_end = jit_page.Address() + jit_page.Size();

    if (to_free_end < jit_page_end) {
      // There's a tail after the page that we release. Shrink the page and
      // add the tail to the map.
      size_t tail_size = jit_page_end - to_free_end;
      JitPage* tail;
      ConstructNew(&tail, tail_size);
      jit_page.Shrink(tail);
      trusted_data_.jit_pages_->emplace(to_free_end, tail);
    }

    DCHECK_EQ(to_free_end, jit_page.Address() + jit_page.Size());

    if (address == jit_page.Address()) {
      // We remove the start of the region, just remove it from the map.
      to_delete = jit_page.JitPage();
      trusted_data_.jit_pages_->erase(address);
    } else {
      // Otherwise, we need to shrink the region.
      DCHECK_GT(address, jit_page.Address());
      JitPage* tail;
      ConstructNew(&tail, size);
      jit_page.Shrink(tail);
      to_delete = tail;
    }
  }
  Delete(to_delete);
}

// static
bool ThreadIsolation::MakeExecutable(Address address, size_t size) {
  DCHECK(Enabled());

  // TODO(sroettger): need to make sure that the memory is zero-initialized.
  // maybe map over it with MAP_FIXED, or call MADV_DONTNEED, or fall back to
  // memset.

  // Check that the range is inside a tracked jit page.
  JitPageReference jit_page = LookupJitPage(address, size);

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  return base::MemoryProtectionKey::SetPermissionsAndKey(
      {address, size}, PageAllocator::Permission::kReadWriteExecute, pkey());
#else   // V8_HAS_PKU_JIT_WRITE_PROTECT
  UNREACHABLE();
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
}

// static
void ThreadIsolation::RegisterJitAllocation(Address obj, size_t size) {
  // This method is private, so we skip the Enabled() check.
  LookupJitPage(obj, size).RegisterAllocation(obj, size);
}

// static
void ThreadIsolation::RegisterInstructionStreamAllocation(Address addr,
                                                          size_t size) {
  if (!Enabled()) return;

  return RegisterJitAllocation(addr, size);
}

// static
void ThreadIsolation::UnregisterAllocationsInPageExcept(
    Address page, size_t page_size, const std::vector<Address>& keep) {
  LookupJitPage(page, page_size)
      .UnregisterAllocationsExcept(page, page_size, keep);
}

// static
void ThreadIsolation::UnregisterInstructionStreamsInPageExcept(
    MemoryChunk* chunk, const std::vector<Address>& keep) {
  if (!Enabled()) return;

  UnregisterAllocationsInPageExcept(chunk->area_start(), chunk->area_size(),
                                    keep);
}

// static
void ThreadIsolation::RegisterWasmAllocation(Address addr, size_t size) {
  if (!Enabled()) return;

  return RegisterJitAllocation(addr, size);
}

// static
void ThreadIsolation::UnregisterWasmAllocation(Address addr, size_t size) {
  if (!Enabled()) return;

  LookupJitPage(addr, size).UnregisterAllocation(addr);
}

#if DEBUG

// static
void ThreadIsolation::CheckTrackedMemoryEmpty() {
  if (!Enabled()) return;

  DCHECK(trusted_data_.jit_pages_->empty());
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
