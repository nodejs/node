// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CONTEXT_SERIALIZER_H_
#define V8_SNAPSHOT_CONTEXT_SERIALIZER_H_

#include "src/objects/contexts.h"
#include "src/snapshot/serializer.h"
#include "src/utils/address-map.h"

namespace v8 {
namespace internal {

class StartupSerializer;

class V8_EXPORT_PRIVATE ContextSerializer : public Serializer {
 public:
  ContextSerializer(Isolate* isolate, Snapshot::SerializerFlags flags,
                    StartupSerializer* startup_serializer,
                    v8::SerializeEmbedderFieldsCallback callback);

  ~ContextSerializer() override;

  // Serialize the objects reachable from a single object pointer.
  void Serialize(Context* o, const DisallowHeapAllocation& no_gc);

  bool can_be_rehashed() const { return can_be_rehashed_; }

 private:
  void SerializeObject(HeapObject o) override;
  bool ShouldBeInTheStartupObjectCache(HeapObject o);
  bool SerializeJSObjectWithEmbedderFields(Object obj);
  void CheckRehashability(HeapObject obj);

  StartupSerializer* startup_serializer_;
  v8::SerializeEmbedderFieldsCallback serialize_embedder_fields_;
  // Indicates whether we only serialized hash tables that we can rehash.
  // TODO(yangguo): generalize rehashing, and remove this flag.
  bool can_be_rehashed_;
  Context context_;

  // Used to store serialized data for embedder fields.
  SnapshotByteSink embedder_fields_sink_;
  DISALLOW_COPY_AND_ASSIGN(ContextSerializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CONTEXT_SERIALIZER_H_
