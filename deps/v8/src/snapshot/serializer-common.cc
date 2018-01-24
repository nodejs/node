// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/serializer-common.h"

#include "src/external-reference-table.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

ExternalReferenceEncoder::ExternalReferenceEncoder(Isolate* isolate) {
#ifdef DEBUG
  api_references_ = isolate->api_external_references();
  if (api_references_ != nullptr) {
    for (uint32_t i = 0; api_references_[i] != 0; ++i) count_.push_back(0);
  }
#endif  // DEBUG
  map_ = isolate->external_reference_map();
  if (map_ != nullptr) return;
  map_ = new AddressToIndexHashMap();
  isolate->set_external_reference_map(map_);
  // Add V8's external references.
  ExternalReferenceTable* table = ExternalReferenceTable::instance(isolate);
  for (uint32_t i = 0; i < table->size(); ++i) {
    Address addr = table->address(i);
    // Ignore duplicate references.
    // This can happen due to ICF. See http://crbug.com/726896.
    if (map_->Get(addr).IsNothing()) map_->Set(addr, Value::Encode(i, false));
    DCHECK(map_->Get(addr).IsJust());
  }
  // Add external references provided by the embedder.
  const intptr_t* api_references = isolate->api_external_references();
  if (api_references == nullptr) return;
  for (uint32_t i = 0; api_references[i] != 0; ++i) {
    Address addr = reinterpret_cast<Address>(api_references[i]);
    // Ignore duplicate references.
    // This can happen due to ICF. See http://crbug.com/726896.
    if (map_->Get(addr).IsNothing()) map_->Set(addr, Value::Encode(i, true));
    DCHECK(map_->Get(addr).IsJust());
  }
}

ExternalReferenceEncoder::~ExternalReferenceEncoder() {
#ifdef DEBUG
  if (!i::FLAG_external_reference_stats) return;
  if (api_references_ == nullptr) return;
  for (uint32_t i = 0; api_references_[i] != 0; ++i) {
    Address addr = reinterpret_cast<Address>(api_references_[i]);
    DCHECK(map_->Get(addr).IsJust());
    v8::base::OS::Print("index=%5d count=%5d  %-60s\n", i, count_[i],
                        ExternalReferenceTable::ResolveSymbol(addr));
  }
#endif  // DEBUG
}

ExternalReferenceEncoder::Value ExternalReferenceEncoder::Encode(
    Address address) {
  Maybe<uint32_t> maybe_index = map_->Get(address);
  if (maybe_index.IsNothing()) {
    void* addr = address;
    v8::base::OS::PrintError("Unknown external reference %p.\n", addr);
    v8::base::OS::PrintError("%s", ExternalReferenceTable::ResolveSymbol(addr));
    v8::base::OS::Abort();
  }
  Value result(maybe_index.FromJust());
#ifdef DEBUG
  if (result.is_from_api()) count_[result.index()]++;
#endif  // DEBUG
  return result;
}

const char* ExternalReferenceEncoder::NameOfAddress(Isolate* isolate,
                                                    Address address) const {
  Maybe<uint32_t> maybe_index = map_->Get(address);
  if (maybe_index.IsNothing()) return "<unknown>";
  return ExternalReferenceTable::instance(isolate)->name(
      maybe_index.FromJust());
}

void SerializedData::AllocateData(uint32_t size) {
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
void SerializerDeserializer::Iterate(Isolate* isolate, RootVisitor* visitor) {
  std::vector<Object*>* cache = isolate->partial_snapshot_cache();
  for (size_t i = 0;; ++i) {
    // Extend the array ready to get a value when deserializing.
    if (cache->size() <= i) cache->push_back(Smi::kZero);
    // During deserialization, the visitor populates the partial snapshot cache
    // and eventually terminates the cache with undefined.
    visitor->VisitRootPointer(Root::kPartialSnapshotCache, &cache->at(i));
    if (cache->at(i)->IsUndefined(isolate)) break;
  }
}

bool SerializerDeserializer::CanBeDeferred(HeapObject* o) {
  return !o->IsString() && !o->IsScript() && !o->IsJSTypedArray();
}

void SerializerDeserializer::RestoreExternalReferenceRedirectors(
    const std::vector<AccessorInfo*>& accessor_infos) {
  // Restore wiped accessor infos.
  for (AccessorInfo* info : accessor_infos) {
    Foreign::cast(info->js_getter())
        ->set_foreign_address(info->redirected_getter());
  }
}

void SerializerDeserializer::RestoreExternalReferenceRedirectors(
    const std::vector<CallHandlerInfo*>& call_handler_infos) {
  for (CallHandlerInfo* info : call_handler_infos) {
    Foreign::cast(info->js_callback())
        ->set_foreign_address(info->redirected_callback());
  }
}

}  // namespace internal
}  // namespace v8
