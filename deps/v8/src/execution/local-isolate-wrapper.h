// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_LOCAL_ISOLATE_WRAPPER_H_
#define V8_EXECUTION_LOCAL_ISOLATE_WRAPPER_H_

#include "src/utils/pointer-with-payload.h"

namespace v8 {
namespace internal {

// LocalWrapperBase is the base-class for wrapper classes around a main-thread
// and off-thread type, e.g. Isolate and OffThreadIsolate, and a bit stating
// which of the two the wrapper wraps.
//
// The shared methods are defined on MethodCaller, which will dispatch to the
// right type depending on the state of the wrapper. The reason for a separate
// MethodCaller is to
//
//   a) Move the method definitions into an -inl.h so that this header can have
//      minimal dependencies, and
//   b) To allow the type methods to be called with operator-> (e.g.
//      isolate_wrapper->heap()), while forcing the wrapper methods to be called
//      with a dot (e.g. isolate_wrapper.is_main_thread()).
template <typename MainThreadType, typename OffThreadType,
          typename MethodCaller>
class LocalWrapperBase {
 public:
  // Helper for returning a MethodCaller* by value from operator->.
  class MethodCallerRef {
   public:
    MethodCaller* operator->() { return &caller_; }

   private:
    friend class LocalWrapperBase;
    explicit MethodCallerRef(LocalWrapperBase* wrapper) : caller_(wrapper) {}

    MethodCaller caller_;
  };

  explicit LocalWrapperBase(std::nullptr_t) : pointer_and_tag_(nullptr) {}
  explicit LocalWrapperBase(MainThreadType* pointer)
      : pointer_and_tag_(pointer, false) {}
  explicit LocalWrapperBase(OffThreadType* pointer)
      : pointer_and_tag_(pointer, true) {}

  MainThreadType* main_thread() {
    DCHECK(is_main_thread());
    return static_cast<MainThreadType*>(
        pointer_and_tag_.GetPointerWithKnownPayload(false));
  }
  OffThreadType* off_thread() {
    DCHECK(is_off_thread());
    return static_cast<OffThreadType*>(
        pointer_and_tag_.GetPointerWithKnownPayload(true));
  }

  bool is_main_thread() const {
    return !is_null() && !pointer_and_tag_.GetPayload();
  }
  bool is_off_thread() const {
    return !is_null() && pointer_and_tag_.GetPayload();
  }
  bool is_null() const { return pointer_and_tag_.GetPointer() == nullptr; }

  // Access the methods via wrapper->Method.
  MethodCallerRef operator->() { return MethodCallerRef(this); }

 private:
  PointerWithPayload<void, bool, 1> pointer_and_tag_;
};

using LocalHeapWrapper =
    LocalWrapperBase<class Heap, class OffThreadHeap, class HeapMethodCaller>;
using LocalLoggerWrapper = LocalWrapperBase<class Logger, class OffThreadLogger,
                                            class LoggerMethodCaller>;
using LocalIsolateWrapper =
    LocalWrapperBase<class Isolate, class OffThreadIsolate,
                     class IsolateMethodCaller>;

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_LOCAL_ISOLATE_WRAPPER_H_
