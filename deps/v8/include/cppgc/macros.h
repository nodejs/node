// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_MACROS_H_
#define INCLUDE_CPPGC_MACROS_H_

#include <cstddef>

#include "cppgc/internal/compiler-specific.h"

namespace cppgc {

#define CPPGC_DISALLOW_NEW()                                      \
 public:                                                          \
  using IsDisallowNewMarker CPPGC_UNUSED = int;                   \
  void* operator new(size_t, void* location) { return location; } \
  void* operator new(size_t) = delete;                            \
  static_assert(true, "Force semicolon.")

// Use CPPGC_STACK_ALLOCATED if the object is only stack allocated.
// Add the CPPGC_STACK_ALLOCATED_IGNORE annotation on a case-by-case basis when
// enforcement of CPPGC_STACK_ALLOCATED should be suppressed.
#if defined(__clang__)

#define CPPGC_STACK_ALLOCATED()                        \
 public:                                               \
  using IsStackAllocatedTypeMarker CPPGC_UNUSED = int; \
                                                       \
 private:                                              \
  void* operator new(size_t) = delete;                 \
  void* operator new(size_t, void*) = delete;          \
  static_assert(true, "Force semicolon.")

#define CPPGC_STACK_ALLOCATED_IGNORE(bug_or_reason) \
  __attribute__((annotate("stack_allocated_ignore")))

#define CPPGC_PLUGIN_IGNORE(bug_or_reason)           \
  __attribute__((annotate("blink_gc_plugin_ignore"), \
                 annotate("stack_allocated_ignore")))

#else  // !defined(__clang__)

#define CPPGC_STACK_ALLOCATED() static_assert(true, "Force semicolon.")
#define CPPGC_STACK_ALLOCATED_IGNORE(bug_or_reason)
#define CPPGC_PLUGIN_IGNORE(bug_or_reason)

#endif  // !defined(__clang__)

template <typename T>
concept IsStackAllocatedType =
    requires { typename T::IsStackAllocatedTypeMarker; };

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_MACROS_H_
