// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/roots.h"

#include "src/elements-kind.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

const char* RootsTable::root_names_[RootsTable::kEntriesCount] = {
#define ROOT_NAME(type, name, CamelName) #name,
    ROOT_LIST(ROOT_NAME)
#undef ROOT_NAME
};

// static
RootIndex RootsTable::RootIndexForFixedTypedArray(
    ExternalArrayType array_type) {
  switch (array_type) {
#define ARRAY_TYPE_TO_ROOT_INDEX(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                            \
    return RootIndex::kFixed##Type##ArrayMap;

    TYPED_ARRAYS(ARRAY_TYPE_TO_ROOT_INDEX)
#undef ARRAY_TYPE_TO_ROOT_INDEX
  }
  UNREACHABLE();
}

// static
RootIndex RootsTable::RootIndexForFixedTypedArray(ElementsKind elements_kind) {
  switch (elements_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    return RootIndex::kFixed##Type##ArrayMap;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}

// static
RootIndex RootsTable::RootIndexForEmptyFixedTypedArray(
    ElementsKind elements_kind) {
  switch (elements_kind) {
#define ELEMENT_KIND_TO_ROOT_INDEX(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                                     \
    return RootIndex::kEmptyFixed##Type##Array;

    TYPED_ARRAYS(ELEMENT_KIND_TO_ROOT_INDEX)
#undef ELEMENT_KIND_TO_ROOT_INDEX
    default:
      UNREACHABLE();
  }
}

void ReadOnlyRoots::Iterate(RootVisitor* visitor) {
  visitor->VisitRootPointers(Root::kReadOnlyRootList, nullptr,
                             roots_table_.read_only_roots_begin(),
                             roots_table_.read_only_roots_end());
  visitor->Synchronize(VisitorSynchronization::kReadOnlyRootList);
}

}  // namespace internal
}  // namespace v8
