// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/runtime/runtime-utils.h"


namespace v8 {
namespace internal {


RUNTIME_FUNCTION(Runtime_StringGetRawHashField) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  return *isolate->factory()->NewNumberFromUint(string->hash_field());
}


RUNTIME_FUNCTION(Runtime_TheHole) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->heap()->the_hole_value();
}


RUNTIME_FUNCTION(Runtime_FixedArrayGet) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(FixedArray, object, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  return object->get(index);
}


RUNTIME_FUNCTION(Runtime_FixedArraySet) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, object, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_CHECKED(Object, value, 2);
  object->set(index, value);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_JSCollectionGetTable) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  RUNTIME_ASSERT(object->IsJSSet() || object->IsJSMap());
  return static_cast<JSCollection*>(object)->table();
}


RUNTIME_FUNCTION(Runtime_GenericHash) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<Smi> hash = Object::GetOrCreateHash(isolate, object);
  return *hash;
}


RUNTIME_FUNCTION(Runtime_SetInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table = isolate->factory()->NewOrderedHashSet();
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_SetGrow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::EnsureGrowable(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetShrink) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Shrink(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetClear) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Clear(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetIteratorInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, set, 1);
  CONVERT_SMI_ARG_CHECKED(kind, 2)
  RUNTIME_ASSERT(kind == JSSetIterator::kKindValues ||
                 kind == JSSetIterator::kKindEntries);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(set->table()));
  holder->set_table(*table);
  holder->set_index(Smi::FromInt(0));
  holder->set_kind(Smi::FromInt(kind));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetIteratorClone) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);

  Handle<JSSetIterator> result = isolate->factory()->NewJSSetIterator();
  result->set_table(holder->table());
  result->set_index(Smi::FromInt(Smi::cast(holder->index())->value()));
  result->set_kind(Smi::FromInt(Smi::cast(holder->kind())->value()));

  return *result;
}


RUNTIME_FUNCTION(Runtime_SetIteratorNext) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSSetIterator, holder, 0);
  CONVERT_ARG_CHECKED(JSArray, value_array, 1);
  return holder->Next(value_array);
}


// The array returned contains the following information:
// 0: HasMore flag
// 1: Iteration index
// 2: Iteration kind
RUNTIME_FUNCTION(Runtime_SetIteratorDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(4);
  details->set(0, isolate->heap()->ToBoolean(holder->HasMore()));
  details->set(1, holder->index());
  details->set(2, holder->kind());
  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_MapInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table = isolate->factory()->NewOrderedHashMap();
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_MapShrink) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  table = OrderedHashMap::Shrink(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapClear) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  table = OrderedHashMap::Clear(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapGrow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  table = OrderedHashMap::EnsureGrowable(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapIteratorInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, map, 1);
  CONVERT_SMI_ARG_CHECKED(kind, 2)
  RUNTIME_ASSERT(kind == JSMapIterator::kKindKeys ||
                 kind == JSMapIterator::kKindValues ||
                 kind == JSMapIterator::kKindEntries);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(map->table()));
  holder->set_table(*table);
  holder->set_index(Smi::FromInt(0));
  holder->set_kind(Smi::FromInt(kind));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapIteratorClone) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);

  Handle<JSMapIterator> result = isolate->factory()->NewJSMapIterator();
  result->set_table(holder->table());
  result->set_index(Smi::FromInt(Smi::cast(holder->index())->value()));
  result->set_kind(Smi::FromInt(Smi::cast(holder->kind())->value()));

  return *result;
}


// The array returned contains the following information:
// 0: HasMore flag
// 1: Iteration index
// 2: Iteration kind
RUNTIME_FUNCTION(Runtime_MapIteratorDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(4);
  details->set(0, isolate->heap()->ToBoolean(holder->HasMore()));
  details->set(1, holder->index());
  details->set(2, holder->kind());
  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_GetWeakMapEntries) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, holder, 0);
  CONVERT_NUMBER_CHECKED(int, max_entries, Int32, args[1]);
  RUNTIME_ASSERT(max_entries >= 0);

  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  if (max_entries == 0 || max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }
  Handle<FixedArray> entries =
      isolate->factory()->NewFixedArray(max_entries * 2);
  // Allocation can cause GC can delete weak elements. Reload.
  if (max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }

  {
    DisallowHeapAllocation no_gc;
    int count = 0;
    for (int i = 0; count / 2 < max_entries && i < table->Capacity(); i++) {
      Handle<Object> key(table->KeyAt(i), isolate);
      if (table->IsKey(*key)) {
        entries->set(count++, *key);
        Object* value = table->Lookup(key);
        entries->set(count++, value);
      }
    }
    DCHECK_EQ(max_entries * 2, count);
  }
  return *isolate->factory()->NewJSArrayWithElements(entries);
}


RUNTIME_FUNCTION(Runtime_MapIteratorNext) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSMapIterator, holder, 0);
  CONVERT_ARG_CHECKED(JSArray, value_array, 1);
  return holder->Next(value_array);
}


void Runtime::WeakCollectionInitialize(
    Isolate* isolate, Handle<JSWeakCollection> weak_collection) {
  DCHECK(weak_collection->map()->inobject_properties() == 0);
  Handle<ObjectHashTable> table = ObjectHashTable::New(isolate, 0);
  weak_collection->set_table(*table);
}


RUNTIME_FUNCTION(Runtime_WeakCollectionInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  Runtime::WeakCollectionInitialize(isolate, weak_collection);
  return *weak_collection;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionGet) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionHas) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


bool Runtime::WeakCollectionDelete(Handle<JSWeakCollection> weak_collection,
                                   Handle<Object> key) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  DCHECK(table->IsKey(*key));
  bool was_present = false;
  Handle<ObjectHashTable> new_table =
      ObjectHashTable::Remove(table, key, &was_present);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    table->FillWithHoles(0, table->length());
  }
  return was_present;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionDelete) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  bool was_present = Runtime::WeakCollectionDelete(weak_collection, key);
  return isolate->heap()->ToBoolean(was_present);
}


void Runtime::WeakCollectionSet(Handle<JSWeakCollection> weak_collection,
                                Handle<Object> key, Handle<Object> value) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  DCHECK(table->IsKey(*key));
  Handle<ObjectHashTable> new_table = ObjectHashTable::Put(table, key, value);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    table->FillWithHoles(0, table->length());
  }
}


RUNTIME_FUNCTION(Runtime_WeakCollectionSet) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  Runtime::WeakCollectionSet(weak_collection, key, value);
  return *weak_collection;
}


RUNTIME_FUNCTION(Runtime_GetWeakSetValues) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, holder, 0);
  CONVERT_NUMBER_CHECKED(int, max_values, Int32, args[1]);
  RUNTIME_ASSERT(max_values >= 0);

  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  if (max_values == 0 || max_values > table->NumberOfElements()) {
    max_values = table->NumberOfElements();
  }
  Handle<FixedArray> values = isolate->factory()->NewFixedArray(max_values);
  // Recompute max_values because GC could have removed elements from the table.
  if (max_values > table->NumberOfElements()) {
    max_values = table->NumberOfElements();
  }
  {
    DisallowHeapAllocation no_gc;
    int count = 0;
    for (int i = 0; count < max_values && i < table->Capacity(); i++) {
      Handle<Object> key(table->KeyAt(i), isolate);
      if (table->IsKey(*key)) values->set(count++, *key);
    }
    DCHECK_EQ(max_values, count);
  }
  return *isolate->factory()->NewJSArrayWithElements(values);
}


RUNTIME_FUNCTION(Runtime_ObservationWeakMapCreate) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  Handle<JSWeakMap> weakmap = isolate->factory()->NewJSWeakMap();
  Runtime::WeakCollectionInitialize(isolate, weakmap);
  return *weakmap;
}
}
}  // namespace v8::internal
