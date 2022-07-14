// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_
#define V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_

#include <queue>

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

struct WebSnapshotData : public std::enable_shared_from_this<WebSnapshotData> {
  uint8_t* buffer = nullptr;
  size_t buffer_size = 0;
  WebSnapshotData() = default;
  WebSnapshotData(const WebSnapshotData&) = delete;
  WebSnapshotData& operator=(const WebSnapshotData&) = delete;
  ~WebSnapshotData() { free(buffer); }
};

class WebSnapshotSerializerDeserializer {
 public:
  inline bool has_error() const { return error_message_ != nullptr; }
  const char* error_message() const { return error_message_; }

  enum ValueType : uint8_t {
    FALSE_CONSTANT,
    TRUE_CONSTANT,
    NULL_CONSTANT,
    UNDEFINED_CONSTANT,
    INTEGER,
    DOUBLE,
    STRING_ID,
    ARRAY_ID,
    OBJECT_ID,
    FUNCTION_ID,
    CLASS_ID,
    REGEXP,
    EXTERNAL_ID,
    IN_PLACE_STRING_ID
  };

  static constexpr uint8_t kMagicNumber[4] = {'+', '+', '+', ';'};

  enum ContextType : uint8_t { FUNCTION, BLOCK };

  enum PropertyAttributesType : uint8_t { DEFAULT, CUSTOM };

  uint32_t FunctionKindToFunctionFlags(FunctionKind kind);
  FunctionKind FunctionFlagsToFunctionKind(uint32_t flags);
  bool IsFunctionOrMethod(uint32_t flags);
  bool IsConstructor(uint32_t flags);

  uint32_t GetDefaultAttributeFlags();
  uint32_t AttributesToFlags(PropertyDetails details);
  PropertyAttributes FlagsToAttributes(uint32_t flags);

  // The maximum count of items for each value type (strings, objects etc.)
  static constexpr uint32_t kMaxItemCount =
      static_cast<uint32_t>(FixedArray::kMaxLength - 1);
  // This ensures indices and lengths can be converted between uint32_t and int
  // without problems:
  STATIC_ASSERT(kMaxItemCount < std::numeric_limits<int32_t>::max());

 protected:
  explicit WebSnapshotSerializerDeserializer(Isolate* isolate)
      : isolate_(isolate) {}
  // Not virtual, on purpose (because it doesn't need to be).
  void Throw(const char* message);

  inline Factory* factory() const { return isolate_->factory(); }

  Isolate* isolate_;
  const char* error_message_ = nullptr;

 private:
  WebSnapshotSerializerDeserializer(const WebSnapshotSerializerDeserializer&) =
      delete;
  WebSnapshotSerializerDeserializer& operator=(
      const WebSnapshotSerializerDeserializer&) = delete;

  // Keep most common function kinds in the 7 least significant bits to make the
  // flags fit in 1 byte.
  using AsyncFunctionBitField = base::BitField<bool, 0, 1>;
  using GeneratorFunctionBitField = AsyncFunctionBitField::Next<bool, 1>;
  using ArrowFunctionBitField = GeneratorFunctionBitField::Next<bool, 1>;
  using MethodBitField = ArrowFunctionBitField::Next<bool, 1>;
  using StaticBitField = MethodBitField::Next<bool, 1>;
  using ClassConstructorBitField = StaticBitField::Next<bool, 1>;
  using DefaultConstructorBitField = ClassConstructorBitField::Next<bool, 1>;
  using DerivedConstructorBitField = DefaultConstructorBitField::Next<bool, 1>;

  using ReadOnlyBitField = base::BitField<bool, 0, 1>;
  using ConfigurableBitField = ReadOnlyBitField::Next<bool, 1>;
  using EnumerableBitField = ConfigurableBitField::Next<bool, 1>;
};

class V8_EXPORT WebSnapshotSerializer
    : public WebSnapshotSerializerDeserializer {
 public:
  explicit WebSnapshotSerializer(v8::Isolate* isolate);
  explicit WebSnapshotSerializer(Isolate* isolate);

  ~WebSnapshotSerializer();

  bool TakeSnapshot(v8::Local<v8::Context> context,
                    v8::Local<v8::PrimitiveArray> exports,
                    WebSnapshotData& data_out);
  bool TakeSnapshot(Handle<Object> object, MaybeHandle<FixedArray> block_list,
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

  uint32_t class_count() const {
    return static_cast<uint32_t>(class_ids_.size());
  }

  uint32_t array_count() const {
    return static_cast<uint32_t>(array_ids_.size());
  }

  uint32_t object_count() const {
    return static_cast<uint32_t>(object_ids_.size());
  }

  uint32_t external_objects_count() const {
    return static_cast<uint32_t>(external_objects_ids_.size());
  }

  Handle<FixedArray> GetExternals();

 private:
  WebSnapshotSerializer(const WebSnapshotSerializer&) = delete;
  WebSnapshotSerializer& operator=(const WebSnapshotSerializer&) = delete;

  enum class AllowInPlace {
    No,   // This reference cannot be replace with an in-place item.
    Yes,  // This reference can be replaced with an in-place item.
  };

  void SerializePendingItems();
  void WriteSnapshot(uint8_t*& buffer, size_t& buffer_size);
  void WriteObjects(ValueSerializer& destination, size_t count,
                    ValueSerializer& source, const char* name);

  // Returns true if the object was already in the map, false if it was added.
  bool InsertIntoIndexMap(ObjectCacheIndexMap& map, HeapObject heap_object,
                          uint32_t& id);

  void ShallowDiscoverExternals(FixedArray externals);
  void Discover(Handle<HeapObject> object);
  void DiscoverString(Handle<String> string,
                      AllowInPlace can_be_in_place = AllowInPlace::No);
  void DiscoverMap(Handle<Map> map);
  void DiscoverFunction(Handle<JSFunction> function);
  void DiscoverClass(Handle<JSFunction> function);
  void DiscoverContextAndPrototype(Handle<JSFunction> function);
  void DiscoverContext(Handle<Context> context);
  void DiscoverArray(Handle<JSArray> array);
  void DiscoverObject(Handle<JSObject> object);
  void DiscoverSource(Handle<JSFunction> function);
  void ConstructSource();

  void SerializeFunctionInfo(ValueSerializer* serializer,
                             Handle<JSFunction> function);

  void SerializeString(Handle<String> string, ValueSerializer& serializer);
  void SerializeMap(Handle<Map> map);
  void SerializeFunction(Handle<JSFunction> function);
  void SerializeClass(Handle<JSFunction> function);
  void SerializeContext(Handle<Context> context);
  void SerializeArray(Handle<JSArray> array);
  void SerializeObject(Handle<JSObject> object);

  void SerializeExport(Handle<Object> object, Handle<String> export_name);
  void WriteValue(Handle<Object> object, ValueSerializer& serializer);
  void WriteStringMaybeInPlace(Handle<String> string,
                               ValueSerializer& serializer);
  void WriteStringId(Handle<String> string, ValueSerializer& serializer);

  uint32_t GetStringId(Handle<String> string, bool& in_place);
  uint32_t GetMapId(Map map);
  uint32_t GetFunctionId(JSFunction function);
  uint32_t GetClassId(JSFunction function);
  uint32_t GetContextId(Context context);
  uint32_t GetArrayId(JSArray array);
  uint32_t GetObjectId(JSObject object);
  uint32_t GetExternalId(HeapObject object);

  ValueSerializer string_serializer_;
  ValueSerializer map_serializer_;
  ValueSerializer context_serializer_;
  ValueSerializer function_serializer_;
  ValueSerializer class_serializer_;
  ValueSerializer array_serializer_;
  ValueSerializer object_serializer_;
  ValueSerializer export_serializer_;

  // These are needed for being able to serialize items in order.
  Handle<ArrayList> contexts_;
  Handle<ArrayList> functions_;
  Handle<ArrayList> classes_;
  Handle<ArrayList> arrays_;
  Handle<ArrayList> objects_;
  Handle<ArrayList> strings_;
  Handle<ArrayList> maps_;

  // IndexMap to keep track of explicitly blocked external objects and
  // non-serializable/not-supported objects (e.g. API Objects).
  ObjectCacheIndexMap external_objects_ids_;

  // ObjectCacheIndexMap implements fast lookup item -> id. Some items (context,
  // function, class, array, object) can point to other items and we serialize
  // them in the reverse order. This ensures that the items this item points to
  // have a lower ID and will be deserialized first.
  ObjectCacheIndexMap string_ids_;
  ObjectCacheIndexMap map_ids_;
  ObjectCacheIndexMap context_ids_;
  ObjectCacheIndexMap function_ids_;
  ObjectCacheIndexMap class_ids_;
  ObjectCacheIndexMap array_ids_;
  ObjectCacheIndexMap object_ids_;
  uint32_t export_count_ = 0;

  std::queue<Handle<HeapObject>> discovery_queue_;

  // For keeping track of which strings have exactly one reference. Strings are
  // inserted here when the first reference is discovered, and never removed.
  // Strings which have more than one reference get an ID and are inserted to
  // strings_.
  IdentityMap<int, base::DefaultAllocationPolicy> all_strings_;

  // For constructing the minimal, "compacted", source string to cover all
  // function bodies.
  Handle<String> full_source_;
  uint32_t source_id_;
  // Ordered set of (start, end) pairs of all functions we've discovered.
  std::set<std::pair<int, int>> source_intervals_;
  // Maps function positions in the real source code into the function positions
  // in the constructed source code (which we'll include in the web snapshot).
  std::unordered_map<int, int> source_offset_to_compacted_source_offset_;
};

class V8_EXPORT WebSnapshotDeserializer
    : public WebSnapshotSerializerDeserializer {
 public:
  WebSnapshotDeserializer(v8::Isolate* v8_isolate, const uint8_t* data,
                          size_t buffer_size);
  WebSnapshotDeserializer(Isolate* isolate, Handle<Script> snapshot_as_script);
  ~WebSnapshotDeserializer();
  bool Deserialize(MaybeHandle<FixedArray> external_references = {},
                   bool skip_exports = false);

  // For inspecting the state after deserializing a snapshot.
  uint32_t string_count() const { return string_count_; }
  uint32_t map_count() const { return map_count_; }
  uint32_t context_count() const { return context_count_; }
  uint32_t function_count() const { return function_count_; }
  uint32_t class_count() const { return class_count_; }
  uint32_t array_count() const { return array_count_; }
  uint32_t object_count() const { return object_count_; }

  static void UpdatePointersCallback(v8::Isolate* isolate, v8::GCType type,
                                     v8::GCCallbackFlags flags,
                                     void* deserializer) {
    reinterpret_cast<WebSnapshotDeserializer*>(deserializer)->UpdatePointers();
  }

  void UpdatePointers();

  MaybeHandle<Object> value() const { return return_value_; }

 private:
  WebSnapshotDeserializer(Isolate* isolate, Handle<Object> script_name,
                          base::Vector<const uint8_t> buffer);
  base::Vector<const uint8_t> ExtractScriptBuffer(
      Isolate* isolate, Handle<Script> snapshot_as_script);
  bool DeserializeSnapshot(bool skip_exports);
  bool DeserializeScript();

  WebSnapshotDeserializer(const WebSnapshotDeserializer&) = delete;
  WebSnapshotDeserializer& operator=(const WebSnapshotDeserializer&) = delete;

  void DeserializeStrings();
  void DeserializeMaps();
  void DeserializeContexts();
  Handle<ScopeInfo> CreateScopeInfo(uint32_t variable_count, bool has_parent,
                                    ContextType context_type);
  Handle<JSFunction> CreateJSFunction(int index, uint32_t start,
                                      uint32_t length, uint32_t parameter_count,
                                      uint32_t flags, uint32_t context_id);
  void DeserializeFunctionData(uint32_t count, uint32_t current_count);
  void DeserializeFunctions();
  void DeserializeClasses();
  void DeserializeArrays();
  void DeserializeObjects();
  void DeserializeExports(bool skip_exports);

  Object ReadValue(
      Handle<HeapObject> object_for_deferred_reference = Handle<HeapObject>(),
      uint32_t index_for_deferred_reference = 0);

  Object ReadInteger();
  Object ReadNumber();
  String ReadString(bool internalize = false);
  String ReadInPlaceString(bool internalize = false);
  Object ReadArray(Handle<HeapObject> container, uint32_t container_index);
  Object ReadObject(Handle<HeapObject> container, uint32_t container_index);
  Object ReadFunction(Handle<HeapObject> container, uint32_t container_index);
  Object ReadClass(Handle<HeapObject> container, uint32_t container_index);
  Object ReadRegexp();
  Object ReadExternalReference();

  void ReadFunctionPrototype(Handle<JSFunction> function);
  bool SetFunctionPrototype(JSFunction function, JSReceiver prototype);

  HeapObject AddDeferredReference(Handle<HeapObject> container, uint32_t index,
                                  ValueType target_type,
                                  uint32_t target_object_index);
  void ProcessDeferredReferences();
  // Not virtual, on purpose (because it doesn't need to be).
  void Throw(const char* message);

  Handle<FixedArray> strings_handle_;
  FixedArray strings_;

  Handle<FixedArray> maps_handle_;
  FixedArray maps_;

  Handle<FixedArray> contexts_handle_;
  FixedArray contexts_;

  Handle<FixedArray> functions_handle_;
  FixedArray functions_;

  Handle<FixedArray> classes_handle_;
  FixedArray classes_;

  Handle<FixedArray> arrays_handle_;
  FixedArray arrays_;

  Handle<FixedArray> objects_handle_;
  FixedArray objects_;

  Handle<FixedArray> external_references_handle_;
  FixedArray external_references_;

  Handle<ArrayList> deferred_references_;

  Handle<WeakFixedArray> shared_function_infos_handle_;
  WeakFixedArray shared_function_infos_;

  Handle<ObjectHashTable> shared_function_info_table_;

  Handle<Script> script_;
  Handle<Object> script_name_;

  Handle<Object> return_value_;

  uint32_t string_count_ = 0;
  uint32_t map_count_ = 0;
  uint32_t context_count_ = 0;
  uint32_t function_count_ = 0;
  uint32_t current_function_count_ = 0;
  uint32_t class_count_ = 0;
  uint32_t current_class_count_ = 0;
  uint32_t array_count_ = 0;
  uint32_t current_array_count_ = 0;
  uint32_t object_count_ = 0;
  uint32_t current_object_count_ = 0;

  ValueDeserializer deserializer_;
  ReadOnlyRoots roots_;

  bool deserialized_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_
