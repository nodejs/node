// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-inl.h"
#include "src/objects/cell.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/script.h"
#include "src/roots/roots-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {
AllocationSpace GetSpaceFromObject(Object object) {
  DCHECK(object.IsHeapObject());
  return MemoryChunk::FromHeapObject(HeapObject::cast(object))
      ->owner_identity();
}
}  // namespace

#define CHECK_IN_RO_SPACE(type, name, CamelName) \
  HeapObject name = roots.name();                \
  CHECK_EQ(RO_SPACE, GetSpaceFromObject(name));

// The following tests check that all the roots accessible via ReadOnlyRoots are
// in RO_SPACE.
TEST(TestReadOnlyRoots) {
  ReadOnlyRoots roots(CcTest::i_isolate());

  READ_ONLY_ROOT_LIST(CHECK_IN_RO_SPACE)
}

#undef CHECK_IN_RO_SPACE

namespace {
bool IsInitiallyMutable(Factory* factory, Address object_address) {
// Entries in this list are in STRONG_MUTABLE_MOVABLE_ROOT_LIST, but may
// initially point to objects that are in RO_SPACE.
#define INITIALLY_READ_ONLY_ROOT_LIST(V)  \
  V(api_private_symbol_table)             \
  V(api_symbol_table)                     \
  V(builtins_constants_table)             \
  V(current_microtask)                    \
  V(detached_contexts)                    \
  V(dirty_js_finalization_groups)         \
  V(feedback_vectors_for_profiling_tools) \
  V(materialized_objects)                 \
  V(noscript_shared_function_infos)       \
  V(public_symbol_table)                  \
  V(retained_maps)                        \
  V(retaining_path_targets)               \
  V(serialized_global_proxy_sizes)        \
  V(serialized_objects)                   \
  V(weak_refs_keep_during_job)

#define TEST_CAN_BE_READ_ONLY(name) \
  if (factory->name().address() == object_address) return false;
  INITIALLY_READ_ONLY_ROOT_LIST(TEST_CAN_BE_READ_ONLY)
#undef TEST_CAN_BE_READ_ONLY
#undef INITIALLY_READ_ONLY_ROOT_LIST
  return true;
}
}  // namespace

// The CHECK_EQ line is there just to ensure that the root is publicly
// accessible from Heap, but ultimately the factory is used as it provides
// handles that have the address in the root table.
#define CHECK_NOT_IN_RO_SPACE(type, name, CamelName)                         \
  Handle<Object> name = factory->name();                                     \
  CHECK_EQ(*name, heap->name());                                             \
  if (name->IsHeapObject() && IsInitiallyMutable(factory, name.address()) && \
      !name->IsUndefined(CcTest::i_isolate())) {                             \
    CHECK_NE(RO_SPACE, GetSpaceFromObject(HeapObject::cast(*name)));         \
  }

// The following tests check that all the roots accessible via public Heap
// accessors are not in RO_SPACE with the exception of the objects listed in
// INITIALLY_READ_ONLY_ROOT_LIST.
TEST(TestHeapRootsNotReadOnly) {
  Factory* factory = CcTest::i_isolate()->factory();
  Heap* heap = CcTest::i_isolate()->heap();

  MUTABLE_ROOT_LIST(CHECK_NOT_IN_RO_SPACE)
}

#undef CHECK_NOT_IN_RO_SPACE

}  // namespace internal
}  // namespace v8
