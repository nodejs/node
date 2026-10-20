// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_HANDLE_SCOPE_IMPLEMENTER_H_
#define V8_HANDLES_HANDLE_SCOPE_IMPLEMENTER_H_

#include <memory>

#include "include/v8-internal.h"
#include "src/base/macros.h"
#include "src/objects/contexts.h"
#include "src/utils/detachable-vector.h"

namespace v8 {
namespace internal {

class Isolate;
class PersistentHandles;

// This class is here in order to be able to declare it a friend of
// HandleScope.  Moving these methods to be members of HandleScope would be
// neat in some ways, but it would expose internal implementation details in
// our public header file, which is undesirable.
//
// An isolate has a single instance of this class to hold the current thread's
// data. In multithreaded V8 programs this data is copied in and out of storage
// so that the currently executing thread always has its own copy of this
// data.
class HandleScopeImplementer {
 public:
  static constexpr int kSizeInBytes =
      11 * kSystemPointerSize + HandleScopeData::kSizeInBytes;

  class V8_NODISCARD EnteredContextRewindScope {
   public:
    explicit inline EnteredContextRewindScope(HandleScopeImplementer* hsi);
    inline ~EnteredContextRewindScope();

   private:
    HandleScopeImplementer* hsi_;
    size_t saved_entered_context_count_;
  };

  HandleScopeImplementer() : spare_(nullptr) {}

  ~HandleScopeImplementer() { DeleteArray(spare_); }

  HandleScopeImplementer(const HandleScopeImplementer&) = delete;
  HandleScopeImplementer& operator=(const HandleScopeImplementer&) = delete;

  // Threading support for handle data.
  static int ArchiveSpacePerThread();
  char* RestoreThread(char* from);
  char* ArchiveThread(char* to);
  void FreeThreadResources();

  // Garbage collection support.
  V8_EXPORT_PRIVATE void Iterate(RootVisitor* v);
  V8_EXPORT_PRIVATE static char* Iterate(RootVisitor* v, char* data);

  inline internal::Address* GetSpareOrNewBlock();
  inline void DeleteExtensions(internal::Address* prev_limit);

  inline void EnterContext(Tagged<NativeContext> context);
  inline void LeaveContext();
  inline bool LastEnteredContextWas(Tagged<NativeContext> context);
  inline size_t EnteredContextCount() const { return entered_contexts_.size(); }

  // Returns the last entered context or an empty handle if no
  // contexts have been entered.
  inline DirectHandle<NativeContext> LastEnteredContext();

  inline void SaveContext(Tagged<Context> context);
  inline Tagged<Context> RestoreContext();
  inline bool HasSavedContexts();

  inline DetachableVector<Address*>* blocks() { return &blocks_; }

  void ReturnBlock(Address* block) {
    DCHECK_NOT_NULL(block);
    if (spare_ != nullptr) DeleteArray(spare_);
    spare_ = block;
  }

  static const size_t kEnteredContextsOffset;

 private:
  static constexpr Address kSentinel = kNullAddress;
  static constexpr Address* kInvalidLastHandle =
      const_cast<Address*>(&kSentinel);

  void ResetAfterArchive();

  void Free();
  void BeginPersistentScope();
  bool HasPersistentScope() const {
    return last_handle_before_persistent_block_ != kInvalidLastHandle;
  }
  std::unique_ptr<PersistentHandles> DetachPersistent(Address* first_block);

  DetachableVector<Address*> blocks_;

  // Used as a stack to keep track of entered contexts.
  DetachableVector<Tagged<NativeContext>> entered_contexts_;

  // Used as a stack to keep track of saved contexts.
  DetachableVector<Tagged<Context>> saved_contexts_;
  Address* spare_;
  Address* last_handle_before_persistent_block_ = kInvalidLastHandle;
  // This is only used for threading support.
  HandleScopeData handle_scope_data_;

  void IterateThis(RootVisitor* v);
  char* RestoreThreadHelper(char* from);
  char* ArchiveThreadHelper(char* to);

  friend class HandleScopeImplementerOffsets;
  friend class PersistentHandlesScope;
};

inline constexpr size_t HandleScopeImplementer::kEnteredContextsOffset =
    offsetof(HandleScopeImplementer, entered_contexts_);

static_assert(HandleScopeImplementer::kSizeInBytes ==
              sizeof(HandleScopeImplementer));

const int kHandleBlockSize = KB - 2;  // fit in one page

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_HANDLE_SCOPE_IMPLEMENTER_H_
