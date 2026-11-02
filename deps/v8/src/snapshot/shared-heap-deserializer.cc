// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/shared-heap-deserializer.h"

#include "src/base/platform/elapsed-timer.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

void SharedHeapDeserializer::DeserializeIntoIsolate() {
  // Don't deserialize into isolates that don't own their string table. If there
  // are client Isolates, the shared heap object cache should already be
  // populated.
  // TODO(372493838): The shared heap object cache can only contain strings.
  // Update name to reflect this.
  if (!isolate()->OwnsStringTables()) {
    DCHECK(!isolate()->shared_heap_object_cache()->empty());
    return;
  }

  base::ElapsedTimer timer;
  if (V8_UNLIKELY(v8_flags.profile_deserialization)) timer.Start();
  DCHECK(isolate()->shared_heap_object_cache()->empty());
  HandleScope scope(isolate());

  IterateSharedHeapObjectCache(isolate(), this);
  DeserializeStringTable();
  DeserializeDeferredObjects();
  PostProcessExposedTrustedObjects();

  if (should_rehash()) {
    // The hash seed has already been initialized in ReadOnlyDeserializer, thus
    // there is no need to call `HashSeed::InitializeRoots(isolate());`.
    Rehash();
  }

  if (V8_UNLIKELY(v8_flags.profile_deserialization)) {
    // ATTENTION: The Memory.json benchmark greps for this exact output. Do not
    // change it without also updating Memory.json.
    const size_t bytes = source()->length();
    const double ms = timer.Elapsed().InMillisecondsF();
    PrintF("[Deserializing shared heap (%zu bytes) took %0.3f ms]\n", bytes,
           ms);
  }
}

void SharedHeapDeserializer::DeserializeStringTable() {
  // See SharedHeapSerializer::SerializeStringTable.

  DCHECK(isolate()->OwnsStringTables());

  // Get the string table size.
  const int length = source()->GetUint30();

  // .. and the contents.
  DirectHandleVector<InternalizedString> strings(isolate());
  strings.reserve(length);
  for (int i = 0; i < length; ++i) {
    strings.emplace_back(Cast<InternalizedString>(ReadObject()));
  }

  StringTable* t = isolate()->string_table();
  DCHECK_EQ(t->NumberOfElements(), 0);
  t->InsertForIsolateDeserialization(
      isolate(), base::VectorOf(strings.data(), strings.size()));
  DCHECK_EQ(t->NumberOfElements(), length);
}

}  // namespace internal
}  // namespace v8
