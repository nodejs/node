// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_ISOLATE_GROUP_H_
#define V8_INIT_ISOLATE_GROUP_H_

#include <memory>

#include "include/v8-memory-span.h"
#include "src/base/once.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/mutex.h"
#include "src/codegen/external-reference-table.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/sandbox/code-pointer-table.h"
#include "src/utils/allocation.h"

namespace v8 {

namespace base {
template <typename T>
class LeakyObject;
}  // namespace base

namespace internal {

#ifdef V8_ENABLE_SANDBOX
class Sandbox;
#endif
class CodeRange;
class Isolate;
class ReadOnlyHeap;
class ReadOnlyArtifacts;

// An IsolateGroup allows an API user to control which isolates get allocated
// together in a shared pointer cage.
//
// The standard configuration of V8 is to enable pointer compression and to
// allocate all isolates in a single shared pointer cage
// (V8_COMPRESS_POINTERS_IN_SHARED_CAGE).  This also enables the sandbox
// (V8_ENABLE_SANDBOX), of which there can currently be only one per process, as
// it requires a large part of the virtual address space.
//
// The standard configuration comes with a limitation, in that the total size of
// the compressed pointer cage is limited to 4 GB.  Some API users would like
// pointer compression but also want to avoid the 4 GB limit of the shared
// pointer cage.  Isolate groups allow users to declare which isolates should be
// co-located in a single pointer cage.
//
// Isolate groups are useful only if pointer compression is enabled.  Otherwise,
// the isolate could just allocate pages from the global system allocator;
// there's no need to stay within any particular address range.  If pointer
// compression is disabled, isolate groups are a no-op.
//
// Note that JavaScript objects can only be passed between isolates of the same
// group.  Ensuring this invariant is the responsibility of the API user.
class V8_EXPORT_PRIVATE IsolateGroup final {
 public:
  // Create a new isolate group, allocating a fresh pointer cage if pointer
  // compression is enabled.
  //
  // The pointer cage for isolates in this group will be released when the
  // group's refcount drops to zero.  The group's initial refcount is 1.
  //
  // Note that if pointer compression is disabled, isolates are not grouped and
  // no memory is associated with the isolate group.
  static IsolateGroup* New();

  // Some configurations (e.g. V8_ENABLE_SANDBOX) put all isolates into a single
  // group.  InitializeOncePerProcess should be called early on to initialize
  // the process-wide group.  If this configuration has no process-wide isolate
  // group, the result is nullptr.
  static IsolateGroup* AcquireGlobal();

  static void InitializeOncePerProcess();

  // Obtain a fresh reference on the isolate group.
  IsolateGroup* Acquire() {
    DCHECK_LT(0, reference_count_.load());
    reference_count_++;
    return this;
  }

  // Release a reference on an isolate group, possibly freeing any shared memory
  // resources.
  void Release() {
    DCHECK_LT(0, reference_count_.load());
    if (--reference_count_ == 0) delete this;
  }

  int IncrementIsolateCount() { return ++isolate_count_; }
  int DecrementIsolateCount() { return --isolate_count_; }

  v8::PageAllocator* page_allocator() const { return page_allocator_; }

#ifdef V8_COMPRESS_POINTERS
  VirtualMemoryCage* GetPtrComprCage() const {
    return pointer_compression_cage_;
  }
  VirtualMemoryCage* GetTrustedPtrComprCage() const {
    return trusted_pointer_compression_cage_;
  }
  Address GetPtrComprCageBase() const { return GetPtrComprCage()->base(); }
  Address GetTrustedPtrComprCageBase() const {
    return GetTrustedPtrComprCage()->base();
  }
#endif  // V8_COMPRESS_POINTERS

  CodeRange* EnsureCodeRange(size_t requested_size);
  CodeRange* GetCodeRange() const { return code_range_.get(); }

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
#ifdef USING_V8_SHARED_PRIVATE
  static IsolateGroup* current() { return current_non_inlined(); }
  static void set_current(IsolateGroup* group) {
    set_current_non_inlined(group);
  }
#else   // !USING_V8_SHARED_PRIVATE
  static IsolateGroup* current() { return current_; }
  static void set_current(IsolateGroup* group) { current_ = group; }
#endif  // !USING_V8_SHARED_PRIVATE
#else   // !V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  static IsolateGroup* current() { return GetProcessWideIsolateGroup(); }
#endif  // !V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES

  MemorySpan<Address> external_ref_table() { return external_ref_table_; }

  bool has_shared_space_isolate() const {
    return shared_space_isolate_ != nullptr;
  }

  Isolate* shared_space_isolate() const {
    DCHECK(has_shared_space_isolate());
    return shared_space_isolate_;
  }

  void init_shared_space_isolate(Isolate* isolate) {
    DCHECK(!has_shared_space_isolate());
    shared_space_isolate_ = isolate;
  }

  void ClearSharedSpaceIsolate();

  ReadOnlyHeap* shared_read_only_heap() const { return shared_read_only_heap_; }
  void set_shared_read_only_heap(ReadOnlyHeap* heap) {
    shared_read_only_heap_ = heap;
  }

  base::Mutex* read_only_heap_creation_mutex() {
    return &read_only_heap_creation_mutex_;
  }

  ReadOnlyArtifacts* read_only_artifacts() {
    return read_only_artifacts_.get();
  }

  ReadOnlyArtifacts* InitializeReadOnlyArtifacts();
  void ClearReadOnlyArtifacts();

#ifdef V8_ENABLE_SANDBOX
  CodePointerTable* code_pointer_table() { return &code_pointer_table_; }
#endif  // V8_ENABLE_SANDBOX

 private:
  friend class base::LeakyObject<IsolateGroup>;
#ifndef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  // Unless multiple pointer compression cages are enabled, all isolates in a
  // process are in the same isolate group and share process-wide resources from
  // that group.
  static IsolateGroup* GetProcessWideIsolateGroup();
#endif  // V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES

  IsolateGroup();
  ~IsolateGroup();
  IsolateGroup(const IsolateGroup&) = delete;
  IsolateGroup& operator=(const IsolateGroup&) = delete;

  friend class PoolTest;
  // Only used for testing.
  static void ReleaseGlobal();

#ifdef V8_ENABLE_SANDBOX
  void Initialize(bool process_wide, Sandbox* sandbox);
#else   // V8_ENABLE_SANDBOX
  void Initialize(bool process_wide);
#endif  // V8_ENABLE_SANDBOX

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  static IsolateGroup* current_non_inlined();
  static void set_current_non_inlined(IsolateGroup* group);
#endif

  int IsolateCount() const { return isolate_count_.load(); }

  std::atomic<int> reference_count_{1};
  std::atomic<int> isolate_count_{0};
  v8::PageAllocator* page_allocator_ = nullptr;

#ifdef V8_COMPRESS_POINTERS
  VirtualMemoryCage* trusted_pointer_compression_cage_ = nullptr;
  VirtualMemoryCage* pointer_compression_cage_ = nullptr;
  VirtualMemoryCage reservation_;
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  thread_local static IsolateGroup* current_;
#endif  // V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES

  base::OnceType init_code_range_ = V8_ONCE_INIT;
  std::unique_ptr<CodeRange> code_range_;
  Address external_ref_table_[ExternalReferenceTable::kSizeIsolateIndependent] =
      {0};

  bool process_wide_;

  // Mutex used to ensure that ReadOnlyArtifacts creation is only done once.
  base::Mutex read_only_heap_creation_mutex_;
  std::unique_ptr<ReadOnlyArtifacts> read_only_artifacts_;
  ReadOnlyHeap* shared_read_only_heap_ = nullptr;
  Isolate* shared_space_isolate_ = nullptr;

#ifdef V8_ENABLE_SANDBOX
  CodePointerTable code_pointer_table_;
#endif  // V8_ENABLE_SANDBOX
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_ISOLATE_GROUP_H_
