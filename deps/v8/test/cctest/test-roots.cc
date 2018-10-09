// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"
#include "src/roots-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {
AllocationSpace GetSpaceFromObject(Object* object) {
  DCHECK(object->IsHeapObject());
  return MemoryChunk::FromHeapObject(HeapObject::cast(object))
      ->owner()
      ->identity();
}
}  // namespace

#define CHECK_IN_RO_SPACE(name)    \
  HeapObject* name = roots.name(); \
  CHECK_EQ(RO_SPACE, GetSpaceFromObject(name));

// The following tests check that all the roots accessible via ReadOnlyRoots are
// in RO_SPACE.
TEST(TestStrongReadOnlyRoots) {
  ReadOnlyRoots roots(CcTest::i_isolate());

#define TEST_ROOT(type, name, camel_name) CHECK_IN_RO_SPACE(name)
  STRONG_READ_ONLY_ROOT_LIST(TEST_ROOT)
#undef TEST_ROOT
}

TEST(TestInternalizedStrings) {
  ReadOnlyRoots roots(CcTest::i_isolate());

#define TEST_ROOT(name, str) CHECK_IN_RO_SPACE(name)
  INTERNALIZED_STRING_LIST(TEST_ROOT)
#undef TEST_ROOT
}

TEST(TestPrivateSymbols) {
  ReadOnlyRoots roots(CcTest::i_isolate());

  PRIVATE_SYMBOL_LIST(CHECK_IN_RO_SPACE)
}

TEST(TestPublicSymbols) {
  ReadOnlyRoots roots(CcTest::i_isolate());

#define TEST_ROOT(name, description) CHECK_IN_RO_SPACE(name)
  PUBLIC_SYMBOL_LIST(TEST_ROOT)
  WELL_KNOWN_SYMBOL_LIST(TEST_ROOT)
#undef TEST_ROOT
}

TEST(TestStructMaps) {
  ReadOnlyRoots roots(CcTest::i_isolate());

#define TEST_ROOT(NAME, Name, name) CHECK_IN_RO_SPACE(name##_map)
  STRUCT_LIST(TEST_ROOT)
#undef TEST_ROOT
}

TEST(TestAllocationSiteMaps) {
  ReadOnlyRoots roots(CcTest::i_isolate());

#define TEST_ROOT(NAME, Name, Size, name) CHECK_IN_RO_SPACE(name##_map)
  ALLOCATION_SITE_LIST(TEST_ROOT)
#undef TEST_ROOT
}

#undef CHECK_IN_RO_SPACE

namespace {
bool IsInitiallyMutable(Factory* factory, Address object_address) {
// Entries in this list are in STRONG_MUTABLE_ROOT_LIST, but may initially point
// to objects that in RO_SPACE.
#define INITIALLY_READ_ONLY_ROOT_LIST(V)  \
  V(builtins_constants_table)             \
  V(detached_contexts)                    \
  V(feedback_vectors_for_profiling_tools) \
  V(materialized_objects)                 \
  V(microtask_queue)                      \
  V(noscript_shared_function_infos)       \
  V(retained_maps)                        \
  V(retaining_path_targets)               \
  V(serialized_global_proxy_sizes)        \
  V(serialized_objects)

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
#define CHECK_NOT_IN_RO_SPACE(name)                                        \
  Handle<Object> name = factory->name();                                   \
  CHECK_EQ(*name, heap->name());                                           \
  if (name->IsHeapObject() && IsInitiallyMutable(factory, name.address())) \
    CHECK_NE(RO_SPACE,                                                     \
             GetSpaceFromObject(reinterpret_cast<HeapObject*>(*name)));

// The following tests check that all the roots accessible via public Heap
// accessors are not in RO_SPACE with the exception of the objects listed in
// INITIALLY_READ_ONLY_ROOT_LIST.
TEST(TestHeapRootsNotReadOnly) {
  Factory* factory = CcTest::i_isolate()->factory();
  Heap* heap = CcTest::i_isolate()->heap();

#define TEST_ROOT(type, name, camel_name) CHECK_NOT_IN_RO_SPACE(name)
  MUTABLE_ROOT_LIST(TEST_ROOT)
#undef TEST_ROOT
}

TEST(TestAccessorInfosNotReadOnly) {
  Factory* factory = CcTest::i_isolate()->factory();
  Heap* heap = CcTest::i_isolate()->heap();

#define TEST_ROOT(name, AccessorName) CHECK_NOT_IN_RO_SPACE(name##_accessor)
  ACCESSOR_INFO_LIST(TEST_ROOT)
#undef TEST_ROOT
}

#undef CHECK_NOT_IN_RO_SPACE

}  // namespace internal
}  // namespace v8
