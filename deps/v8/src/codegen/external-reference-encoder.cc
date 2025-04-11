// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/external-reference-encoder.h"

#include "src/codegen/external-reference-table.h"
#include "src/execution/isolate.h"

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
  ExternalReferenceTable* table = isolate->external_reference_table();
  for (uint32_t i = 0; i < ExternalReferenceTable::kSize; ++i) {
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
    Address addr = static_cast<Address>(api_references[i]);
    // Ignore duplicate references.
    // This can happen due to ICF. See http://crbug.com/726896.
    if (map_->Get(addr).IsNothing()) map_->Set(addr, Value::Encode(i, true));
    DCHECK(map_->Get(addr).IsJust());
  }
}

#ifdef DEBUG
ExternalReferenceEncoder::~ExternalReferenceEncoder() {
  if (!v8_flags.external_reference_stats) return;
  if (api_references_ == nullptr) return;
  for (uint32_t i = 0; api_references_[i] != 0; ++i) {
    Address addr = static_cast<Address>(api_references_[i]);
    DCHECK(map_->Get(addr).IsJust());
    v8::base::OS::Print(
        "index=%5d count=%5d  %-60s\n", i, count_[i],
        ExternalReferenceTable::ResolveSymbol(reinterpret_cast<void*>(addr)));
  }
}
#endif  // DEBUG

Maybe<ExternalReferenceEncoder::Value> ExternalReferenceEncoder::TryEncode(
    Address address) const {
  Maybe<uint32_t> maybe_index = map_->Get(address);
  if (maybe_index.IsNothing()) return Nothing<Value>();
  Value result(maybe_index.FromJust());
#ifdef DEBUG
  if (result.is_from_api()) count_[result.index()]++;
#endif  // DEBUG
  return Just<Value>(result);
}

ExternalReferenceEncoder::Value ExternalReferenceEncoder::Encode(
    Address address) const {
  Maybe<uint32_t> maybe_index = map_->Get(address);
  if (maybe_index.IsNothing()) {
    void* addr = reinterpret_cast<void*>(address);
    v8::base::OS::PrintError("Unknown external reference %p.\n", addr);
    v8::base::OS::PrintError("%s\n",
                             ExternalReferenceTable::ResolveSymbol(addr));
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
  Value value(maybe_index.FromJust());
  if (value.is_from_api()) return "<from api>";
  return isolate->external_reference_table()->name(value.index());
}

}  // namespace internal
}  // namespace v8
