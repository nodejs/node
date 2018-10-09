// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_INL_H_
#define V8_ROOTS_INL_H_

#include "src/roots.h"

#include "src/heap/heap-inl.h"
#include "src/objects/api-callbacks.h"

namespace v8 {

namespace internal {

ReadOnlyRoots::ReadOnlyRoots(Isolate* isolate) : heap_(isolate->heap()) {}

#define ROOT_ACCESSOR(type, name, camel_name)                              \
  type* ReadOnlyRoots::name() {                                            \
    return type::cast(heap_->roots_[Heap::k##camel_name##RootIndex]);      \
  }                                                                        \
  Handle<type> ReadOnlyRoots::name##_handle() {                            \
    return Handle<type>(                                                   \
        bit_cast<type**>(&heap_->roots_[Heap::k##camel_name##RootIndex])); \
  }
STRONG_READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRING_ACCESSOR(name, str)                                     \
  String* ReadOnlyRoots::name() {                                      \
    return String::cast(heap_->roots_[Heap::k##name##RootIndex]);      \
  }                                                                    \
  Handle<String> ReadOnlyRoots::name##_handle() {                      \
    return Handle<String>(                                             \
        bit_cast<String**>(&heap_->roots_[Heap::k##name##RootIndex])); \
  }
INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name)                                          \
  Symbol* ReadOnlyRoots::name() {                                      \
    return Symbol::cast(heap_->roots_[Heap::k##name##RootIndex]);      \
  }                                                                    \
  Handle<Symbol> ReadOnlyRoots::name##_handle() {                      \
    return Handle<Symbol>(                                             \
        bit_cast<Symbol**>(&heap_->roots_[Heap::k##name##RootIndex])); \
  }
PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description)                             \
  Symbol* ReadOnlyRoots::name() {                                      \
    return Symbol::cast(heap_->roots_[Heap::k##name##RootIndex]);      \
  }                                                                    \
  Handle<Symbol> ReadOnlyRoots::name##_handle() {                      \
    return Handle<Symbol>(                                             \
        bit_cast<Symbol**>(&heap_->roots_[Heap::k##name##RootIndex])); \
  }
PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name)                          \
  Map* ReadOnlyRoots::name##_map() {                                   \
    return Map::cast(heap_->roots_[Heap::k##Name##MapRootIndex]);      \
  }                                                                    \
  Handle<Map> ReadOnlyRoots::name##_map_handle() {                     \
    return Handle<Map>(                                                \
        bit_cast<Map**>(&heap_->roots_[Heap::k##Name##MapRootIndex])); \
  }
STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define ALLOCATION_SITE_MAP_ACCESSOR(NAME, Name, Size, name)                 \
  Map* ReadOnlyRoots::name##_map() {                                         \
    return Map::cast(heap_->roots_[Heap::k##Name##Size##MapRootIndex]);      \
  }                                                                          \
  Handle<Map> ReadOnlyRoots::name##_map_handle() {                           \
    return Handle<Map>(                                                      \
        bit_cast<Map**>(&heap_->roots_[Heap::k##Name##Size##MapRootIndex])); \
  }
ALLOCATION_SITE_LIST(ALLOCATION_SITE_MAP_ACCESSOR)
#undef ALLOCATION_SITE_MAP_ACCESSOR

FixedTypedArrayBase* ReadOnlyRoots::EmptyFixedTypedArrayForMap(const Map* map) {
  // TODO(delphick): All of these empty fixed type arrays are in RO_SPACE so
  // this the method below can be moved into ReadOnlyRoots.
  return heap_->EmptyFixedTypedArrayForMap(map);
}

}  // namespace internal

}  // namespace v8

#endif  // V8_ROOTS_INL_H_
