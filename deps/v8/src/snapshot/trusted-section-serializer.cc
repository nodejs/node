// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/trusted-section-serializer.h"

#include "src/base/numerics/safe_conversions.h"
#include "src/handles/global-handles-inl.h"
#include "src/objects/objects-inl.h"
#include "src/sandbox/check.h"
#include "src/sandbox/sandbox.h"

namespace v8 {
namespace internal {

TrustedSectionSerializer::TrustedSectionSerializer(
    Isolate* isolate, Serializer* untrusted_serializer)
    : Serializer(isolate, untrusted_serializer->flags_),
      untrusted_serializer_(untrusted_serializer),
      exports_(isolate->heap()),
      export_queue_(isolate->heap()) {}

uint32_t TrustedSectionSerializer::Export(Tagged<HeapObject> obj) {
  SBXCHECK(OutsideSandbox(obj.address()));
  int index;
  if (!exports_.LookupOrInsert(obj, &index)) export_queue_.Enqueue(obj);
  return base::checked_cast<uint32_t>(index);
}

uint32_t TrustedSectionSerializer::AddExternalReference(
    ExternalReferenceEncoder::Value reference, ExternalPointerTag tag) {
  external_references_.push_back({reference, tag});
  return base::checked_cast<uint32_t>(external_references_.size() - 1);
}

void TrustedSectionSerializer::SerializeExports() {
  if (v8_flags.trace_serializer) {
    PrintF("Serializing trusted exports\n");
  }
  // Serializing an export can add untrusted objects to the untrusted section,
  // and those can export more trusted objects. Export() appends them to the
  // queue, so keep going until it is drained.
  while (!export_queue_.empty()) {
    HandleScope scope(isolate());
    Handle<HeapObject> obj = handle(export_queue_.Dequeue(), isolate());
    SerializeObject(obj, SlotType::kAnySlot);
  }
  DCHECK(deferred_objects_empty());
}

void TrustedSectionSerializer::FinishSection() {
  sink_.Put(kSynchronize, "EndOfTrustedSection");
  // The external references are read before the untrusted section, so they
  // precede Payload() in the trusted section, which is padded as a whole.
  external_references_sink_.PutUint30(
      base::checked_cast<uint32_t>(external_references_.size()),
      "ExternalReferenceCount");
  external_references_sink_.PutRaw(
      reinterpret_cast<const uint8_t*>(external_references_.data()),
      external_references_.size() * sizeof(EncodedExternalReference),
      "ExternalReferences");
  OptimizeBackReferences();
  Pad(base::checked_cast<int>(external_references_sink_.Position()));
}

bool TrustedSectionSerializer::SerializeUntrustedReference(
    Handle<HeapObject> obj) {
  if (OutsideSandbox(obj->address())) return false;
  if (!untrusted_serializer_->ReferenceMapContains(obj)) {
    untrusted_serializer_->SerializeObject(obj, SlotType::kAnySlot);
    untrusted_serializer_->SerializeDeferredObjects();
  }
  const SerializerReference* reference =
      untrusted_serializer_->reference_map()->LookupReference(*obj);
  DCHECK_NOT_NULL(reference);
  if (v8_flags.trace_serializer) {
    PrintF(" Encoding untrusted reference to: ");
    ShortPrint(*obj);
    PrintF("\n");
  }
  if (reference->is_attached_reference()) {
    PutAttachedReference(*reference);
  } else {
    sink_.Put(kUntrustedRef, "UntrustedRef");
    sink_.PutUint30(reference->back_ref_index(), "UntrustedRefIndex");
  }
  return true;
}

void TrustedSectionSerializer::SerializeObjectImpl(Handle<HeapObject> obj,
                                                   SlotType slot_type) {
  {
    DisallowGarbageCollection no_gc;
    Tagged<HeapObject> raw = *obj;
    if (SerializeRoot(raw)) return;
    if (SerializeReadOnlyObjectReference(raw, &sink_)) return;
    if (SerializeUntrustedReference(obj)) return;
    if (SerializeBackReference(raw)) return;
#ifdef DEBUG
    DCHECK(!untrusted_serializer_->ReferenceMapContains(obj));
    if (IsExposedTrustedObject(raw)) {
      int index;
      DCHECK(exports_.Lookup(raw, &index));
      DCHECK_GE(index, 0);
    }
#endif
  }
  ObjectSerializer serializer(this, obj, &sink_);
  serializer.Serialize(slot_type);
}

}  // namespace internal
}  // namespace v8
