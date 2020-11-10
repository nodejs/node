// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PREFINALIZER_HANDLER_H_
#define V8_HEAP_CPPGC_PREFINALIZER_HANDLER_H_

#include <vector>

#include "include/cppgc/prefinalizer.h"

namespace cppgc {
namespace internal {

class PreFinalizerHandler final {
 public:
  using PreFinalizer =
      cppgc::internal::PreFinalizerRegistrationDispatcher::PreFinalizer;

  PreFinalizerHandler();

  void RegisterPrefinalizer(PreFinalizer prefinalzier);

  void InvokePreFinalizers();

 private:
  // Checks that the current thread is the thread that created the heap.
  bool CurrentThreadIsCreationThread();

  // Pre-finalizers are called in the reverse order in which they are
  // registered by the constructors (including constructors of Mixin
  // objects) for an object, by processing the ordered_pre_finalizers_
  // back-to-front.
  std::vector<PreFinalizer> ordered_pre_finalizers_;

#ifdef DEBUG
  int creation_thread_id_;
#endif
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PREFINALIZER_HANDLER_H_
