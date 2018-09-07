// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/conversions-inl.h"
#include "src/heap/factory.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-collection-inl.h"

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

RUNTIME_FUNCTION(Runtime_SetIteratorClone) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  return *isolate->factory()->NewJSSetIterator(
      handle(holder->map(), isolate),
      handle(OrderedHashSet::cast(holder->table()), isolate),
      Smi::ToInt(holder->index()));
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

RUNTIME_FUNCTION(Runtime_MapIteratorClone) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  return *isolate->factory()->NewJSMapIterator(
      handle(holder->map(), isolate),
      handle(OrderedHashMap::cast(holder->table()), isolate),
      Smi::ToInt(holder->index()));
}

RUNTIME_FUNCTION(Runtime_GetWeakMapEntries) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, holder, 0);
  CONVERT_NUMBER_CHECKED(int, max_entries, Int32, args[1]);
  CHECK_GE(max_entries, 0);
  return *JSWeakCollection::GetEntries(holder, max_entries);
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

RUNTIME_FUNCTION(Runtime_GetWeakSetValues) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, holder, 0);
  CONVERT_NUMBER_CHECKED(int, max_values, Int32, args[1]);
  CHECK_GE(max_values, 0);
  return *JSWeakCollection::GetEntries(holder, max_values);
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

RUNTIME_FUNCTION(Runtime_IsJSWeakMap) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSWeakMap());
}

RUNTIME_FUNCTION(Runtime_IsJSWeakSet) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSWeakSet());
}

}  // namespace internal
}  // namespace v8
