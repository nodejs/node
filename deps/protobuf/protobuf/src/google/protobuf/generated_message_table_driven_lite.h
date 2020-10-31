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

#ifndef GOOGLE_PROTOBUF_GENERATED_MESSAGE_TABLE_DRIVEN_LITE_H__
#define GOOGLE_PROTOBUF_GENERATED_MESSAGE_TABLE_DRIVEN_LITE_H__

#include <google/protobuf/generated_message_table_driven.h>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/implicit_weak_message.h>
#include <google/protobuf/inlined_string_field.h>
#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/wire_format_lite.h>
#include <type_traits>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {


enum StringType {
  StringType_STRING = 0,
  StringType_INLINED = 3
};

// Logically a superset of StringType, consisting of all field types that
// require special initialization.
enum ProcessingType {
  ProcessingType_STRING = 0,
  ProcessingType_CORD = 1,
  ProcessingType_STRING_PIECE = 2,
  ProcessingType_INLINED = 3,
  ProcessingType_MESSAGE = 4,
};

enum Cardinality {
  Cardinality_SINGULAR = 0,
  Cardinality_REPEATED = 1,
  Cardinality_ONEOF = 3
};

template <typename Type>
inline Type* Raw(MessageLite* msg, int64 offset) {
  return reinterpret_cast<Type*>(reinterpret_cast<uint8*>(msg) + offset);
}

template <typename Type>
inline const Type* Raw(const MessageLite* msg, int64 offset) {
  return reinterpret_cast<const Type*>(reinterpret_cast<const uint8*>(msg) +
                                       offset);
}

template <typename InternalMetadata>
inline Arena* GetArena(MessageLite* msg, int64 arena_offset) {
  if (PROTOBUF_PREDICT_FALSE(arena_offset == -1)) {
    return NULL;
  }

  return Raw<InternalMetadata>(msg, arena_offset)->arena();
}

inline ExtensionSet* GetExtensionSet(MessageLite* msg, int64 extension_offset) {
  if (extension_offset == -1) {
    return NULL;
  }

  return Raw<ExtensionSet>(msg, extension_offset);
}

template <typename Type>
inline Type* AddField(MessageLite* msg, int64 offset) {
  static_assert(std::is_pod<Type>::value ||
                    std::is_same<Type, InlinedStringField>::value,
                "Do not assign");

  RepeatedField<Type>* repeated = Raw<RepeatedField<Type>>(msg, offset);
  return repeated->Add();
}

template <>
inline std::string* AddField<std::string>(MessageLite* msg, int64 offset) {
  RepeatedPtrField<std::string>* repeated =
      Raw<RepeatedPtrField<std::string>>(msg, offset);
  return repeated->Add();
}


template <typename Type>
inline void AddField(MessageLite* msg, int64 offset, Type value) {
  static_assert(std::is_pod<Type>::value,
                "Do not assign");
  *AddField<Type>(msg, offset) = value;
}

inline void SetBit(uint32* has_bits, uint32 has_bit_index) {
  GOOGLE_DCHECK(has_bits != nullptr);

  uint32 mask = static_cast<uint32>(1u) << (has_bit_index % 32);
  has_bits[has_bit_index / 32u] |= mask;
}

template <typename Type>
inline Type* MutableField(MessageLite* msg, uint32* has_bits,
                          uint32 has_bit_index, int64 offset) {
  SetBit(has_bits, has_bit_index);
  return Raw<Type>(msg, offset);
}

template <typename Type>
inline void SetField(MessageLite* msg, uint32* has_bits, uint32 has_bit_index,
                     int64 offset, Type value) {
  static_assert(std::is_pod<Type>::value,
                "Do not assign");
  *MutableField<Type>(msg, has_bits, has_bit_index, offset) = value;
}

template <typename Type>
inline void SetOneofField(MessageLite* msg, uint32* oneof_case,
                          uint32 oneof_case_index, int64 offset,
                          int field_number, Type value) {
  oneof_case[oneof_case_index] = field_number;
  *Raw<Type>(msg, offset) = value;
}

// Clears a oneof field. The field argument should correspond to the particular
// field that is currently set in the oneof.
inline void ClearOneofField(const ParseTableField& field, Arena* arena,
                            MessageLite* msg) {
  switch (field.processing_type & kTypeMask) {
    case WireFormatLite::TYPE_MESSAGE:
      if (arena == NULL) {
        delete *Raw<MessageLite*>(msg, field.offset);
      }
      break;

    case WireFormatLite::TYPE_STRING:
    case WireFormatLite::TYPE_BYTES:
      Raw<ArenaStringPtr>(msg, field.offset)
          ->Destroy(&GetEmptyStringAlreadyInited(), arena);
      break;

    case TYPE_STRING_INLINED:
    case TYPE_BYTES_INLINED:
      Raw<InlinedStringField>(msg, field.offset)->DestroyNoArena(NULL);
      break;

    default:
      // No cleanup needed.
      break;
  }
}

// Clears and reinitializes a oneof field as necessary, in preparation for
// parsing a new value with type field_type and field number field_number.
//
// Note: the oneof_case argument should point directly to the _oneof_case_
// element corresponding to this particular oneof, not to the beginning of the
// _oneof_case_ array.
template <ProcessingType field_type>
inline void ResetOneofField(const ParseTable& table, int field_number,
                            Arena* arena, MessageLite* msg, uint32* oneof_case,
                            int64 offset, const void* default_ptr) {
  if (*oneof_case == field_number) {
    // The oneof is already set to the right type, so there is no need to clear
    // it.
    return;
  }

  if (*oneof_case != 0) {
    ClearOneofField(table.fields[*oneof_case], arena, msg);
  }
  *oneof_case = field_number;

  switch (field_type) {
    case ProcessingType_STRING:
      Raw<ArenaStringPtr>(msg, offset)
          ->UnsafeSetDefault(static_cast<const std::string*>(default_ptr));
      break;
    case ProcessingType_INLINED:
      new (Raw<InlinedStringField>(msg, offset))
          InlinedStringField(*static_cast<const std::string*>(default_ptr));
      break;
    case ProcessingType_MESSAGE:
      MessageLite** submessage = Raw<MessageLite*>(msg, offset);
      const MessageLite* prototype =
          table.aux[field_number].messages.default_message();
      *submessage = prototype->New(arena);
      break;
  }
}

template <typename UnknownFieldHandler, Cardinality cardinality,
          bool is_string_type, StringType ctype>
static inline bool HandleString(io::CodedInputStream* input, MessageLite* msg,
                                Arena* arena, uint32* has_bits,
                                uint32 has_bit_index, int64 offset,
                                const void* default_ptr,
                                const char* field_name) {
  StringPiece utf8_string_data;
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  constexpr bool kValidateUtf8 = is_string_type;
#else
  constexpr bool kValidateUtf8 = false;
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED

  switch (ctype) {
    case StringType_INLINED: {
      InlinedStringField* s = nullptr;
      switch (cardinality) {
        case Cardinality_SINGULAR:
          // TODO(ckennelly): Is this optimal?
          s = MutableField<InlinedStringField>(msg, has_bits, has_bit_index,
                                               offset);
          break;
        case Cardinality_REPEATED:
          s = AddField<InlinedStringField>(msg, offset);
          break;
        case Cardinality_ONEOF:
          s = Raw<InlinedStringField>(msg, offset);
          break;
      }
      GOOGLE_DCHECK(s != nullptr);
      std::string* value = s->MutableNoArena(NULL);
      if (PROTOBUF_PREDICT_FALSE(!WireFormatLite::ReadString(input, value))) {
        return false;
      }
      utf8_string_data = *value;
      break;
    }
    case StringType_STRING: {
      switch (cardinality) {
        case Cardinality_SINGULAR: {
          ArenaStringPtr* field = MutableField<ArenaStringPtr>(
              msg, has_bits, has_bit_index, offset);
          std::string* value = field->Mutable(
              static_cast<const std::string*>(default_ptr), arena);
          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadString(input, value))) {
            return false;
          }
          utf8_string_data = field->Get();
        } break;
        case Cardinality_REPEATED: {
          std::string* value = AddField<std::string>(msg, offset);
          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadString(input, value))) {
            return false;
          }
          utf8_string_data = *value;
        } break;
        case Cardinality_ONEOF: {
          ArenaStringPtr* field = Raw<ArenaStringPtr>(msg, offset);
          std::string* value = field->Mutable(
              static_cast<const std::string*>(default_ptr), arena);
          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadString(input, value))) {
            return false;
          }
          utf8_string_data = field->Get();
        } break;
      }
      break;
    }
  }

  if (kValidateUtf8) {
    // TODO(b/118759213): fail if proto3
    WireFormatLite::VerifyUtf8String(utf8_string_data.data(),
                                     utf8_string_data.length(),
                                     WireFormatLite::PARSE, field_name);
  }
  return true;
}

template <typename UnknownFieldHandler, typename InternalMetadata,
          Cardinality cardinality>
inline bool HandleEnum(const ParseTable& table, io::CodedInputStream* input,
                       MessageLite* msg, uint32* presence,
                       uint32 presence_index, int64 offset, uint32 tag,
                       int field_number) {
  int value;
  if (PROTOBUF_PREDICT_FALSE(
          (!WireFormatLite::ReadPrimitive<int, WireFormatLite::TYPE_ENUM>(
              input, &value)))) {
    return false;
  }

  AuxillaryParseTableField::EnumValidator validator =
      table.aux[field_number].enums.validator;
  if (validator == nullptr || validator(value)) {
    switch (cardinality) {
      case Cardinality_SINGULAR:
        SetField(msg, presence, presence_index, offset, value);
        break;
      case Cardinality_REPEATED:
        AddField(msg, offset, value);
        break;
      case Cardinality_ONEOF:
        ClearOneofField(table.fields[presence[presence_index]],
                        GetArena<InternalMetadata>(msg, table.arena_offset),
                        msg);
        SetOneofField(msg, presence, presence_index, offset, field_number,
                      value);
        break;
    }
  } else {
    UnknownFieldHandler::Varint(msg, table, tag, value);
  }

  return true;
}

// RepeatedMessageTypeHandler allows us to operate on RepeatedPtrField fields
// without instantiating the specific template.
class RepeatedMessageTypeHandler {
 public:
  typedef MessageLite Type;
  typedef MessageLite WeakType;
  static Arena* GetArena(Type* t) { return t->GetArena(); }
  static void* GetMaybeArenaPointer(Type* t) {
    return t->GetMaybeArenaPointer();
  }
  static inline Type* NewFromPrototype(const Type* prototype,
                                       Arena* arena = NULL) {
    return prototype->New(arena);
  }
  static void Delete(Type* t, Arena* arena = NULL) {
    if (arena == NULL) {
      delete t;
    }
  }
};

class MergePartialFromCodedStreamHelper {
 public:
  static MessageLite* Add(RepeatedPtrFieldBase* field,
                          const MessageLite* prototype) {
    return field->Add<RepeatedMessageTypeHandler>(
        const_cast<MessageLite*>(prototype));
  }
};

template <typename UnknownFieldHandler, typename InternalMetadata,
          uint32 kMaxTag>
bool MergePartialFromCodedStreamInlined(MessageLite* msg,
                                        const ParseTable& table,
                                        io::CodedInputStream* input) {
  // We require that has_bits are present, as to avoid having to check for them
  // for every field.
  //
  // TODO(ckennelly):  Make this a compile-time parameter with templates.
  GOOGLE_DCHECK_GE(table.has_bits_offset, 0);
  uint32* has_bits = Raw<uint32>(msg, table.has_bits_offset);
  GOOGLE_DCHECK(has_bits != NULL);

  while (true) {
    uint32 tag = input->ReadTagWithCutoffNoLastTag(kMaxTag).first;
    const WireFormatLite::WireType wire_type =
        WireFormatLite::GetTagWireType(tag);
    const int field_number = WireFormatLite::GetTagFieldNumber(tag);

    if (PROTOBUF_PREDICT_FALSE(field_number > table.max_field_number)) {
      // check for possible extensions
      if (UnknownFieldHandler::ParseExtension(msg, table, input, tag)) {
        // successfully parsed
        continue;
      }

      if (PROTOBUF_PREDICT_FALSE(
              !UnknownFieldHandler::Skip(msg, table, input, tag))) {
        return false;
      }

      continue;
    }

    // We implicitly verify that data points to a valid field as we check the
    // wire types.  Entries in table.fields[i] that do not correspond to valid
    // field numbers have their normal_wiretype and packed_wiretype fields set
    // with the kInvalidMask value.  As wire_type cannot take on that value, we
    // will never match.
    const ParseTableField* data = table.fields + field_number;

    // TODO(ckennelly): Avoid sign extension
    const int64 presence_index = data->presence_index;
    const int64 offset = data->offset;
    const unsigned char processing_type = data->processing_type;

    if (data->normal_wiretype == static_cast<unsigned char>(wire_type)) {
      // TODO(ckennelly): Use a computed goto on GCC/LLVM or otherwise eliminate
      // the bounds check on processing_type.

      switch (processing_type) {
#define HANDLE_TYPE(TYPE, CPPTYPE)                                             \
  case (WireFormatLite::TYPE_##TYPE): {                                        \
    CPPTYPE value;                                                             \
    if (PROTOBUF_PREDICT_FALSE(                                                \
            (!WireFormatLite::ReadPrimitive<                                   \
                CPPTYPE, WireFormatLite::TYPE_##TYPE>(input, &value)))) {      \
      return false;                                                            \
    }                                                                          \
    SetField(msg, has_bits, presence_index, offset, value);                    \
    break;                                                                     \
  }                                                                            \
  case (WireFormatLite::TYPE_##TYPE) | kRepeatedMask: {                        \
    RepeatedField<CPPTYPE>* values = Raw<RepeatedField<CPPTYPE>>(msg, offset); \
    if (PROTOBUF_PREDICT_FALSE((!WireFormatLite::ReadRepeatedPrimitive<        \
                                CPPTYPE, WireFormatLite::TYPE_##TYPE>(         \
            data->tag_size, tag, input, values)))) {                           \
      return false;                                                            \
    }                                                                          \
    break;                                                                     \
  }                                                                            \
  case (WireFormatLite::TYPE_##TYPE) | kOneofMask: {                           \
    uint32* oneof_case = Raw<uint32>(msg, table.oneof_case_offset);            \
    CPPTYPE value;                                                             \
    if (PROTOBUF_PREDICT_FALSE(                                                \
            (!WireFormatLite::ReadPrimitive<                                   \
                CPPTYPE, WireFormatLite::TYPE_##TYPE>(input, &value)))) {      \
      return false;                                                            \
    }                                                                          \
    ClearOneofField(table.fields[oneof_case[presence_index]],                  \
                    GetArena<InternalMetadata>(msg, table.arena_offset), msg); \
    SetOneofField(msg, oneof_case, presence_index, offset, field_number,       \
                  value);                                                      \
    break;                                                                     \
  }

        HANDLE_TYPE(INT32, int32)
        HANDLE_TYPE(INT64, int64)
        HANDLE_TYPE(SINT32, int32)
        HANDLE_TYPE(SINT64, int64)
        HANDLE_TYPE(UINT32, uint32)
        HANDLE_TYPE(UINT64, uint64)

        HANDLE_TYPE(FIXED32, uint32)
        HANDLE_TYPE(FIXED64, uint64)
        HANDLE_TYPE(SFIXED32, int32)
        HANDLE_TYPE(SFIXED64, int64)

        HANDLE_TYPE(FLOAT, float)
        HANDLE_TYPE(DOUBLE, double)

        HANDLE_TYPE(BOOL, bool)
#undef HANDLE_TYPE
        case WireFormatLite::TYPE_BYTES:
#ifndef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case WireFormatLite::TYPE_STRING:
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_SINGULAR,
                                 false, StringType_STRING>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, nullptr)))) {
            return false;
          }
          break;
        }
        case TYPE_BYTES_INLINED:
#ifndef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case TYPE_STRING_INLINED:
#endif  // !GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_SINGULAR,
                                 false, StringType_INLINED>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, nullptr)))) {
            return false;
          }
          break;
        }
        case WireFormatLite::TYPE_BYTES | kOneofMask:
#ifndef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case WireFormatLite::TYPE_STRING | kOneofMask:
#endif  // !GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          uint32* oneof_case = Raw<uint32>(msg, table.oneof_case_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;

          ResetOneofField<ProcessingType_STRING>(
              table, field_number, arena, msg, oneof_case + presence_index,
              offset, default_ptr);

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_ONEOF, false,
                                 StringType_STRING>(input, msg, arena, has_bits,
                                                    presence_index, offset,
                                                    default_ptr, nullptr)))) {
            return false;
          }
          break;
        }
        case (WireFormatLite::TYPE_BYTES) | kRepeatedMask:
        case TYPE_BYTES_INLINED | kRepeatedMask:
#ifndef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case (WireFormatLite::TYPE_STRING) | kRepeatedMask:
        case TYPE_STRING_INLINED | kRepeatedMask:
#endif  // !GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_REPEATED,
                                 false, StringType_STRING>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, nullptr)))) {
            return false;
          }
          break;
        }
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case (WireFormatLite::TYPE_STRING): {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;
          const char* field_name = table.aux[field_number].strings.field_name;

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_SINGULAR,
                                 true, StringType_STRING>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, field_name)))) {
            return false;
          }
          break;
        }
        case TYPE_STRING_INLINED | kRepeatedMask:
        case (WireFormatLite::TYPE_STRING) | kRepeatedMask: {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;
          const char* field_name = table.aux[field_number].strings.field_name;

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_REPEATED,
                                 true, StringType_STRING>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, field_name)))) {
            return false;
          }
          break;
        }
        case (WireFormatLite::TYPE_STRING) | kOneofMask: {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          uint32* oneof_case = Raw<uint32>(msg, table.oneof_case_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;
          const char* field_name = table.aux[field_number].strings.field_name;

          ResetOneofField<ProcessingType_STRING>(
              table, field_number, arena, msg, oneof_case + presence_index,
              offset, default_ptr);

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_ONEOF, true,
                                 StringType_STRING>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, field_name)))) {
            return false;
          }
          break;
        }
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case WireFormatLite::TYPE_ENUM: {
          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleEnum<UnknownFieldHandler, InternalMetadata,
                               Cardinality_SINGULAR>(
                      table, input, msg, has_bits, presence_index, offset, tag,
                      field_number)))) {
            return false;
          }
          break;
        }
        case WireFormatLite::TYPE_ENUM | kRepeatedMask: {
          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleEnum<UnknownFieldHandler, InternalMetadata,
                               Cardinality_REPEATED>(
                      table, input, msg, has_bits, presence_index, offset, tag,
                      field_number)))) {
            return false;
          }
          break;
        }
        case WireFormatLite::TYPE_ENUM | kOneofMask: {
          uint32* oneof_case = Raw<uint32>(msg, table.oneof_case_offset);
          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleEnum<UnknownFieldHandler, InternalMetadata,
                               Cardinality_ONEOF>(table, input, msg, oneof_case,
                                                  presence_index, offset, tag,
                                                  field_number)))) {
            return false;
          }
          break;
        }
        case WireFormatLite::TYPE_GROUP: {
          MessageLite** submsg_holder =
              MutableField<MessageLite*>(msg, has_bits, presence_index, offset);
          MessageLite* submsg = *submsg_holder;

          if (submsg == NULL) {
            Arena* const arena =
                GetArena<InternalMetadata>(msg, table.arena_offset);
            const MessageLite* prototype =
                table.aux[field_number].messages.default_message();
            submsg = prototype->New(arena);
            *submsg_holder = submsg;
          }

          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadGroup(field_number, input, submsg))) {
            return false;
          }

          break;
        }
        case WireFormatLite::TYPE_GROUP | kRepeatedMask: {
          RepeatedPtrFieldBase* field = Raw<RepeatedPtrFieldBase>(msg, offset);
          const MessageLite* prototype =
              table.aux[field_number].messages.default_message();
          GOOGLE_DCHECK(prototype != NULL);

          MessageLite* submsg =
              MergePartialFromCodedStreamHelper::Add(field, prototype);

          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadGroup(field_number, input, submsg))) {
            return false;
          }

          break;
        }
        case WireFormatLite::TYPE_MESSAGE: {
          MessageLite** submsg_holder =
              MutableField<MessageLite*>(msg, has_bits, presence_index, offset);
          MessageLite* submsg = *submsg_holder;

          if (submsg == NULL) {
            Arena* const arena =
                GetArena<InternalMetadata>(msg, table.arena_offset);
            const MessageLite* prototype =
                table.aux[field_number].messages.default_message();
            if (prototype == NULL) {
              prototype = ImplicitWeakMessage::default_instance();
            }
            submsg = prototype->New(arena);
            *submsg_holder = submsg;
          }

          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadMessage(input, submsg))) {
            return false;
          }

          break;
        }
        // TODO(ckennelly):  Adapt ReadMessageNoVirtualNoRecursionDepth and
        // manage input->IncrementRecursionDepth() here.
        case WireFormatLite::TYPE_MESSAGE | kRepeatedMask: {
          RepeatedPtrFieldBase* field = Raw<RepeatedPtrFieldBase>(msg, offset);
          const MessageLite* prototype =
              table.aux[field_number].messages.default_message();
          if (prototype == NULL) {
            prototype = ImplicitWeakMessage::default_instance();
          }

          MessageLite* submsg =
              MergePartialFromCodedStreamHelper::Add(field, prototype);

          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadMessage(input, submsg))) {
            return false;
          }

          break;
        }
        case WireFormatLite::TYPE_MESSAGE | kOneofMask: {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          uint32* oneof_case = Raw<uint32>(msg, table.oneof_case_offset);
          MessageLite** submsg_holder = Raw<MessageLite*>(msg, offset);
          ResetOneofField<ProcessingType_MESSAGE>(
              table, field_number, arena, msg, oneof_case + presence_index,
              offset, NULL);
          MessageLite* submsg = *submsg_holder;

          if (PROTOBUF_PREDICT_FALSE(
                  !WireFormatLite::ReadMessage(input, submsg))) {
            return false;
          }

          break;
        }
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case TYPE_STRING_INLINED: {
          Arena* const arena =
              GetArena<InternalMetadata>(msg, table.arena_offset);
          const void* default_ptr = table.aux[field_number].strings.default_ptr;
          const char* field_name = table.aux[field_number].strings.field_name;

          if (PROTOBUF_PREDICT_FALSE(
                  (!HandleString<UnknownFieldHandler, Cardinality_SINGULAR,
                                 true, StringType_INLINED>(
                      input, msg, arena, has_bits, presence_index, offset,
                      default_ptr, field_name)))) {
            return false;
          }
          break;
        }
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
        case TYPE_MAP: {
          if (PROTOBUF_PREDICT_FALSE(!(*table.aux[field_number].maps.parse_map)(
                  input, Raw<void>(msg, offset)))) {
            return false;
          }
          break;
        }
        case 0: {
          // Done.
          input->SetLastTag(tag);
          return true;
        }
        default:
          break;
      }
    } else if (data->packed_wiretype == static_cast<unsigned char>(wire_type)) {
      // Non-packable fields have their packed_wiretype masked with
      // kNotPackedMask, which is impossible to match here.
      GOOGLE_DCHECK(processing_type & kRepeatedMask);
      GOOGLE_DCHECK_NE(processing_type, kRepeatedMask);
      GOOGLE_DCHECK_EQ(0, processing_type & kOneofMask);

      GOOGLE_DCHECK_NE(TYPE_BYTES_INLINED | kRepeatedMask, processing_type);
      GOOGLE_DCHECK_NE(TYPE_STRING_INLINED | kRepeatedMask, processing_type);

      // TODO(ckennelly): Use a computed goto on GCC/LLVM.
      //
      // Mask out kRepeatedMask bit, allowing the jump table to be smaller.
      switch (static_cast<WireFormatLite::FieldType>(processing_type ^
                                                     kRepeatedMask)) {
#define HANDLE_PACKED_TYPE(TYPE, CPPTYPE, CPPTYPE_METHOD)                      \
  case WireFormatLite::TYPE_##TYPE: {                                          \
    RepeatedField<CPPTYPE>* values = Raw<RepeatedField<CPPTYPE>>(msg, offset); \
    if (PROTOBUF_PREDICT_FALSE(                                                \
            (!WireFormatLite::ReadPackedPrimitive<                             \
                CPPTYPE, WireFormatLite::TYPE_##TYPE>(input, values)))) {      \
      return false;                                                            \
    }                                                                          \
    break;                                                                     \
  }

        HANDLE_PACKED_TYPE(INT32, int32, Int32)
        HANDLE_PACKED_TYPE(INT64, int64, Int64)
        HANDLE_PACKED_TYPE(SINT32, int32, Int32)
        HANDLE_PACKED_TYPE(SINT64, int64, Int64)
        HANDLE_PACKED_TYPE(UINT32, uint32, UInt32)
        HANDLE_PACKED_TYPE(UINT64, uint64, UInt64)

        HANDLE_PACKED_TYPE(FIXED32, uint32, UInt32)
        HANDLE_PACKED_TYPE(FIXED64, uint64, UInt64)
        HANDLE_PACKED_TYPE(SFIXED32, int32, Int32)
        HANDLE_PACKED_TYPE(SFIXED64, int64, Int64)

        HANDLE_PACKED_TYPE(FLOAT, float, Float)
        HANDLE_PACKED_TYPE(DOUBLE, double, Double)

        HANDLE_PACKED_TYPE(BOOL, bool, Bool)
#undef HANDLE_PACKED_TYPE
        case WireFormatLite::TYPE_ENUM: {
          // To avoid unnecessarily calling MutableUnknownFields (which mutates
          // InternalMetadataWithArena) when all inputs in the repeated series
          // are valid, we implement our own parser rather than call
          // WireFormat::ReadPackedEnumPreserveUnknowns.
          uint32 length;
          if (PROTOBUF_PREDICT_FALSE(!input->ReadVarint32(&length))) {
            return false;
          }

          AuxillaryParseTableField::EnumValidator validator =
              table.aux[field_number].enums.validator;
          RepeatedField<int>* values = Raw<RepeatedField<int>>(msg, offset);

          io::CodedInputStream::Limit limit = input->PushLimit(length);
          while (input->BytesUntilLimit() > 0) {
            int value;
            if (PROTOBUF_PREDICT_FALSE(
                    (!WireFormatLite::ReadPrimitive<
                        int, WireFormatLite::TYPE_ENUM>(input, &value)))) {
              return false;
            }

            if (validator == nullptr || validator(value)) {
              values->Add(value);
            } else {
              // TODO(ckennelly): Consider caching here.
              UnknownFieldHandler::Varint(msg, table, tag, value);
            }
          }
          input->PopLimit(limit);

          break;
        }
        case WireFormatLite::TYPE_STRING:
        case WireFormatLite::TYPE_GROUP:
        case WireFormatLite::TYPE_MESSAGE:
        case WireFormatLite::TYPE_BYTES:
          GOOGLE_DCHECK(false);
          return false;
        default:
          break;
      }
    } else {
      if (wire_type == WireFormatLite::WIRETYPE_END_GROUP) {
        // Must be the end of the message.
        input->SetLastTag(tag);
        return true;
      }

      // check for possible extensions
      if (UnknownFieldHandler::ParseExtension(msg, table, input, tag)) {
        // successfully parsed
        continue;
      }

      // process unknown field.
      if (PROTOBUF_PREDICT_FALSE(
              !UnknownFieldHandler::Skip(msg, table, input, tag))) {
        return false;
      }
    }
  }
}

template <typename UnknownFieldHandler, typename InternalMetadata>
bool MergePartialFromCodedStreamImpl(MessageLite* msg, const ParseTable& table,
                                     io::CodedInputStream* input) {
  // The main beneficial cutoff values are 1 and 2 byte tags.
  // Instantiate calls with the appropriate upper tag range
  if (table.max_field_number <= (0x7F >> 3)) {
    return MergePartialFromCodedStreamInlined<UnknownFieldHandler,
                                              InternalMetadata, 0x7F>(
        msg, table, input);
  } else if (table.max_field_number <= (0x3FFF >> 3)) {
    return MergePartialFromCodedStreamInlined<UnknownFieldHandler,
                                              InternalMetadata, 0x3FFF>(
        msg, table, input);
  } else {
    return MergePartialFromCodedStreamInlined<
        UnknownFieldHandler, InternalMetadata,
        std::numeric_limits<uint32>::max()>(msg, table, input);
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_GENERATED_MESSAGE_TABLE_DRIVEN_LITE_H__
