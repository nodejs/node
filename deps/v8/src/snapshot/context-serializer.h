// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CONTEXT_SERIALIZER_H_
#define V8_SNAPSHOT_CONTEXT_SERIALIZER_H_

#include "src/objects/contexts.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class StartupSerializer;

class V8_EXPORT_PRIVATE ContextSerializer : public Serializer {
 public:
  ContextSerializer(Isolate* isolate, Snapshot::SerializerFlags flags,
                    StartupSerializer* startup_serializer,
                    v8::SerializeEmbedderFieldsCallback callback);

  ~ContextSerializer() override;
  ContextSerializer(const ContextSerializer&) = delete;
  ContextSerializer& operator=(const ContextSerializer&) = delete;

  // Serialize the objects reachable from a single object pointer.
  void Serialize(Context* o, const DisallowGarbageCollection& no_gc);

  bool can_be_rehashed() const { return can_be_rehashed_; }

 private:
  void SerializeObjectImpl(Handle<HeapObject> o, SlotType slot_type) override;
  bool ShouldBeInTheStartupObjectCache(Tagged<HeapObject> o);
  bool ShouldBeInTheSharedObjectCache(Tagged<HeapObject> o);
  bool SerializeJSObjectWithEmbedderFields(Handle<JSObject> obj);
  void CheckRehashability(Tagged<HeapObject> obj);

  StartupSerializer* startup_serializer_;
  v8::SerializeEmbedderFieldsCallback serialize_embedder_fields_;
  // Indicates whether we only serialized hash tables that we can rehash.
  // TODO(yangguo): generalize rehashing, and remove this flag.
  bool can_be_rehashed_;
  Context context_;

  // Used to store serialized data for embedder fields.
  SnapshotByteSink embedder_fields_sink_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CONTEXT_SERIALIZER_H_
