// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_
#define V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_

#include <queue>

#include "src/handles/handles.h"
#include "src/objects/bigint.h"
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
    // It corresponds to the hole value.
    NO_ELEMENT_CONSTANT,
    INTEGER,
    DOUBLE,
    REGEXP,
    STRING_ID,
    ARRAY_ID,
    OBJECT_ID,
    FUNCTION_ID,
    CLASS_ID,
    SYMBOL_ID,
    EXTERNAL_ID,
    BUILTIN_OBJECT_ID,
    IN_PLACE_STRING_ID,
    ARRAY_BUFFER_ID,
    TYPED_ARRAY_ID,
    DATA_VIEW_ID,
    BIGINT_ID
  };

  enum SymbolType : uint8_t {
    kNonGlobalNoDesription = 0,
    kNonGlobal = 1,
    kGlobal = 2
  };

  enum ElementsType : uint8_t { kDense = 0, kSparse = 1 };

  enum TypedArrayType : uint8_t {
    kInt8Array,
    kUint8Array,
    kUint8ClampedArray,
    kInt16Array,
    kUint16Array,
    kInt32Array,
    kUint32Array,
    kFloat32Array,
    kFloat64Array,
    kBigInt64Array,
    kBigUint64Array,
  };

  static inline ExternalArrayType TypedArrayTypeToExternalArrayType(
      TypedArrayType type);
  static inline TypedArrayType ExternalArrayTypeToTypedArrayType(
      ExternalArrayType type);

  static constexpr uint8_t kMagicNumber[4] = {'+', '+', '+', ';'};

  enum ContextType : uint8_t { FUNCTION, BLOCK };

  enum PropertyAttributesType : uint8_t { DEFAULT, CUSTOM };

  uint8_t FunctionKindToFunctionFlags(FunctionKind kind);
  FunctionKind FunctionFlagsToFunctionKind(uint8_t flags);
  bool IsFunctionOrMethod(uint8_t flags);
  bool IsConstructor(uint8_t flags);

  uint8_t GetDefaultAttributeFlags();
  uint8_t AttributesToFlags(PropertyDetails details);
  PropertyAttributes FlagsToAttributes(uint8_t flags);

  uint8_t ArrayBufferViewKindToFlags(
      Handle<JSArrayBufferView> array_buffer_view);

  uint8_t ArrayBufferKindToFlags(Handle<JSArrayBuffer> array_buffer);

  uint32_t BigIntSignAndLengthToFlags(Handle<BigInt> bigint);
  uint32_t BigIntFlagsToBitField(uint32_t flags);
  // The maximum count of items for each value type (strings, objects etc.)
  static constexpr uint32_t kMaxItemCount =
      static_cast<uint32_t>(FixedArray::kMaxLength - 1);
  // This ensures indices and lengths can be converted between uint32_t and int
  // without problems:
  static_assert(kMaxItemCount <
                static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));

 protected:
  explicit WebSnapshotSerializerDeserializer(Isolate* isolate)
      : isolate_(isolate) {}
  // Not virtual, on purpose (because it doesn't need to be).
  void Throw(const char* message);

  void IterateBuiltinObjects(
      std::function<void(Handle<String>, Handle<HeapObject>)> func);

  static constexpr int kBuiltinObjectCount = 12;

  inline Factory* factory() const { return isolate_->factory(); }

  Isolate* isolate_;
  const char* error_message_ = nullptr;

  // Encode JSArrayBufferFlags, including was_detached, is_shared, is_resizable.
  // DetachedBitField indicates whether the ArrayBuffer was detached.
  using DetachedBitField = base::BitField<bool, 0, 1, uint8_t>;
  // SharedBitField indicates whether the ArrayBuffer is SharedArrayBuffer.
  using SharedBitField = DetachedBitField::Next<bool, 1>;
  // ResizableBitField indicates whether the ArrayBuffer is ResizableArrayBuffer
  // or GrowableSharedArrayBuffer.
  using ResizableBitField = SharedBitField::Next<bool, 1>;

  // Encode JSArrayBufferViewFlags, including is_length_tracking, see
  // https://github.com/tc39/proposal-resizablearraybuffer.
  // LengthTrackingBitField indicates whether the ArrayBufferView should track
  // the length of the backing buffer, that is whether the ArrayBufferView is
  // constructed without the specified length argument.
  using LengthTrackingBitField = base::BitField<bool, 0, 1, uint8_t>;

  // Encode BigInt's sign and digits length.
  using BigIntSignBitField = base::BitField<bool, 0, 1>;
  using BigIntLengthBitField =
      BigIntSignBitField::Next<int, BigInt::kLengthFieldBits>;
  static_assert(BigIntLengthBitField::kSize == BigInt::LengthBits::kSize);

 private:
  WebSnapshotSerializerDeserializer(const WebSnapshotSerializerDeserializer&) =
      delete;
  WebSnapshotSerializerDeserializer& operator=(
      const WebSnapshotSerializerDeserializer&) = delete;

  using AsyncFunctionBitField = base::BitField<bool, 0, 1, uint8_t>;
  using GeneratorFunctionBitField = AsyncFunctionBitField::Next<bool, 1>;
  using ArrowFunctionBitField = GeneratorFunctionBitField::Next<bool, 1>;
  using MethodBitField = ArrowFunctionBitField::Next<bool, 1>;
  using StaticBitField = MethodBitField::Next<bool, 1>;
  using ClassConstructorBitField = StaticBitField::Next<bool, 1>;
  using DefaultConstructorBitField = ClassConstructorBitField::Next<bool, 1>;
  using DerivedConstructorBitField = DefaultConstructorBitField::Next<bool, 1>;

  using ReadOnlyBitField = base::BitField<bool, 0, 1, uint8_t>;
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

  uint32_t symbol_count() const {
    return static_cast<uint32_t>(symbol_ids_.size());
  }

  uint32_t bigint_count() const {
    return static_cast<uint32_t>(bigint_ids_.size());
  }

  uint32_t map_count() const { return static_cast<uint32_t>(map_ids_.size()); }

  uint32_t builtin_object_count() const {
    return static_cast<uint32_t>(builtin_object_ids_.size());
  }

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

  uint32_t array_buffer_count() const {
    return static_cast<uint32_t>(array_buffer_ids_.size());
  }

  uint32_t typed_array_count() const {
    return static_cast<uint32_t>(typed_array_ids_.size());
  }

  uint32_t data_view_count() const {
    return static_cast<uint32_t>(data_view_ids_.size());
  }

  uint32_t object_count() const {
    return static_cast<uint32_t>(object_ids_.size());
  }

  uint32_t external_object_count() const {
    return static_cast<uint32_t>(external_object_ids_.size());
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
  void ShallowDiscoverBuiltinObjects(v8::Local<v8::Context> context);
  void Discover(Handle<HeapObject> object);
  void DiscoverString(Handle<String> string,
                      AllowInPlace can_be_in_place = AllowInPlace::No);
  void DiscoverSymbol(Handle<Symbol> symbol);
  void DiscoverBigInt(Handle<BigInt> bigint);
  void DiscoverMap(Handle<Map> map, bool allow_property_in_descriptor = false);
  void DiscoverPropertyKey(Handle<Name> key);
  void DiscoverMapForFunction(Handle<JSFunction> function);
  void DiscoverFunction(Handle<JSFunction> function);
  void DiscoverClass(Handle<JSFunction> function);
  void DiscoverContextAndPrototype(Handle<JSFunction> function);
  void DiscoverContext(Handle<Context> context);
  void DiscoverArray(Handle<JSArray> array);
  void DiscoverTypedArray(Handle<JSTypedArray> typed_array);
  void DiscoverDataView(Handle<JSDataView> data_view);
  void DiscoverArrayBuffer(Handle<JSArrayBuffer> array_buffer);
  void DiscoverElements(Handle<JSObject> object);
  void DiscoverObject(Handle<JSObject> object);
  bool DiscoverIfBuiltinObject(Handle<HeapObject> object);
  void DiscoverSource(Handle<JSFunction> function);
  template <typename T>
  void DiscoverObjectPropertiesWithDictionaryMap(T dict);
  bool ShouldBeSerialized(Handle<Name> key);
  void ConstructSource();

  void SerializeFunctionInfo(Handle<JSFunction> function,
                             ValueSerializer& serializer);
  void SerializeFunctionProperties(Handle<JSFunction> function,
                                   ValueSerializer& serializer);
  void SerializeString(Handle<String> string, ValueSerializer& serializer);
  void SerializeSymbol(Handle<Symbol> symbol);
  void SerializeBigInt(Handle<BigInt> bigint);
  void SerializeMap(Handle<Map> map);
  void SerializeBuiltinObject(uint32_t name_id);
  void SerializeObjectPrototype(Handle<Map> map, ValueSerializer& serializer);

  template <typename T>
  void SerializeObjectPropertiesWithDictionaryMap(T dict);
  void SerializeFunction(Handle<JSFunction> function);
  void SerializeClass(Handle<JSFunction> function);
  void SerializeContext(Handle<Context> context, uint32_t id);
  void SerializeArray(Handle<JSArray> array);
  void SerializeElements(Handle<JSObject> object, ValueSerializer& serializer,
                         Maybe<uint32_t> length);
  void SerializeObject(Handle<JSObject> object);
  void SerializeArrayBufferView(Handle<JSArrayBufferView> array_buffer_view,
                                ValueSerializer& serializer);
  void SerializeArrayBuffer(Handle<JSArrayBuffer> array_buffer);
  void SerializeTypedArray(Handle<JSTypedArray> typed_array);
  void SerializeDataView(Handle<JSDataView> data_view);

  void SerializeExport(Handle<Object> object, Handle<String> export_name);
  void WriteValue(Handle<Object> object, ValueSerializer& serializer);
  void WriteStringMaybeInPlace(Handle<String> string,
                               ValueSerializer& serializer);
  void WriteStringId(Handle<String> string, ValueSerializer& serializer);

  uint32_t GetStringId(Handle<String> string, bool& in_place);
  uint32_t GetSymbolId(Symbol symbol);
  uint32_t GetBigIntId(BigInt bigint);
  uint32_t GetMapId(Map map);
  uint32_t GetFunctionId(JSFunction function);
  uint32_t GetClassId(JSFunction function);
  uint32_t GetContextId(Context context);
  uint32_t GetArrayId(JSArray array);
  uint32_t GetTypedArrayId(JSTypedArray typed_array);
  uint32_t GetDataViewId(JSDataView data_view);
  uint32_t GetArrayBufferId(JSArrayBuffer array_buffer);
  uint32_t GetObjectId(JSObject object);
  bool GetExternalId(HeapObject object, uint32_t* id = nullptr);
  // Returns index into builtin_object_name_strings_.
  bool GetBuiltinObjectNameIndex(HeapObject object, uint32_t& index);
  bool GetBuiltinObjectId(HeapObject object, uint32_t& id);

  ValueSerializer string_serializer_;
  ValueSerializer symbol_serializer_;
  ValueSerializer bigint_serializer_;
  ValueSerializer map_serializer_;
  ValueSerializer builtin_object_serializer_;
  ValueSerializer context_serializer_;
  ValueSerializer function_serializer_;
  ValueSerializer class_serializer_;
  ValueSerializer array_serializer_;
  ValueSerializer typed_array_serializer_;
  ValueSerializer array_buffer_serializer_;
  ValueSerializer data_view_serializer_;
  ValueSerializer object_serializer_;
  ValueSerializer export_serializer_;

  // These are needed for being able to serialize items in order.
  Handle<ArrayList> strings_;
  Handle<ArrayList> symbols_;
  Handle<ArrayList> bigints_;
  Handle<ArrayList> maps_;
  Handle<ArrayList> contexts_;
  Handle<ArrayList> functions_;
  Handle<ArrayList> classes_;
  Handle<ArrayList> arrays_;
  Handle<ArrayList> typed_arrays_;
  Handle<ArrayList> array_buffers_;
  Handle<ArrayList> data_views_;
  Handle<ArrayList> objects_;

  // IndexMap to keep track of explicitly blocked external objects and
  // non-serializable/not-supported objects (e.g. API Objects).
  ObjectCacheIndexMap external_object_ids_;

  // ObjectCacheIndexMap implements fast lookup item -> id. Some items (context,
  // function, class, array, object) can point to other items and we serialize
  // them in the reverse order. This ensures that the items this item points to
  // have a lower ID and will be deserialized first.
  ObjectCacheIndexMap string_ids_;
  ObjectCacheIndexMap symbol_ids_;
  ObjectCacheIndexMap bigint_ids_;
  ObjectCacheIndexMap map_ids_;
  ObjectCacheIndexMap context_ids_;
  ObjectCacheIndexMap function_ids_;
  ObjectCacheIndexMap class_ids_;
  ObjectCacheIndexMap array_ids_;
  ObjectCacheIndexMap typed_array_ids_;
  ObjectCacheIndexMap array_buffer_ids_;
  ObjectCacheIndexMap data_view_ids_;
  ObjectCacheIndexMap object_ids_;
  uint32_t export_count_ = 0;

  // For handling references to builtin objects:
  // --------------------------------
  // String objects for the names of all known builtins.
  Handle<FixedArray> builtin_object_name_strings_;

  // Map object -> index in builtin_name_strings_ for all known builtins.
  ObjectCacheIndexMap builtin_object_to_name_;

  // Map object -> index in builtins_. Includes only builtins which will be
  // incluced in the snapshot.
  ObjectCacheIndexMap builtin_object_ids_;

  // For creating the Builtin wrappers in the snapshot. Includes only builtins
  // which will be incluced in the snapshot. Each element is the id of the
  // builtin name string in the snapshot.
  std::vector<uint32_t> builtin_objects_;
  // --------------------------------

  std::queue<Handle<HeapObject>> discovery_queue_;

  // For keeping track of which strings have exactly one reference. Strings are
  // inserted here when the first reference is discovered, and never removed.
  // Strings which have more than one reference get an ID and are inserted to
  // strings_.
  IdentityMap<int, base::DefaultAllocationPolicy> all_strings_;

  // For constructing the minimal, "compacted", source string to cover all
  // function bodies.
  // --------------------------------
  // Script id -> offset of the script source code in full_source_.
  std::map<int, int> script_offsets_;
  Handle<String> full_source_;
  uint32_t source_id_;
  // Ordered set of (start, end) pairs of all functions we've discovered.
  std::set<std::pair<int, int>> source_intervals_;
  // Maps function positions in the real source code into the function positions
  // in the constructed source code (which we'll include in the web snapshot).
  std::unordered_map<int, int> source_offset_to_compacted_source_offset_;
  // --------------------------------
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
  uint32_t symbol_count() const { return symbol_count_; }
  uint32_t map_count() const { return map_count_; }
  uint32_t builtin_object_count() const { return builtin_object_count_; }
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
  enum class InternalizeStrings {
    kNo,
    kYes,
  };

  WebSnapshotDeserializer(Isolate* isolate, Handle<Object> script_name,
                          base::Vector<const uint8_t> buffer);
  // Return value: {data, length, data_owned}.
  std::tuple<const uint8_t*, uint32_t, bool> ExtractScriptBuffer(
      Isolate* isolate, Handle<Script> snapshot_as_script);
  bool DeserializeSnapshot(bool skip_exports);
  void CollectBuiltinObjects();
  bool DeserializeScript();

  WebSnapshotDeserializer(const WebSnapshotDeserializer&) = delete;
  WebSnapshotDeserializer& operator=(const WebSnapshotDeserializer&) = delete;

  void DeserializeStrings();
  void DeserializeSymbols();
  void DeserializeBigInts();
  void DeserializeMaps();
  void DeserializeBuiltinObjects();
  void DeserializeContexts();
  Handle<ScopeInfo> CreateScopeInfo(uint32_t variable_count, bool has_parent,
                                    ContextType context_type,
                                    bool has_inlined_local_names);
  Handle<JSFunction> CreateJSFunction(int index, uint32_t start,
                                      uint32_t length, uint32_t parameter_count,
                                      uint8_t flags, uint32_t context_id);
  void DeserializeFunctionData(uint32_t count, uint32_t current_count);
  void DeserializeFunctions();
  void DeserializeClasses();
  void DeserializeArrays();
  void DeserializeArrayBuffers();
  void DeserializeTypedArrays();
  void DeserializeDataViews();
  void DeserializeObjects();
  void DeserializeObjectElements(Handle<JSObject> object,
                                 bool map_from_snapshot);
  void DeserializeExports(bool skip_exports);
  void DeserializeObjectPrototype(Handle<Map> map);
  Handle<Map> DeserializeObjectPrototypeAndCreateEmptyMap();
  void DeserializeObjectPrototypeForFunction(Handle<JSFunction> function);
  void SetPrototype(Handle<Map> map, Handle<Object> prototype);
  void DeserializeFunctionProperties(Handle<JSFunction> function);
  bool ReadCount(uint32_t& count);

  bool IsInitialFunctionPrototype(Object prototype);

  template <typename T>
  void DeserializeObjectPropertiesWithDictionaryMap(
      T dict, uint32_t property_count, bool has_custom_property_attributes);

  Handle<PropertyArray> DeserializePropertyArray(
      Handle<DescriptorArray> descriptors, int no_properties);

  // Return value: (object, was_deferred)
  std::tuple<Object, bool> ReadValue(
      Handle<HeapObject> object_for_deferred_reference = Handle<HeapObject>(),
      uint32_t index_for_deferred_reference = 0,
      InternalizeStrings internalize_strings = InternalizeStrings::kNo);

  Object ReadInteger();
  Object ReadNumber();
  String ReadString(
      InternalizeStrings internalize_strings = InternalizeStrings::kNo);
  String ReadInPlaceString(
      InternalizeStrings internalize_strings = InternalizeStrings::kNo);
  Object ReadSymbol();
  Object ReadBigInt();
  std::tuple<Object, bool> ReadArray(Handle<HeapObject> container,
                                     uint32_t container_index);
  std::tuple<Object, bool> ReadArrayBuffer(Handle<HeapObject> container,
                                           uint32_t container_index);
  std::tuple<Object, bool> ReadTypedArray(Handle<HeapObject> container,
                                          uint32_t container_index);
  std::tuple<Object, bool> ReadDataView(Handle<HeapObject> container,
                                        uint32_t container_index);
  std::tuple<Object, bool> ReadObject(Handle<HeapObject> container,
                                      uint32_t container_index);
  std::tuple<Object, bool> ReadFunction(Handle<HeapObject> container,
                                        uint32_t container_index);
  std::tuple<Object, bool> ReadClass(Handle<HeapObject> container,
                                     uint32_t container_index);
  Object ReadRegexp();
  Object ReadBuiltinObjectReference();
  Object ReadExternalReference();
  bool ReadMapType();
  std::tuple<Handle<FixedArrayBase>, ElementsKind, uint32_t>
  DeserializeElements();
  ElementsType ReadElementsType();
  std::tuple<Handle<FixedArrayBase>, ElementsKind, uint32_t> ReadDenseElements(
      uint32_t length);
  std::tuple<Handle<FixedArrayBase>, ElementsKind, uint32_t> ReadSparseElements(
      uint32_t length);

  void ReadFunctionPrototype(Handle<JSFunction> function);
  bool SetFunctionPrototype(JSFunction function, JSReceiver prototype);

  HeapObject AddDeferredReference(Handle<HeapObject> container, uint32_t index,
                                  ValueType target_type,
                                  uint32_t target_object_index);
  void ProcessDeferredReferences();
  // Not virtual, on purpose (because it doesn't need to be).
  void Throw(const char* message);
  void VerifyObjects();

  Handle<FixedArray> strings_handle_;
  FixedArray strings_;

  Handle<FixedArray> symbols_handle_;
  FixedArray symbols_;

  Handle<FixedArray> bigints_handle_;
  FixedArray bigints_;

  Handle<FixedArray> builtin_objects_handle_;
  FixedArray builtin_objects_;

  Handle<FixedArray> maps_handle_;
  FixedArray maps_;
  std::map<int, Handle<Map>> deserialized_function_maps_;

  Handle<FixedArray> contexts_handle_;
  FixedArray contexts_;

  Handle<FixedArray> functions_handle_;
  FixedArray functions_;

  Handle<FixedArray> classes_handle_;
  FixedArray classes_;

  Handle<FixedArray> arrays_handle_;
  FixedArray arrays_;

  Handle<FixedArray> array_buffers_handle_;
  FixedArray array_buffers_;

  Handle<FixedArray> typed_arrays_handle_;
  FixedArray typed_arrays_;

  Handle<FixedArray> data_views_handle_;
  FixedArray data_views_;

  Handle<FixedArray> objects_handle_;
  FixedArray objects_;

  Handle<FixedArray> external_references_handle_;
  FixedArray external_references_;

  // Map: String -> builtin object.
  Handle<ObjectHashTable> builtin_object_name_to_object_;

  Handle<ArrayList> deferred_references_;

  Handle<WeakFixedArray> shared_function_infos_handle_;
  WeakFixedArray shared_function_infos_;

  Handle<ObjectHashTable> shared_function_info_table_;

  Handle<Script> script_;
  Handle<Object> script_name_;

  Handle<Object> return_value_;

  uint32_t string_count_ = 0;
  uint32_t symbol_count_ = 0;
  uint32_t bigint_count_ = 0;
  uint32_t map_count_ = 0;
  uint32_t builtin_object_count_ = 0;
  uint32_t context_count_ = 0;
  uint32_t function_count_ = 0;
  uint32_t current_function_count_ = 0;
  uint32_t class_count_ = 0;
  uint32_t current_class_count_ = 0;
  uint32_t array_count_ = 0;
  uint32_t current_array_count_ = 0;
  uint32_t array_buffer_count_ = 0;
  uint32_t current_array_buffer_count_ = 0;
  uint32_t typed_array_count_ = 0;
  uint32_t current_typed_array_count_ = 0;
  uint32_t data_view_count_ = 0;
  uint32_t current_data_view_count_ = 0;
  uint32_t object_count_ = 0;
  uint32_t current_object_count_ = 0;

  std::unique_ptr<ValueDeserializer> deserializer_;
  std::unique_ptr<const uint8_t[]> owned_data_;
  ReadOnlyRoots roots_;

  bool deserialized_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_WEB_SNAPSHOT_WEB_SNAPSHOT_H_
