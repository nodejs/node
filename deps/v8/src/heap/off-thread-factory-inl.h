// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OFF_THREAD_FACTORY_INL_H_
#define V8_HEAP_OFF_THREAD_FACTORY_INL_H_

#include "src/heap/off-thread-factory.h"

#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

#define ROOT_ACCESSOR(Type, name, CamelName)                \
  OffThreadHandle<Type> OffThreadFactory::name() {          \
    return OffThreadHandle<Type>(read_only_roots().name()); \
  }
READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#endif  // V8_HEAP_OFF_THREAD_FACTORY_INL_H_

}  // namespace internal
}  // namespace v8
