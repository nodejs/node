// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/serializer-common.h"

#include "src/external-reference-table.h"
#include "src/ic/stub-cache.h"
#include "src/list-inl.h"

namespace v8 {
namespace internal {

ExternalReferenceEncoder::ExternalReferenceEncoder(Isolate* isolate) {
  map_ = isolate->external_reference_map();
  if (map_ != NULL) return;
  map_ = new base::HashMap();
  ExternalReferenceTable* table = ExternalReferenceTable::instance(isolate);
  for (int i = 0; i < table->size(); ++i) {
    Address addr = table->address(i);
    if (addr == ExternalReferenceTable::NotAvailable()) continue;
    // We expect no duplicate external references entries in the table.
    // AccessorRefTable getter may have duplicates, indicated by an empty string
    // as name.
    DCHECK(table->name(i)[0] == '\0' ||
           map_->Lookup(addr, Hash(addr)) == nullptr);
    map_->LookupOrInsert(addr, Hash(addr))->value = reinterpret_cast<void*>(i);
  }
  isolate->set_external_reference_map(map_);
}

uint32_t ExternalReferenceEncoder::Encode(Address address) const {
  DCHECK_NOT_NULL(address);
  base::HashMap::Entry* entry =
      const_cast<base::HashMap*>(map_)->Lookup(address, Hash(address));
  DCHECK_NOT_NULL(entry);
  return static_cast<uint32_t>(reinterpret_cast<intptr_t>(entry->value));
}

const char* ExternalReferenceEncoder::NameOfAddress(Isolate* isolate,
                                                    Address address) const {
  base::HashMap::Entry* entry =
      const_cast<base::HashMap*>(map_)->Lookup(address, Hash(address));
  if (entry == NULL) return "<unknown>";
  uint32_t i = static_cast<uint32_t>(reinterpret_cast<intptr_t>(entry->value));
  return ExternalReferenceTable::instance(isolate)->name(i);
}

void SerializedData::AllocateData(int size) {
  DCHECK(!owns_data_);
  data_ = NewArray<byte>(size);
  size_ = size;
  owns_data_ = true;
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(data_), kPointerAlignment));
}

// The partial snapshot cache is terminated by undefined. We visit the
// partial snapshot...
//  - during deserialization to populate it.
//  - during normal GC to keep its content alive.
//  - not during serialization. The partial serializer adds to it explicitly.
DISABLE_CFI_PERF
void SerializerDeserializer::Iterate(Isolate* isolate, ObjectVisitor* visitor) {
  List<Object*>* cache = isolate->partial_snapshot_cache();
  for (int i = 0;; ++i) {
    // Extend the array ready to get a value when deserializing.
    if (cache->length() <= i) cache->Add(Smi::FromInt(0));
    // During deserialization, the visitor populates the partial snapshot cache
    // and eventually terminates the cache with undefined.
    visitor->VisitPointer(&cache->at(i));
    if (cache->at(i)->IsUndefined(isolate)) break;
  }
}

bool SerializerDeserializer::CanBeDeferred(HeapObject* o) {
  return !o->IsString() && !o->IsScript();
}

}  // namespace internal
}  // namespace v8
