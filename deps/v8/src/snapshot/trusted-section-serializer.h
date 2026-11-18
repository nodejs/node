// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_TRUSTED_SECTION_SERIALIZER_H_
#define V8_SNAPSHOT_TRUSTED_SECTION_SERIALIZER_H_

#include <unordered_map>
#include <vector>

#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

// Serializes the trusted section of a snapshot that is split in two:
// - The untrusted section holds the objects inside the sandbox. It is written
//   by |untrusted_serializer| and deserialized in a mode that can't write
//   outside the sandbox. It refers to the trusted section only by index, see
//   Export, AddExternalReference, AddCppHeapPointer and AddJSDispatchEntry.
// - The trusted section holds the objects outside the sandbox and the table of
//   external references, CppHeap pointers and JS dispatch entries. It is
//   written by this class.
class TrustedSectionSerializer final : public Serializer {
 public:
  TrustedSectionSerializer(Isolate* isolate, Serializer* untrusted_serializer);
  ~TrustedSectionSerializer() override {
    OutputStatistics("TrustedSectionSerializer");
  }

  // Returns the index of |obj| in the exports, adding it if it is new.
  uint32_t Export(Tagged<HeapObject> obj);
  // Adds an entry to the table of external references and returns its index.
  uint32_t AddExternalReference(ExternalReferenceEncoder::Value reference,
                                ExternalPointerTag tag);
  // Adds an entry for a CppHeap pointer to |reference| and returns its index.
  uint32_t AddCppHeapPointer(ExternalReferenceEncoder::Value reference,
                             CppHeapPointerTag tag);
  // Returns the index of the dispatch entry |handle| in the table, adding it if
  // it is new.
  uint32_t AddJSDispatchEntry(JSDispatchHandle handle);

  void SerializeExports();
  void FinishSection();
  const std::vector<uint8_t>* ExternalReferencesPayload() const {
    return external_references_sink_.data();
  }

 private:
  void SerializeObjectImpl(Handle<HeapObject> o, SlotType slot_type) override;
  // Puts in-sandbox objects into the untrusted section and refers to them.
  bool SerializeUntrustedReference(Handle<HeapObject> obj);

  Serializer* const untrusted_serializer_;
  ObjectCacheIndexMap exports_;
  GlobalHandleCircularQueue<HeapObject> export_queue_;
  std::vector<EncodedExternalReference> external_references_;
  std::unordered_map<JSDispatchHandle, uint32_t> dispatch_entry_indices_;
  SnapshotByteSink external_references_sink_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_TRUSTED_SECTION_SERIALIZER_H_
