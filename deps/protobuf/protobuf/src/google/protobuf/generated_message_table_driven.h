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

#ifndef GOOGLE_PROTOBUF_GENERATED_MESSAGE_TABLE_DRIVEN_H__
#define GOOGLE_PROTOBUF_GENERATED_MESSAGE_TABLE_DRIVEN_H__

#include <google/protobuf/map.h>
#include <google/protobuf/map_entry_lite.h>
#include <google/protobuf/map_field_lite.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/wire_format_lite.h>

// We require C++11 and Clang to use constexpr for variables, as GCC 4.8
// requires constexpr to be consistent between declarations of variables
// unnecessarily (see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58541).
// VS 2017 Update 3 also supports this usage of constexpr.
#if defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1911)
#define PROTOBUF_CONSTEXPR_VAR constexpr
#else  // !__clang__
#define PROTOBUF_CONSTEXPR_VAR
#endif  // !_clang

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {

// Processing-type masks.
static constexpr const unsigned char kOneofMask = 0x40;
static constexpr const unsigned char kRepeatedMask = 0x20;
// Mask for the raw type: either a WireFormatLite::FieldType or one of the
// ProcessingTypes below, without the oneof or repeated flag.
static constexpr const unsigned char kTypeMask = 0x1f;

// Wire type masks.
static constexpr const unsigned char kNotPackedMask = 0x10;
static constexpr const unsigned char kInvalidMask = 0x20;

enum ProcessingTypes {
  TYPE_STRING_CORD = 19,
  TYPE_STRING_STRING_PIECE = 20,
  TYPE_BYTES_CORD = 21,
  TYPE_BYTES_STRING_PIECE = 22,
  TYPE_STRING_INLINED = 23,
  TYPE_BYTES_INLINED = 24,
  TYPE_MAP = 25,
};

static_assert(TYPE_MAP < kRepeatedMask, "Invalid enum");

struct PROTOBUF_EXPORT FieldMetadata {
  uint32 offset;  // offset of this field in the struct
  uint32 tag;     // field * 8 + wire_type
  // byte offset * 8 + bit_offset;
  // if the high bit is set then this is the byte offset of the oneof_case
  // for this field.
  uint32 has_offset;
  uint32 type;      // the type of this field.
  const void* ptr;  // auxiliary data

  // From the serializer point of view each fundamental type can occur in
  // 4 different ways. For simplicity we treat all combinations as a cartesion
  // product although not all combinations are allowed.
  enum FieldTypeClass {
    kPresence,
    kNoPresence,
    kRepeated,
    kPacked,
    kOneOf,
    kNumTypeClasses  // must be last enum
  };
  // C++ protobuf has 20 fundamental types, were we added Cord and StringPiece
  // and also distinquish the same types if they have different wire format.
  enum {
    kCordType = 19,
    kStringPieceType = 20,
    kInlinedType = 21,
    kNumTypes = 21,
    kSpecial = kNumTypes * kNumTypeClasses,
  };

  static int CalculateType(int fundamental_type, FieldTypeClass type_class);
};

// TODO(ckennelly):  Add a static assertion to ensure that these masks do not
// conflict with wiretypes.

// ParseTableField is kept small to help simplify instructions for computing
// offsets, as we will always need this information to parse a field.
// Additional data, needed for some types, is stored in
// AuxillaryParseTableField.
struct ParseTableField {
  uint32 offset;
  // The presence_index ordinarily represents a has_bit index, but for fields
  // inside a oneof it represents the index in _oneof_case_.
  uint32 presence_index;
  unsigned char normal_wiretype;
  unsigned char packed_wiretype;

  // processing_type is given by:
  //   (FieldDescriptor->type() << 1) | FieldDescriptor->is_packed()
  unsigned char processing_type;

  unsigned char tag_size;
};

struct ParseTable;

union AuxillaryParseTableField {
  typedef bool (*EnumValidator)(int);

  // Enums
  struct enum_aux {
    EnumValidator validator;
  };
  enum_aux enums;
  // Group, messages
  struct message_aux {
    // ExplicitlyInitialized<T> -> T requires a reinterpret_cast, which prevents
    // the tables from being constructed as a constexpr.  We use void to avoid
    // the cast.
    const void* default_message_void;
    const MessageLite* default_message() const {
      return static_cast<const MessageLite*>(default_message_void);
    }
  };
  message_aux messages;
  // Strings
  struct string_aux {
    const void* default_ptr;
    const char* field_name;
  };
  string_aux strings;

  struct map_aux {
    bool (*parse_map)(io::CodedInputStream*, void*);
  };
  map_aux maps;

  AuxillaryParseTableField() = default;
  constexpr AuxillaryParseTableField(AuxillaryParseTableField::enum_aux e)
      : enums(e) {}
  constexpr AuxillaryParseTableField(AuxillaryParseTableField::message_aux m)
      : messages(m) {}
  constexpr AuxillaryParseTableField(AuxillaryParseTableField::string_aux s)
      : strings(s) {}
  constexpr AuxillaryParseTableField(AuxillaryParseTableField::map_aux m)
      : maps(m) {}
};

struct ParseTable {
  const ParseTableField* fields;
  const AuxillaryParseTableField* aux;
  int max_field_number;
  // TODO(ckennelly): Do something with this padding.

  // TODO(ckennelly): Vet these for sign extension.
  int64 has_bits_offset;
  int64 oneof_case_offset;
  int64 extension_offset;
  int64 arena_offset;

  // ExplicitlyInitialized<T> -> T requires a reinterpret_cast, which prevents
  // the tables from being constructed as a constexpr.  We use void to avoid
  // the cast.
  const void* default_instance_void;
  const MessageLite* default_instance() const {
    return static_cast<const MessageLite*>(default_instance_void);
  }

  bool unknown_field_set;
};

static_assert(sizeof(ParseTableField) <= 16, "ParseTableField is too large");
// The tables must be composed of POD components to ensure link-time
// initialization.
static_assert(std::is_pod<ParseTableField>::value, "");
static_assert(std::is_pod<AuxillaryParseTableField>::value, "");
static_assert(std::is_pod<AuxillaryParseTableField::enum_aux>::value, "");
static_assert(std::is_pod<AuxillaryParseTableField::message_aux>::value, "");
static_assert(std::is_pod<AuxillaryParseTableField::string_aux>::value, "");
static_assert(std::is_pod<ParseTable>::value, "");

// TODO(ckennelly): Consolidate these implementations into a single one, using
// dynamic dispatch to the appropriate unknown field handler.
bool MergePartialFromCodedStream(MessageLite* msg, const ParseTable& table,
                                 io::CodedInputStream* input);
bool MergePartialFromCodedStreamLite(MessageLite* msg, const ParseTable& table,
                                     io::CodedInputStream* input);

template <typename Entry>
bool ParseMap(io::CodedInputStream* input, void* map_field) {
  typedef typename MapEntryToMapField<Entry>::MapFieldType MapFieldType;
  typedef Map<typename Entry::EntryKeyType, typename Entry::EntryValueType>
      MapType;
  typedef typename Entry::template Parser<MapFieldType, MapType> ParserType;

  ParserType parser(static_cast<MapFieldType*>(map_field));
  return WireFormatLite::ReadMessageNoVirtual(input, &parser);
}

struct SerializationTable {
  int num_fields;
  const FieldMetadata* field_table;
};

PROTOBUF_EXPORT void SerializeInternal(const uint8* base,
                                       const FieldMetadata* table,
                                       int32 num_fields,
                                       io::CodedOutputStream* output);

inline void TableSerialize(const MessageLite& msg,
                           const SerializationTable* table,
                           io::CodedOutputStream* output) {
  const FieldMetadata* field_table = table->field_table;
  int num_fields = table->num_fields - 1;
  const uint8* base = reinterpret_cast<const uint8*>(&msg);
  // TODO(gerbens) This skips the first test if we could use the fast
  // array serialization path, we should make this
  // int cached_size =
  //    *reinterpret_cast<const int32*>(base + field_table->offset);
  // SerializeWithCachedSize(msg, field_table + 1, num_fields, cached_size, ...)
  // But we keep conformance with the old way for now.
  SerializeInternal(base, field_table + 1, num_fields, output);
}

uint8* SerializeInternalToArray(const uint8* base, const FieldMetadata* table,
                                int32 num_fields, bool is_deterministic,
                                uint8* buffer);

inline uint8* TableSerializeToArray(const MessageLite& msg,
                                    const SerializationTable* table,
                                    bool is_deterministic, uint8* buffer) {
  const uint8* base = reinterpret_cast<const uint8*>(&msg);
  const FieldMetadata* field_table = table->field_table + 1;
  int num_fields = table->num_fields - 1;
  return SerializeInternalToArray(base, field_table, num_fields,
                                  is_deterministic, buffer);
}

template <typename T>
struct CompareHelper {
  bool operator()(const T& a, const T& b) const { return a < b; }
};

template <>
struct CompareHelper<ArenaStringPtr> {
  bool operator()(const ArenaStringPtr& a, const ArenaStringPtr& b) const {
    return a.Get() < b.Get();
  }
};

struct CompareMapKey {
  template <typename T>
  bool operator()(const MapEntryHelper<T>& a,
                  const MapEntryHelper<T>& b) const {
    return Compare(a.key_, b.key_);
  }
  template <typename T>
  bool Compare(const T& a, const T& b) const {
    return CompareHelper<T>()(a, b);
  }
};

template <typename MapFieldType, const SerializationTable* table>
void MapFieldSerializer(const uint8* base, uint32 offset, uint32 tag,
                        uint32 has_offset, io::CodedOutputStream* output) {
  typedef MapEntryHelper<typename MapFieldType::EntryTypeTrait> Entry;
  typedef typename MapFieldType::MapType::const_iterator Iter;

  const MapFieldType& map_field =
      *reinterpret_cast<const MapFieldType*>(base + offset);
  const SerializationTable* t =
      table +
      has_offset;  // has_offset is overloaded for maps to mean table offset
  if (!output->IsSerializationDeterministic()) {
    for (Iter it = map_field.GetMap().begin(); it != map_field.GetMap().end();
         ++it) {
      Entry map_entry(*it);
      output->WriteVarint32(tag);
      output->WriteVarint32(map_entry._cached_size_);
      SerializeInternal(reinterpret_cast<const uint8*>(&map_entry),
                        t->field_table, t->num_fields, output);
    }
  } else {
    std::vector<Entry> v;
    for (Iter it = map_field.GetMap().begin(); it != map_field.GetMap().end();
         ++it) {
      v.push_back(Entry(*it));
    }
    std::sort(v.begin(), v.end(), CompareMapKey());
    for (int i = 0; i < v.size(); i++) {
      output->WriteVarint32(tag);
      output->WriteVarint32(v[i]._cached_size_);
      SerializeInternal(reinterpret_cast<const uint8*>(&v[i]), t->field_table,
                        t->num_fields, output);
    }
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_GENERATED_MESSAGE_TABLE_DRIVEN_H__
