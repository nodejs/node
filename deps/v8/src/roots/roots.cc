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

MapWord ReadOnlyRoots::one_pointer_filler_map_word() {
  return MapWord::FromMap(one_pointer_filler_map());
}

void ReadOnlyRoots::Iterate(RootVisitor* visitor) {
  visitor->VisitRootPointers(Root::kReadOnlyRootList, nullptr,
                             FullObjectSlot(read_only_roots_),
                             FullObjectSlot(&read_only_roots_[kEntriesCount]));
  visitor->Synchronize(VisitorSynchronization::kReadOnlyRootList);
}

#ifdef DEBUG
void ReadOnlyRoots::VerifyNameForProtectors() {
  DisallowGarbageCollection no_gc;
  Name prev;
  for (RootIndex root_index = RootIndex::kFirstNameForProtector;
       root_index <= RootIndex::kLastNameForProtector; ++root_index) {
    Name current = Name::cast(Object(at(root_index)));
    DCHECK(IsNameForProtector(current));
    if (root_index != RootIndex::kFirstNameForProtector) {
      // Make sure the objects are adjacent in memory.
      CHECK_LT(prev.address(), current.address());
      Address computed_address =
          prev.address() + ALIGN_TO_ALLOCATION_ALIGNMENT(prev.Size());
      CHECK_EQ(computed_address, current.address());
    }
    prev = current;
  }
}

#define ROOT_TYPE_CHECK(Type, name, CamelName)   \
  bool ReadOnlyRoots::CheckType_##name() const { \
    return unchecked_##name().Is##Type();        \
  }

READ_ONLY_ROOT_LIST(ROOT_TYPE_CHECK)
#undef ROOT_TYPE_CHECK
#endif

}  // namespace internal
}  // namespace v8
