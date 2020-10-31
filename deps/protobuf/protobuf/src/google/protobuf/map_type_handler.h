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

#ifndef GOOGLE_PROTOBUF_TYPE_HANDLER_H__
#define GOOGLE_PROTOBUF_TYPE_HANDLER_H__

#include <google/protobuf/parse_context.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/wire_format_lite.h>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
namespace internal {

// Used for compile time type selection. MapIf::type will be TrueType if Flag is
// true and FalseType otherwise.
template <bool Flag, typename TrueType, typename FalseType>
struct MapIf;

template <typename TrueType, typename FalseType>
struct MapIf<true, TrueType, FalseType> {
  typedef TrueType type;
};

template <typename TrueType, typename FalseType>
struct MapIf<false, TrueType, FalseType> {
  typedef FalseType type;
};

// In proto2 Map, enum needs to be initialized to given default value, while
// other types' default value can be inferred from the type.
template <bool IsEnum, typename Type>
class MapValueInitializer {
 public:
  static inline void Initialize(Type& type, int default_enum_value);
};

template <typename Type>
class MapValueInitializer<true, Type> {
 public:
  static inline void Initialize(Type& value, int default_enum_value) {
    value = static_cast<Type>(default_enum_value);
  }
};

template <typename Type>
class MapValueInitializer<false, Type> {
 public:
  static inline void Initialize(Type& /* value */,
                                int /* default_enum_value */) {}
};

template <typename Type, bool is_arena_constructable>
class MapArenaMessageCreator {
 public:
  // Use arena to create message if Type is arena constructable. Otherwise,
  // create the message on heap.
  static inline Type* CreateMessage(Arena* arena);
};
template <typename Type>
class MapArenaMessageCreator<Type, true> {
 public:
  static inline Type* CreateMessage(Arena* arena) {
    return Arena::CreateMessage<Type>(arena);
  }
};
template <typename Type>
class MapArenaMessageCreator<Type, false> {
 public:
  static inline Type* CreateMessage(Arena* arena) {
    return Arena::Create<Type>(arena);
  }
};

// Define constants for given wire field type
template <WireFormatLite::FieldType field_type, typename Type>
class MapWireFieldTypeTraits {};

#define TYPE_TRAITS(FieldType, CType, WireFormatType, IsMessage, IsEnum)   \
  template <typename Type>                                                 \
  class MapWireFieldTypeTraits<WireFormatLite::TYPE_##FieldType, Type> {   \
   public:                                                                 \
    static const bool kIsMessage = IsMessage;                              \
    static const bool kIsEnum = IsEnum;                                    \
    typedef typename MapIf<kIsMessage, Type*, CType>::type TypeOnMemory;   \
    typedef typename MapIf<kIsEnum, int, Type>::type MapEntryAccessorType; \
    static const WireFormatLite::WireType kWireType =                      \
        WireFormatLite::WIRETYPE_##WireFormatType;                         \
  };

TYPE_TRAITS(MESSAGE, Type, LENGTH_DELIMITED, true, false)
TYPE_TRAITS(STRING, ArenaStringPtr, LENGTH_DELIMITED, false, false)
TYPE_TRAITS(BYTES, ArenaStringPtr, LENGTH_DELIMITED, false, false)
TYPE_TRAITS(INT64, int64, VARINT, false, false)
TYPE_TRAITS(UINT64, uint64, VARINT, false, false)
TYPE_TRAITS(INT32, int32, VARINT, false, false)
TYPE_TRAITS(UINT32, uint32, VARINT, false, false)
TYPE_TRAITS(SINT64, int64, VARINT, false, false)
TYPE_TRAITS(SINT32, int32, VARINT, false, false)
TYPE_TRAITS(ENUM, int, VARINT, false, true)
TYPE_TRAITS(DOUBLE, double, FIXED64, false, false)
TYPE_TRAITS(FLOAT, float, FIXED32, false, false)
TYPE_TRAITS(FIXED64, uint64, FIXED64, false, false)
TYPE_TRAITS(FIXED32, uint32, FIXED32, false, false)
TYPE_TRAITS(SFIXED64, int64, FIXED64, false, false)
TYPE_TRAITS(SFIXED32, int32, FIXED32, false, false)
TYPE_TRAITS(BOOL, bool, VARINT, false, false)

#undef TYPE_TRAITS

template <WireFormatLite::FieldType field_type, typename Type>
class MapTypeHandler {};

template <typename Type>
class MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type> {
 public:
  // Enum type cannot be used for MapTypeHandler::Read. Define a type which will
  // replace Enum with int.
  typedef typename MapWireFieldTypeTraits<WireFormatLite::TYPE_MESSAGE,
                                          Type>::MapEntryAccessorType
      MapEntryAccessorType;
  // Internal stored type in MapEntryLite for given wire field type.
  typedef typename MapWireFieldTypeTraits<WireFormatLite::TYPE_MESSAGE,
                                          Type>::TypeOnMemory TypeOnMemory;
  // Corresponding wire type for field type.
  static const WireFormatLite::WireType kWireType =
      MapWireFieldTypeTraits<WireFormatLite::TYPE_MESSAGE, Type>::kWireType;
  // Whether wire type is for message.
  static const bool kIsMessage =
      MapWireFieldTypeTraits<WireFormatLite::TYPE_MESSAGE, Type>::kIsMessage;
  // Whether wire type is for enum.
  static const bool kIsEnum =
      MapWireFieldTypeTraits<WireFormatLite::TYPE_MESSAGE, Type>::kIsEnum;

  // Functions used in parsing and serialization. ===================
  static inline size_t ByteSize(const MapEntryAccessorType& value);
  static inline int GetCachedSize(const MapEntryAccessorType& value);
  static inline bool Read(io::CodedInputStream* input,
                          MapEntryAccessorType* value);
  static inline const char* Read(const char* ptr, ParseContext* ctx,
                                 MapEntryAccessorType* value);

  static inline void Write(int field, const MapEntryAccessorType& value,
                           io::CodedOutputStream* output);
  static inline uint8* WriteToArray(int field,
                                    const MapEntryAccessorType& value,
                                    uint8* target);

  // Functions to manipulate data on memory. ========================
  static inline const Type& GetExternalReference(const Type* value);
  static inline void DeleteNoArena(const Type* x);
  static inline void Merge(const Type& from, Type** to, Arena* arena);
  static inline void Clear(Type** value, Arena* arena);
  static inline void ClearMaybeByDefaultEnum(Type** value, Arena* arena,
                                             int default_enum_value);
  static inline void Initialize(Type** x, Arena* arena);

  static inline void InitializeMaybeByDefaultEnum(Type** x,
                                                  int default_enum_value,
                                                  Arena* arena);
  static inline Type* EnsureMutable(Type** value, Arena* arena);
  // SpaceUsedInMapEntry: Return bytes used by value in MapEntry, excluding
  // those already calculate in sizeof(MapField).
  static inline size_t SpaceUsedInMapEntryLong(const Type* value);
  // Return bytes used by value in Map.
  static inline size_t SpaceUsedInMapLong(const Type& value);
  // Assign default value to given instance.
  static inline void AssignDefaultValue(Type** value);
  // Return default instance if value is not initialized when calling const
  // reference accessor.
  static inline const Type& DefaultIfNotInitialized(const Type* value,
                                                    const Type* default_value);
  // Check if all required fields have values set.
  static inline bool IsInitialized(Type* value);
};

#define MAP_HANDLER(FieldType)                                                \
  template <typename Type>                                                    \
  class MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type> {              \
   public:                                                                    \
    typedef typename MapWireFieldTypeTraits<WireFormatLite::TYPE_##FieldType, \
                                            Type>::MapEntryAccessorType       \
        MapEntryAccessorType;                                                 \
    typedef typename MapWireFieldTypeTraits<WireFormatLite::TYPE_##FieldType, \
                                            Type>::TypeOnMemory TypeOnMemory; \
    static const WireFormatLite::WireType kWireType =                         \
        MapWireFieldTypeTraits<WireFormatLite::TYPE_##FieldType,              \
                               Type>::kWireType;                              \
    static const bool kIsMessage =                                            \
        MapWireFieldTypeTraits<WireFormatLite::TYPE_##FieldType,              \
                               Type>::kIsMessage;                             \
    static const bool kIsEnum =                                               \
        MapWireFieldTypeTraits<WireFormatLite::TYPE_##FieldType,              \
                               Type>::kIsEnum;                                \
    static inline int ByteSize(const MapEntryAccessorType& value);            \
    static inline int GetCachedSize(const MapEntryAccessorType& value);       \
    static inline bool Read(io::CodedInputStream* input,                      \
                            MapEntryAccessorType* value);                     \
    static inline const char* Read(const char* begin, ParseContext* ctx,      \
                                   MapEntryAccessorType* value);              \
    static inline void Write(int field, const MapEntryAccessorType& value,    \
                             io::CodedOutputStream* output);                  \
    static inline uint8* WriteToArray(int field,                              \
                                      const MapEntryAccessorType& value,      \
                                      uint8* target);                         \
    static inline const MapEntryAccessorType& GetExternalReference(           \
        const TypeOnMemory& value);                                           \
    static inline void DeleteNoArena(const TypeOnMemory& x);                  \
    static inline void Merge(const MapEntryAccessorType& from,                \
                             TypeOnMemory* to, Arena* arena);                 \
    static inline void Clear(TypeOnMemory* value, Arena* arena);              \
    static inline void ClearMaybeByDefaultEnum(TypeOnMemory* value,           \
                                               Arena* arena,                  \
                                               int default_enum);             \
    static inline size_t SpaceUsedInMapEntryLong(const TypeOnMemory& value);  \
    static inline size_t SpaceUsedInMapLong(const TypeOnMemory& value);       \
    static inline size_t SpaceUsedInMapLong(const std::string& value);        \
    static inline void AssignDefaultValue(TypeOnMemory* value);               \
    static inline const MapEntryAccessorType& DefaultIfNotInitialized(        \
        const TypeOnMemory& value, const TypeOnMemory& default_value);        \
    static inline bool IsInitialized(const TypeOnMemory& value);              \
    static void DeleteNoArena(TypeOnMemory& value);                           \
    static inline void Initialize(TypeOnMemory* value, Arena* arena);         \
    static inline void InitializeMaybeByDefaultEnum(TypeOnMemory* value,      \
                                                    int default_enum_value,   \
                                                    Arena* arena);            \
    static inline MapEntryAccessorType* EnsureMutable(TypeOnMemory* value,    \
                                                      Arena* arena);          \
  };
MAP_HANDLER(STRING)
MAP_HANDLER(BYTES)
MAP_HANDLER(INT64)
MAP_HANDLER(UINT64)
MAP_HANDLER(INT32)
MAP_HANDLER(UINT32)
MAP_HANDLER(SINT64)
MAP_HANDLER(SINT32)
MAP_HANDLER(ENUM)
MAP_HANDLER(DOUBLE)
MAP_HANDLER(FLOAT)
MAP_HANDLER(FIXED64)
MAP_HANDLER(FIXED32)
MAP_HANDLER(SFIXED64)
MAP_HANDLER(SFIXED32)
MAP_HANDLER(BOOL)
#undef MAP_HANDLER

template <typename Type>
inline size_t MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::ByteSize(
    const MapEntryAccessorType& value) {
  return WireFormatLite::MessageSizeNoVirtual(value);
}

#define GOOGLE_PROTOBUF_BYTE_SIZE(FieldType, DeclaredType)                     \
  template <typename Type>                                                     \
  inline int MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::ByteSize( \
      const MapEntryAccessorType& value) {                                     \
    return static_cast<int>(WireFormatLite::DeclaredType##Size(value));        \
  }

GOOGLE_PROTOBUF_BYTE_SIZE(STRING, String)
GOOGLE_PROTOBUF_BYTE_SIZE(BYTES, Bytes)
GOOGLE_PROTOBUF_BYTE_SIZE(INT64, Int64)
GOOGLE_PROTOBUF_BYTE_SIZE(UINT64, UInt64)
GOOGLE_PROTOBUF_BYTE_SIZE(INT32, Int32)
GOOGLE_PROTOBUF_BYTE_SIZE(UINT32, UInt32)
GOOGLE_PROTOBUF_BYTE_SIZE(SINT64, SInt64)
GOOGLE_PROTOBUF_BYTE_SIZE(SINT32, SInt32)
GOOGLE_PROTOBUF_BYTE_SIZE(ENUM, Enum)

#undef GOOGLE_PROTOBUF_BYTE_SIZE

#define FIXED_BYTE_SIZE(FieldType, DeclaredType)                               \
  template <typename Type>                                                     \
  inline int MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::ByteSize( \
      const MapEntryAccessorType& /* value */) {                               \
    return WireFormatLite::k##DeclaredType##Size;                              \
  }

FIXED_BYTE_SIZE(DOUBLE, Double)
FIXED_BYTE_SIZE(FLOAT, Float)
FIXED_BYTE_SIZE(FIXED64, Fixed64)
FIXED_BYTE_SIZE(FIXED32, Fixed32)
FIXED_BYTE_SIZE(SFIXED64, SFixed64)
FIXED_BYTE_SIZE(SFIXED32, SFixed32)
FIXED_BYTE_SIZE(BOOL, Bool)

#undef FIXED_BYTE_SIZE

template <typename Type>
inline int MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::GetCachedSize(
    const MapEntryAccessorType& value) {
  return static_cast<int>(WireFormatLite::LengthDelimitedSize(
      static_cast<size_t>(value.GetCachedSize())));
}

#define GET_CACHED_SIZE(FieldType, DeclaredType)                         \
  template <typename Type>                                               \
  inline int                                                             \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::GetCachedSize( \
      const MapEntryAccessorType& value) {                               \
    return static_cast<int>(WireFormatLite::DeclaredType##Size(value));  \
  }

GET_CACHED_SIZE(STRING, String)
GET_CACHED_SIZE(BYTES, Bytes)
GET_CACHED_SIZE(INT64, Int64)
GET_CACHED_SIZE(UINT64, UInt64)
GET_CACHED_SIZE(INT32, Int32)
GET_CACHED_SIZE(UINT32, UInt32)
GET_CACHED_SIZE(SINT64, SInt64)
GET_CACHED_SIZE(SINT32, SInt32)
GET_CACHED_SIZE(ENUM, Enum)

#undef GET_CACHED_SIZE

#define GET_FIXED_CACHED_SIZE(FieldType, DeclaredType)                   \
  template <typename Type>                                               \
  inline int                                                             \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::GetCachedSize( \
      const MapEntryAccessorType& /* value */) {                         \
    return WireFormatLite::k##DeclaredType##Size;                        \
  }

GET_FIXED_CACHED_SIZE(DOUBLE, Double)
GET_FIXED_CACHED_SIZE(FLOAT, Float)
GET_FIXED_CACHED_SIZE(FIXED64, Fixed64)
GET_FIXED_CACHED_SIZE(FIXED32, Fixed32)
GET_FIXED_CACHED_SIZE(SFIXED64, SFixed64)
GET_FIXED_CACHED_SIZE(SFIXED32, SFixed32)
GET_FIXED_CACHED_SIZE(BOOL, Bool)

#undef GET_FIXED_CACHED_SIZE

template <typename Type>
inline void MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::Write(
    int field, const MapEntryAccessorType& value,
    io::CodedOutputStream* output) {
  WireFormatLite::WriteMessageMaybeToArray(field, value, output);
}

template <typename Type>
inline uint8* MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::WriteToArray(
    int field, const MapEntryAccessorType& value, uint8* target) {
  return WireFormatLite::InternalWriteMessageToArray(field, value, target);
}

#define WRITE_METHOD(FieldType, DeclaredType)                                  \
  template <typename Type>                                                     \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Write(   \
      int field, const MapEntryAccessorType& value,                            \
      io::CodedOutputStream* output) {                                         \
    return WireFormatLite::Write##DeclaredType(field, value, output);          \
  }                                                                            \
  template <typename Type>                                                     \
  inline uint8*                                                                \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::WriteToArray(        \
      int field, const MapEntryAccessorType& value, uint8* target) {           \
    return WireFormatLite::Write##DeclaredType##ToArray(field, value, target); \
  }

WRITE_METHOD(STRING, String)
WRITE_METHOD(BYTES, Bytes)
WRITE_METHOD(INT64, Int64)
WRITE_METHOD(UINT64, UInt64)
WRITE_METHOD(INT32, Int32)
WRITE_METHOD(UINT32, UInt32)
WRITE_METHOD(SINT64, SInt64)
WRITE_METHOD(SINT32, SInt32)
WRITE_METHOD(ENUM, Enum)
WRITE_METHOD(DOUBLE, Double)
WRITE_METHOD(FLOAT, Float)
WRITE_METHOD(FIXED64, Fixed64)
WRITE_METHOD(FIXED32, Fixed32)
WRITE_METHOD(SFIXED64, SFixed64)
WRITE_METHOD(SFIXED32, SFixed32)
WRITE_METHOD(BOOL, Bool)

#undef WRITE_METHOD

template <typename Type>
inline bool MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::Read(
    io::CodedInputStream* input, MapEntryAccessorType* value) {
  return WireFormatLite::ReadMessageNoVirtual(input, value);
}

template <typename Type>
inline bool MapTypeHandler<WireFormatLite::TYPE_STRING, Type>::Read(
    io::CodedInputStream* input, MapEntryAccessorType* value) {
  return WireFormatLite::ReadString(input, value);
}

template <typename Type>
inline bool MapTypeHandler<WireFormatLite::TYPE_BYTES, Type>::Read(
    io::CodedInputStream* input, MapEntryAccessorType* value) {
  return WireFormatLite::ReadBytes(input, value);
}

template <typename Type>
const char* MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::Read(
    const char* ptr, ParseContext* ctx, MapEntryAccessorType* value) {
  return ctx->ParseMessage(value, ptr);
}

template <typename Type>
const char* MapTypeHandler<WireFormatLite::TYPE_STRING, Type>::Read(
    const char* ptr, ParseContext* ctx, MapEntryAccessorType* value) {
  int size = ReadSize(&ptr);
  GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
  return ctx->ReadString(ptr, size, value);
}

template <typename Type>
const char* MapTypeHandler<WireFormatLite::TYPE_BYTES, Type>::Read(
    const char* ptr, ParseContext* ctx, MapEntryAccessorType* value) {
  int size = ReadSize(&ptr);
  GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
  return ctx->ReadString(ptr, size, value);
}

inline const char* ReadINT64(const char* ptr, int64* value) {
  return ParseVarint64(ptr, reinterpret_cast<uint64*>(value));
}
inline const char* ReadUINT64(const char* ptr, uint64* value) {
  return ParseVarint64(ptr, value);
}
inline const char* ReadINT32(const char* ptr, int32* value) {
  uint64 tmp;
  auto res = ParseVarint64(ptr, &tmp);
  *value = static_cast<uint32>(tmp);
  return res;
}
inline const char* ReadUINT32(const char* ptr, uint32* value) {
  uint64 tmp;
  auto res = ParseVarint64(ptr, &tmp);
  *value = static_cast<uint32>(tmp);
  return res;
}
inline const char* ReadSINT64(const char* ptr, int64* value) {
  uint64 tmp;
  auto res = ParseVarint64(ptr, &tmp);
  *value = WireFormatLite::ZigZagDecode64(tmp);
  return res;
}
inline const char* ReadSINT32(const char* ptr, int32* value) {
  uint64 tmp;
  auto res = ParseVarint64(ptr, &tmp);
  *value = WireFormatLite::ZigZagDecode32(static_cast<uint32>(tmp));
  return res;
}
template <typename E>
inline const char* ReadENUM(const char* ptr, E* value) {
  uint64 tmp;
  auto res = ParseVarint64(ptr, &tmp);
  *value = static_cast<E>(tmp);
  return res;
}
inline const char* ReadBOOL(const char* ptr, bool* value) {
  uint64 tmp;
  auto res = ParseVarint64(ptr, &tmp);
  *value = static_cast<bool>(tmp);
  return res;
}

template <typename F>
inline const char* ReadUnaligned(const char* ptr, F* value) {
  *value = UnalignedLoad<F>(ptr);
  return ptr + sizeof(F);
}
inline const char* ReadFLOAT(const char* ptr, float* value) {
  return ReadUnaligned(ptr, value);
}
inline const char* ReadDOUBLE(const char* ptr, double* value) {
  return ReadUnaligned(ptr, value);
}
inline const char* ReadFIXED64(const char* ptr, uint64* value) {
  return ReadUnaligned(ptr, value);
}
inline const char* ReadFIXED32(const char* ptr, uint32* value) {
  return ReadUnaligned(ptr, value);
}
inline const char* ReadSFIXED64(const char* ptr, int64* value) {
  return ReadUnaligned(ptr, value);
}
inline const char* ReadSFIXED32(const char* ptr, int32* value) {
  return ReadUnaligned(ptr, value);
}

#define READ_METHOD(FieldType)                                              \
  template <typename Type>                                                  \
  inline bool MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Read( \
      io::CodedInputStream* input, MapEntryAccessorType* value) {           \
    return WireFormatLite::ReadPrimitive<TypeOnMemory,                      \
                                         WireFormatLite::TYPE_##FieldType>( \
        input, value);                                                      \
  }                                                                         \
  template <typename Type>                                                  \
  const char* MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Read( \
      const char* begin, ParseContext* ctx, MapEntryAccessorType* value) {  \
    return Read##FieldType(begin, value);                                   \
  }

READ_METHOD(INT64)
READ_METHOD(UINT64)
READ_METHOD(INT32)
READ_METHOD(UINT32)
READ_METHOD(SINT64)
READ_METHOD(SINT32)
READ_METHOD(ENUM)
READ_METHOD(DOUBLE)
READ_METHOD(FLOAT)
READ_METHOD(FIXED64)
READ_METHOD(FIXED32)
READ_METHOD(SFIXED64)
READ_METHOD(SFIXED32)
READ_METHOD(BOOL)

#undef READ_METHOD

// Definition for message handler

template <typename Type>
inline const Type&
MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::GetExternalReference(
    const Type* value) {
  return *value;
}

template <typename Type>
inline size_t MapTypeHandler<WireFormatLite::TYPE_MESSAGE,
                             Type>::SpaceUsedInMapEntryLong(const Type* value) {
  return value->SpaceUsedLong();
}

template <typename Type>
size_t MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::SpaceUsedInMapLong(
    const Type& value) {
  return value.SpaceUsedLong();
}

template <typename Type>
inline void MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::Clear(
    Type** value, Arena* /* arena */) {
  if (*value != NULL) (*value)->Clear();
}
template <typename Type>
inline void
MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::ClearMaybeByDefaultEnum(
    Type** value, Arena* /* arena */, int /* default_enum_value */) {
  if (*value != NULL) (*value)->Clear();
}
template <typename Type>
inline void MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::Merge(
    const Type& from, Type** to, Arena* /* arena */) {
  (*to)->MergeFrom(from);
}

template <typename Type>
void MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::DeleteNoArena(
    const Type* ptr) {
  delete ptr;
}

template <typename Type>
inline void MapTypeHandler<WireFormatLite::TYPE_MESSAGE,
                           Type>::AssignDefaultValue(Type** value) {
  *value = const_cast<Type*>(Type::internal_default_instance());
}

template <typename Type>
inline void MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::Initialize(
    Type** x, Arena* /* arena */) {
  *x = NULL;
}

template <typename Type>
inline void MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::
    InitializeMaybeByDefaultEnum(Type** x, int /* default_enum_value */,
                                 Arena* /* arena */) {
  *x = NULL;
}

template <typename Type>
inline Type* MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::EnsureMutable(
    Type** value, Arena* arena) {
  if (*value == NULL) {
    *value = MapArenaMessageCreator<
        Type,
        Arena::is_arena_constructable<Type>::type::value>::CreateMessage(arena);
  }
  return *value;
}

template <typename Type>
inline const Type&
MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::DefaultIfNotInitialized(
    const Type* value, const Type* default_value) {
  return value != NULL ? *value : *default_value;
}

template <typename Type>
inline bool MapTypeHandler<WireFormatLite::TYPE_MESSAGE, Type>::IsInitialized(
    Type* value) {
  return value->IsInitialized();
}

// Definition for string/bytes handler

#define STRING_OR_BYTES_HANDLER_FUNCTIONS(FieldType)                          \
  template <typename Type>                                                    \
  inline const typename MapTypeHandler<WireFormatLite::TYPE_##FieldType,      \
                                       Type>::MapEntryAccessorType&           \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType,                            \
                 Type>::GetExternalReference(const TypeOnMemory& value) {     \
    return value.Get();                                                       \
  }                                                                           \
  template <typename Type>                                                    \
  inline size_t                                                               \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType,                            \
                 Type>::SpaceUsedInMapEntryLong(const TypeOnMemory& value) {  \
    return sizeof(value);                                                     \
  }                                                                           \
  template <typename Type>                                                    \
  inline size_t                                                               \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::SpaceUsedInMapLong( \
      const TypeOnMemory& value) {                                            \
    return sizeof(value);                                                     \
  }                                                                           \
  template <typename Type>                                                    \
  inline size_t                                                               \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::SpaceUsedInMapLong( \
      const std::string& value) {                                             \
    return sizeof(value);                                                     \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Clear(  \
      TypeOnMemory* value, Arena* arena) {                                    \
    value->ClearToEmpty(&internal::GetEmptyStringAlreadyInited(), arena);     \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::        \
      ClearMaybeByDefaultEnum(TypeOnMemory* value, Arena* arena,              \
                              int /* default_enum */) {                       \
    Clear(value, arena);                                                      \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Merge(  \
      const MapEntryAccessorType& from, TypeOnMemory* to, Arena* arena) {     \
    to->Set(&internal::GetEmptyStringAlreadyInited(), from, arena);           \
  }                                                                           \
  template <typename Type>                                                    \
  void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::DeleteNoArena( \
      TypeOnMemory& value) {                                                  \
    value.DestroyNoArena(&internal::GetEmptyStringAlreadyInited());           \
  }                                                                           \
  template <typename Type>                                                    \
  inline void                                                                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::AssignDefaultValue( \
      TypeOnMemory* /* value */) {}                                           \
  template <typename Type>                                                    \
  inline void                                                                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Initialize(         \
      TypeOnMemory* value, Arena* /* arena */) {                              \
    value->UnsafeSetDefault(&internal::GetEmptyStringAlreadyInited());        \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::        \
      InitializeMaybeByDefaultEnum(                                           \
          TypeOnMemory* value, int /* default_enum_value */, Arena* arena) {  \
    Initialize(value, arena);                                                 \
  }                                                                           \
  template <typename Type>                                                    \
  inline typename MapTypeHandler<WireFormatLite::TYPE_##FieldType,            \
                                 Type>::MapEntryAccessorType*                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::EnsureMutable(      \
      TypeOnMemory* value, Arena* arena) {                                    \
    return value->Mutable(&internal::GetEmptyStringAlreadyInited(), arena);   \
  }                                                                           \
  template <typename Type>                                                    \
  inline const typename MapTypeHandler<WireFormatLite::TYPE_##FieldType,      \
                                       Type>::MapEntryAccessorType&           \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::                    \
      DefaultIfNotInitialized(const TypeOnMemory& value,                      \
                              const TypeOnMemory& /* default_value */) {      \
    return value.Get();                                                       \
  }                                                                           \
  template <typename Type>                                                    \
  inline bool                                                                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::IsInitialized(      \
      const TypeOnMemory& /* value */) {                                      \
    return true;                                                              \
  }
STRING_OR_BYTES_HANDLER_FUNCTIONS(STRING)
STRING_OR_BYTES_HANDLER_FUNCTIONS(BYTES)
#undef STRING_OR_BYTES_HANDLER_FUNCTIONS

#define PRIMITIVE_HANDLER_FUNCTIONS(FieldType)                                \
  template <typename Type>                                                    \
  inline const typename MapTypeHandler<WireFormatLite::TYPE_##FieldType,      \
                                       Type>::MapEntryAccessorType&           \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType,                            \
                 Type>::GetExternalReference(const TypeOnMemory& value) {     \
    return value;                                                             \
  }                                                                           \
  template <typename Type>                                                    \
  inline size_t MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::      \
      SpaceUsedInMapEntryLong(const TypeOnMemory& /* value */) {              \
    return 0;                                                                 \
  }                                                                           \
  template <typename Type>                                                    \
  inline size_t                                                               \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::SpaceUsedInMapLong( \
      const TypeOnMemory& /* value */) {                                      \
    return sizeof(Type);                                                      \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Clear(  \
      TypeOnMemory* value, Arena* /* arena */) {                              \
    *value = 0;                                                               \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::        \
      ClearMaybeByDefaultEnum(TypeOnMemory* value, Arena* /* arena */,        \
                              int default_enum_value) {                       \
    *value = static_cast<TypeOnMemory>(default_enum_value);                   \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Merge(  \
      const MapEntryAccessorType& from, TypeOnMemory* to,                     \
      Arena* /* arena */) {                                                   \
    *to = from;                                                               \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType,                \
                             Type>::DeleteNoArena(TypeOnMemory& /* x */) {}   \
  template <typename Type>                                                    \
  inline void                                                                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::AssignDefaultValue( \
      TypeOnMemory* /* value */) {}                                           \
  template <typename Type>                                                    \
  inline void                                                                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::Initialize(         \
      TypeOnMemory* value, Arena* /* arena */) {                              \
    *value = 0;                                                               \
  }                                                                           \
  template <typename Type>                                                    \
  inline void MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::        \
      InitializeMaybeByDefaultEnum(                                           \
          TypeOnMemory* value, int default_enum_value, Arena* /* arena */) {  \
    *value = static_cast<TypeOnMemory>(default_enum_value);                   \
  }                                                                           \
  template <typename Type>                                                    \
  inline typename MapTypeHandler<WireFormatLite::TYPE_##FieldType,            \
                                 Type>::MapEntryAccessorType*                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::EnsureMutable(      \
      TypeOnMemory* value, Arena* /* arena */) {                              \
    return value;                                                             \
  }                                                                           \
  template <typename Type>                                                    \
  inline const typename MapTypeHandler<WireFormatLite::TYPE_##FieldType,      \
                                       Type>::MapEntryAccessorType&           \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::                    \
      DefaultIfNotInitialized(const TypeOnMemory& value,                      \
                              const TypeOnMemory& /* default_value */) {      \
    return value;                                                             \
  }                                                                           \
  template <typename Type>                                                    \
  inline bool                                                                 \
  MapTypeHandler<WireFormatLite::TYPE_##FieldType, Type>::IsInitialized(      \
      const TypeOnMemory& /* value */) {                                      \
    return true;                                                              \
  }
PRIMITIVE_HANDLER_FUNCTIONS(INT64)
PRIMITIVE_HANDLER_FUNCTIONS(UINT64)
PRIMITIVE_HANDLER_FUNCTIONS(INT32)
PRIMITIVE_HANDLER_FUNCTIONS(UINT32)
PRIMITIVE_HANDLER_FUNCTIONS(SINT64)
PRIMITIVE_HANDLER_FUNCTIONS(SINT32)
PRIMITIVE_HANDLER_FUNCTIONS(ENUM)
PRIMITIVE_HANDLER_FUNCTIONS(DOUBLE)
PRIMITIVE_HANDLER_FUNCTIONS(FLOAT)
PRIMITIVE_HANDLER_FUNCTIONS(FIXED64)
PRIMITIVE_HANDLER_FUNCTIONS(FIXED32)
PRIMITIVE_HANDLER_FUNCTIONS(SFIXED64)
PRIMITIVE_HANDLER_FUNCTIONS(SFIXED32)
PRIMITIVE_HANDLER_FUNCTIONS(BOOL)
#undef PRIMITIVE_HANDLER_FUNCTIONS

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_TYPE_HANDLER_H__
