// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/roots/roots.h"

#include "src/objects/elements-kind.h"
#include "src/objects/objects-inl.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

const char* RootsTable::root_names_[RootsTable::kEntriesCount] = {
#define ROOT_NAME(type, name, CamelName) #name,
    ROOT_LIST(ROOT_NAME)
#undef ROOT_NAME
};

void ReadOnlyRoots::Iterate(RootVisitor* visitor) {
  visitor->VisitRootPointers(Root::kReadOnlyRootList, nullptr,
                             FullObjectSlot(read_only_roots_),
                             FullObjectSlot(&read_only_roots_[kEntriesCount]));
  visitor->Synchronize(VisitorSynchronization::kReadOnlyRootList);
}

#ifdef DEBUG

bool ReadOnlyRoots::CheckType(RootIndex index) const {
  Object root(at(index));
  switch (index) {
#define CHECKTYPE(Type, name, CamelName) \
  case RootIndex::k##CamelName:          \
    return root.Is##Type();
    READ_ONLY_ROOT_LIST(CHECKTYPE)
#undef CHECKTYPE

    default:
      UNREACHABLE();
      return false;
  }
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
