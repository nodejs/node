// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_OFF_THREAD_ISOLATE_H_
#define V8_EXECUTION_OFF_THREAD_ISOLATE_H_

#include "src/base/macros.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/off-thread-factory.h"
#include "src/heap/off-thread-heap.h"

namespace v8 {
namespace internal {

class Isolate;
class OffThreadLogger;
class OffThreadTransferHandleStorage;

class OffThreadTransferHandleBase {
 protected:
  explicit OffThreadTransferHandleBase(OffThreadTransferHandleStorage* storage)
      : storage_(storage) {}

  V8_EXPORT_PRIVATE Address* ToHandleLocation() const;

 private:
  OffThreadTransferHandleStorage* storage_;
};

// Helper class for transferring ownership of an off-thread allocated object's
// handler to the main thread. OffThreadTransferHandles should be created before
// the OffThreadIsolate is finished, and can be accessed as a Handle after the
// OffThreadIsolate is published.
template <typename T>
class OffThreadTransferHandle : public OffThreadTransferHandleBase {
 public:
  OffThreadTransferHandle() : OffThreadTransferHandleBase(nullptr) {}
  explicit OffThreadTransferHandle(OffThreadTransferHandleStorage* storage)
      : OffThreadTransferHandleBase(storage) {}

  Handle<T> ToHandle() const { return Handle<T>(ToHandleLocation()); }
};

template <typename T>
class OffThreadTransferMaybeHandle : public OffThreadTransferHandleBase {
 public:
  OffThreadTransferMaybeHandle() : OffThreadTransferHandleBase(nullptr) {}
  explicit OffThreadTransferMaybeHandle(OffThreadTransferHandleStorage* storage)
      : OffThreadTransferHandleBase(storage) {}

  MaybeHandle<T> ToHandle() const {
    Address* location = ToHandleLocation();
    return location ? Handle<T>(location) : MaybeHandle<T>();
  }
};

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

  static OffThreadIsolate* FromHeap(OffThreadHeap* heap) {
    return reinterpret_cast<OffThreadIsolate*>(
        reinterpret_cast<Address>(heap) - OFFSET_OF(OffThreadIsolate, heap_));
  }

  OffThreadHeap* heap() { return &heap_; }

  inline Address isolate_root() const;
  inline ReadOnlyHeap* read_only_heap();
  inline Object root(RootIndex index);

  v8::internal::OffThreadFactory* factory() {
    // Upcast to the privately inherited base-class using c-style casts to avoid
    // undefined behavior (as static_cast cannot cast across private bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (
        v8::internal::OffThreadFactory*)this;  // NOLINT(readability/casting)
  }

  // This method finishes the use of the off-thread Isolate, and can be safely
  // called off-thread.
  void FinishOffThread();

  // This method publishes the off-thread Isolate to the main-thread Isolate,
  // moving all off-thread allocated objects to be visible to the GC, and fixing
  // up any other state (e.g. internalized strings). This method must be called
  // on the main thread.
  void Publish(Isolate* isolate);

  bool has_pending_exception() const { return false; }

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

  template <typename T>
  OffThreadTransferHandle<T> TransferHandle(Handle<T> handle) {
    DCHECK_NOT_NULL(handle_zone_);
    if (handle.is_null()) {
      return OffThreadTransferHandle<T>();
    }
    return OffThreadTransferHandle<T>(heap()->AddTransferHandleStorage(handle));
  }

  template <typename T>
  OffThreadTransferMaybeHandle<T> TransferHandle(MaybeHandle<T> maybe_handle) {
    DCHECK_NOT_NULL(handle_zone_);
    Handle<T> handle;
    if (!maybe_handle.ToHandle(&handle)) {
      return OffThreadTransferMaybeHandle<T>();
    }
    return OffThreadTransferMaybeHandle<T>(
        heap()->AddTransferHandleStorage(handle));
  }

  int GetNextScriptId();
#if V8_SFI_HAS_UNIQUE_ID
  int GetNextUniqueSharedFunctionInfoId();
#endif  // V8_SFI_HAS_UNIQUE_ID

  bool is_collecting_type_profile();

  OffThreadLogger* logger() { return logger_.get(); }

  void PinToCurrentThread();
  ThreadId thread_id() { return thread_id_; }

 private:
  friend class v8::internal::OffThreadFactory;

  OffThreadHeap heap_;

  // TODO(leszeks): Extract out the fields of the Isolate we want and store
  // those instead of the whole thing.
  Isolate* isolate_;

  std::unique_ptr<OffThreadLogger> logger_;
  ThreadId thread_id_;
  Zone* handle_zone_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_OFF_THREAD_ISOLATE_H_
