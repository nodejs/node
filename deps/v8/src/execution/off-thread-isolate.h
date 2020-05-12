// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_OFF_THREAD_ISOLATE_H_
#define V8_EXECUTION_OFF_THREAD_ISOLATE_H_

#include "src/base/logging.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles.h"
#include "src/heap/off-thread-factory.h"

namespace v8 {
namespace internal {

class Isolate;
class OffThreadLogger;

// HiddenOffThreadFactory parallels Isolate's HiddenFactory
class V8_EXPORT_PRIVATE HiddenOffThreadFactory : private OffThreadFactory {
 public:
  // Forward constructors.
  using OffThreadFactory::OffThreadFactory;
};

// And Isolate-like class that can be passed in to templated methods that need
// an isolate syntactically, but are usable off-thread.
//
// This class holds an OffThreadFactory, but is otherwise effectively a stub
// implementation of an Isolate. In particular, it doesn't allow throwing
// exceptions, and hard crashes if you try.
class V8_EXPORT_PRIVATE OffThreadIsolate final
    : private HiddenOffThreadFactory {
 public:
  using HandleScopeType = OffThreadHandleScope;

  explicit OffThreadIsolate(Isolate* isolate, Zone* zone);
  ~OffThreadIsolate();

  v8::internal::OffThreadFactory* factory() {
    // Upcast to the privately inherited base-class using c-style casts to avoid
    // undefined behavior (as static_cast cannot cast across private bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (
        v8::internal::OffThreadFactory*)this;  // NOLINT(readability/casting)
  }

  // This method finishes the use of the off-thread Isolate, and can be safely
  // called off-thread.
  void FinishOffThread() {
    factory()->FinishOffThread();
    handle_zone_ = nullptr;
  }

  template <typename T>
  Handle<T> Throw(Handle<Object> exception) {
    UNREACHABLE();
  }
  [[noreturn]] void FatalProcessOutOfHeapMemory(const char* location) {
    UNREACHABLE();
  }

  Address* NewHandle(Address object) {
    DCHECK_NOT_NULL(handle_zone_);
    Address* location =
        static_cast<Address*>(handle_zone_->New(sizeof(Address)));
    *location = object;
    return location;
  }

  int GetNextScriptId();
#if V8_SFI_HAS_UNIQUE_ID
  int GetNextUniqueSharedFunctionInfoId();
#endif  // V8_SFI_HAS_UNIQUE_ID

  bool NeedsSourcePositionsForProfiling();
  bool is_collecting_type_profile();

  OffThreadLogger* logger() { return logger_; }

  void PinToCurrentThread();
  ThreadId thread_id() { return thread_id_; }

 private:
  friend class v8::internal::OffThreadFactory;

  // TODO(leszeks): Extract out the fields of the Isolate we want and store
  // those instead of the whole thing.
  Isolate* isolate_;

  OffThreadLogger* logger_;
  ThreadId thread_id_;
  Zone* handle_zone_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_OFF_THREAD_ISOLATE_H_
