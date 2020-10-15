// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_LOCAL_ISOLATE_H_
#define V8_EXECUTION_LOCAL_ISOLATE_H_

#include "src/base/macros.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles.h"
#include "src/handles/local-handles.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/local-factory.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

class Isolate;
class LocalLogger;

// HiddenLocalFactory parallels Isolate's HiddenFactory
class V8_EXPORT_PRIVATE HiddenLocalFactory : private LocalFactory {
 public:
  // Forward constructors.
  using LocalFactory::LocalFactory;
};

// And Isolate-like class that can be passed in to templated methods that need
// an isolate syntactically, but are usable off-thread.
//
// This class holds an LocalFactory, but is otherwise effectively a stub
// implementation of an Isolate. In particular, it doesn't allow throwing
// exceptions, and hard crashes if you try.
class V8_EXPORT_PRIVATE LocalIsolate final : private HiddenLocalFactory {
 public:
  using HandleScopeType = LocalHandleScope;

  explicit LocalIsolate(Isolate* isolate);
  ~LocalIsolate();

  // Kinda sketchy.
  static LocalIsolate* FromHeap(LocalHeap* heap) {
    return reinterpret_cast<LocalIsolate*>(reinterpret_cast<Address>(heap) -
                                           OFFSET_OF(LocalIsolate, heap_));
  }

  LocalHeap* heap() { return &heap_; }

  inline Address isolate_root() const;
  inline ReadOnlyHeap* read_only_heap();
  inline Object root(RootIndex index);

  StringTable* string_table() { return isolate_->string_table(); }

  const Isolate* GetIsolateForPtrCompr() const { return isolate_; }

  v8::internal::LocalFactory* factory() {
    // Upcast to the privately inherited base-class using c-style casts to avoid
    // undefined behavior (as static_cast cannot cast across private bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (v8::internal::LocalFactory*)this;  // NOLINT(readability/casting)
  }

  bool has_pending_exception() const { return false; }

  template <typename T>
  Handle<T> Throw(Handle<Object> exception) {
    UNREACHABLE();
  }
  [[noreturn]] void FatalProcessOutOfHeapMemory(const char* location) {
    UNREACHABLE();
  }

  int GetNextScriptId();
#if V8_SFI_HAS_UNIQUE_ID
  int GetNextUniqueSharedFunctionInfoId();
#endif  // V8_SFI_HAS_UNIQUE_ID

  bool is_collecting_type_profile();

  LocalLogger* logger() { return logger_.get(); }
  ThreadId thread_id() { return thread_id_; }

 private:
  friend class v8::internal::LocalFactory;

  LocalHeap heap_;

  // TODO(leszeks): Extract out the fields of the Isolate we want and store
  // those instead of the whole thing.
  Isolate* isolate_;

  std::unique_ptr<LocalLogger> logger_;
  ThreadId thread_id_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_LOCAL_ISOLATE_H_
