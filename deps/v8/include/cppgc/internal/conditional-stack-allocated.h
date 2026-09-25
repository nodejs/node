// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_CONDITIONAL_STACK_ALLOCATED_H_
#define INCLUDE_CPPGC_INTERNAL_CONDITIONAL_STACK_ALLOCATED_H_

#include <type_traits>

#include "cppgc/macros.h"       // NOLINT(build/include_directory)
#include "cppgc/type-traits.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

// Base class that is marked as stack allocated if T is either marked as stack
// allocated or a traceable type.
template <typename T>
class ConditionalStackAllocatedBase;

template <typename T>
concept RequiresStackAllocated =
    !std::is_void_v<T> &&
    (cppgc::IsStackAllocatedType<T> || cppgc::internal::IsTraceableV<T> ||
     cppgc::IsGarbageCollectedOrMixinTypeV<T>);

template <typename T>
  requires(RequiresStackAllocated<T>)
class ConditionalStackAllocatedBase<T> {
 public:
  CPPGC_STACK_ALLOCATED();
};

template <typename T>
  requires(!RequiresStackAllocated<T>)
class ConditionalStackAllocatedBase<T> {};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_CONDITIONAL_STACK_ALLOCATED_H_
