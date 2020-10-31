// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_MAP_FIELD_H__
#define GOOGLE_PROTOBUF_MAP_FIELD_H__

#include <atomic>

#include <google/protobuf/arena.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/map_entry.h>
#include <google/protobuf/map_field_lite.h>
#include <google/protobuf/map_type_handler.h>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/mutex.h>
#include <google/protobuf/port.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/unknown_field_set.h>


#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
class DynamicMessage;
class MapKey;
class MapIterator;
namespace internal {

class ContendedMapCleanTest;
class GeneratedMessageReflection;
class MapFieldAccessor;

// This class provides access to map field using reflection, which is the same
// as those provided for RepeatedPtrField<Message>. It is used for internal
// reflection implentation only. Users should never use this directly.
class PROTOBUF_EXPORT MapFieldBase {
 public:
  MapFieldBase()
      : arena_(NULL), repeated_field_(NULL), state_(STATE_MODIFIED_MAP) {}
  explicit MapFieldBase(Arena* arena)
      : arena_(arena), repeated_field_(NULL), state_(STATE_MODIFIED_MAP) {
    // Mutex's destructor needs to be called explicitly to release resources
    // acquired in its constructor.
    arena->OwnDestructor(&mutex_);
  }
  virtual ~MapFieldBase();

  // Returns reference to internal repeated field. Data written using
  // Map's api prior to calling this function is guarantted to be
  // included in repeated field.
  const RepeatedPtrFieldBase& GetRepeatedField() const;

  // Like above. Returns mutable pointer to the internal repeated field.
  RepeatedPtrFieldBase* MutableRepeatedField();

  // Pure virtual map APIs for Map Reflection.
  virtual bool ContainsMapKey(const MapKey& map_key) const = 0;
  virtual bool InsertOrLookupMapValue(const MapKey& map_key,
                                      MapValueRef* val) = 0;
  // Returns whether changes to the map are reflected in the repeated field.
  bool IsRepeatedFieldValid() const;
  // Insures operations after won't get executed before calling this.
  bool IsMapValid() const;
  virtual bool DeleteMapValue(const MapKey& map_key) = 0;
  virtual bool EqualIterator(const MapIterator& a,
                             const MapIterator& b) const = 0;
  virtual void MapBegin(MapIterator* map_iter) const = 0;
  virtual void MapEnd(MapIterator* map_iter) const = 0;
  virtual void MergeFrom(const MapFieldBase& other) = 0;
  virtual void Swap(MapFieldBase* other) = 0;
  // Sync Map with repeated field and returns the size of map.
  virtual int size() const = 0;
  virtual void Clear() = 0;

  // Returns the number of bytes used by the repeated field, excluding
  // sizeof(*this)
  size_t SpaceUsedExcludingSelfLong() const;

  int SpaceUsedExcludingSelf() const {
    return internal::ToIntSize(SpaceUsedExcludingSelfLong());
  }

 protected:
  // Gets the size of space used by map field.
  virtual size_t SpaceUsedExcludingSelfNoLock() const;

  // Synchronizes the content in Map to RepeatedPtrField if there is any change
  // to Map after last synchronization.
  void SyncRepeatedFieldWithMap() const;
  virtual void SyncRepeatedFieldWithMapNoLock() const;

  // Synchronizes the content in RepeatedPtrField to Map if there is any change
  // to RepeatedPtrField after last synchronization.
  void SyncMapWithRepeatedField() const;
  virtual void SyncMapWithRepeatedFieldNoLock() const {}

  // Tells MapFieldBase that there is new change to Map.
  void SetMapDirty();

  // Tells MapFieldBase that there is new change to RepeatedPTrField.
  void SetRepeatedDirty();

  // Provides derived class the access to repeated field.
  void* MutableRepeatedPtrField() const;

  enum State {
    STATE_MODIFIED_MAP = 0,       // map has newly added data that has not been
                                  // synchronized to repeated field
    STATE_MODIFIED_REPEATED = 1,  // repeated field has newly added data that
                                  // has not been synchronized to map
    CLEAN = 2,                    // data in map and repeated field are same
  };

  Arena* arena_;
  mutable RepeatedPtrField<Message>* repeated_field_;

  mutable internal::WrappedMutex
      mutex_;  // The thread to synchronize map and repeated field
               // needs to get lock first;
  mutable std::atomic<State> state_;

 private:
  friend class ContendedMapCleanTest;
  friend class GeneratedMessageReflection;
  friend class MapFieldAccessor;
  friend class DynamicMessage;

  // Virtual helper methods for MapIterator. MapIterator doesn't have the
  // type helper for key and value. Call these help methods to deal with
  // different types. Real helper methods are implemented in
  // TypeDefinedMapFieldBase.
  friend class ::PROTOBUF_NAMESPACE_ID::MapIterator;
  // Allocate map<...>::iterator for MapIterator.
  virtual void InitializeIterator(MapIterator* map_iter) const = 0;

  // DeleteIterator() is called by the destructor of MapIterator only.
  // It deletes map<...>::iterator for MapIterator.
  virtual void DeleteIterator(MapIterator* map_iter) const = 0;

  // Copy the map<...>::iterator from other_iterator to
  // this_iterator.
  virtual void CopyIterator(MapIterator* this_iterator,
                            const MapIterator& other_iterator) const = 0;

  // IncreaseIterator() is called by operator++() of MapIterator only.
  // It implements the ++ operator of MapIterator.
  virtual void IncreaseIterator(MapIterator* map_iter) const = 0;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MapFieldBase);
};

// This class provides common Map Reflection implementations for generated
// message and dynamic message.
template <typename Key, typename T>
class TypeDefinedMapFieldBase : public MapFieldBase {
 public:
  TypeDefinedMapFieldBase() {}
  explicit TypeDefinedMapFieldBase(Arena* arena) : MapFieldBase(arena) {}
  ~TypeDefinedMapFieldBase() override {}
  void MapBegin(MapIterator* map_iter) const override;
  void MapEnd(MapIterator* map_iter) const override;
  bool EqualIterator(const MapIterator& a, const MapIterator& b) const override;

  virtual const Map<Key, T>& GetMap() const = 0;
  virtual Map<Key, T>* MutableMap() = 0;

 protected:
  typename Map<Key, T>::const_iterator& InternalGetIterator(
      const MapIterator* map_iter) const;

 private:
  void InitializeIterator(MapIterator* map_iter) const override;
  void DeleteIterator(MapIterator* map_iter) const override;
  void CopyIterator(MapIterator* this_iteratorm,
                    const MapIterator& that_iterator) const override;
  void IncreaseIterator(MapIterator* map_iter) const override;

  virtual void SetMapIteratorValue(MapIterator* map_iter) const = 0;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TypeDefinedMapFieldBase);
};

// This class provides access to map field using generated api. It is used for
// internal generated message implentation only. Users should never use this
// directly.
template <typename Derived, typename Key, typename T,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value = 0>
class MapField : public TypeDefinedMapFieldBase<Key, T> {
  // Provide utilities to parse/serialize key/value.  Provide utilities to
  // manipulate internal stored type.
  typedef MapTypeHandler<kKeyFieldType, Key> KeyTypeHandler;
  typedef MapTypeHandler<kValueFieldType, T> ValueTypeHandler;

  // Define message type for internal repeated field.
  typedef Derived EntryType;

  // Define abbreviation for parent MapFieldLite
  typedef MapFieldLite<Derived, Key, T, kKeyFieldType, kValueFieldType,
                       default_enum_value>
      MapFieldLiteType;

  // Enum needs to be handled differently from other types because it has
  // different exposed type in Map's api and repeated field's api. For
  // details see the comment in the implementation of
  // SyncMapWithRepeatedFieldNoLock.
  static const bool kIsValueEnum = ValueTypeHandler::kIsEnum;
  typedef typename MapIf<kIsValueEnum, T, const T&>::type CastValueType;

 public:
  typedef typename Derived::SuperType EntryTypeTrait;
  typedef Map<Key, T> MapType;

  MapField() {}
  explicit MapField(Arena* arena)
      : TypeDefinedMapFieldBase<Key, T>(arena), impl_(arena) {}

  // Implement MapFieldBase
  bool ContainsMapKey(const MapKey& map_key) const override;
  bool InsertOrLookupMapValue(const MapKey& map_key, MapValueRef* val) override;
  bool DeleteMapValue(const MapKey& map_key) override;

  const Map<Key, T>& GetMap() const override {
    MapFieldBase::SyncMapWithRepeatedField();
    return impl_.GetMap();
  }

  Map<Key, T>* MutableMap() override {
    MapFieldBase::SyncMapWithRepeatedField();
    Map<Key, T>* result = impl_.MutableMap();
    MapFieldBase::SetMapDirty();
    return result;
  }

  int size() const override;
  void Clear() override;
  void MergeFrom(const MapFieldBase& other) override;
  void Swap(MapFieldBase* other) override;

  // Used in the implementation of parsing. Caller should take the ownership iff
  // arena_ is NULL.
  EntryType* NewEntry() const { return impl_.NewEntry(); }
  // Used in the implementation of serializing enum value type. Caller should
  // take the ownership iff arena_ is NULL.
  EntryType* NewEnumEntryWrapper(const Key& key, const T t) const {
    return impl_.NewEnumEntryWrapper(key, t);
  }
  // Used in the implementation of serializing other value types. Caller should
  // take the ownership iff arena_ is NULL.
  EntryType* NewEntryWrapper(const Key& key, const T& t) const {
    return impl_.NewEntryWrapper(key, t);
  }

  const char* _InternalParse(const char* ptr, ParseContext* ctx) {
    return impl_._InternalParse(ptr, ctx);
  }
  template <typename Metadata>
  const char* ParseWithEnumValidation(const char* ptr, ParseContext* ctx,
                                      bool (*is_valid)(int), uint32 field_num,
                                      Metadata* metadata) {
    return impl_.ParseWithEnumValidation(ptr, ctx, is_valid, field_num,
                                         metadata);
  }

 private:
  MapFieldLiteType impl_;

  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;

  // Implements MapFieldBase
  void SyncRepeatedFieldWithMapNoLock() const override;
  void SyncMapWithRepeatedFieldNoLock() const override;
  size_t SpaceUsedExcludingSelfNoLock() const override;

  void SetMapIteratorValue(MapIterator* map_iter) const override;

  friend class ::PROTOBUF_NAMESPACE_ID::Arena;
  friend class MapFieldStateTest;  // For testing, it needs raw access to impl_
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MapField);
};

template <typename T, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
struct MapEntryToMapField<MapEntry<T, Key, Value, kKeyFieldType,
                                   kValueFieldType, default_enum_value>> {
  typedef MapField<T, Key, Value, kKeyFieldType, kValueFieldType,
                   default_enum_value>
      MapFieldType;
};

class PROTOBUF_EXPORT DynamicMapField
    : public TypeDefinedMapFieldBase<MapKey, MapValueRef> {
 public:
  explicit DynamicMapField(const Message* default_entry);
  DynamicMapField(const Message* default_entry, Arena* arena);
  ~DynamicMapField() override;

  // Implement MapFieldBase
  bool ContainsMapKey(const MapKey& map_key) const override;
  bool InsertOrLookupMapValue(const MapKey& map_key, MapValueRef* val) override;
  bool DeleteMapValue(const MapKey& map_key) override;
  void MergeFrom(const MapFieldBase& other) override;
  void Swap(MapFieldBase* other) override;

  const Map<MapKey, MapValueRef>& GetMap() const override;
  Map<MapKey, MapValueRef>* MutableMap() override;

  int size() const override;
  void Clear() override;

 private:
  Map<MapKey, MapValueRef> map_;
  const Message* default_entry_;

  void AllocateMapValue(MapValueRef* map_val);

  // Implements MapFieldBase
  void SyncRepeatedFieldWithMapNoLock() const override;
  void SyncMapWithRepeatedFieldNoLock() const override;
  size_t SpaceUsedExcludingSelfNoLock() const override;
  void SetMapIteratorValue(MapIterator* map_iter) const override;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DynamicMapField);
};

}  // namespace internal

#define TYPE_CHECK(EXPECTEDTYPE, METHOD)                                   \
  if (type() != EXPECTEDTYPE) {                                            \
    GOOGLE_LOG(FATAL) << "Protocol Buffer map usage error:\n"                     \
               << METHOD << " type does not match\n"                       \
               << "  Expected : "                                          \
               << FieldDescriptor::CppTypeName(EXPECTEDTYPE) << "\n"       \
               << "  Actual   : " << FieldDescriptor::CppTypeName(type()); \
  }

// MapKey is an union type for representing any possible
// map key.
class PROTOBUF_EXPORT MapKey {
 public:
  MapKey() : type_(0) {}
  MapKey(const MapKey& other) : type_(0) { CopyFrom(other); }
  MapKey& operator=(const MapKey& other) {
    CopyFrom(other);
    return *this;
  }

  ~MapKey() {
    if (type_ == FieldDescriptor::CPPTYPE_STRING) {
      delete val_.string_value_;
    }
  }

  FieldDescriptor::CppType type() const {
    if (type_ == 0) {
      GOOGLE_LOG(FATAL) << "Protocol Buffer map usage error:\n"
                 << "MapKey::type MapKey is not initialized. "
                 << "Call set methods to initialize MapKey.";
    }
    return (FieldDescriptor::CppType)type_;
  }

  void SetInt64Value(int64 value) {
    SetType(FieldDescriptor::CPPTYPE_INT64);
    val_.int64_value_ = value;
  }
  void SetUInt64Value(uint64 value) {
    SetType(FieldDescriptor::CPPTYPE_UINT64);
    val_.uint64_value_ = value;
  }
  void SetInt32Value(int32 value) {
    SetType(FieldDescriptor::CPPTYPE_INT32);
    val_.int32_value_ = value;
  }
  void SetUInt32Value(uint32 value) {
    SetType(FieldDescriptor::CPPTYPE_UINT32);
    val_.uint32_value_ = value;
  }
  void SetBoolValue(bool value) {
    SetType(FieldDescriptor::CPPTYPE_BOOL);
    val_.bool_value_ = value;
  }
  void SetStringValue(const std::string& val) {
    SetType(FieldDescriptor::CPPTYPE_STRING);
    *val_.string_value_ = val;
  }

  int64 GetInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT64, "MapKey::GetInt64Value");
    return val_.int64_value_;
  }
  uint64 GetUInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT64, "MapKey::GetUInt64Value");
    return val_.uint64_value_;
  }
  int32 GetInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT32, "MapKey::GetInt32Value");
    return val_.int32_value_;
  }
  uint32 GetUInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT32, "MapKey::GetUInt32Value");
    return val_.uint32_value_;
  }
  bool GetBoolValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_BOOL, "MapKey::GetBoolValue");
    return val_.bool_value_;
  }
  const std::string& GetStringValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_STRING, "MapKey::GetStringValue");
    return *val_.string_value_;
  }

  bool operator<(const MapKey& other) const {
    if (type_ != other.type_) {
      // We could define a total order that handles this case, but
      // there currently no need.  So, for now, fail.
      GOOGLE_LOG(FATAL) << "Unsupported: type mismatch";
    }
    switch (type()) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
      case FieldDescriptor::CPPTYPE_FLOAT:
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        GOOGLE_LOG(FATAL) << "Unsupported";
        return false;
      case FieldDescriptor::CPPTYPE_STRING:
        return *val_.string_value_ < *other.val_.string_value_;
      case FieldDescriptor::CPPTYPE_INT64:
        return val_.int64_value_ < other.val_.int64_value_;
      case FieldDescriptor::CPPTYPE_INT32:
        return val_.int32_value_ < other.val_.int32_value_;
      case FieldDescriptor::CPPTYPE_UINT64:
        return val_.uint64_value_ < other.val_.uint64_value_;
      case FieldDescriptor::CPPTYPE_UINT32:
        return val_.uint32_value_ < other.val_.uint32_value_;
      case FieldDescriptor::CPPTYPE_BOOL:
        return val_.bool_value_ < other.val_.bool_value_;
    }
    return false;
  }

  bool operator==(const MapKey& other) const {
    if (type_ != other.type_) {
      // To be consistent with operator<, we don't allow this either.
      GOOGLE_LOG(FATAL) << "Unsupported: type mismatch";
    }
    switch (type()) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
      case FieldDescriptor::CPPTYPE_FLOAT:
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        GOOGLE_LOG(FATAL) << "Unsupported";
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        return *val_.string_value_ == *other.val_.string_value_;
      case FieldDescriptor::CPPTYPE_INT64:
        return val_.int64_value_ == other.val_.int64_value_;
      case FieldDescriptor::CPPTYPE_INT32:
        return val_.int32_value_ == other.val_.int32_value_;
      case FieldDescriptor::CPPTYPE_UINT64:
        return val_.uint64_value_ == other.val_.uint64_value_;
      case FieldDescriptor::CPPTYPE_UINT32:
        return val_.uint32_value_ == other.val_.uint32_value_;
      case FieldDescriptor::CPPTYPE_BOOL:
        return val_.bool_value_ == other.val_.bool_value_;
    }
    GOOGLE_LOG(FATAL) << "Can't get here.";
    return false;
  }

  void CopyFrom(const MapKey& other) {
    SetType(other.type());
    switch (type_) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
      case FieldDescriptor::CPPTYPE_FLOAT:
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        GOOGLE_LOG(FATAL) << "Unsupported";
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        *val_.string_value_ = *other.val_.string_value_;
        break;
      case FieldDescriptor::CPPTYPE_INT64:
        val_.int64_value_ = other.val_.int64_value_;
        break;
      case FieldDescriptor::CPPTYPE_INT32:
        val_.int32_value_ = other.val_.int32_value_;
        break;
      case FieldDescriptor::CPPTYPE_UINT64:
        val_.uint64_value_ = other.val_.uint64_value_;
        break;
      case FieldDescriptor::CPPTYPE_UINT32:
        val_.uint32_value_ = other.val_.uint32_value_;
        break;
      case FieldDescriptor::CPPTYPE_BOOL:
        val_.bool_value_ = other.val_.bool_value_;
        break;
    }
  }

 private:
  template <typename K, typename V>
  friend class internal::TypeDefinedMapFieldBase;
  friend class ::PROTOBUF_NAMESPACE_ID::MapIterator;
  friend class internal::DynamicMapField;

  union KeyValue {
    KeyValue() {}
    std::string* string_value_;
    int64 int64_value_;
    int32 int32_value_;
    uint64 uint64_value_;
    uint32 uint32_value_;
    bool bool_value_;
  } val_;

  void SetType(FieldDescriptor::CppType type) {
    if (type_ == type) return;
    if (type_ == FieldDescriptor::CPPTYPE_STRING) {
      delete val_.string_value_;
    }
    type_ = type;
    if (type_ == FieldDescriptor::CPPTYPE_STRING) {
      val_.string_value_ = new std::string;
    }
  }

  // type_ is 0 or a valid FieldDescriptor::CppType.
  int type_;
};

// MapValueRef points to a map value.
class PROTOBUF_EXPORT MapValueRef {
 public:
  MapValueRef() : data_(NULL), type_(0) {}

  void SetInt64Value(int64 value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT64, "MapValueRef::SetInt64Value");
    *reinterpret_cast<int64*>(data_) = value;
  }
  void SetUInt64Value(uint64 value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT64, "MapValueRef::SetUInt64Value");
    *reinterpret_cast<uint64*>(data_) = value;
  }
  void SetInt32Value(int32 value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT32, "MapValueRef::SetInt32Value");
    *reinterpret_cast<int32*>(data_) = value;
  }
  void SetUInt32Value(uint32 value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT32, "MapValueRef::SetUInt32Value");
    *reinterpret_cast<uint32*>(data_) = value;
  }
  void SetBoolValue(bool value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_BOOL, "MapValueRef::SetBoolValue");
    *reinterpret_cast<bool*>(data_) = value;
  }
  // TODO(jieluo) - Checks that enum is member.
  void SetEnumValue(int value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_ENUM, "MapValueRef::SetEnumValue");
    *reinterpret_cast<int*>(data_) = value;
  }
  void SetStringValue(const std::string& value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_STRING, "MapValueRef::SetStringValue");
    *reinterpret_cast<std::string*>(data_) = value;
  }
  void SetFloatValue(float value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_FLOAT, "MapValueRef::SetFloatValue");
    *reinterpret_cast<float*>(data_) = value;
  }
  void SetDoubleValue(double value) {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_DOUBLE, "MapValueRef::SetDoubleValue");
    *reinterpret_cast<double*>(data_) = value;
  }

  int64 GetInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT64, "MapValueRef::GetInt64Value");
    return *reinterpret_cast<int64*>(data_);
  }
  uint64 GetUInt64Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT64, "MapValueRef::GetUInt64Value");
    return *reinterpret_cast<uint64*>(data_);
  }
  int32 GetInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_INT32, "MapValueRef::GetInt32Value");
    return *reinterpret_cast<int32*>(data_);
  }
  uint32 GetUInt32Value() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_UINT32, "MapValueRef::GetUInt32Value");
    return *reinterpret_cast<uint32*>(data_);
  }
  bool GetBoolValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_BOOL, "MapValueRef::GetBoolValue");
    return *reinterpret_cast<bool*>(data_);
  }
  int GetEnumValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_ENUM, "MapValueRef::GetEnumValue");
    return *reinterpret_cast<int*>(data_);
  }
  const std::string& GetStringValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_STRING, "MapValueRef::GetStringValue");
    return *reinterpret_cast<std::string*>(data_);
  }
  float GetFloatValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_FLOAT, "MapValueRef::GetFloatValue");
    return *reinterpret_cast<float*>(data_);
  }
  double GetDoubleValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_DOUBLE, "MapValueRef::GetDoubleValue");
    return *reinterpret_cast<double*>(data_);
  }

  const Message& GetMessageValue() const {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_MESSAGE,
               "MapValueRef::GetMessageValue");
    return *reinterpret_cast<Message*>(data_);
  }

  Message* MutableMessageValue() {
    TYPE_CHECK(FieldDescriptor::CPPTYPE_MESSAGE,
               "MapValueRef::MutableMessageValue");
    return reinterpret_cast<Message*>(data_);
  }

 private:
  template <typename Derived, typename K, typename V,
            internal::WireFormatLite::FieldType key_wire_type,
            internal::WireFormatLite::FieldType value_wire_type,
            int default_enum_value>
  friend class internal::MapField;
  template <typename K, typename V>
  friend class internal::TypeDefinedMapFieldBase;
  friend class ::PROTOBUF_NAMESPACE_ID::MapIterator;
  friend class Reflection;
  friend class internal::DynamicMapField;

  void SetType(FieldDescriptor::CppType type) { type_ = type; }

  FieldDescriptor::CppType type() const {
    if (type_ == 0 || data_ == NULL) {
      GOOGLE_LOG(FATAL) << "Protocol Buffer map usage error:\n"
                 << "MapValueRef::type MapValueRef is not initialized.";
    }
    return (FieldDescriptor::CppType)type_;
  }
  void SetValue(const void* val) { data_ = const_cast<void*>(val); }
  void CopyFrom(const MapValueRef& other) {
    type_ = other.type_;
    data_ = other.data_;
  }
  // Only used in DynamicMapField
  void DeleteData() {
    switch (type_) {
#define HANDLE_TYPE(CPPTYPE, TYPE)           \
  case FieldDescriptor::CPPTYPE_##CPPTYPE: { \
    delete reinterpret_cast<TYPE*>(data_);   \
    break;                                   \
  }
      HANDLE_TYPE(INT32, int32);
      HANDLE_TYPE(INT64, int64);
      HANDLE_TYPE(UINT32, uint32);
      HANDLE_TYPE(UINT64, uint64);
      HANDLE_TYPE(DOUBLE, double);
      HANDLE_TYPE(FLOAT, float);
      HANDLE_TYPE(BOOL, bool);
      HANDLE_TYPE(STRING, std::string);
      HANDLE_TYPE(ENUM, int32);
      HANDLE_TYPE(MESSAGE, Message);
#undef HANDLE_TYPE
    }
  }
  // data_ point to a map value. MapValueRef does not
  // own this value.
  void* data_;
  // type_ is 0 or a valid FieldDescriptor::CppType.
  int type_;
};

#undef TYPE_CHECK

class PROTOBUF_EXPORT MapIterator {
 public:
  MapIterator(Message* message, const FieldDescriptor* field) {
    const Reflection* reflection = message->GetReflection();
    map_ = reflection->MutableMapData(message, field);
    key_.SetType(field->message_type()->FindFieldByName("key")->cpp_type());
    value_.SetType(field->message_type()->FindFieldByName("value")->cpp_type());
    map_->InitializeIterator(this);
  }
  MapIterator(const MapIterator& other) {
    map_ = other.map_;
    map_->InitializeIterator(this);
    map_->CopyIterator(this, other);
  }
  ~MapIterator() { map_->DeleteIterator(this); }
  MapIterator& operator=(const MapIterator& other) {
    map_ = other.map_;
    map_->CopyIterator(this, other);
    return *this;
  }
  friend bool operator==(const MapIterator& a, const MapIterator& b) {
    return a.map_->EqualIterator(a, b);
  }
  friend bool operator!=(const MapIterator& a, const MapIterator& b) {
    return !a.map_->EqualIterator(a, b);
  }
  MapIterator& operator++() {
    map_->IncreaseIterator(this);
    return *this;
  }
  MapIterator operator++(int) {
    // iter_ is copied from Map<...>::iterator, no need to
    // copy from its self again. Use the same implementation
    // with operator++()
    map_->IncreaseIterator(this);
    return *this;
  }
  const MapKey& GetKey() { return key_; }
  const MapValueRef& GetValueRef() { return value_; }
  MapValueRef* MutableValueRef() {
    map_->SetMapDirty();
    return &value_;
  }

 private:
  template <typename Key, typename T>
  friend class internal::TypeDefinedMapFieldBase;
  friend class internal::DynamicMapField;
  template <typename Derived, typename Key, typename T,
            internal::WireFormatLite::FieldType kKeyFieldType,
            internal::WireFormatLite::FieldType kValueFieldType,
            int default_enum_value>
  friend class internal::MapField;

  // reinterpret_cast from heap-allocated Map<...>::iterator*. MapIterator owns
  // the iterator. It is allocated by MapField<...>::InitializeIterator() called
  // in constructor and deleted by MapField<...>::DeleteIterator() called in
  // destructor.
  void* iter_;
  // Point to a MapField to call helper methods implemented in MapField.
  // MapIterator does not own this object.
  internal::MapFieldBase* map_;
  MapKey key_;
  MapValueRef value_;
};

}  // namespace protobuf
}  // namespace google

GOOGLE_PROTOBUF_HASH_NAMESPACE_DECLARATION_START
template <>
struct hash<::PROTOBUF_NAMESPACE_ID::MapKey> {
  size_t operator()(const ::PROTOBUF_NAMESPACE_ID::MapKey& map_key) const {
    switch (map_key.type()) {
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_DOUBLE:
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_FLOAT:
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_ENUM:
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_MESSAGE:
        GOOGLE_LOG(FATAL) << "Unsupported";
        break;
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_STRING:
        return hash<std::string>()(map_key.GetStringValue());
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_INT64:
        return hash<int64>()(map_key.GetInt64Value());
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_INT32:
        return hash<int32>()(map_key.GetInt32Value());
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_UINT64:
        return hash<uint64>()(map_key.GetUInt64Value());
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_UINT32:
        return hash<uint32>()(map_key.GetUInt32Value());
      case ::PROTOBUF_NAMESPACE_ID::FieldDescriptor::CPPTYPE_BOOL:
        return hash<bool>()(map_key.GetBoolValue());
    }
    GOOGLE_LOG(FATAL) << "Can't get here.";
    return 0;
  }
  bool operator()(const ::PROTOBUF_NAMESPACE_ID::MapKey& map_key1,
                  const ::PROTOBUF_NAMESPACE_ID::MapKey& map_key2) const {
    return map_key1 < map_key2;
  }
};
GOOGLE_PROTOBUF_HASH_NAMESPACE_DECLARATION_END

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_MAP_FIELD_H__
