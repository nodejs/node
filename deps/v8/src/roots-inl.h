// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_INL_H_
#define V8_ROOTS_INL_H_

#include "src/roots.h"

#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

V8_INLINE bool operator<(RootIndex lhs, RootIndex rhs) {
  typedef typename std::underlying_type<RootIndex>::type type;
  return static_cast<type>(lhs) < static_cast<type>(rhs);
}

V8_INLINE RootIndex operator++(RootIndex& index) {
  typedef typename std::underlying_type<RootIndex>::type type;
  index = static_cast<RootIndex>(static_cast<type>(index) + 1);
  return index;
}

ReadOnlyRoots::ReadOnlyRoots(Heap* heap) : roots_table_(heap->roots_table()) {}

ReadOnlyRoots::ReadOnlyRoots(Isolate* isolate)
    : roots_table_(isolate->heap()->roots_table()) {}

#define ROOT_ACCESSOR(type, name, CamelName)                       \
  type* ReadOnlyRoots::name() {                                    \
    return type::cast(roots_table_[RootIndex::k##CamelName]);      \
  }                                                                \
  Handle<type> ReadOnlyRoots::name##_handle() {                    \
    return Handle<type>(                                           \
        bit_cast<type**>(&roots_table_[RootIndex::k##CamelName])); \
  }

READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

Map* ReadOnlyRoots::MapForFixedTypedArray(ExternalArrayType array_type) {
  RootIndex root_index = RootsTable::RootIndexForFixedTypedArray(array_type);
  return Map::cast(roots_table_[root_index]);
}

Map* ReadOnlyRoots::MapForFixedTypedArray(ElementsKind elements_kind) {
  RootIndex root_index = RootsTable::RootIndexForFixedTypedArray(elements_kind);
  return Map::cast(roots_table_[root_index]);
}

FixedTypedArrayBase* ReadOnlyRoots::EmptyFixedTypedArrayForMap(const Map* map) {
  RootIndex root_index =
      RootsTable::RootIndexForEmptyFixedTypedArray(map->elements_kind());
  return FixedTypedArrayBase::cast(roots_table_[root_index]);
}

Object** RootsTable::read_only_roots_end() {
// Enumerate the read-only roots into an expression of the form:
// (root_1, root_2, root_3, ..., root_n)
// This evaluates to root_n, but Clang warns that the other values in the list
// are unused so suppress that warning.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#endif
#define ROOT(type, name, CamelName) , RootIndex::k##CamelName
  constexpr RootIndex kLastReadOnlyRoot =
      (RootIndex::kFirstRoot READ_ONLY_ROOT_LIST(ROOT));
#undef ROOT
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
  return &roots_[static_cast<size_t>(kLastReadOnlyRoot) + 1];
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ROOTS_INL_H_
