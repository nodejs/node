// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OFF_THREAD_FACTORY_INL_H_
#define V8_HEAP_OFF_THREAD_FACTORY_INL_H_

#include "src/heap/off-thread-factory.h"

#include "src/heap/factory-base-inl.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

#define ROOT_ACCESSOR(Type, name, CamelName)  \
  Handle<Type> OffThreadFactory::name() {     \
    return read_only_roots().name##_handle(); \
  }
READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define ACCESSOR_INFO_ACCESSOR(Type, name, CamelName)                          \
  Handle<Type> OffThreadFactory::name() {                                      \
    /* Do a bit of handle location magic to cast the Handle without having */  \
    /* to pull in Type::cast. We know the type is right by construction.   */  \
    return Handle<Type>(                                                       \
        isolate()->isolate_->root_handle(RootIndex::k##CamelName).location()); \
  }
ACCESSOR_INFO_ROOT_LIST(ACCESSOR_INFO_ACCESSOR)
#undef ACCESSOR_INFO_ACCESSOR

#endif  // V8_HEAP_OFF_THREAD_FACTORY_INL_H_

}  // namespace internal
}  // namespace v8
