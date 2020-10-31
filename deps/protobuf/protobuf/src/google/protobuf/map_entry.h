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

#ifndef GOOGLE_PROTOBUF_MAP_ENTRY_H__
#define GOOGLE_PROTOBUF_MAP_ENTRY_H__

#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/map_entry_lite.h>
#include <google/protobuf/map_type_handler.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/port.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/wire_format_lite.h>

#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
class Arena;
namespace internal {
template <typename Derived, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
class MapField;
}
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace internal {

// MapEntry is the returned google::protobuf::Message when calling AddMessage of
// google::protobuf::Reflection. In order to let it work with generated message
// reflection, its in-memory type is the same as generated message with the same
// fields. However, in order to decide the in-memory type of key/value, we need
// to know both their cpp type in generated api and proto type. In
// implementation, all in-memory types have related wire format functions to
// support except ArenaStringPtr. Therefore, we need to define another type with
// supporting wire format functions. Since this type is only used as return type
// of MapEntry accessors, it's named MapEntry accessor type.
//
// cpp type:               the type visible to users in public API.
// proto type:             WireFormatLite::FieldType of the field.
// in-memory type:         type of the data member used to stored this field.
// MapEntry accessor type: type used in MapEntry getters/mutators to access the
//                         field.
//
// cpp type | proto type  | in-memory type | MapEntry accessor type
// int32      TYPE_INT32    int32            int32
// int32      TYPE_FIXED32  int32            int32
// string     TYPE_STRING   ArenaStringPtr   string
// FooEnum    TYPE_ENUM     int              int
// FooMessage TYPE_MESSAGE  FooMessage*      FooMessage
//
// The in-memory types of primitive types can be inferred from its proto type,
// while we need to explicitly specify the cpp type if proto type is
// TYPE_MESSAGE to infer the in-memory type.  Moreover, default_enum_value is
// used to initialize enum field in proto2.
template <typename Derived, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
class MapEntry
    : public MapEntryImpl<Derived, Message, Key, Value, kKeyFieldType,
                          kValueFieldType, default_enum_value> {
 public:
  MapEntry() : _internal_metadata_(NULL) {}
  explicit MapEntry(Arena* arena)
      : MapEntryImpl<Derived, Message, Key, Value, kKeyFieldType,
                     kValueFieldType, default_enum_value>(arena),
        _internal_metadata_(arena) {}
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;

  typedef
      typename MapEntryImpl<Derived, Message, Key, Value, kKeyFieldType,
                            kValueFieldType, default_enum_value>::KeyTypeHandler
          KeyTypeHandler;
  typedef typename MapEntryImpl<
      Derived, Message, Key, Value, kKeyFieldType, kValueFieldType,
      default_enum_value>::ValueTypeHandler ValueTypeHandler;
  size_t SpaceUsedLong() const override {
    size_t size = sizeof(Derived);
    size += KeyTypeHandler::SpaceUsedInMapEntryLong(this->key_);
    size += ValueTypeHandler::SpaceUsedInMapEntryLong(this->value_);
    return size;
  }

  InternalMetadataWithArena _internal_metadata_;

 private:
  friend class ::PROTOBUF_NAMESPACE_ID::Arena;
  template <typename C, typename K, typename V,
            WireFormatLite::FieldType k_wire_type, WireFormatLite::FieldType,
            int default_enum>
  friend class internal::MapField;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MapEntry);
};

// Specialization for the full runtime
template <typename Derived, typename Key, typename Value,
          WireFormatLite::FieldType kKeyFieldType,
          WireFormatLite::FieldType kValueFieldType, int default_enum_value>
struct MapEntryHelper<MapEntry<Derived, Key, Value, kKeyFieldType,
                               kValueFieldType, default_enum_value> >
    : MapEntryHelper<MapEntryLite<Derived, Key, Value, kKeyFieldType,
                                  kValueFieldType, default_enum_value> > {
  explicit MapEntryHelper(const MapPair<Key, Value>& map_pair)
      : MapEntryHelper<MapEntryLite<Derived, Key, Value, kKeyFieldType,
                                    kValueFieldType, default_enum_value> >(
            map_pair) {}
};

template <typename Derived, typename K, typename V,
          WireFormatLite::FieldType key, WireFormatLite::FieldType value,
          int default_enum>
struct DeconstructMapEntry<MapEntry<Derived, K, V, key, value, default_enum> > {
  typedef K Key;
  typedef V Value;
  static const WireFormatLite::FieldType kKeyFieldType = key;
  static const WireFormatLite::FieldType kValueFieldType = value;
  static const int default_enum_value = default_enum;
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_MAP_ENTRY_H__
