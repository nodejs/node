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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/generated_message_util.h>

#include <limits>

#ifndef GOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP
// We're only using this as a standard way for getting the thread id.
// We're not using any thread functionality.
#include <thread>  // NOLINT
#endif             // #ifndef GOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP

#include <vector>

#include <google/protobuf/io/coded_stream_inl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/generated_message_table_driven.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/stubs/mutex.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/wire_format_lite.h>

#include <google/protobuf/port_def.inc>


namespace google {
namespace protobuf {
namespace internal {

void DestroyMessage(const void* message) {
  static_cast<const MessageLite*>(message)->~MessageLite();
}
void DestroyString(const void* s) {
  static_cast<const std::string*>(s)->~string();
}

ExplicitlyConstructed<std::string> fixed_address_empty_string;


static bool InitProtobufDefaultsImpl() {
  fixed_address_empty_string.DefaultConstruct();
  OnShutdownDestroyString(fixed_address_empty_string.get_mutable());
  return true;
}

void InitProtobufDefaults() {
  static bool is_inited = InitProtobufDefaultsImpl();
  (void)is_inited;
}

size_t StringSpaceUsedExcludingSelfLong(const std::string& str) {
  const void* start = &str;
  const void* end = &str + 1;
  if (start <= str.data() && str.data() < end) {
    // The string's data is stored inside the string object itself.
    return 0;
  } else {
    return str.capacity();
  }
}

template <typename T>
const T& Get(const void* ptr) {
  return *static_cast<const T*>(ptr);
}

// PrimitiveTypeHelper is a wrapper around the interface of WireFormatLite.
// WireFormatLite has a very inconvenient interface with respect to template
// meta-programming. This class wraps the different named functions into
// a single Serialize / SerializeToArray interface.
template <int type>
struct PrimitiveTypeHelper;

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_BOOL> {
  typedef bool Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteBoolNoTag(Get<bool>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteBoolNoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_INT32> {
  typedef int32 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteInt32NoTag(Get<int32>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteInt32NoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_SINT32> {
  typedef int32 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteSInt32NoTag(Get<int32>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteSInt32NoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_UINT32> {
  typedef uint32 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteUInt32NoTag(Get<uint32>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteUInt32NoTagToArray(Get<Type>(ptr), buffer);
  }
};
template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_INT64> {
  typedef int64 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteInt64NoTag(Get<int64>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteInt64NoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_SINT64> {
  typedef int64 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteSInt64NoTag(Get<int64>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteSInt64NoTagToArray(Get<Type>(ptr), buffer);
  }
};
template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_UINT64> {
  typedef uint64 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteUInt64NoTag(Get<uint64>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteUInt64NoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_FIXED32> {
  typedef uint32 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteFixed32NoTag(Get<uint32>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteFixed32NoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_FIXED64> {
  typedef uint64 Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    WireFormatLite::WriteFixed64NoTag(Get<uint64>(ptr), output);
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    return WireFormatLite::WriteFixed64NoTagToArray(Get<Type>(ptr), buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_ENUM>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_INT32> {};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_SFIXED32>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_FIXED32> {
  typedef int32 Type;
};
template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_SFIXED64>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_FIXED64> {
  typedef int64 Type;
};
template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_FLOAT>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_FIXED32> {
  typedef float Type;
};
template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_DOUBLE>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_FIXED64> {
  typedef double Type;
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_STRING> {
  typedef std::string Type;
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    const Type& value = *static_cast<const Type*>(ptr);
    output->WriteVarint32(value.size());
    output->WriteRawMaybeAliased(value.data(), value.size());
  }
  static uint8* SerializeToArray(const void* ptr, uint8* buffer) {
    const Type& value = *static_cast<const Type*>(ptr);
    return io::CodedOutputStream::WriteStringWithSizeToArray(value, buffer);
  }
};

template <>
struct PrimitiveTypeHelper<WireFormatLite::TYPE_BYTES>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_STRING> {};


template <>
struct PrimitiveTypeHelper<FieldMetadata::kInlinedType>
    : PrimitiveTypeHelper<WireFormatLite::TYPE_STRING> {};

// We want to serialize to both CodedOutputStream and directly into byte arrays
// without duplicating the code. In fact we might want extra output channels in
// the future.
template <typename O, int type>
struct OutputHelper;

template <int type, typename O>
void SerializeTo(const void* ptr, O* output) {
  OutputHelper<O, type>::Serialize(ptr, output);
}

template <typename O>
void WriteTagTo(uint32 tag, O* output) {
  SerializeTo<WireFormatLite::TYPE_UINT32>(&tag, output);
}

template <typename O>
void WriteLengthTo(uint32 length, O* output) {
  SerializeTo<WireFormatLite::TYPE_UINT32>(&length, output);
}

// Specialization for coded output stream
template <int type>
struct OutputHelper<io::CodedOutputStream, type> {
  static void Serialize(const void* ptr, io::CodedOutputStream* output) {
    PrimitiveTypeHelper<type>::Serialize(ptr, output);
  }
};

// Specialization for writing into a plain array
struct ArrayOutput {
  uint8* ptr;
  bool is_deterministic;
};

template <int type>
struct OutputHelper<ArrayOutput, type> {
  static void Serialize(const void* ptr, ArrayOutput* output) {
    output->ptr = PrimitiveTypeHelper<type>::SerializeToArray(ptr, output->ptr);
  }
};

void SerializeMessageNoTable(const MessageLite* msg,
                             io::CodedOutputStream* output) {
  msg->SerializeWithCachedSizes(output);
}

void SerializeMessageNoTable(const MessageLite* msg, ArrayOutput* output) {
  if (output->is_deterministic) {
    io::ArrayOutputStream array_stream(output->ptr, INT_MAX);
    io::CodedOutputStream o(&array_stream);
    o.SetSerializationDeterministic(true);
    msg->SerializeWithCachedSizes(&o);
    output->ptr += o.ByteCount();
  } else {
    output->ptr = msg->InternalSerializeWithCachedSizesToArray(output->ptr);
  }
}

// Helper to branch to fast path if possible
void SerializeMessageDispatch(const MessageLite& msg,
                              const FieldMetadata* field_table, int num_fields,
                              int32 cached_size,
                              io::CodedOutputStream* output) {
  const uint8* base = reinterpret_cast<const uint8*>(&msg);
  if (!output->IsSerializationDeterministic()) {
    // Try the fast path
    uint8* ptr = output->GetDirectBufferForNBytesAndAdvance(cached_size);
    if (ptr) {
      // We use virtual dispatch to enable dedicated generated code for the
      // fast path.
      msg.InternalSerializeWithCachedSizesToArray(ptr);
      return;
    }
  }
  SerializeInternal(base, field_table, num_fields, output);
}

// Helper to branch to fast path if possible
void SerializeMessageDispatch(const MessageLite& msg,
                              const FieldMetadata* field_table, int num_fields,
                              int32 cached_size, ArrayOutput* output) {
  const uint8* base = reinterpret_cast<const uint8*>(&msg);
  output->ptr = SerializeInternalToArray(base, field_table, num_fields,
                                         output->is_deterministic, output->ptr);
}

// Serializing messages is special as it's not a primitive type and needs an
// explicit overload for each output type.
template <typename O>
void SerializeMessageTo(const MessageLite* msg, const void* table_ptr,
                        O* output) {
  const SerializationTable* table =
      static_cast<const SerializationTable*>(table_ptr);
  if (!table) {
    // Proto1
    WriteLengthTo(msg->GetCachedSize(), output);
    SerializeMessageNoTable(msg, output);
    return;
  }
  const FieldMetadata* field_table = table->field_table;
  const uint8* base = reinterpret_cast<const uint8*>(msg);
  int cached_size = *reinterpret_cast<const int32*>(base + field_table->offset);
  WriteLengthTo(cached_size, output);
  int num_fields = table->num_fields - 1;
  SerializeMessageDispatch(*msg, field_table + 1, num_fields, cached_size,
                           output);
}

// Almost the same as above only it doesn't output the length field.
template <typename O>
void SerializeGroupTo(const MessageLite* msg, const void* table_ptr,
                      O* output) {
  const SerializationTable* table =
      static_cast<const SerializationTable*>(table_ptr);
  if (!table) {
    // Proto1
    SerializeMessageNoTable(msg, output);
    return;
  }
  const FieldMetadata* field_table = table->field_table;
  const uint8* base = reinterpret_cast<const uint8*>(msg);
  int cached_size = *reinterpret_cast<const int32*>(base + field_table->offset);
  int num_fields = table->num_fields - 1;
  SerializeMessageDispatch(*msg, field_table + 1, num_fields, cached_size,
                           output);
}

template <int type>
struct SingularFieldHelper {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    WriteTagTo(md.tag, output);
    SerializeTo<type>(field, output);
  }
};

template <>
struct SingularFieldHelper<WireFormatLite::TYPE_STRING> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    WriteTagTo(md.tag, output);
    SerializeTo<WireFormatLite::TYPE_STRING>(&Get<ArenaStringPtr>(field).Get(),
                                             output);
  }
};

template <>
struct SingularFieldHelper<WireFormatLite::TYPE_BYTES>
    : SingularFieldHelper<WireFormatLite::TYPE_STRING> {};

template <>
struct SingularFieldHelper<WireFormatLite::TYPE_GROUP> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    WriteTagTo(md.tag, output);
    SerializeGroupTo(Get<const MessageLite*>(field),
                     static_cast<const SerializationTable*>(md.ptr), output);
    WriteTagTo(md.tag + 1, output);
  }
};

template <>
struct SingularFieldHelper<WireFormatLite::TYPE_MESSAGE> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    WriteTagTo(md.tag, output);
    SerializeMessageTo(Get<const MessageLite*>(field),
                       static_cast<const SerializationTable*>(md.ptr), output);
  }
};

template <>
struct SingularFieldHelper<FieldMetadata::kInlinedType> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    WriteTagTo(md.tag, output);
    SerializeTo<FieldMetadata::kInlinedType>(&Get<std::string>(field), output);
  }
};

template <int type>
struct RepeatedFieldHelper {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    typedef typename PrimitiveTypeHelper<type>::Type T;
    const RepeatedField<T>& array = Get<RepeatedField<T> >(field);
    for (int i = 0; i < array.size(); i++) {
      WriteTagTo(md.tag, output);
      SerializeTo<type>(&array[i], output);
    }
  }
};

// We need to use a helper class to get access to the private members
class AccessorHelper {
 public:
  static int Size(const RepeatedPtrFieldBase& x) { return x.size(); }
  static void const* Get(const RepeatedPtrFieldBase& x, int idx) {
    return x.raw_data()[idx];
  }
};

template <>
struct RepeatedFieldHelper<WireFormatLite::TYPE_STRING> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    const internal::RepeatedPtrFieldBase& array =
        Get<internal::RepeatedPtrFieldBase>(field);
    for (int i = 0; i < AccessorHelper::Size(array); i++) {
      WriteTagTo(md.tag, output);
      SerializeTo<WireFormatLite::TYPE_STRING>(AccessorHelper::Get(array, i),
                                               output);
    }
  }
};

template <>
struct RepeatedFieldHelper<WireFormatLite::TYPE_BYTES>
    : RepeatedFieldHelper<WireFormatLite::TYPE_STRING> {};

template <>
struct RepeatedFieldHelper<WireFormatLite::TYPE_GROUP> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    const internal::RepeatedPtrFieldBase& array =
        Get<internal::RepeatedPtrFieldBase>(field);
    for (int i = 0; i < AccessorHelper::Size(array); i++) {
      WriteTagTo(md.tag, output);
      SerializeGroupTo(
          static_cast<const MessageLite*>(AccessorHelper::Get(array, i)),
          static_cast<const SerializationTable*>(md.ptr), output);
      WriteTagTo(md.tag + 1, output);
    }
  }
};

template <>
struct RepeatedFieldHelper<WireFormatLite::TYPE_MESSAGE> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    const internal::RepeatedPtrFieldBase& array =
        Get<internal::RepeatedPtrFieldBase>(field);
    for (int i = 0; i < AccessorHelper::Size(array); i++) {
      WriteTagTo(md.tag, output);
      SerializeMessageTo(
          static_cast<const MessageLite*>(AccessorHelper::Get(array, i)),
          md.ptr, output);
    }
  }
};


template <>
struct RepeatedFieldHelper<FieldMetadata::kInlinedType>
    : RepeatedFieldHelper<WireFormatLite::TYPE_STRING> {};

template <int type>
struct PackedFieldHelper {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    typedef typename PrimitiveTypeHelper<type>::Type T;
    const RepeatedField<T>& array = Get<RepeatedField<T> >(field);
    if (array.empty()) return;
    WriteTagTo(md.tag, output);
    int cached_size =
        Get<int>(static_cast<const uint8*>(field) + sizeof(RepeatedField<T>));
    WriteLengthTo(cached_size, output);
    for (int i = 0; i < array.size(); i++) {
      SerializeTo<type>(&array[i], output);
    }
  }
};

template <>
struct PackedFieldHelper<WireFormatLite::TYPE_STRING> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    GOOGLE_LOG(FATAL) << "Not implemented field number " << md.tag << " with type "
               << md.type;
  }
};

template <>
struct PackedFieldHelper<WireFormatLite::TYPE_BYTES>
    : PackedFieldHelper<WireFormatLite::TYPE_STRING> {};
template <>
struct PackedFieldHelper<WireFormatLite::TYPE_GROUP>
    : PackedFieldHelper<WireFormatLite::TYPE_STRING> {};
template <>
struct PackedFieldHelper<WireFormatLite::TYPE_MESSAGE>
    : PackedFieldHelper<WireFormatLite::TYPE_STRING> {};
template <>
struct PackedFieldHelper<FieldMetadata::kInlinedType>
    : PackedFieldHelper<WireFormatLite::TYPE_STRING> {};

template <int type>
struct OneOfFieldHelper {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    SingularFieldHelper<type>::Serialize(field, md, output);
  }
};


template <>
struct OneOfFieldHelper<FieldMetadata::kInlinedType> {
  template <typename O>
  static void Serialize(const void* field, const FieldMetadata& md, O* output) {
    SingularFieldHelper<FieldMetadata::kInlinedType>::Serialize(
        Get<const std::string*>(field), md, output);
  }
};

void SerializeNotImplemented(int field) {
  GOOGLE_LOG(FATAL) << "Not implemented field number " << field;
}

// When switching to c++11 we should make these constexpr functions
#define SERIALIZE_TABLE_OP(type, type_class) \
  ((type - 1) + static_cast<int>(type_class) * FieldMetadata::kNumTypes)

int FieldMetadata::CalculateType(int type,
                                 FieldMetadata::FieldTypeClass type_class) {
  return SERIALIZE_TABLE_OP(type, type_class);
}

template <int type>
bool IsNull(const void* ptr) {
  return *static_cast<const typename PrimitiveTypeHelper<type>::Type*>(ptr) ==
         0;
}

template <>
bool IsNull<WireFormatLite::TYPE_STRING>(const void* ptr) {
  return static_cast<const ArenaStringPtr*>(ptr)->Get().size() == 0;
}

template <>
bool IsNull<WireFormatLite::TYPE_BYTES>(const void* ptr) {
  return static_cast<const ArenaStringPtr*>(ptr)->Get().size() == 0;
}

template <>
bool IsNull<WireFormatLite::TYPE_GROUP>(const void* ptr) {
  return Get<const MessageLite*>(ptr) == NULL;
}

template <>
bool IsNull<WireFormatLite::TYPE_MESSAGE>(const void* ptr) {
  return Get<const MessageLite*>(ptr) == NULL;
}


template <>
bool IsNull<FieldMetadata::kInlinedType>(const void* ptr) {
  return static_cast<const std::string*>(ptr)->empty();
}

#define SERIALIZERS_FOR_TYPE(type)                                            \
  case SERIALIZE_TABLE_OP(type, FieldMetadata::kPresence):                    \
    if (!IsPresent(base, field_metadata.has_offset)) continue;                \
    SingularFieldHelper<type>::Serialize(ptr, field_metadata, output);        \
    break;                                                                    \
  case SERIALIZE_TABLE_OP(type, FieldMetadata::kNoPresence):                  \
    if (IsNull<type>(ptr)) continue;                                          \
    SingularFieldHelper<type>::Serialize(ptr, field_metadata, output);        \
    break;                                                                    \
  case SERIALIZE_TABLE_OP(type, FieldMetadata::kRepeated):                    \
    RepeatedFieldHelper<type>::Serialize(ptr, field_metadata, output);        \
    break;                                                                    \
  case SERIALIZE_TABLE_OP(type, FieldMetadata::kPacked):                      \
    PackedFieldHelper<type>::Serialize(ptr, field_metadata, output);          \
    break;                                                                    \
  case SERIALIZE_TABLE_OP(type, FieldMetadata::kOneOf):                       \
    if (!IsOneofPresent(base, field_metadata.has_offset, field_metadata.tag)) \
      continue;                                                               \
    OneOfFieldHelper<type>::Serialize(ptr, field_metadata, output);           \
    break

void SerializeInternal(const uint8* base,
                       const FieldMetadata* field_metadata_table,
                       int32 num_fields, io::CodedOutputStream* output) {
  SpecialSerializer func = nullptr;
  for (int i = 0; i < num_fields; i++) {
    const FieldMetadata& field_metadata = field_metadata_table[i];
    const uint8* ptr = base + field_metadata.offset;
    switch (field_metadata.type) {
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_DOUBLE);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_FLOAT);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_INT64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_UINT64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_INT32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_FIXED64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_FIXED32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_BOOL);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_STRING);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_GROUP);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_MESSAGE);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_BYTES);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_UINT32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_ENUM);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SFIXED32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SFIXED64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SINT32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SINT64);
      SERIALIZERS_FOR_TYPE(FieldMetadata::kInlinedType);

      // Special cases
      case FieldMetadata::kSpecial:
        func = reinterpret_cast<SpecialSerializer>(
            const_cast<void*>(field_metadata.ptr));
        func(base, field_metadata.offset, field_metadata.tag,
             field_metadata.has_offset, output);
        break;
      default:
        // __builtin_unreachable()
        SerializeNotImplemented(field_metadata.type);
    }
  }
}

uint8* SerializeInternalToArray(const uint8* base,
                                const FieldMetadata* field_metadata_table,
                                int32 num_fields, bool is_deterministic,
                                uint8* buffer) {
  ArrayOutput array_output = {buffer, is_deterministic};
  ArrayOutput* output = &array_output;
  SpecialSerializer func = nullptr;
  for (int i = 0; i < num_fields; i++) {
    const FieldMetadata& field_metadata = field_metadata_table[i];
    const uint8* ptr = base + field_metadata.offset;
    switch (field_metadata.type) {
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_DOUBLE);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_FLOAT);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_INT64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_UINT64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_INT32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_FIXED64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_FIXED32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_BOOL);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_STRING);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_GROUP);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_MESSAGE);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_BYTES);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_UINT32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_ENUM);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SFIXED32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SFIXED64);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SINT32);
      SERIALIZERS_FOR_TYPE(WireFormatLite::TYPE_SINT64);
      SERIALIZERS_FOR_TYPE(FieldMetadata::kInlinedType);
      // Special cases
      case FieldMetadata::kSpecial: {
        io::ArrayOutputStream array_stream(array_output.ptr, INT_MAX);
        io::CodedOutputStream output(&array_stream);
        output.SetSerializationDeterministic(is_deterministic);
        func = reinterpret_cast<SpecialSerializer>(
            const_cast<void*>(field_metadata.ptr));
        func(base, field_metadata.offset, field_metadata.tag,
             field_metadata.has_offset, &output);
        array_output.ptr += output.ByteCount();
      } break;
      default:
        // __builtin_unreachable()
        SerializeNotImplemented(field_metadata.type);
    }
  }
  return array_output.ptr;
}
#undef SERIALIZERS_FOR_TYPE

void ExtensionSerializer(const uint8* ptr, uint32 offset, uint32 tag,
                         uint32 has_offset, io::CodedOutputStream* output) {
  reinterpret_cast<const ExtensionSet*>(ptr + offset)
      ->SerializeWithCachedSizes(tag, has_offset, output);
}

void UnknownFieldSerializerLite(const uint8* ptr, uint32 offset, uint32 tag,
                                uint32 has_offset,
                                io::CodedOutputStream* output) {
  output->WriteString(
      reinterpret_cast<const InternalMetadataWithArenaLite*>(ptr + offset)
          ->unknown_fields());
}

MessageLite* DuplicateIfNonNullInternal(MessageLite* message) {
  if (message) {
    MessageLite* ret = message->New();
    ret->CheckTypeAndMergeFrom(*message);
    return ret;
  } else {
    return NULL;
  }
}

void GenericSwap(MessageLite* m1, MessageLite* m2) {
  std::unique_ptr<MessageLite> tmp(m1->New());
  tmp->CheckTypeAndMergeFrom(*m1);
  m1->Clear();
  m1->CheckTypeAndMergeFrom(*m2);
  m2->Clear();
  m2->CheckTypeAndMergeFrom(*tmp);
}

// Returns a message owned by this Arena.  This may require Own()ing or
// duplicating the message.
MessageLite* GetOwnedMessageInternal(Arena* message_arena,
                                     MessageLite* submessage,
                                     Arena* submessage_arena) {
  GOOGLE_DCHECK(submessage->GetArena() == submessage_arena);
  GOOGLE_DCHECK(message_arena != submessage_arena);
  if (message_arena != NULL && submessage_arena == NULL) {
    message_arena->Own(submessage);
    return submessage;
  } else {
    MessageLite* ret = submessage->New(message_arena);
    ret->CheckTypeAndMergeFrom(*submessage);
    return ret;
  }
}

namespace {

void InitSCC_DFS(SCCInfoBase* scc) {
  if (scc->visit_status.load(std::memory_order_relaxed) !=
      SCCInfoBase::kUninitialized)
    return;
  scc->visit_status.store(SCCInfoBase::kRunning, std::memory_order_relaxed);
  // Each base is followed by an array of pointers to deps
  auto deps = reinterpret_cast<SCCInfoBase* const*>(scc + 1);
  for (int i = 0; i < scc->num_deps; i++) {
    if (deps[i]) InitSCC_DFS(deps[i]);
  }
  scc->init_func();
  // Mark done (note we use memory order release here), other threads could
  // now see this as initialized and thus the initialization must have happened
  // before.
  scc->visit_status.store(SCCInfoBase::kInitialized, std::memory_order_release);
}

}  // namespace

void InitSCCImpl(SCCInfoBase* scc) {
  static WrappedMutex mu{GOOGLE_PROTOBUF_LINKER_INITIALIZED};
  // Either the default in case no initialization is running or the id of the
  // thread that is currently initializing.
#ifndef GOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP
  static std::atomic<std::thread::id> runner;
  auto me = std::this_thread::get_id();
#else
  // This is a lightweight replacement for std::thread::id. std::thread does not
  // work on Windows XP SP2 with the latest VC++ libraries, because it utilizes
  // the Concurrency Runtime that is only supported on Windows XP SP3 and above.
  static std::atomic_llong runner(-1);
  auto me = ::GetCurrentThreadId();
#endif  // #ifndef GOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP

  // This will only happen because the constructor will call InitSCC while
  // constructing the default instance.
  if (runner.load(std::memory_order_relaxed) == me) {
    // Because we're in the process of constructing the default instance.
    // We can be assured that we're already exploring this SCC.
    GOOGLE_CHECK_EQ(scc->visit_status.load(std::memory_order_relaxed),
             SCCInfoBase::kRunning);
    return;
  }
  InitProtobufDefaults();
  mu.Lock();
  runner.store(me, std::memory_order_relaxed);
  InitSCC_DFS(scc);

#ifndef GOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP
  runner.store(std::thread::id{}, std::memory_order_relaxed);
#else
  runner.store(-1, std::memory_order_relaxed);
#endif  // #ifndef GOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP

  mu.Unlock();
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
