// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_MACROS_H_
#define INCLUDE_CPPGC_MACROS_H_

#include <stddef.h>

#include "cppgc/internal/compiler-specific.h"

namespace cppgc {

// Use if the object is only stack allocated.
#define CPPGC_STACK_ALLOCATED()                        \
 public:                                               \
  using IsStackAllocatedTypeMarker CPPGC_UNUSED = int; \
                                                       \
 private:                                              \
  void* operator new(size_t) = delete;                 \
  void* operator new(size_t, void*) = delete;          \
  static_assert(true, "Force semicolon.")

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_MACROS_H_
