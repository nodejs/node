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
#ifdef DEBUG
  table_ = ExternalReferenceTable::instance(isolate);
#endif  // DEBUG
  if (map_ != nullptr) return;
  map_ = new AddressToIndexHashMap();
  ExternalReferenceTable* table = ExternalReferenceTable::instance(isolate);
  for (uint32_t i = 0; i < table->size(); ++i) {
    Address addr = table->address(i);
    DCHECK(map_->Get(addr).IsNothing());
    map_->Set(addr, i);
    DCHECK(map_->Get(addr).IsJust());
  }
  isolate->set_external_reference_map(map_);
}

uint32_t ExternalReferenceEncoder::Encode(Address address) const {
  Maybe<uint32_t> maybe_index = map_->Get(address);
  if (maybe_index.IsNothing()) {
    void* addr = address;
    v8::base::OS::PrintError("Unknown external reference %p.\n", addr);
    v8::base::OS::PrintError("%s", ExternalReferenceTable::ResolveSymbol(addr));
    v8::base::OS::Abort();
  }
#ifdef DEBUG
  table_->increment_count(maybe_index.FromJust());
#endif  // DEBUG
  return maybe_index.FromJust();
}

const char* ExternalReferenceEncoder::NameOfAddress(Isolate* isolate,
                                                    Address address) const {
  Maybe<uint32_t> maybe_index = map_->Get(address);
  if (maybe_index.IsNothing()) return "<unknown>";
  return ExternalReferenceTable::instance(isolate)->name(
      maybe_index.FromJust());
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
    if (cache->length() <= i) cache->Add(Smi::kZero);
    // During deserialization, the visitor populates the partial snapshot cache
    // and eventually terminates the cache with undefined.
    visitor->VisitPointer(&cache->at(i));
    if (cache->at(i)->IsUndefined(isolate)) break;
  }
}

bool SerializerDeserializer::CanBeDeferred(HeapObject* o) {
  return !o->IsString() && !o->IsScript();
}

void SerializerDeserializer::RestoreExternalReferenceRedirectors(
    List<AccessorInfo*>* accessor_infos) {
  // Restore wiped accessor infos.
  for (AccessorInfo* info : *accessor_infos) {
    Foreign::cast(info->js_getter())
        ->set_foreign_address(info->redirected_getter());
  }
}

}  // namespace internal
}  // namespace v8
