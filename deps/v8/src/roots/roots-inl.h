// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_ROOTS_INL_H_
#define V8_ROOTS_ROOTS_INL_H_

#include "src/execution/isolate.h"
#include "src/execution/local-isolate.h"
#include "src/handles/handles.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/heap-number.h"
#include "src/objects/literal-objects.h"
#include "src/objects/map.h"
#include "src/objects/oddball.h"
#include "src/objects/property-array.h"
#include "src/objects/property-cell.h"
#include "src/objects/scope-info.h"
#include "src/objects/slots.h"
#include "src/objects/string.h"
#include "src/objects/swiss-name-dictionary.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

V8_INLINE constexpr bool operator<(RootIndex lhs, RootIndex rhs) {
  using type = typename std::underlying_type<RootIndex>::type;
  return static_cast<type>(lhs) < static_cast<type>(rhs);
}

V8_INLINE RootIndex operator++(RootIndex& index) {
  using type = typename std::underlying_type<RootIndex>::type;
  index = static_cast<RootIndex>(static_cast<type>(index) + 1);
  return index;
}

bool RootsTable::IsRootHandleLocation(Address* handle_location,
                                      RootIndex* index) const {
  FullObjectSlot location(handle_location);
  FullObjectSlot first_root(&roots_[0]);
  FullObjectSlot last_root(&roots_[kEntriesCount]);
  if (location >= last_root) return false;
  if (location < first_root) return false;
  *index = static_cast<RootIndex>(location - first_root);
  return true;
}

template <typename T>
bool RootsTable::IsRootHandle(Handle<T> handle, RootIndex* index) const {
  // This can't use handle.location() because it is called from places
  // where handle dereferencing is disallowed. Comparing the handle's
  // location against the root handle list is safe though.
  Address* handle_location = reinterpret_cast<Address*>(handle.address());
  return IsRootHandleLocation(handle_location, index);
}

ReadOnlyRoots::ReadOnlyRoots(Heap* heap)
    : ReadOnlyRoots(Isolate::FromHeap(heap)) {}

ReadOnlyRoots::ReadOnlyRoots(Isolate* isolate)
    : read_only_roots_(reinterpret_cast<Address*>(
          isolate->roots_table().read_only_roots_begin().address())) {}

ReadOnlyRoots::ReadOnlyRoots(LocalIsolate* isolate)
    : ReadOnlyRoots(isolate->factory()->read_only_roots()) {}

// We use unchecked_cast below because we trust our read-only roots to
// have the right type, and to avoid the heavy #includes that would be
// required for checked casts.

#define ROOT_ACCESSOR(Type, name, CamelName)                  \
  Type ReadOnlyRoots::name() const {                          \
    DCHECK(CheckType_##name());                               \
    return unchecked_##name();                                \
  }                                                           \
  Type ReadOnlyRoots::unchecked_##name() const {              \
    return Type::unchecked_cast(                              \
        Object(*GetLocation(RootIndex::k##CamelName)));       \
  }                                                           \
  Handle<Type> ReadOnlyRoots::name##_handle() const {         \
    DCHECK(CheckType_##name());                               \
    Address* location = GetLocation(RootIndex::k##CamelName); \
    return Handle<Type>(location);                            \
  }

READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

Address* ReadOnlyRoots::GetLocation(RootIndex root_index) const {
  size_t index = static_cast<size_t>(root_index);
  DCHECK_LT(index, kEntriesCount);
  return &read_only_roots_[index];
}

Address ReadOnlyRoots::first_name_for_protector() const {
  return at(RootIndex::kFirstNameForProtector);
}

Address ReadOnlyRoots::last_name_for_protector() const {
  return at(RootIndex::kLastNameForProtector);
}

bool ReadOnlyRoots::IsNameForProtector(HeapObject object) const {
  return base::IsInRange(object.ptr(), first_name_for_protector(),
                         last_name_for_protector());
}

void ReadOnlyRoots::VerifyNameForProtectorsPages() const {
  // The symbols and strings that can cause protector invalidation should
  // reside on the same page so we can do a fast range check.
  CHECK_EQ(Page::FromAddress(first_name_for_protector()),
           Page::FromAddress(last_name_for_protector()));
}

Address ReadOnlyRoots::at(RootIndex root_index) const {
  return *GetLocation(root_index);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ROOTS_ROOTS_INL_H_
