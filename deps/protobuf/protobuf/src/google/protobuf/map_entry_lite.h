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

#ifndef GOOGLE_PROTOBUF_MAP_ENTRY_LITE_H__
#define GOOGLE_PROTOBUF_MAP_ENTRY_LITE_H__

#include <assert.h>
#include <string>

#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/parse_context.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/map.h>
#include <google/protobuf/map_type_handler.h>
#include <google/protobuf/port.h>
#include <google/protobuf/wire_format_lite.h>

#include <google/protobuf/port_def.inc>
#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
namespace internal {
template <typename Derived, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
class MapEntry;
template <typename Derived, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
class MapFieldLite;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace internal {

// MoveHelper::Move is used to set *dest.  It copies *src, or moves it (in
// the C++11 sense), or swaps it. *src is left in a sane state for
// subsequent destruction, but shouldn't be used for anything.
template <bool is_enum, bool is_message, bool is_stringlike, typename T>
struct MoveHelper {  // primitives
  static void Move(T* src, T* dest) { *dest = *src; }
};

template <bool is_message, bool is_stringlike, typename T>
struct MoveHelper<true, is_message, is_stringlike, T> {  // enums
  static void Move(T* src, T* dest) { *dest = *src; }
  // T is an enum here, so allow conversions to and from int.
  static void Move(T* src, int* dest) { *dest = static_cast<int>(*src); }
  static void Move(int* src, T* dest) { *dest = static_cast<T>(*src); }
};

template <bool is_stringlike, typename T>
struct MoveHelper<false, true, is_stringlike, T> {  // messages
  static void Move(T* src, T* dest) { dest->Swap(src); }
};

template <typename T>
struct MoveHelper<false, false, true, T> {  // strings and similar
  static void Move(T* src, T* dest) {
#if __cplusplus >= 201103L
    *dest = std::move(*src);
#else
    dest->swap(*src);
#endif
  }
};

// Functions for operating on a map entry.  Does not contain any representation
// (this class is not intended to be instantiated).
template <typename Key, typename Value, WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType>
struct MapEntryFuncs {
  typedef MapTypeHandler<kKeyFieldType, Key> KeyTypeHandler;
  typedef MapTypeHandler<kValueFieldType, Value> ValueTypeHandler;
  static const int kKeyFieldNumber = 1;
  static const int kValueFieldNumber = 2;

  static void SerializeToCodedStream(int field_number, const Key& key,
                                     const Value& value,
                                     io::CodedOutputStream* output) {
    WireFormatLite::WriteTag(field_number,
                             WireFormatLite::WIRETYPE_LENGTH_DELIMITED, output);
    output->WriteVarint32(GetCachedSize(key, value));
    KeyTypeHandler::Write(kKeyFieldNumber, key, output);
    ValueTypeHandler::Write(kValueFieldNumber, value, output);
  }

  static ::google::protobuf::uint8* SerializeToArray(int field_number, const Key& key,
                                   const Value& value, ::google::protobuf::uint8* output) {
    output = WireFormatLite::WriteTagToArray(
        field_number, WireFormatLite::WIRETYPE_LENGTH_DELIMITED, output);
    output = io::CodedOutputStream::WriteVarint32ToArray(
        static_cast<uint32>(GetCachedSize(key, value)), output);
    output = KeyTypeHandler::WriteToArray(kKeyFieldNumber, key, output);
    output = ValueTypeHandler::WriteToArray(kValueFieldNumber, value, output);
    return output;
  }

  static size_t ByteSizeLong(const Key& key, const Value& value) {
    // Tags for key and value will both be one byte (field numbers 1 and 2).
    size_t inner_length =
        2 + KeyTypeHandler::ByteSize(key) + ValueTypeHandler::ByteSize(value);
    return inner_length + io::CodedOutputStream::VarintSize32(inner_length);
  }

  static int GetCachedSize(const Key& key, const Value& value) {
    // Tags for key and value will both be one byte (field numbers 1 and 2).
    return 2 + KeyTypeHandler::GetCachedSize(key) +
           ValueTypeHandler::GetCachedSize(value);
  }
};

// MapEntryImpl is used to implement parsing and serialization of map entries.
// It uses Curious Recursive Template Pattern (CRTP) to provide the type of
// the eventual code to the template code.
template <typename Derived, typename Base, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
class MapEntryImpl : public Base {
 public:
  typedef MapEntryFuncs<Key, Value, kKeyFieldType, kValueFieldType> Funcs;

 protected:
  // Provide utilities to parse/serialize key/value.  Provide utilities to
  // manipulate internal stored type.
  typedef MapTypeHandler<kKeyFieldType, Key> KeyTypeHandler;
  typedef MapTypeHandler<kValueFieldType, Value> ValueTypeHandler;

  // Define internal memory layout. Strings and messages are stored as
  // pointers, while other types are stored as values.
  typedef typename KeyTypeHandler::TypeOnMemory KeyOnMemory;
  typedef typename ValueTypeHandler::TypeOnMemory ValueOnMemory;

  // Enum type cannot be used for MapTypeHandler::Read. Define a type
  // which will replace Enum with int.
  typedef typename KeyTypeHandler::MapEntryAccessorType KeyMapEntryAccessorType;
  typedef
      typename ValueTypeHandler::MapEntryAccessorType ValueMapEntryAccessorType;

  // Constants for field number.
  static const int kKeyFieldNumber = 1;
  static const int kValueFieldNumber = 2;

  // Constants for field tag.
  static const uint8 kKeyTag =
      GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(kKeyFieldNumber, KeyTypeHandler::kWireType);
  static const uint8 kValueTag = GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(
      kValueFieldNumber, ValueTypeHandler::kWireType);
  static const size_t kTagSize = 1;

 public:
  // Work-around for a compiler bug (see repeated_field.h).
  typedef void MapEntryHasMergeTypeTrait;
  typedef Derived EntryType;
  typedef Key EntryKeyType;
  typedef Value EntryValueType;
  static const WireFormatLite::FieldType kEntryKeyFieldType = kKeyFieldType;
  static const WireFormatLite::FieldType kEntryValueFieldType = kValueFieldType;
  static const int kEntryDefaultEnumValue = default_enum_value;

  MapEntryImpl() : arena_(NULL) {
    KeyTypeHandler::Initialize(&key_, NULL);
    ValueTypeHandler::InitializeMaybeByDefaultEnum(&value_, default_enum_value,
                                                   NULL);
    _has_bits_[0] = 0;
  }

  explicit MapEntryImpl(Arena* arena) : arena_(arena) {
    KeyTypeHandler::Initialize(&key_, arena);
    ValueTypeHandler::InitializeMaybeByDefaultEnum(&value_, default_enum_value,
                                                   arena);
    _has_bits_[0] = 0;
  }

  ~MapEntryImpl() {
    if (GetArenaNoVirtual() != NULL) return;
    KeyTypeHandler::DeleteNoArena(key_);
    ValueTypeHandler::DeleteNoArena(value_);
  }

  // accessors ======================================================

  virtual inline const KeyMapEntryAccessorType& key() const {
    return KeyTypeHandler::GetExternalReference(key_);
  }
  virtual inline const ValueMapEntryAccessorType& value() const {
    return ValueTypeHandler::DefaultIfNotInitialized(
        value_, Derived::internal_default_instance()->value_);
  }
  inline KeyMapEntryAccessorType* mutable_key() {
    set_has_key();
    return KeyTypeHandler::EnsureMutable(&key_, GetArenaNoVirtual());
  }
  inline ValueMapEntryAccessorType* mutable_value() {
    set_has_value();
    return ValueTypeHandler::EnsureMutable(&value_, GetArenaNoVirtual());
  }

  // implements MessageLite =========================================

  // MapEntryImpl is for implementation only and this function isn't called
  // anywhere. Just provide a fake implementation here for MessageLite.
  std::string GetTypeName() const override { return ""; }

  void CheckTypeAndMergeFrom(const MessageLite& other) override {
    MergeFromInternal(*::google::protobuf::internal::DownCast<const Derived*>(&other));
  }

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
  const char* _InternalParse(const char* ptr, ParseContext* ctx) final {
    while (!ctx->Done(&ptr)) {
      uint32 tag;
      ptr = ReadTag(ptr, &tag);
      GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
      if (tag == kKeyTag) {
        set_has_key();
        KeyMapEntryAccessorType* key = mutable_key();
        ptr = KeyTypeHandler::Read(ptr, ctx, key);
        if (!Derived::ValidateKey(key)) return nullptr;
      } else if (tag == kValueTag) {
        set_has_value();
        ValueMapEntryAccessorType* value = mutable_value();
        ptr = ValueTypeHandler::Read(ptr, ctx, value);
        if (!Derived::ValidateValue(value)) return nullptr;
      } else {
        if (tag == 0 || WireFormatLite::GetTagWireType(tag) ==
                            WireFormatLite::WIRETYPE_END_GROUP) {
          ctx->SetLastTag(tag);
          return ptr;
        }
        ptr = UnknownFieldParse(tag, static_cast<string*>(nullptr), ptr, ctx);
      }
      GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
    }
    return ptr;
  }
#else
  bool MergePartialFromCodedStream(io::CodedInputStream* input) override {
    uint32 tag;

    for (;;) {
      // 1) corrupted data: return false;
      // 2) unknown field: skip without putting into unknown field set;
      // 3) unknown enum value: keep it in parsing. In proto2, caller should
      // check the value and put this entry into containing message's unknown
      // field set if the value is an unknown enum. In proto3, caller doesn't
      // need to care whether the value is unknown enum;
      // 4) missing key/value: missed key/value will have default value. caller
      // should take this entry as if key/value is set to default value.
      tag = input->ReadTagNoLastTag();
      switch (tag) {
        case kKeyTag:
          if (!KeyTypeHandler::Read(input, mutable_key())) {
            return false;
          }
          set_has_key();
          break;

        case kValueTag:
          if (!ValueTypeHandler::Read(input, mutable_value())) {
            return false;
          }
          set_has_value();
          if (input->ExpectAtEnd()) return true;
          break;

        default:
          if (tag == 0 || WireFormatLite::GetTagWireType(tag) ==
                              WireFormatLite::WIRETYPE_END_GROUP) {
            return true;
          }
          if (!WireFormatLite::SkipField(input, tag)) return false;
          break;
      }
    }
  }
#endif

  size_t ByteSizeLong() const override {
    size_t size = 0;
    size += has_key() ? kTagSize +
                            static_cast<size_t>(KeyTypeHandler::ByteSize(key()))
                      : 0;
    size += has_value()
                ? kTagSize +
                      static_cast<size_t>(ValueTypeHandler::ByteSize(value()))
                : 0;
    return size;
  }

  void SerializeWithCachedSizes(io::CodedOutputStream* output) const override {
    KeyTypeHandler::Write(kKeyFieldNumber, key(), output);
    ValueTypeHandler::Write(kValueFieldNumber, value(), output);
  }

  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      ::google::protobuf::uint8* output) const override {
    output = KeyTypeHandler::WriteToArray(kKeyFieldNumber, key(), output);
    output = ValueTypeHandler::WriteToArray(kValueFieldNumber, value(), output);
    return output;
  }

  // Don't override SerializeWithCachedSizesToArray.  Use MessageLite's.

  int GetCachedSize() const override {
    int size = 0;
    size += has_key() ? static_cast<int>(kTagSize) +
                            KeyTypeHandler::GetCachedSize(key())
                      : 0;
    size += has_value() ? static_cast<int>(kTagSize) +
                              ValueTypeHandler::GetCachedSize(value())
                        : 0;
    return size;
  }

  bool IsInitialized() const override {
    return ValueTypeHandler::IsInitialized(value_);
  }

  Base* New() const override {
    Derived* entry = new Derived;
    return entry;
  }

  Base* New(Arena* arena) const override {
    Derived* entry = Arena::CreateMessage<Derived>(arena);
    return entry;
  }

 protected:
  // We can't declare this function directly here as it would hide the other
  // overload (const Message&).
  void MergeFromInternal(const MapEntryImpl& from) {
    if (from._has_bits_[0]) {
      if (from.has_key()) {
        KeyTypeHandler::EnsureMutable(&key_, GetArenaNoVirtual());
        KeyTypeHandler::Merge(from.key(), &key_, GetArenaNoVirtual());
        set_has_key();
      }
      if (from.has_value()) {
        ValueTypeHandler::EnsureMutable(&value_, GetArenaNoVirtual());
        ValueTypeHandler::Merge(from.value(), &value_, GetArenaNoVirtual());
        set_has_value();
      }
    }
  }

 public:
  void Clear() override {
    KeyTypeHandler::Clear(&key_, GetArenaNoVirtual());
    ValueTypeHandler::ClearMaybeByDefaultEnum(&value_, GetArenaNoVirtual(),
                                              default_enum_value);
    clear_has_key();
    clear_has_value();
  }

  static void InitAsDefaultInstance() {
    Derived* d = const_cast<Derived*>(Derived::internal_default_instance());
    KeyTypeHandler::AssignDefaultValue(&d->key_);
    ValueTypeHandler::AssignDefaultValue(&d->value_);
  }

  Arena* GetArena() const override { return GetArenaNoVirtual(); }

  // Parsing using MergePartialFromCodedStream, above, is not as
  // efficient as it could be.  This helper class provides a speedier way.
  template <typename MapField, typename Map>
  class Parser {
   public:
    explicit Parser(MapField* mf) : mf_(mf), map_(mf->MutableMap()) {}
    ~Parser() {
      if (entry_ != nullptr && entry_->GetArena() == nullptr) delete entry_;
    }

    // This does what the typical MergePartialFromCodedStream() is expected to
    // do, with the additional side-effect that if successful (i.e., if true is
    // going to be its return value) it inserts the key-value pair into map_.
    bool MergePartialFromCodedStream(io::CodedInputStream* input) {
      // Look for the expected thing: a key and then a value.  If it fails,
      // invoke the enclosing class's MergePartialFromCodedStream, or return
      // false if that would be pointless.
      if (input->ExpectTag(kKeyTag)) {
        if (!KeyTypeHandler::Read(input, &key_)) {
          return false;
        }
        // Peek at the next byte to see if it is kValueTag.  If not, bail out.
        const void* data;
        int size;
        input->GetDirectBufferPointerInline(&data, &size);
        // We could use memcmp here, but we don't bother. The tag is one byte.
        static_assert(kTagSize == 1, "tag size must be 1");
        if (size > 0 && *reinterpret_cast<const char*>(data) == kValueTag) {
          typename Map::size_type map_size = map_->size();
          value_ptr_ = &(*map_)[key_];
          if (PROTOBUF_PREDICT_TRUE(map_size != map_->size())) {
            // We created a new key-value pair.  Fill in the value.
            typedef
                typename MapIf<ValueTypeHandler::kIsEnum, int*, Value*>::type T;
            input->Skip(kTagSize);  // Skip kValueTag.
            if (!ValueTypeHandler::Read(input,
                                        reinterpret_cast<T>(value_ptr_))) {
              map_->erase(key_);  // Failure! Undo insertion.
              return false;
            }
            if (input->ExpectAtEnd()) return true;
            return ReadBeyondKeyValuePair(input);
          }
        }
      } else {
        key_ = Key();
      }

      NewEntry();
      *entry_->mutable_key() = key_;
      const bool result = entry_->MergePartialFromCodedStream(input);
      if (result) UseKeyAndValueFromEntry();
      return result;
    }

    const char* _InternalParse(const char* ptr, ParseContext* ctx) {
      if (PROTOBUF_PREDICT_TRUE(!ctx->Done(&ptr) && *ptr == kKeyTag)) {
        ptr = KeyTypeHandler::Read(ptr + 1, ctx, &key_);
        if (PROTOBUF_PREDICT_FALSE(!ptr || !Derived::ValidateKey(&key_))) {
          return nullptr;
        }
        if (PROTOBUF_PREDICT_TRUE(!ctx->Done(&ptr) && *ptr == kValueTag)) {
          typename Map::size_type map_size = map_->size();
          value_ptr_ = &(*map_)[key_];
          if (PROTOBUF_PREDICT_TRUE(map_size != map_->size())) {
            using T =
                typename MapIf<ValueTypeHandler::kIsEnum, int*, Value*>::type;
            ptr = ValueTypeHandler::Read(ptr + 1, ctx,
                                         reinterpret_cast<T>(value_ptr_));
            if (PROTOBUF_PREDICT_FALSE(!ptr ||
                                       !Derived::ValidateValue(value_ptr_))) {
              map_->erase(key_);  // Failure! Undo insertion.
              return nullptr;
            }
            if (PROTOBUF_PREDICT_TRUE(ctx->Done(&ptr))) return ptr;
            if (!ptr) return nullptr;
            NewEntry();
            ValueMover::Move(value_ptr_, entry_->mutable_value());
            map_->erase(key_);
            goto move_key;
          }
        } else {
          if (!ptr) return nullptr;
        }
        NewEntry();
      move_key:
        KeyMover::Move(&key_, entry_->mutable_key());
      } else {
        if (!ptr) return nullptr;
        NewEntry();
      }
      ptr = entry_->_InternalParse(ptr, ctx);
      if (ptr) UseKeyAndValueFromEntry();
      return ptr;
    }

    template <typename Metadata>
    const char* ParseWithEnumValidation(const char* ptr, ParseContext* ctx,
                                        bool (*is_valid)(int), uint32 field_num,
                                        Metadata* metadata) {
      auto entry = NewEntry();
      ptr = entry->_InternalParse(ptr, ctx);
      if (!ptr) return nullptr;
      if (is_valid(entry->value())) {
        UseKeyAndValueFromEntry();
      } else {
        WriteLengthDelimited(field_num, entry->SerializeAsString(),
                             metadata->mutable_unknown_fields());
      }
      return ptr;
    }

    MapEntryImpl* NewEntry() { return entry_ = mf_->NewEntry(); }

    const Key& key() const { return key_; }
    const Value& value() const { return *value_ptr_; }

    const Key& entry_key() const { return entry_->key(); }
    const Value& entry_value() const { return entry_->value(); }

   private:
    void UseKeyAndValueFromEntry() {
      // Update key_ in case we need it later (because key() is called).
      // This is potentially inefficient, especially if the key is
      // expensive to copy (e.g., a long string), but this is a cold
      // path, so it's not a big deal.
      key_ = entry_->key();
      value_ptr_ = &(*map_)[key_];
      ValueMover::Move(entry_->mutable_value(), value_ptr_);
    }

    // After reading a key and value successfully, and inserting that data
    // into map_, we are not at the end of the input.  This is unusual, but
    // allowed by the spec.
    bool ReadBeyondKeyValuePair(io::CodedInputStream* input) PROTOBUF_COLD {
      NewEntry();
      ValueMover::Move(value_ptr_, entry_->mutable_value());
      map_->erase(key_);
      KeyMover::Move(&key_, entry_->mutable_key());
      const bool result = entry_->MergePartialFromCodedStream(input);
      if (result) UseKeyAndValueFromEntry();
      return result;
    }

    typedef MoveHelper<KeyTypeHandler::kIsEnum, KeyTypeHandler::kIsMessage,
                       KeyTypeHandler::kWireType ==
                           WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                       Key>
        KeyMover;
    typedef MoveHelper<ValueTypeHandler::kIsEnum, ValueTypeHandler::kIsMessage,
                       ValueTypeHandler::kWireType ==
                           WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                       Value>
        ValueMover;

    MapField* const mf_;
    Map* const map_;
    Key key_;
    Value* value_ptr_;
    MapEntryImpl* entry_ = nullptr;
  };

 protected:
  void set_has_key() { _has_bits_[0] |= 0x00000001u; }
  bool has_key() const { return (_has_bits_[0] & 0x00000001u) != 0; }
  void clear_has_key() { _has_bits_[0] &= ~0x00000001u; }
  void set_has_value() { _has_bits_[0] |= 0x00000002u; }
  bool has_value() const { return (_has_bits_[0] & 0x00000002u) != 0; }
  void clear_has_value() { _has_bits_[0] &= ~0x00000002u; }

 public:
  inline Arena* GetArenaNoVirtual() const { return arena_; }

 public:  // Needed for constructing tables
  KeyOnMemory key_;
  ValueOnMemory value_;
  Arena* arena_;
  uint32 _has_bits_[1];

 private:
  friend class ::PROTOBUF_NAMESPACE_ID::Arena;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  template <typename C, typename K, typename V, WireFormatLite::FieldType,
            WireFormatLite::FieldType, int>
  friend class internal::MapEntry;
  template <typename C, typename K, typename V, WireFormatLite::FieldType,
            WireFormatLite::FieldType, int>
  friend class internal::MapFieldLite;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MapEntryImpl);
};

template <typename T, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
class MapEntryLite
    : public MapEntryImpl<T, MessageLite, Key, Value, kKeyFieldType,
                          kValueFieldType, default_enum_value> {
 public:
  typedef MapEntryImpl<T, MessageLite, Key, Value, kKeyFieldType,
                       kValueFieldType, default_enum_value>
      SuperType;
  MapEntryLite() {}
  explicit MapEntryLite(Arena* arena) : SuperType(arena) {}
  void MergeFrom(const MapEntryLite& other) { MergeFromInternal(other); }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MapEntryLite);
};
// The completely unprincipled and unwieldy use of template parameters in
// the map code necessitates wrappers to make the code a little bit more
// manageable.
template <typename Derived>
struct DeconstructMapEntry;

template <typename T, typename K, typename V, WireFormatLite::FieldType key,
          WireFormatLite::FieldType value, int default_enum>
struct DeconstructMapEntry<MapEntryLite<T, K, V, key, value, default_enum> > {
  typedef K Key;
  typedef V Value;
  static const WireFormatLite::FieldType kKeyFieldType = key;
  static const WireFormatLite::FieldType kValueFieldType = value;
  static const int default_enum_value = default_enum;
};

// Helpers for deterministic serialization =============================

// This struct can be used with any generic sorting algorithm.  If the Key
// type is relatively small and easy to copy then copying Keys into an
// array of SortItems can be beneficial.  Then all the data the sorting
// algorithm needs to touch is in that one array.
template <typename Key, typename PtrToKeyValuePair>
struct SortItem {
  SortItem() {}
  explicit SortItem(PtrToKeyValuePair p) : first(p->first), second(p) {}

  Key first;
  PtrToKeyValuePair second;
};

template <typename T>
struct CompareByFirstField {
  bool operator()(const T& a, const T& b) const { return a.first < b.first; }
};

template <typename T>
struct CompareByDerefFirst {
  bool operator()(const T& a, const T& b) const { return a->first < b->first; }
};

// Helper for table driven serialization

template <WireFormatLite::FieldType FieldType>
struct FromHelper {
  template <typename T>
  static const T& From(const T& x) {
    return x;
  }
};

template <>
struct FromHelper<WireFormatLite::TYPE_STRING> {
  static ArenaStringPtr From(const std::string& x) {
    ArenaStringPtr res;
    TaggedPtr<std::string> ptr;
    ptr.Set(const_cast<std::string*>(&x));
    res.UnsafeSetTaggedPointer(ptr);
    return res;
  }
};
template <>
struct FromHelper<WireFormatLite::TYPE_BYTES> {
  static ArenaStringPtr From(const std::string& x) {
    ArenaStringPtr res;
    TaggedPtr<std::string> ptr;
    ptr.Set(const_cast<std::string*>(&x));
    res.UnsafeSetTaggedPointer(ptr);
    return res;
  }
};
template <>
struct FromHelper<WireFormatLite::TYPE_MESSAGE> {
  template <typename T>
  static T* From(const T& x) {
    return const_cast<T*>(&x);
  }
};

template <typename MapEntryType>
struct MapEntryHelper;

template <typename T, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
struct MapEntryHelper<MapEntryLite<T, Key, Value, kKeyFieldType,
                                   kValueFieldType, default_enum_value> > {
  // Provide utilities to parse/serialize key/value.  Provide utilities to
  // manipulate internal stored type.
  typedef MapTypeHandler<kKeyFieldType, Key> KeyTypeHandler;
  typedef MapTypeHandler<kValueFieldType, Value> ValueTypeHandler;

  // Define internal memory layout. Strings and messages are stored as
  // pointers, while other types are stored as values.
  typedef typename KeyTypeHandler::TypeOnMemory KeyOnMemory;
  typedef typename ValueTypeHandler::TypeOnMemory ValueOnMemory;

  explicit MapEntryHelper(const MapPair<Key, Value>& map_pair)
      : _has_bits_(3),
        _cached_size_(2 + KeyTypeHandler::GetCachedSize(map_pair.first) +
                      ValueTypeHandler::GetCachedSize(map_pair.second)),
        key_(FromHelper<kKeyFieldType>::From(map_pair.first)),
        value_(FromHelper<kValueFieldType>::From(map_pair.second)) {}

  // Purposely not folowing the style guide naming. These are the names
  // the proto compiler would generate given the map entry descriptor.
  // The proto compiler generates the offsets in this struct as if this was
  // a regular message. This way the table driven code barely notices it's
  // dealing with a map field.
  uint32 _has_bits_;     // NOLINT
  uint32 _cached_size_;  // NOLINT
  KeyOnMemory key_;      // NOLINT
  ValueOnMemory value_;  // NOLINT
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_MAP_ENTRY_LITE_H__
