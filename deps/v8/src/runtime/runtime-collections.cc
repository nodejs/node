// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_TheHole) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return ReadOnlyRoots(isolate).the_hole_value();
}

RUNTIME_FUNCTION(Runtime_SetGrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()), isolate);
  table = OrderedHashSet::EnsureGrowable(isolate, table);
  holder->set_table(*table);
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetShrink) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()), isolate);
  table = OrderedHashSet::Shrink(isolate, table);
  holder->set_table(*table);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_MapShrink) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()), isolate);
  table = OrderedHashMap::Shrink(isolate, table);
  holder->set_table(*table);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_MapGrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()), isolate);
  table = OrderedHashMap::EnsureGrowable(isolate, table);
  holder->set_table(*table);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WeakCollectionDelete) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_SMI_ARG_CHECKED(hash, 2)

#ifdef DEBUG
  DCHECK(key->IsJSReceiver());
  DCHECK(EphemeronHashTableShape::IsLive(ReadOnlyRoots(isolate), *key));
  Handle<EphemeronHashTable> table(
      EphemeronHashTable::cast(weak_collection->table()), isolate);
  // Should only be called when shrinking the table is necessary. See
  // HashTable::Shrink().
  DCHECK(table->NumberOfElements() - 1 <= (table->Capacity() >> 2) &&
         table->NumberOfElements() - 1 >= 16);
#endif

  bool was_present = JSWeakCollection::Delete(weak_collection, key, hash);
  return isolate->heap()->ToBoolean(was_present);
}

RUNTIME_FUNCTION(Runtime_WeakCollectionSet) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(hash, 3)

#ifdef DEBUG
  DCHECK(key->IsJSReceiver());
  DCHECK(EphemeronHashTableShape::IsLive(ReadOnlyRoots(isolate), *key));
  Handle<EphemeronHashTable> table(
      EphemeronHashTable::cast(weak_collection->table()), isolate);
  // Should only be called when rehashing or resizing the table is necessary.
  // See EphemeronHashTable::Put() and HashTable::HasSufficientCapacityToAdd().
  DCHECK((table->NumberOfDeletedElements() << 1) > table->NumberOfElements() ||
         !table->HasSufficientCapacityToAdd(1));
#endif

  JSWeakCollection::Set(weak_collection, key, value, hash);
  return *weak_collection;
}

}  // namespace internal
}  // namespace v8
