// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_TESTING_H_
#define INCLUDE_CPPGC_TESTING_H_

#include "cppgc/common.h"
#include "cppgc/macros.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

class HeapHandle;

/**
 * Namespace contains testing helpers.
 */
namespace testing {

/**
 * Overrides the state of the stack with the provided value. Takes precedence
 * over other parameters that set the stack state. Must no be nested.
 */
class V8_EXPORT V8_NODISCARD OverrideEmbedderStackStateScope final {
  CPPGC_STACK_ALLOCATED();

 public:
  /**
   * Constructs a scoped object that automatically enters and leaves the scope.
   *
   * \param heap_handle The corresponding heap.
   */
  explicit OverrideEmbedderStackStateScope(HeapHandle& heap_handle,
                                           EmbedderStackState state);
  ~OverrideEmbedderStackStateScope();

  OverrideEmbedderStackStateScope(const OverrideEmbedderStackStateScope&) =
      delete;
  OverrideEmbedderStackStateScope& operator=(
      const OverrideEmbedderStackStateScope&) = delete;

 private:
  HeapHandle& heap_handle_;
};

}  // namespace testing
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_TESTING_H_
