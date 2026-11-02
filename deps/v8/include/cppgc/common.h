// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_COMMON_H_
#define INCLUDE_CPPGC_COMMON_H_

#include "cppgc/macros.h"
#include "v8config.h"  // NOLINT(build/include_directory)

#if V8_CC_MSVC
#include <intrin.h>  // _AddressOfReturnAddress()
#endif

namespace cppgc {

/**
 *  Indicator for the stack state of the embedder.
 */
enum class EmbedderStackState {
  /**
   * Stack may contain interesting heap pointers.
   */
  kMayContainHeapPointers,
  /**
   * Stack does not contain any interesting heap pointers.
   */
  kNoHeapPointers,
};

/**
 * A stack-allocated scope that sets the current thread's stack start address
 * for conservative stack scanning and restores the previous stack start on
 * destruction.
 */
class V8_EXPORT V8_NODISCARD StackStartScope final {
  CPPGC_STACK_ALLOCATED();

 public:
#if V8_CC_MSVC
  explicit StackStartScope(void* stack_start = _AddressOfReturnAddress());
#else
  explicit StackStartScope(void* stack_start = __builtin_frame_address(0));
#endif
  ~StackStartScope();

  StackStartScope(const StackStartScope&) = delete;
  StackStartScope& operator=(const StackStartScope&) = delete;

 private:
  void* const previous_stack_start_;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_COMMON_H_
