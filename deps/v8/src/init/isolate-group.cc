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
#include "src/heap/safepoint.h"
#include "src/init/v8.h"
#include "src/sandbox/sandbox.h"
#include "src/utils/memcopy.h"
#include "src/utils/utils.h"

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
void IsolateGroup::BasePageTableEntry::SetMetadata(BasePage* metadata,
                                                   Isolate* isolate) {
  metadata_ = metadata;
  // Read-only and shared pages can be accessed from any isolate, mark the entry
  // with the sentinel.
  if (metadata &&
      (metadata->IsReadOnlyPage() || metadata->is_writable_shared())) {
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

#ifdef V8_ENABLE_SANDBOX
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

#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  void* cage_base = reinterpret_cast<void*>(reservation_.base());
  const void* read_only_reservation_start = page_allocator_->AllocatePages(
      cage_base, kContiguousReadOnlyReservationSize,
      MemoryChunk::GetAlignmentForAllocation(),
      PageAllocator::Permission::kNoAccess);
  CHECK_EQ(read_only_reservation_start, cage_base);
  read_only_page_allocator_ = std::make_unique<v8::base::BoundedPageAllocator>(
      page_allocator_, reinterpret_cast<Address>(read_only_reservation_start),
      kContiguousReadOnlyReservationSize, kRegularPageSize,
      base::PageInitializationMode::kAllocatedPagesCanBeUninitialized,
      base::PageFreeingMode::kMakeInaccessible);
#endif  // CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL

  if (!trusted_range_.InitReservation(kMaximalTrustedRangeSize)) {
    V8::FatalProcessOutOfMemory(
        nullptr, "Failed to reserve virtual memory for TrustedRange");
  }
  trusted_pointer_compression_cage_ = &trusted_range_;
  sandbox_ = sandbox;

  optimizing_compile_task_executor_ =
      std::make_unique<OptimizingCompileTaskExecutor>();

  if (v8_flags.memory_pool) {
    memory_pool_ = std::make_unique<MemoryPool>(MemoryPool::Config{
        .single_threaded = v8_flags.single_threaded,
        .share_memory_on_teardown =
            v8_flags.memory_pool_share_memory_on_teardown,
        .trace_gc_nvp = v8_flags.trace_gc_nvp,
        .max_large_page_pool_size = v8_flags.max_large_page_pool_size,
        .timeout_in_sec = v8_flags.memory_pool_timeout});
  }
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

#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  void* cage_base = reinterpret_cast<void*>(reservation_.base());
  const void* read_only_reservation_start = page_allocator_->AllocatePages(
      cage_base, kContiguousReadOnlyReservationSize,
      MemoryChunk::GetAlignmentForAllocation(),
      PageAllocator::Permission::kNoAccess);
  CHECK_EQ(read_only_reservation_start, cage_base);
  read_only_page_allocator_ = std::make_unique<v8::base::BoundedPageAllocator>(
      page_allocator_, reinterpret_cast<Address>(read_only_reservation_start),
      kContiguousReadOnlyReservationSize, kRegularPageSize,
      base::PageInitializationMode::kAllocatedPagesCanBeUninitialized,
      base::PageFreeingMode::kMakeInaccessible);
#endif  // CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL

  pointer_compression_cage_ = &reservation_;
  trusted_pointer_compression_cage_ = &reservation_;
  optimizing_compile_task_executor_ =
      std::make_unique<OptimizingCompileTaskExecutor>();
  memory_pool_ = std::make_unique<MemoryPool>(MemoryPool::Config{
      .single_threaded = v8_flags.single_threaded,
      .share_memory_on_teardown = v8_flags.memory_pool_share_memory_on_teardown,
      .trace_gc_nvp = v8_flags.trace_gc_nvp,
      .max_large_page_pool_size = v8_flags.max_large_page_pool_size,
      .timeout_in_sec = v8_flags.memory_pool_timeout});
}
#else   // !V8_COMPRESS_POINTERS
void IsolateGroup::Initialize(bool process_wide) {
  process_wide_ = process_wide;
  page_allocator_ = GetPlatformPageAllocator();
  optimizing_compile_task_executor_ =
      std::make_unique<OptimizingCompileTaskExecutor>();
  memory_pool_ = std::make_unique<MemoryPool>(MemoryPool::Config{
      .single_threaded = v8_flags.single_threaded,
      .share_memory_on_teardown = v8_flags.memory_pool_share_memory_on_teardown,
      .trace_gc_nvp = v8_flags.trace_gc_nvp,
      .max_large_page_pool_size = v8_flags.max_large_page_pool_size,
      .timeout_in_sec = v8_flags.memory_pool_timeout});
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
    if (!global_safepoint_) {
      global_safepoint_ = std::make_unique<GlobalSafepoint>(this);
    }
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

size_t IsolateGroup::GetIsolateCount() {
  base::MutexGuard guard(&mutex_);
  return isolates_.size();
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
  Sandbox* sandbox =
      Sandbox::New(V8::GetCurrentPlatform(), GetPlatformVirtualAddressSpace());
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

v8::Allocator* IsolateGroup::GetInSandboxAllocator() {
  return sandbox_->in_sandbox_allocator();
}

#endif  // V8_ENABLE_SANDBOX

OptimizingCompileTaskExecutor*
IsolateGroup::optimizing_compile_task_executor() {
  return optimizing_compile_task_executor_.get();
}

}  // namespace internal
}  // namespace v8
