// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/shared-heap-deserializer.h"

#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

void SharedHeapDeserializer::DeserializeIntoIsolate() {
  // Don't deserialize into client Isolates. If there are client Isolates, the
  // shared heap object cache should already be populated.
  if (isolate()->has_shared_space() && !isolate()->is_shared_space_isolate()) {
    DCHECK(!isolate()->shared_heap_object_cache()->empty());
    return;
  }
  DCHECK(isolate()->shared_heap_object_cache()->empty());
  HandleScope scope(isolate());

  IterateSharedHeapObjectCache(isolate(), this);
  DeserializeStringTable();
  DeserializeDeferredObjects();

  if (should_rehash()) {
    // Hash seed was initialized in ReadOnlyDeserializer.
    Rehash();
  }
}

void SharedHeapDeserializer::DeserializeStringTable() {
  // See SharedHeapSerializer::SerializeStringTable.

  DCHECK(isolate()->OwnsStringTables());

  // Get the string table size.
  int string_table_size = source()->GetInt();

  // Add each string to the Isolate's string table.
  // TODO(leszeks): Consider pre-sizing the string table.
  for (int i = 0; i < string_table_size; ++i) {
    Handle<String> string = Handle<String>::cast(ReadObject());
    StringTableInsertionKey key(
        isolate(), string,
        DeserializingUserCodeOption::kNotDeserializingUserCode);
    Handle<String> result =
        isolate()->string_table()->LookupKey(isolate(), &key);

    // Since this is startup, there should be no duplicate entries in the
    // string table, and the lookup should unconditionally add the given
    // string.
    DCHECK_EQ(*result, *string);
    USE(result);
  }

  DCHECK_EQ(string_table_size, isolate()->string_table()->NumberOfElements());
}

}  // namespace internal
}  // namespace v8
