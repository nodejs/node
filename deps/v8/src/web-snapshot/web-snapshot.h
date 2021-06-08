// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_
#define V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_

#include <queue>
#include <vector>

#include "src/handles/handles.h"
#include "src/objects/value-serializer.h"
#include "src/snapshot/serializer.h"  // For ObjectCacheIndexMap

namespace v8 {

class Context;
class Isolate;

template <typename T>
class Local;

namespace internal {

class Context;
class Map;
class Object;
class String;

struct WebSnapshotData {
  uint8_t* buffer = nullptr;
  size_t buffer_size = 0;
  ~WebSnapshotData() { free(buffer); }
};

class WebSnapshotSerializerDeserializer {
 public:
  inline bool has_error() const { return error_message_ != nullptr; }
  const char* error_message() const { return error_message_; }

  enum ValueType : uint8_t { STRING_ID, OBJECT_ID, FUNCTION_ID };

  // The maximum count of items for each value type (strings, objects etc.)
  static constexpr uint32_t kMaxItemCount =
      static_cast<uint32_t>(FixedArray::kMaxLength);
  // This ensures indices and lengths can be converted between uint32_t and int
  // without problems:
  STATIC_ASSERT(kMaxItemCount < std::numeric_limits<int32_t>::max());

 protected:
  explicit WebSnapshotSerializerDeserializer(Isolate* isolate)
      : isolate_(isolate) {}
  // Not virtual, on purpose (because it doesn't need to be).
  void Throw(const char* message);
  Isolate* isolate_;
  const char* error_message_ = nullptr;

 private:
  WebSnapshotSerializerDeserializer(const WebSnapshotSerializerDeserializer&) =
      delete;
  WebSnapshotSerializerDeserializer& operator=(
      const WebSnapshotSerializerDeserializer&) = delete;
};

class V8_EXPORT WebSnapshotSerializer
    : public WebSnapshotSerializerDeserializer {
 public:
  explicit WebSnapshotSerializer(v8::Isolate* isolate);
  ~WebSnapshotSerializer();

  bool TakeSnapshot(v8::Local<v8::Context> context,
                    const std::vector<std::string>& exports,
                    WebSnapshotData& data_out);

  // For inspecting the state after taking a snapshot.
  uint32_t string_count() const {
    return static_cast<uint32_t>(string_ids_.size());
  }

  uint32_t map_count() const { return static_cast<uint32_t>(map_ids_.size()); }

  uint32_t context_count() const {
    return static_cast<uint32_t>(context_ids_.size());
  }

  uint32_t function_count() const {
    return static_cast<uint32_t>(function_ids_.size());
  }

  uint32_t object_count() const {
    return static_cast<uint32_t>(object_ids_.size());
  }

 private:
  WebSnapshotSerializer(const WebSnapshotSerializer&) = delete;
  WebSnapshotSerializer& operator=(const WebSnapshotSerializer&) = delete;

  void WriteSnapshot(uint8_t*& buffer, size_t& buffer_size);

  // Returns true if the object was already in the map, false if it was added.
  bool InsertIntoIndexMap(ObjectCacheIndexMap& map, Handle<HeapObject> object,
                          uint32_t& id);

  void SerializeString(Handle<String> string, uint32_t& id);
  void SerializeMap(Handle<Map> map, uint32_t& id);
  void SerializeFunction(Handle<JSFunction> function, uint32_t& id);
  void SerializeContext(Handle<Context> context, uint32_t& id);
  void SerializeObject(Handle<JSObject> object, uint32_t& id);
  void SerializePendingObject(Handle<JSObject> object);
  void SerializeExport(Handle<JSObject> object, const std::string& export_name);
  void WriteValue(Handle<Object> object, ValueSerializer& serializer);

  ValueSerializer string_serializer_;
  ValueSerializer map_serializer_;
  ValueSerializer context_serializer_;
  ValueSerializer function_serializer_;
  ValueSerializer object_serializer_;
  ValueSerializer export_serializer_;

  ObjectCacheIndexMap string_ids_;
  ObjectCacheIndexMap map_ids_;
  ObjectCacheIndexMap context_ids_;
  ObjectCacheIndexMap function_ids_;
  ObjectCacheIndexMap object_ids_;
  uint32_t export_count_ = 0;

  std::queue<Handle<JSObject>> pending_objects_;
};

class V8_EXPORT WebSnapshotDeserializer
    : public WebSnapshotSerializerDeserializer {
 public:
  explicit WebSnapshotDeserializer(v8::Isolate* v8_isolate);
  ~WebSnapshotDeserializer();
  bool UseWebSnapshot(const uint8_t* data, size_t buffer_size);

  // For inspecting the state after deserializing a snapshot.
  uint32_t string_count() const { return string_count_; }
  uint32_t map_count() const { return map_count_; }
  uint32_t context_count() const { return context_count_; }
  uint32_t function_count() const { return function_count_; }
  uint32_t object_count() const { return object_count_; }

 private:
  WebSnapshotDeserializer(const WebSnapshotDeserializer&) = delete;
  WebSnapshotDeserializer& operator=(const WebSnapshotDeserializer&) = delete;

  void DeserializeStrings();
  Handle<String> ReadString(bool internalize = false);
  void DeserializeMaps();
  void DeserializeContexts();
  Handle<ScopeInfo> CreateScopeInfo(uint32_t variable_count, bool has_parent);
  void DeserializeFunctions();
  void DeserializeObjects();
  void DeserializeExports();
  void ReadValue(Handle<Object>& value, Representation& representation);

  // Not virtual, on purpose (because it doesn't need to be).
  void Throw(const char* message);

  Handle<FixedArray> strings_;
  Handle<FixedArray> maps_;
  Handle<FixedArray> contexts_;
  Handle<FixedArray> functions_;
  Handle<FixedArray> objects_;

  uint32_t string_count_ = 0;
  uint32_t map_count_ = 0;
  uint32_t context_count_ = 0;
  uint32_t function_count_ = 0;
  uint32_t object_count_ = 0;

  std::unique_ptr<ValueDeserializer> deserializer_;

  bool deserialized_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_
