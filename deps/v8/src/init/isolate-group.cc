// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/isolate-group.h"

#include <memory>

#include "src/base/bounded-page-allocator.h"
#include "src/base/once.h"
#include "src/base/platform/memory.h"
#include "src/base/platform/mutex.h"
#include "src/common/ptr-compr-inl.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/execution/isolate.h"
#include "src/heap/code-range.h"
#include "src/heap/memory-pool.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/read-only-spaces.h"
#include "src/sandbox/code-pointer-table-inl.h"
#include "src/sandbox/sandbox.h"
#include "src/utils/memcopy.h"
#include "src/utils/utils.h"

#ifdef V8_ENABLE_PARTITION_ALLOC
#include <partition_alloc/partition_alloc.h>
#endif

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
thread_local IsolateGroup* IsolateGroup::current_ = nullptr;

// static
IsolateGroup* IsolateGroup::current_non_inlined() { return current_; }
// static
void IsolateGroup::set_current_non_inlined(IsolateGroup* group) {
  current_ = group;
}

class IsolateGroupAccessScope final {
 public:
  explicit IsolateGroupAccessScope(IsolateGroup* group)
      : previous_(IsolateGroup::current()) {
    IsolateGroup::set_current(group);
#ifdef V8_ENABLE_SANDBOX
    Sandbox::set_current(group->sandbox());
#endif
  }

  ~IsolateGroupAccessScope() {
    IsolateGroup::set_current(previous_);
#ifdef V8_ENABLE_SANDBOX
    if (previous_) {
      Sandbox::set_current(previous_->sandbox());
    } else {
      Sandbox::set_current(nullptr);
    }
#endif
  }

 private:
  IsolateGroup* previous_;
};
#else
class IsolateGroupAccessScope final {
 public:
  explicit IsolateGroupAccessScope(IsolateGroup*) {}

  ~IsolateGroupAccessScope() {}
};
#endif  // V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES

#ifdef V8_ENABLE_SANDBOX
void IsolateGroup::MemoryChunkMetadataTableEntry::SetMetadata(
    MemoryChunkMetadata* metadata, Isolate* isolate) {
  metadata_ = metadata;
  // Read-only and shared pages can be accessed from any isolate, mark the entry
  // with the sentinel. Check the RO flag directly here (not calling
  // IsReadOnlySpace()), because the latter will try to access not yet
  // initialized (i.e. this) metadata table entry.
  if (metadata && (metadata->Chunk()->IsFlagSet(MemoryChunk::READ_ONLY_HEAP) ||
                   metadata->Chunk()->InWritableSharedSpace())) {
    isolate_ =
        reinterpret_cast<Isolate*>(kReadOnlyOrSharedEntryIsolateSentinel);
    return;
  }
  isolate_ = isolate;
}
#endif  // V8_ENABLE_SANDBOX

IsolateGroup* IsolateGroup::default_isolate_group_ = nullptr;

#ifdef V8_COMPRESS_POINTERS
struct PtrComprCageReservationParams
    : public VirtualMemoryCage::ReservationParams {
  PtrComprCageReservationParams() {
    page_allocator = GetPlatformPageAllocator();

    reservation_size = kPtrComprCageReservationSize;
    base_alignment = kPtrComprCageBaseAlignment;

    // Simplify BoundedPageAllocator's life by configuring it to use same page
    // size as the Heap will use (MemoryChunk::kPageSize).
    page_size =
        RoundUp(size_t{1} << kPageSizeBits, page_allocator->AllocatePageSize());
    requested_start_hint = RoundDown(
        reinterpret_cast<Address>(page_allocator->GetRandomMmapAddr()),
        base_alignment);

#if V8_OS_FUCHSIA && !V8_EXTERNAL_CODE_SPACE
    // If external code space is not enabled then executable pages (e.g. copied
    // builtins, and JIT pages) will fall under the pointer compression range.
    // Under Fuchsia that means the entire range must be allocated as JITtable.
    permissions = PageAllocator::Permission::kNoAccessWillJitLater;
#else
    permissions = PageAllocator::Permission::kNoAccess;
#endif
    page_initialization_mode =
        base::PageInitializationMode::kAllocatedPagesCanBeUninitialized;
    page_freeing_mode = base::PageFreeingMode::kMakeInaccessible;
  }
};
#endif  // V8_COMPRESS_POINTERS

IsolateGroup::~IsolateGroup() {
  DCHECK_EQ(reference_count_.load(), 0);
  DCHECK(isolates_.empty());
  DCHECK_NULL(main_isolate_);

  if (memory_pool_) {
    memory_pool_->TearDown();
  }

#ifdef V8_ENABLE_LEAPTIERING
  js_dispatch_table_.TearDown();
#endif  // V8_ENABLE_LEAPTIERING

#ifdef V8_ENABLE_SANDBOX
  code_pointer_table_.TearDown();
  trusted_range_.Free();
#endif  // V8_ENABLE_SANDBOX

  // Reset before `reservation_` for pointer compression but disabled external
  // code space.
  code_range_.reset();

#ifdef V8_COMPRESS_POINTERS
  DCHECK(reservation_.IsReserved());
  reservation_.Free();
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_ENABLE_SANDBOX
  backend_allocator_.TearDown();
  sandbox_->TearDown();
  if (!process_wide_) {
    delete sandbox_;
  }
#endif  // V8_ENABLE_SANDBOX
}

#ifdef V8_ENABLE_SANDBOX
void IsolateGroup::Initialize(bool process_wide, Sandbox* sandbox) {
  DCHECK(!reservation_.IsReserved());
  CHECK(sandbox->is_initialized());
  process_wide_ = process_wide;
  PtrComprCageReservationParams params;
  Address base = sandbox->address_space()->AllocatePages(
    sandbox->base(), params.reservation_size, params.base_alignment,
    PagePermissions::kNoAccess);
  CHECK_EQ(sandbox->base(), base);
  base::AddressRegion existing_reservation(base, params.reservation_size);
  params.page_allocator = sandbox->page_allocator();
  if (!reservation_.InitReservation(params, existing_reservation)) {
    V8::FatalProcessOutOfMemory(
      nullptr,
      "Failed to reserve virtual memory for process-wide V8 "
      "pointer compression cage");
  }
  page_allocator_ = reservation_.page_allocator();
  pointer_compression_cage_ = &reservation_;

  if (!trusted_range_.InitReservation(kMaximalTrustedRangeSize)) {
    V8::FatalProcessOutOfMemory(
        nullptr, "Failed to reserve virtual memory for TrustedRange");
  }
  trusted_pointer_compression_cage_ = &trusted_range_;
  sandbox_ = sandbox;

  code_pointer_table()->Initialize();
  optimizing_compile_task_executor_ =
      std::make_unique<OptimizingCompileTaskExecutor>();

  if (v8_flags.memory_pool) {
    memory_pool_ = std::make_unique<MemoryPool>();
  }

#ifdef V8_ENABLE_LEAPTIERING
  js_dispatch_table()->Initialize();
#endif  // V8_ENABLE_LEAPTIERING
}
#elif defined(V8_COMPRESS_POINTERS)
void IsolateGroup::Initialize(bool process_wide) {
  DCHECK(!reservation_.IsReserved());
  process_wide_ = process_wide;
  PtrComprCageReservationParams params;
  if (!reservation_.InitReservation(params)) {
    V8::FatalProcessOutOfMemory(
        nullptr,
        "Failed to reserve virtual memory for process-wide V8 "
        "pointer compression cage");
  }
  page_allocator_ = reservation_.page_allocator();
  pointer_compression_cage_ = &reservation_;
  trusted_pointer_compression_cage_ = &reservation_;
  optimizing_compile_task_executor_ =
      std::make_unique<OptimizingCompileTaskExecutor>();
  memory_pool_ = std::make_unique<MemoryPool>();
#ifdef V8_ENABLE_LEAPTIERING
  js_dispatch_table()->Initialize();
#endif  // V8_ENABLE_LEAPTIERING
}
#else   // !V8_COMPRESS_POINTERS
void IsolateGroup::Initialize(bool process_wide) {
  process_wide_ = process_wide;
  page_allocator_ = GetPlatformPageAllocator();
  optimizing_compile_task_executor_ =
      std::make_unique<OptimizingCompileTaskExecutor>();
  memory_pool_ = std::make_unique<MemoryPool>();
#ifdef V8_ENABLE_LEAPTIERING
  js_dispatch_table()->Initialize();
#endif  // V8_ENABLE_LEAPTIERING
}
#endif  // V8_ENABLE_SANDBOX

// static
void IsolateGroup::InitializeOncePerProcess() {
  CHECK_NULL(default_isolate_group_);
  default_isolate_group_ = new IsolateGroup;
  IsolateGroup* group = GetDefault();

  DCHECK_NULL(group->page_allocator_);
#ifdef V8_ENABLE_SANDBOX
  group->Initialize(true, Sandbox::GetDefault());
#else
  group->Initialize(true);
#endif
  CHECK_NOT_NULL(group->page_allocator_);

#ifdef V8_COMPRESS_POINTERS
  V8HeapCompressionScheme::InitBase(group->GetPtrComprCageBase());
#endif  // V8_COMPRESS_POINTERS
#ifdef V8_ENABLE_SANDBOX
  TrustedSpaceCompressionScheme::InitBase(group->GetTrustedPtrComprCageBase());
#endif
#ifdef V8_EXTERNAL_CODE_SPACE
  // Speculatively set the code cage base to the same value in case jitless
  // mode will be used. Once the process-wide CodeRange instance is created
  // the code cage base will be set accordingly.
  ExternalCodeCompressionScheme::InitBase(V8HeapCompressionScheme::base());
#endif  // V8_EXTERNAL_CODE_SPACE
#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  IsolateGroup::set_current(group);
#endif
}

// static
void IsolateGroup::TearDownOncePerProcess() { ReleaseDefault(); }

void IsolateGroup::Release() {
  DCHECK_LT(0, reference_count_.load());

  if (--reference_count_ == 0) {
    delete this;
  }
}

namespace {
void InitCodeRangeOnce(std::unique_ptr<CodeRange>* code_range_member,
                       v8::PageAllocator* page_allocator, size_t requested_size,
                       bool immutable) {
  CodeRange* code_range = new CodeRange();
  if (!code_range->InitReservation(page_allocator, requested_size, immutable)) {
    V8::FatalProcessOutOfMemory(
        nullptr, "Failed to reserve virtual memory for CodeRange");
  }
  code_range_member->reset(code_range);
#ifdef V8_EXTERNAL_CODE_SPACE
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  ExternalCodeCompressionScheme::InitBase(
      ExternalCodeCompressionScheme::PrepareCageBaseAddress(
          code_range->base()));
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE
#endif  // V8_EXTERNAL_CODE_SPACE
}
}  // namespace

CodeRange* IsolateGroup::EnsureCodeRange(size_t requested_size) {
  base::CallOnce(&init_code_range_, InitCodeRangeOnce, &code_range_,
                 page_allocator_, requested_size, process_wide_);
  return code_range_.get();
}

ReadOnlyArtifacts* IsolateGroup::InitializeReadOnlyArtifacts() {
  mutex_.AssertHeld();
  DCHECK(!read_only_artifacts_);
  read_only_artifacts_ = std::make_unique<ReadOnlyArtifacts>();
  return read_only_artifacts_.get();
}

#ifdef V8_ENABLE_SANDBOX
std::weak_ptr<PageAllocator> IsolateGroup::GetBackingStorePageAllocator() {
  return sandbox()->page_allocator_weak();
}
#endif  // V8_ENABLE_SANDBOX

void IsolateGroup::SetupReadOnlyHeap(Isolate* isolate,
                                     SnapshotData* read_only_snapshot_data,
                                     bool can_rehash) {
  DCHECK_EQ(isolate->isolate_group(), this);
  base::MutexGuard guard(&mutex_);
  ReadOnlyHeap::SetUp(isolate, read_only_snapshot_data, can_rehash);
}

void IsolateGroup::AddIsolate(Isolate* isolate) {
  DCHECK_EQ(isolate->isolate_group(), this);
  base::MutexGuard guard(&mutex_);

  const bool inserted = isolates_.insert(isolate).second;
  CHECK(inserted);

  if (!main_isolate_) {
    main_isolate_ = isolate;
  }

  optimizing_compile_task_executor_->EnsureStarted();

  if (v8_flags.shared_heap) {
    if (has_shared_space_isolate()) {
      isolate->owns_shareable_data_ = false;
    } else {
      init_shared_space_isolate(isolate);
      isolate->is_shared_space_isolate_ = true;
      DCHECK(isolate->owns_shareable_data_);
    }
  }
}

void IsolateGroup::RemoveIsolate(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);

  if (isolates_.size() == 1) {
    read_only_artifacts_.reset();

    optimizing_compile_task_executor_->Stop();

    // We are removing the last isolate from the group. If this group has a
    // shared heap, the last isolate has to be the shared space isolate.
    DCHECK_EQ(has_shared_space_isolate(), isolate->is_shared_space_isolate());

    if (isolate->is_shared_space_isolate()) {
      CHECK_EQ(isolate, shared_space_isolate_);
      shared_space_isolate_ = nullptr;
    }
  } else {
    // The shared space isolate needs to be removed last.
    DCHECK(!isolate->is_shared_space_isolate());
  }

  CHECK_EQ(isolates_.erase(isolate), 1);

  if (main_isolate_ == isolate) {
    if (isolates_.empty()) {
      main_isolate_ = nullptr;
    } else {
      main_isolate_ = *isolates_.begin();
    }
  }
}

// static
IsolateGroup* IsolateGroup::New() {
  if (!CanCreateNewGroups()) {
    FATAL(
        "Creation of new isolate groups requires enabling "
        "multiple pointer compression cages at build-time");
  }

  IsolateGroup* group = new IsolateGroup;
#ifdef V8_ENABLE_SANDBOX
  Sandbox* sandbox = Sandbox::New(GetPlatformVirtualAddressSpace());
  group->Initialize(false, sandbox);
#else
  group->Initialize(false);
#endif
  CHECK_NOT_NULL(group->page_allocator_);

  // We need to set this early, because it is needed while initializing the
  // external reference table, eg. in the js_dispatch_table_address and
  // code_pointer_table_address functions.  This is also done in
  // IsolateGroup::InitializeOncePerProcess for the single-IsolateGroup
  // configurations.
  IsolateGroupAccessScope group_access_scope(group);
  ExternalReferenceTable::InitializeOncePerIsolateGroup(
      group->external_ref_table());
  return group;
}

// static
void IsolateGroup::ReleaseDefault() {
  IsolateGroup* group = GetDefault();
  CHECK_EQ(group->reference_count_.load(), 1);
  CHECK(!group->has_shared_space_isolate());
  group->Release();
  default_isolate_group_ = nullptr;
}

#ifdef V8_ENABLE_SANDBOX
void SandboxedArrayBufferAllocator::LazyInitialize(Sandbox* sandbox) {
  base::MutexGuard guard(&mutex_);
  if (is_initialized()) {
    return;
  }
  CHECK(sandbox->is_initialized());
  sandbox_ = sandbox;
  constexpr size_t max_backing_memory_size = 8ULL * GB;
  constexpr size_t min_backing_memory_size = 1ULL * GB;
  size_t backing_memory_size = max_backing_memory_size;
  Address backing_memory_base = 0;
  while (!backing_memory_base &&
         backing_memory_size >= min_backing_memory_size) {
    backing_memory_base = sandbox_->address_space()->AllocatePages(
        VirtualAddressSpace::kNoHint, backing_memory_size, kChunkSize,
        PagePermissions::kNoAccess);
    if (!backing_memory_base) {
      backing_memory_size /= 2;
    }
  }
  if (!backing_memory_base) {
    V8::FatalProcessOutOfMemory(
        nullptr, "Could not reserve backing memory for ArrayBufferAllocators");
  }
  DCHECK(IsAligned(backing_memory_base, kChunkSize));

  region_alloc_ = std::make_unique<base::RegionAllocator>(
      backing_memory_base, backing_memory_size, kAllocationGranularity);
  end_of_accessible_region_ = region_alloc_->begin();

  // Install an on-merge callback to discard or decommit unused pages.
  region_alloc_->set_on_merge_callback([this](Address start, size_t size) {
    mutex_.AssertHeld();
    Address end = start + size;
    if (end == region_alloc_->end() &&
        start <= end_of_accessible_region_ - kChunkSize) {
      // Can shrink the accessible region.
      Address new_end_of_accessible_region = RoundUp(start, kChunkSize);
      size_t size_to_decommit =
          end_of_accessible_region_ - new_end_of_accessible_region;
      if (!sandbox_->address_space()->DecommitPages(
              new_end_of_accessible_region, size_to_decommit)) {
        V8::FatalProcessOutOfMemory(nullptr, "SandboxedArrayBufferAllocator()");
      }
      end_of_accessible_region_ = new_end_of_accessible_region;
    } else if (size >= 2 * kChunkSize) {
      // Can discard pages. The pages stay accessible, so the size of the
      // accessible region doesn't change.
      Address chunk_start = RoundUp(start, kChunkSize);
      Address chunk_end = RoundDown(start + size, kChunkSize);
      if (!sandbox_->address_space()->DiscardSystemPages(
              chunk_start, chunk_end - chunk_start)) {
        V8::FatalProcessOutOfMemory(nullptr, "SandboxedArrayBufferAllocator()");
      }
    }
  });
}

void* SandboxedArrayBufferAllocator::Allocate(size_t length) {
  base::MutexGuard guard(&mutex_);

  length = RoundUp(length, kAllocationGranularity);
  Address region = region_alloc_->AllocateRegion(length);
  if (region == base::RegionAllocator::kAllocationFailure) return nullptr;

  // Check if the memory is inside the accessible region. If not, grow it.
  Address end = region + length;
  size_t length_to_memset = length;
  if (end > end_of_accessible_region_) {
    Address new_end_of_accessible_region = RoundUp(end, kChunkSize);
    size_t size = new_end_of_accessible_region - end_of_accessible_region_;
    if (!sandbox_->address_space()->SetPagePermissions(
            end_of_accessible_region_, size, PagePermissions::kReadWrite)) {
      if (!region_alloc_->FreeRegion(region)) {
        V8::FatalProcessOutOfMemory(
            nullptr, "SandboxedArrayBufferAllocator::Allocate()");
      }
      return nullptr;
    }

    // The pages that were inaccessible are guaranteed to be zeroed, so only
    // memset until the previous end of the accessible region.
    length_to_memset = end_of_accessible_region_ - region;
    end_of_accessible_region_ = new_end_of_accessible_region;
  }

  void* mem = reinterpret_cast<void*>(region);
  memset(mem, 0, length_to_memset);
  return mem;
}

void* SandboxedArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return Allocate(length);
}

void SandboxedArrayBufferAllocator::Free(void* data) {
  base::MutexGuard guard(&mutex_);
  region_alloc_->FreeRegion(reinterpret_cast<Address>(data));
}

void SandboxedArrayBufferAllocator::TearDown() {
  // The sandbox may already have been torn down, in which case there's no
  // need to free any memory.
  if (is_initialized() && sandbox_->is_initialized()) {
    sandbox_->address_space()->FreePages(region_alloc_->begin(),
                                         region_alloc_->size());
  }
  sandbox_ = nullptr;
  region_alloc_.reset(nullptr);
}

#ifdef V8_ENABLE_PARTITION_ALLOC
class PABackedSandboxedArrayBufferAllocator::Impl final {
 public:
  explicit Impl(Sandbox* sandbox) {
    const size_t max_pool_size = partition_alloc::internal::
        PartitionAddressSpace::ConfigurablePoolMaxSize();
    const size_t min_pool_size = partition_alloc::internal::
        PartitionAddressSpace::ConfigurablePoolMinSize();
    size_t pool_size = max_pool_size;
    // Try to reserve the maximum size of the pool at first, then keep halving
    // the size on failure until it succeeds.
    uintptr_t pool_base = 0;
    while (!pool_base && pool_size >= min_pool_size) {
      pool_base = sandbox->address_space()->AllocatePages(
          VirtualAddressSpace::kNoHint, pool_size, pool_size,
          v8::PagePermissions::kNoAccess);
      if (!pool_base) {
        pool_size /= 2;
      }
    }
    // The V8 sandbox is guaranteed to be large enough to host the pool.
    CHECK(pool_base);
    partition_alloc::internal::PartitionAddressSpace::InitConfigurablePool(
        pool_base, pool_size);

    partition_alloc::PartitionOptions opts;
    opts.backup_ref_ptr = partition_alloc::PartitionOptions::kDisabled;
    opts.use_configurable_pool = partition_alloc::PartitionOptions::kAllowed;
    partition_.init(std::move(opts));
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void* Allocate(size_t length) {
    constexpr partition_alloc::AllocFlags flags =
        partition_alloc::AllocFlags::kZeroFill |
        partition_alloc::AllocFlags::kReturnNull;
    return AllocateInternal<flags>(length);
  }

  void* AllocateUninitialized(size_t length) {
    constexpr partition_alloc::AllocFlags flags =
        partition_alloc::AllocFlags::kReturnNull;
    return AllocateInternal<flags>(length);
  }

  void Free(void* data) {
    partition_.root()->Free<partition_alloc::FreeFlags::kNoMemoryToolOverride>(
        data);
  }

 private:
  template <partition_alloc::AllocFlags flags>
  void* AllocateInternal(size_t length) {
    // The V8 sandbox requires all ArrayBuffer backing stores to be allocated
    // inside the sandbox address space. This isn't guaranteed if allocation
    // override hooks (which are e.g. used by GWP-ASan) are enabled or if a
    // memory tool (e.g. ASan) overrides malloc, so disable both.
    constexpr auto new_flags =
        flags | partition_alloc::AllocFlags::kNoOverrideHooks |
        partition_alloc::AllocFlags::kNoMemoryToolOverride;
    return partition_.root()->AllocInline<new_flags>(
        length, "PABackedSandboxedArrayBufferAllocator");
  }

  partition_alloc::PartitionAllocator partition_;
};

PABackedSandboxedArrayBufferAllocator::
    ~PABackedSandboxedArrayBufferAllocator() = default;

void PABackedSandboxedArrayBufferAllocator::LazyInitialize(Sandbox* sandbox) {
  if (impl_) {
    return;
  }
  impl_ = std::make_unique<Impl>(sandbox);
}

void* PABackedSandboxedArrayBufferAllocator::Allocate(size_t length) {
  DCHECK(impl_);
  return impl_->Allocate(length);
}

void* PABackedSandboxedArrayBufferAllocator::AllocateUninitialized(
    size_t length) {
  DCHECK(impl_);
  return impl_->AllocateUninitialized(length);
}

void PABackedSandboxedArrayBufferAllocator::Free(void* data) {
  DCHECK(impl_);
  return impl_->Free(data);
}

void PABackedSandboxedArrayBufferAllocator::TearDown() { impl_.reset(); }
#endif  // V8_ENABLE_PARTITION_ALLOC

SandboxedArrayBufferAllocatorBase*
IsolateGroup::GetSandboxedArrayBufferAllocator() {
  // TODO(342905186): Consider initializing it during IsolateGroup
  // initialization instead of doing it lazily.
  //
  backend_allocator_.LazyInitialize(sandbox());
  return &backend_allocator_;
}

#endif  // V8_ENABLE_SANDBOX

OptimizingCompileTaskExecutor*
IsolateGroup::optimizing_compile_task_executor() {
  return optimizing_compile_task_executor_.get();
}

}  // namespace internal
}  // namespace v8
