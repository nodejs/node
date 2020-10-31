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

#include <stack>
#include <string>
#include <vector>

#include <google/protobuf/wire_format.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/map_field.h>
#include <google/protobuf/map_field_inl.h>
#include <google/protobuf/unknown_field_set.h>


#include <google/protobuf/port_def.inc>

const size_t kMapEntryTagByteSize = 2;

namespace google {
namespace protobuf {
namespace internal {

// Forward declare static functions
static size_t MapKeyDataOnlyByteSize(const FieldDescriptor* field,
                                     const MapKey& value);
static size_t MapValueRefDataOnlyByteSize(const FieldDescriptor* field,
                                          const MapValueRef& value);

// ===================================================================

bool UnknownFieldSetFieldSkipper::SkipField(io::CodedInputStream* input,
                                            uint32 tag) {
  return WireFormat::SkipField(input, tag, unknown_fields_);
}

bool UnknownFieldSetFieldSkipper::SkipMessage(io::CodedInputStream* input) {
  return WireFormat::SkipMessage(input, unknown_fields_);
}

void UnknownFieldSetFieldSkipper::SkipUnknownEnum(int field_number, int value) {
  unknown_fields_->AddVarint(field_number, value);
}

bool WireFormat::SkipField(io::CodedInputStream* input, uint32 tag,
                           UnknownFieldSet* unknown_fields) {
  int number = WireFormatLite::GetTagFieldNumber(tag);
  // Field number 0 is illegal.
  if (number == 0) return false;

  switch (WireFormatLite::GetTagWireType(tag)) {
    case WireFormatLite::WIRETYPE_VARINT: {
      uint64 value;
      if (!input->ReadVarint64(&value)) return false;
      if (unknown_fields != NULL) unknown_fields->AddVarint(number, value);
      return true;
    }
    case WireFormatLite::WIRETYPE_FIXED64: {
      uint64 value;
      if (!input->ReadLittleEndian64(&value)) return false;
      if (unknown_fields != NULL) unknown_fields->AddFixed64(number, value);
      return true;
    }
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
      uint32 length;
      if (!input->ReadVarint32(&length)) return false;
      if (unknown_fields == NULL) {
        if (!input->Skip(length)) return false;
      } else {
        if (!input->ReadString(unknown_fields->AddLengthDelimited(number),
                               length)) {
          return false;
        }
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_START_GROUP: {
      if (!input->IncrementRecursionDepth()) return false;
      if (!SkipMessage(input, (unknown_fields == NULL)
                                  ? NULL
                                  : unknown_fields->AddGroup(number))) {
        return false;
      }
      input->DecrementRecursionDepth();
      // Check that the ending tag matched the starting tag.
      if (!input->LastTagWas(
              WireFormatLite::MakeTag(WireFormatLite::GetTagFieldNumber(tag),
                                      WireFormatLite::WIRETYPE_END_GROUP))) {
        return false;
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_END_GROUP: {
      return false;
    }
    case WireFormatLite::WIRETYPE_FIXED32: {
      uint32 value;
      if (!input->ReadLittleEndian32(&value)) return false;
      if (unknown_fields != NULL) unknown_fields->AddFixed32(number, value);
      return true;
    }
    default: {
      return false;
    }
  }
}

bool WireFormat::SkipMessage(io::CodedInputStream* input,
                             UnknownFieldSet* unknown_fields) {
  while (true) {
    uint32 tag = input->ReadTag();
    if (tag == 0) {
      // End of input.  This is a valid place to end, so return true.
      return true;
    }

    WireFormatLite::WireType wire_type = WireFormatLite::GetTagWireType(tag);

    if (wire_type == WireFormatLite::WIRETYPE_END_GROUP) {
      // Must be the end of the message.
      return true;
    }

    if (!SkipField(input, tag, unknown_fields)) return false;
  }
}

bool WireFormat::ReadPackedEnumPreserveUnknowns(io::CodedInputStream* input,
                                                uint32 field_number,
                                                bool (*is_valid)(int),
                                                UnknownFieldSet* unknown_fields,
                                                RepeatedField<int>* values) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  while (input->BytesUntilLimit() > 0) {
    int value;
    if (!WireFormatLite::ReadPrimitive<int, WireFormatLite::TYPE_ENUM>(
            input, &value)) {
      return false;
    }
    if (is_valid == NULL || is_valid(value)) {
      values->Add(value);
    } else {
      unknown_fields->AddVarint(field_number, value);
    }
  }
  input->PopLimit(limit);
  return true;
}

void WireFormat::SerializeUnknownFields(const UnknownFieldSet& unknown_fields,
                                        io::CodedOutputStream* output) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);
    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        output->WriteVarint32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_VARINT));
        output->WriteVarint64(field.varint());
        break;
      case UnknownField::TYPE_FIXED32:
        output->WriteVarint32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_FIXED32));
        output->WriteLittleEndian32(field.fixed32());
        break;
      case UnknownField::TYPE_FIXED64:
        output->WriteVarint32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_FIXED64));
        output->WriteLittleEndian64(field.fixed64());
        break;
      case UnknownField::TYPE_LENGTH_DELIMITED:
        output->WriteVarint32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
        output->WriteVarint32(field.length_delimited().size());
        output->WriteRawMaybeAliased(field.length_delimited().data(),
                                     field.length_delimited().size());
        break;
      case UnknownField::TYPE_GROUP:
        output->WriteVarint32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_START_GROUP));
        SerializeUnknownFields(field.group(), output);
        output->WriteVarint32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_END_GROUP));
        break;
    }
  }
}

uint8* WireFormat::SerializeUnknownFieldsToArray(
    const UnknownFieldSet& unknown_fields, uint8* target) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        target = WireFormatLite::WriteInt64ToArray(field.number(),
                                                   field.varint(), target);
        break;
      case UnknownField::TYPE_FIXED32:
        target = WireFormatLite::WriteFixed32ToArray(field.number(),
                                                     field.fixed32(), target);
        break;
      case UnknownField::TYPE_FIXED64:
        target = WireFormatLite::WriteFixed64ToArray(field.number(),
                                                     field.fixed64(), target);
        break;
      case UnknownField::TYPE_LENGTH_DELIMITED:
        target = WireFormatLite::WriteBytesToArray(
            field.number(), field.length_delimited(), target);
        break;
      case UnknownField::TYPE_GROUP:
        target = WireFormatLite::WriteTagToArray(
            field.number(), WireFormatLite::WIRETYPE_START_GROUP, target);
        target = SerializeUnknownFieldsToArray(field.group(), target);
        target = WireFormatLite::WriteTagToArray(
            field.number(), WireFormatLite::WIRETYPE_END_GROUP, target);
        break;
    }
  }
  return target;
}

void WireFormat::SerializeUnknownMessageSetItems(
    const UnknownFieldSet& unknown_fields, io::CodedOutputStream* output) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);
    // The only unknown fields that are allowed to exist in a MessageSet are
    // messages, which are length-delimited.
    if (field.type() == UnknownField::TYPE_LENGTH_DELIMITED) {
      // Start group.
      output->WriteVarint32(WireFormatLite::kMessageSetItemStartTag);

      // Write type ID.
      output->WriteVarint32(WireFormatLite::kMessageSetTypeIdTag);
      output->WriteVarint32(field.number());

      // Write message.
      output->WriteVarint32(WireFormatLite::kMessageSetMessageTag);
      field.SerializeLengthDelimitedNoTag(output);

      // End group.
      output->WriteVarint32(WireFormatLite::kMessageSetItemEndTag);
    }
  }
}

uint8* WireFormat::SerializeUnknownMessageSetItemsToArray(
    const UnknownFieldSet& unknown_fields, uint8* target) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    // The only unknown fields that are allowed to exist in a MessageSet are
    // messages, which are length-delimited.
    if (field.type() == UnknownField::TYPE_LENGTH_DELIMITED) {
      // Start group.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetItemStartTag, target);

      // Write type ID.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetTypeIdTag, target);
      target =
          io::CodedOutputStream::WriteVarint32ToArray(field.number(), target);

      // Write message.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetMessageTag, target);
      target = field.SerializeLengthDelimitedNoTagToArray(target);

      // End group.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetItemEndTag, target);
    }
  }

  return target;
}

size_t WireFormat::ComputeUnknownFieldsSize(
    const UnknownFieldSet& unknown_fields) {
  size_t size = 0;
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        size += io::CodedOutputStream::VarintSize32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_VARINT));
        size += io::CodedOutputStream::VarintSize64(field.varint());
        break;
      case UnknownField::TYPE_FIXED32:
        size += io::CodedOutputStream::VarintSize32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_FIXED32));
        size += sizeof(int32);
        break;
      case UnknownField::TYPE_FIXED64:
        size += io::CodedOutputStream::VarintSize32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_FIXED64));
        size += sizeof(int64);
        break;
      case UnknownField::TYPE_LENGTH_DELIMITED:
        size += io::CodedOutputStream::VarintSize32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
        size += io::CodedOutputStream::VarintSize32(
            field.length_delimited().size());
        size += field.length_delimited().size();
        break;
      case UnknownField::TYPE_GROUP:
        size += io::CodedOutputStream::VarintSize32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_START_GROUP));
        size += ComputeUnknownFieldsSize(field.group());
        size += io::CodedOutputStream::VarintSize32(WireFormatLite::MakeTag(
            field.number(), WireFormatLite::WIRETYPE_END_GROUP));
        break;
    }
  }

  return size;
}

size_t WireFormat::ComputeUnknownMessageSetItemsSize(
    const UnknownFieldSet& unknown_fields) {
  size_t size = 0;
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    // The only unknown fields that are allowed to exist in a MessageSet are
    // messages, which are length-delimited.
    if (field.type() == UnknownField::TYPE_LENGTH_DELIMITED) {
      size += WireFormatLite::kMessageSetItemTagsSize;
      size += io::CodedOutputStream::VarintSize32(field.number());

      int field_size = field.GetLengthDelimitedSize();
      size += io::CodedOutputStream::VarintSize32(field_size);
      size += field_size;
    }
  }

  return size;
}

// ===================================================================

bool WireFormat::ParseAndMergePartial(io::CodedInputStream* input,
                                      Message* message) {
  const Descriptor* descriptor = message->GetDescriptor();
  const Reflection* message_reflection = message->GetReflection();

  while (true) {
    uint32 tag = input->ReadTag();
    if (tag == 0) {
      // End of input.  This is a valid place to end, so return true.
      return true;
    }

    if (WireFormatLite::GetTagWireType(tag) ==
        WireFormatLite::WIRETYPE_END_GROUP) {
      // Must be the end of the message.
      return true;
    }

    const FieldDescriptor* field = NULL;

    if (descriptor != NULL) {
      int field_number = WireFormatLite::GetTagFieldNumber(tag);
      field = descriptor->FindFieldByNumber(field_number);

      // If that failed, check if the field is an extension.
      if (field == NULL && descriptor->IsExtensionNumber(field_number)) {
        if (input->GetExtensionPool() == NULL) {
          field = message_reflection->FindKnownExtensionByNumber(field_number);
        } else {
          field = input->GetExtensionPool()->FindExtensionByNumber(
              descriptor, field_number);
        }
      }

      // If that failed, but we're a MessageSet, and this is the tag for a
      // MessageSet item, then parse that.
      if (field == NULL && descriptor->options().message_set_wire_format() &&
          tag == WireFormatLite::kMessageSetItemStartTag) {
        if (!ParseAndMergeMessageSetItem(input, message)) {
          return false;
        }
        continue;  // Skip ParseAndMergeField(); already taken care of.
      }
    }

    if (!ParseAndMergeField(tag, field, message, input)) {
      return false;
    }
  }
}

bool WireFormat::SkipMessageSetField(io::CodedInputStream* input,
                                     uint32 field_number,
                                     UnknownFieldSet* unknown_fields) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  return input->ReadString(unknown_fields->AddLengthDelimited(field_number),
                           length);
}

bool WireFormat::ParseAndMergeMessageSetField(uint32 field_number,
                                              const FieldDescriptor* field,
                                              Message* message,
                                              io::CodedInputStream* input) {
  const Reflection* message_reflection = message->GetReflection();
  if (field == NULL) {
    // We store unknown MessageSet extensions as groups.
    return SkipMessageSetField(
        input, field_number, message_reflection->MutableUnknownFields(message));
  } else if (field->is_repeated() ||
             field->type() != FieldDescriptor::TYPE_MESSAGE) {
    // This shouldn't happen as we only allow optional message extensions to
    // MessageSet.
    GOOGLE_LOG(ERROR) << "Extensions of MessageSets must be optional messages.";
    return false;
  } else {
    Message* sub_message = message_reflection->MutableMessage(
        message, field, input->GetExtensionFactory());
    return WireFormatLite::ReadMessage(input, sub_message);
  }
}

static bool StrictUtf8Check(const FieldDescriptor* field) {
  return field->file()->syntax() == FileDescriptor::SYNTAX_PROTO3;
}

bool WireFormat::ParseAndMergeField(
    uint32 tag,
    const FieldDescriptor* field,  // May be NULL for unknown
    Message* message, io::CodedInputStream* input) {
  const Reflection* message_reflection = message->GetReflection();

  enum { UNKNOWN, NORMAL_FORMAT, PACKED_FORMAT } value_format;

  if (field == NULL) {
    value_format = UNKNOWN;
  } else if (WireFormatLite::GetTagWireType(tag) ==
             WireTypeForFieldType(field->type())) {
    value_format = NORMAL_FORMAT;
  } else if (field->is_packable() &&
             WireFormatLite::GetTagWireType(tag) ==
                 WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
    value_format = PACKED_FORMAT;
  } else {
    // We don't recognize this field. Either the field number is unknown
    // or the wire type doesn't match. Put it in our unknown field set.
    value_format = UNKNOWN;
  }

  if (value_format == UNKNOWN) {
    return SkipField(input, tag,
                     message_reflection->MutableUnknownFields(message));
  } else if (value_format == PACKED_FORMAT) {
    uint32 length;
    if (!input->ReadVarint32(&length)) return false;
    io::CodedInputStream::Limit limit = input->PushLimit(length);

    switch (field->type()) {
#define HANDLE_PACKED_TYPE(TYPE, CPPTYPE, CPPTYPE_METHOD)                      \
  case FieldDescriptor::TYPE_##TYPE: {                                         \
    while (input->BytesUntilLimit() > 0) {                                     \
      CPPTYPE value;                                                           \
      if (!WireFormatLite::ReadPrimitive<CPPTYPE,                              \
                                         WireFormatLite::TYPE_##TYPE>(input,   \
                                                                      &value)) \
        return false;                                                          \
      message_reflection->Add##CPPTYPE_METHOD(message, field, value);          \
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

      case FieldDescriptor::TYPE_ENUM: {
        while (input->BytesUntilLimit() > 0) {
          int value;
          if (!WireFormatLite::ReadPrimitive<int, WireFormatLite::TYPE_ENUM>(
                  input, &value))
            return false;
          if (message->GetDescriptor()->file()->syntax() ==
              FileDescriptor::SYNTAX_PROTO3) {
            message_reflection->AddEnumValue(message, field, value);
          } else {
            const EnumValueDescriptor* enum_value =
                field->enum_type()->FindValueByNumber(value);
            if (enum_value != NULL) {
              message_reflection->AddEnum(message, field, enum_value);
            } else {
              // The enum value is not one of the known values.  Add it to the
              // UnknownFieldSet.
              int64 sign_extended_value = static_cast<int64>(value);
              message_reflection->MutableUnknownFields(message)->AddVarint(
                  WireFormatLite::GetTagFieldNumber(tag), sign_extended_value);
            }
          }
        }

        break;
      }

      case FieldDescriptor::TYPE_STRING:
      case FieldDescriptor::TYPE_GROUP:
      case FieldDescriptor::TYPE_MESSAGE:
      case FieldDescriptor::TYPE_BYTES:
        // Can't have packed fields of these types: these should be caught by
        // the protocol compiler.
        return false;
        break;
    }

    input->PopLimit(limit);
  } else {
    // Non-packed value (value_format == NORMAL_FORMAT)
    switch (field->type()) {
#define HANDLE_TYPE(TYPE, CPPTYPE, CPPTYPE_METHOD)                            \
  case FieldDescriptor::TYPE_##TYPE: {                                        \
    CPPTYPE value;                                                            \
    if (!WireFormatLite::ReadPrimitive<CPPTYPE, WireFormatLite::TYPE_##TYPE>( \
            input, &value))                                                   \
      return false;                                                           \
    if (field->is_repeated()) {                                               \
      message_reflection->Add##CPPTYPE_METHOD(message, field, value);         \
    } else {                                                                  \
      message_reflection->Set##CPPTYPE_METHOD(message, field, value);         \
    }                                                                         \
    break;                                                                    \
  }

      HANDLE_TYPE(INT32, int32, Int32)
      HANDLE_TYPE(INT64, int64, Int64)
      HANDLE_TYPE(SINT32, int32, Int32)
      HANDLE_TYPE(SINT64, int64, Int64)
      HANDLE_TYPE(UINT32, uint32, UInt32)
      HANDLE_TYPE(UINT64, uint64, UInt64)

      HANDLE_TYPE(FIXED32, uint32, UInt32)
      HANDLE_TYPE(FIXED64, uint64, UInt64)
      HANDLE_TYPE(SFIXED32, int32, Int32)
      HANDLE_TYPE(SFIXED64, int64, Int64)

      HANDLE_TYPE(FLOAT, float, Float)
      HANDLE_TYPE(DOUBLE, double, Double)

      HANDLE_TYPE(BOOL, bool, Bool)
#undef HANDLE_TYPE

      case FieldDescriptor::TYPE_ENUM: {
        int value;
        if (!WireFormatLite::ReadPrimitive<int, WireFormatLite::TYPE_ENUM>(
                input, &value))
          return false;
        if (field->is_repeated()) {
          message_reflection->AddEnumValue(message, field, value);
        } else {
          message_reflection->SetEnumValue(message, field, value);
        }
        break;
      }

      // Handle strings separately so that we can optimize the ctype=CORD case.
      case FieldDescriptor::TYPE_STRING: {
        bool strict_utf8_check = StrictUtf8Check(field);
        std::string value;
        if (!WireFormatLite::ReadString(input, &value)) return false;
        if (strict_utf8_check) {
          if (!WireFormatLite::VerifyUtf8String(value.data(), value.length(),
                                                WireFormatLite::PARSE,
                                                field->full_name().c_str())) {
            return false;
          }
        } else {
          VerifyUTF8StringNamedField(value.data(), value.length(), PARSE,
                                     field->full_name().c_str());
        }
        if (field->is_repeated()) {
          message_reflection->AddString(message, field, value);
        } else {
          message_reflection->SetString(message, field, value);
        }
        break;
      }

      case FieldDescriptor::TYPE_BYTES: {
        std::string value;
        if (!WireFormatLite::ReadBytes(input, &value)) return false;
        if (field->is_repeated()) {
          message_reflection->AddString(message, field, value);
        } else {
          message_reflection->SetString(message, field, value);
        }
        break;
      }

      case FieldDescriptor::TYPE_GROUP: {
        Message* sub_message;
        if (field->is_repeated()) {
          sub_message = message_reflection->AddMessage(
              message, field, input->GetExtensionFactory());
        } else {
          sub_message = message_reflection->MutableMessage(
              message, field, input->GetExtensionFactory());
        }

        if (!WireFormatLite::ReadGroup(WireFormatLite::GetTagFieldNumber(tag),
                                       input, sub_message))
          return false;
        break;
      }

      case FieldDescriptor::TYPE_MESSAGE: {
        Message* sub_message;
        if (field->is_repeated()) {
          sub_message = message_reflection->AddMessage(
              message, field, input->GetExtensionFactory());
        } else {
          sub_message = message_reflection->MutableMessage(
              message, field, input->GetExtensionFactory());
        }

        if (!WireFormatLite::ReadMessage(input, sub_message)) return false;
        break;
      }
    }
  }

  return true;
}

bool WireFormat::ParseAndMergeMessageSetItem(io::CodedInputStream* input,
                                             Message* message) {
  struct MSReflective {
    bool ParseField(int type_id, io::CodedInputStream* input) {
      const FieldDescriptor* field =
          message_reflection->FindKnownExtensionByNumber(type_id);
      return ParseAndMergeMessageSetField(type_id, field, message, input);
    }

    bool SkipField(uint32 tag, io::CodedInputStream* input) {
      return WireFormat::SkipField(input, tag, NULL);
    }

    const Reflection* message_reflection;
    Message* message;
  };

  return ParseMessageSetItemImpl(
      input, MSReflective{message->GetReflection(), message});
}

// ===================================================================

void WireFormat::SerializeWithCachedSizes(const Message& message, int size,
                                          io::CodedOutputStream* output) {
  const Descriptor* descriptor = message.GetDescriptor();
  const Reflection* message_reflection = message.GetReflection();
  int expected_endpoint = output->ByteCount() + size;

  std::vector<const FieldDescriptor*> fields;

  // Fields of map entry should always be serialized.
  if (descriptor->options().map_entry()) {
    for (int i = 0; i < descriptor->field_count(); i++) {
      fields.push_back(descriptor->field(i));
    }
  } else {
    message_reflection->ListFields(message, &fields);
  }

  for (int i = 0; i < fields.size(); i++) {
    SerializeFieldWithCachedSizes(fields[i], message, output);
  }

  if (descriptor->options().message_set_wire_format()) {
    SerializeUnknownMessageSetItems(
        message_reflection->GetUnknownFields(message), output);
  } else {
    SerializeUnknownFields(message_reflection->GetUnknownFields(message),
                           output);
  }

  GOOGLE_CHECK_EQ(output->ByteCount(), expected_endpoint)
      << ": Protocol message serialized to a size different from what was "
         "originally expected.  Perhaps it was modified by another thread "
         "during serialization?";
}

static void SerializeMapKeyWithCachedSizes(const FieldDescriptor* field,
                                           const MapKey& value,
                                           io::CodedOutputStream* output) {
  switch (field->type()) {
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_FLOAT:
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_BYTES:
    case FieldDescriptor::TYPE_ENUM:
      GOOGLE_LOG(FATAL) << "Unsupported";
      break;
#define CASE_TYPE(FieldType, CamelFieldType, CamelCppType)                     \
  case FieldDescriptor::TYPE_##FieldType:                                      \
    WireFormatLite::Write##CamelFieldType(1, value.Get##CamelCppType##Value(), \
                                          output);                             \
    break;
      CASE_TYPE(INT64, Int64, Int64)
      CASE_TYPE(UINT64, UInt64, UInt64)
      CASE_TYPE(INT32, Int32, Int32)
      CASE_TYPE(FIXED64, Fixed64, UInt64)
      CASE_TYPE(FIXED32, Fixed32, UInt32)
      CASE_TYPE(BOOL, Bool, Bool)
      CASE_TYPE(UINT32, UInt32, UInt32)
      CASE_TYPE(SFIXED32, SFixed32, Int32)
      CASE_TYPE(SFIXED64, SFixed64, Int64)
      CASE_TYPE(SINT32, SInt32, Int32)
      CASE_TYPE(SINT64, SInt64, Int64)
      CASE_TYPE(STRING, String, String)
#undef CASE_TYPE
  }
}

static void SerializeMapValueRefWithCachedSizes(const FieldDescriptor* field,
                                                const MapValueRef& value,
                                                io::CodedOutputStream* output) {
  switch (field->type()) {
#define CASE_TYPE(FieldType, CamelFieldType, CamelCppType)                     \
  case FieldDescriptor::TYPE_##FieldType:                                      \
    WireFormatLite::Write##CamelFieldType(2, value.Get##CamelCppType##Value(), \
                                          output);                             \
    break;
    CASE_TYPE(INT64, Int64, Int64)
    CASE_TYPE(UINT64, UInt64, UInt64)
    CASE_TYPE(INT32, Int32, Int32)
    CASE_TYPE(FIXED64, Fixed64, UInt64)
    CASE_TYPE(FIXED32, Fixed32, UInt32)
    CASE_TYPE(BOOL, Bool, Bool)
    CASE_TYPE(UINT32, UInt32, UInt32)
    CASE_TYPE(SFIXED32, SFixed32, Int32)
    CASE_TYPE(SFIXED64, SFixed64, Int64)
    CASE_TYPE(SINT32, SInt32, Int32)
    CASE_TYPE(SINT64, SInt64, Int64)
    CASE_TYPE(ENUM, Enum, Enum)
    CASE_TYPE(DOUBLE, Double, Double)
    CASE_TYPE(FLOAT, Float, Float)
    CASE_TYPE(STRING, String, String)
    CASE_TYPE(BYTES, Bytes, String)
    CASE_TYPE(MESSAGE, Message, Message)
    CASE_TYPE(GROUP, Group, Message)
#undef CASE_TYPE
  }
}

class MapKeySorter {
 public:
  static std::vector<MapKey> SortKey(const Message& message,
                                     const Reflection* reflection,
                                     const FieldDescriptor* field) {
    std::vector<MapKey> sorted_key_list;
    for (MapIterator it =
             reflection->MapBegin(const_cast<Message*>(&message), field);
         it != reflection->MapEnd(const_cast<Message*>(&message), field);
         ++it) {
      sorted_key_list.push_back(it.GetKey());
    }
    MapKeyComparator comparator;
    std::sort(sorted_key_list.begin(), sorted_key_list.end(), comparator);
    return sorted_key_list;
  }

 private:
  class MapKeyComparator {
   public:
    bool operator()(const MapKey& a, const MapKey& b) const {
      GOOGLE_DCHECK(a.type() == b.type());
      switch (a.type()) {
#define CASE_TYPE(CppType, CamelCppType)                                \
  case FieldDescriptor::CPPTYPE_##CppType: {                            \
    return a.Get##CamelCppType##Value() < b.Get##CamelCppType##Value(); \
  }
        CASE_TYPE(STRING, String)
        CASE_TYPE(INT64, Int64)
        CASE_TYPE(INT32, Int32)
        CASE_TYPE(UINT64, UInt64)
        CASE_TYPE(UINT32, UInt32)
        CASE_TYPE(BOOL, Bool)
#undef CASE_TYPE

        default:
          GOOGLE_LOG(DFATAL) << "Invalid key for map field.";
          return true;
      }
    }
  };
};

static void SerializeMapEntry(const FieldDescriptor* field, const MapKey& key,
                              const MapValueRef& value,
                              io::CodedOutputStream* output) {
  const FieldDescriptor* key_field = field->message_type()->field(0);
  const FieldDescriptor* value_field = field->message_type()->field(1);

  WireFormatLite::WriteTag(field->number(),
                           WireFormatLite::WIRETYPE_LENGTH_DELIMITED, output);
  size_t size = kMapEntryTagByteSize;
  size += MapKeyDataOnlyByteSize(key_field, key);
  size += MapValueRefDataOnlyByteSize(value_field, value);
  output->WriteVarint32(size);
  SerializeMapKeyWithCachedSizes(key_field, key, output);
  SerializeMapValueRefWithCachedSizes(value_field, value, output);
}

void WireFormat::SerializeFieldWithCachedSizes(const FieldDescriptor* field,
                                               const Message& message,
                                               io::CodedOutputStream* output) {
  const Reflection* message_reflection = message.GetReflection();

  if (field->is_extension() &&
      field->containing_type()->options().message_set_wire_format() &&
      field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
      !field->is_repeated()) {
    SerializeMessageSetItemWithCachedSizes(field, message, output);
    return;
  }

  // For map fields, we can use either repeated field reflection or map
  // reflection.  Our choice has some subtle effects.  If we use repeated field
  // reflection here, then the repeated field representation becomes
  // authoritative for this field: any existing references that came from map
  // reflection remain valid for reading, but mutations to them are lost and
  // will be overwritten next time we call map reflection!
  //
  // So far this mainly affects Python, which keeps long-term references to map
  // values around, and always uses map reflection.  See: b/35918691
  //
  // Here we choose to use map reflection API as long as the internal
  // map is valid. In this way, the serialization doesn't change map field's
  // internal state and existing references that came from map reflection remain
  // valid for both reading and writing.
  if (field->is_map()) {
    const MapFieldBase* map_field =
        message_reflection->GetMapData(message, field);
    if (map_field->IsMapValid()) {
      if (output->IsSerializationDeterministic()) {
        std::vector<MapKey> sorted_key_list =
            MapKeySorter::SortKey(message, message_reflection, field);
        for (std::vector<MapKey>::iterator it = sorted_key_list.begin();
             it != sorted_key_list.end(); ++it) {
          MapValueRef map_value;
          message_reflection->InsertOrLookupMapValue(
              const_cast<Message*>(&message), field, *it, &map_value);
          SerializeMapEntry(field, *it, map_value, output);
        }
      } else {
        for (MapIterator it = message_reflection->MapBegin(
                 const_cast<Message*>(&message), field);
             it !=
             message_reflection->MapEnd(const_cast<Message*>(&message), field);
             ++it) {
          SerializeMapEntry(field, it.GetKey(), it.GetValueRef(), output);
        }
      }

      return;
    }
  }

  int count = 0;

  if (field->is_repeated()) {
    count = message_reflection->FieldSize(message, field);
  } else if (field->containing_type()->options().map_entry()) {
    // Map entry fields always need to be serialized.
    count = 1;
  } else if (message_reflection->HasField(message, field)) {
    count = 1;
  }

  // map_entries is for maps that'll be deterministically serialized.
  std::vector<const Message*> map_entries;
  if (count > 1 && field->is_map() && output->IsSerializationDeterministic()) {
    map_entries =
        DynamicMapSorter::Sort(message, count, message_reflection, field);
  }

  const bool is_packed = field->is_packed();
  if (is_packed && count > 0) {
    WireFormatLite::WriteTag(field->number(),
                             WireFormatLite::WIRETYPE_LENGTH_DELIMITED, output);
    const size_t data_size = FieldDataOnlyByteSize(field, message);
    output->WriteVarint32(data_size);
  }

  for (int j = 0; j < count; j++) {
    switch (field->type()) {
#define HANDLE_PRIMITIVE_TYPE(TYPE, CPPTYPE, TYPE_METHOD, CPPTYPE_METHOD)     \
  case FieldDescriptor::TYPE_##TYPE: {                                        \
    const CPPTYPE value =                                                     \
        field->is_repeated()                                                  \
            ? message_reflection->GetRepeated##CPPTYPE_METHOD(message, field, \
                                                              j)              \
            : message_reflection->Get##CPPTYPE_METHOD(message, field);        \
    if (is_packed) {                                                          \
      WireFormatLite::Write##TYPE_METHOD##NoTag(value, output);               \
    } else {                                                                  \
      WireFormatLite::Write##TYPE_METHOD(field->number(), value, output);     \
    }                                                                         \
    break;                                                                    \
  }

      HANDLE_PRIMITIVE_TYPE(INT32, int32, Int32, Int32)
      HANDLE_PRIMITIVE_TYPE(INT64, int64, Int64, Int64)
      HANDLE_PRIMITIVE_TYPE(SINT32, int32, SInt32, Int32)
      HANDLE_PRIMITIVE_TYPE(SINT64, int64, SInt64, Int64)
      HANDLE_PRIMITIVE_TYPE(UINT32, uint32, UInt32, UInt32)
      HANDLE_PRIMITIVE_TYPE(UINT64, uint64, UInt64, UInt64)

      HANDLE_PRIMITIVE_TYPE(FIXED32, uint32, Fixed32, UInt32)
      HANDLE_PRIMITIVE_TYPE(FIXED64, uint64, Fixed64, UInt64)
      HANDLE_PRIMITIVE_TYPE(SFIXED32, int32, SFixed32, Int32)
      HANDLE_PRIMITIVE_TYPE(SFIXED64, int64, SFixed64, Int64)

      HANDLE_PRIMITIVE_TYPE(FLOAT, float, Float, Float)
      HANDLE_PRIMITIVE_TYPE(DOUBLE, double, Double, Double)

      HANDLE_PRIMITIVE_TYPE(BOOL, bool, Bool, Bool)
#undef HANDLE_PRIMITIVE_TYPE

#define HANDLE_TYPE(TYPE, TYPE_METHOD, CPPTYPE_METHOD)                         \
  case FieldDescriptor::TYPE_##TYPE:                                           \
    WireFormatLite::Write##TYPE_METHOD(                                        \
        field->number(),                                                       \
        field->is_repeated()                                                   \
            ? (map_entries.empty()                                             \
                   ? message_reflection->GetRepeated##CPPTYPE_METHOD(message,  \
                                                                     field, j) \
                   : *map_entries[j])                                          \
            : message_reflection->Get##CPPTYPE_METHOD(message, field),         \
        output);                                                               \
    break;

      HANDLE_TYPE(GROUP, Group, Message)
      HANDLE_TYPE(MESSAGE, Message, Message)
#undef HANDLE_TYPE

      case FieldDescriptor::TYPE_ENUM: {
        const EnumValueDescriptor* value =
            field->is_repeated()
                ? message_reflection->GetRepeatedEnum(message, field, j)
                : message_reflection->GetEnum(message, field);
        if (is_packed) {
          WireFormatLite::WriteEnumNoTag(value->number(), output);
        } else {
          WireFormatLite::WriteEnum(field->number(), value->number(), output);
        }
        break;
      }

      // Handle strings separately so that we can get string references
      // instead of copying.
      case FieldDescriptor::TYPE_STRING: {
        bool strict_utf8_check = StrictUtf8Check(field);
        std::string scratch;
        const std::string& value =
            field->is_repeated()
                ? message_reflection->GetRepeatedStringReference(message, field,
                                                                 j, &scratch)
                : message_reflection->GetStringReference(message, field,
                                                         &scratch);
        if (strict_utf8_check) {
          WireFormatLite::VerifyUtf8String(value.data(), value.length(),
                                           WireFormatLite::SERIALIZE,
                                           field->full_name().c_str());
        } else {
          VerifyUTF8StringNamedField(value.data(), value.length(), SERIALIZE,
                                     field->full_name().c_str());
        }
        WireFormatLite::WriteString(field->number(), value, output);
        break;
      }

      case FieldDescriptor::TYPE_BYTES: {
        std::string scratch;
        const std::string& value =
            field->is_repeated()
                ? message_reflection->GetRepeatedStringReference(message, field,
                                                                 j, &scratch)
                : message_reflection->GetStringReference(message, field,
                                                         &scratch);
        WireFormatLite::WriteBytes(field->number(), value, output);
        break;
      }
    }
  }
}

void WireFormat::SerializeMessageSetItemWithCachedSizes(
    const FieldDescriptor* field, const Message& message,
    io::CodedOutputStream* output) {
  const Reflection* message_reflection = message.GetReflection();

  // Start group.
  output->WriteVarint32(WireFormatLite::kMessageSetItemStartTag);

  // Write type ID.
  output->WriteVarint32(WireFormatLite::kMessageSetTypeIdTag);
  output->WriteVarint32(field->number());

  // Write message.
  output->WriteVarint32(WireFormatLite::kMessageSetMessageTag);

  const Message& sub_message = message_reflection->GetMessage(message, field);
  output->WriteVarint32(sub_message.GetCachedSize());
  sub_message.SerializeWithCachedSizes(output);

  // End group.
  output->WriteVarint32(WireFormatLite::kMessageSetItemEndTag);
}

// ===================================================================

size_t WireFormat::ByteSize(const Message& message) {
  const Descriptor* descriptor = message.GetDescriptor();
  const Reflection* message_reflection = message.GetReflection();

  size_t our_size = 0;

  std::vector<const FieldDescriptor*> fields;

  // Fields of map entry should always be serialized.
  if (descriptor->options().map_entry()) {
    for (int i = 0; i < descriptor->field_count(); i++) {
      fields.push_back(descriptor->field(i));
    }
  } else {
    message_reflection->ListFields(message, &fields);
  }

  for (int i = 0; i < fields.size(); i++) {
    our_size += FieldByteSize(fields[i], message);
  }

  if (descriptor->options().message_set_wire_format()) {
    our_size += ComputeUnknownMessageSetItemsSize(
        message_reflection->GetUnknownFields(message));
  } else {
    our_size +=
        ComputeUnknownFieldsSize(message_reflection->GetUnknownFields(message));
  }

  return our_size;
}

size_t WireFormat::FieldByteSize(const FieldDescriptor* field,
                                 const Message& message) {
  const Reflection* message_reflection = message.GetReflection();

  if (field->is_extension() &&
      field->containing_type()->options().message_set_wire_format() &&
      field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
      !field->is_repeated()) {
    return MessageSetItemByteSize(field, message);
  }

  size_t count = 0;
  if (field->is_repeated()) {
    count = FromIntSize(message_reflection->FieldSize(message, field));
  } else if (field->containing_type()->options().map_entry()) {
    // Map entry fields always need to be serialized.
    count = 1;
  } else if (message_reflection->HasField(message, field)) {
    count = 1;
  }

  const size_t data_size = FieldDataOnlyByteSize(field, message);
  size_t our_size = data_size;
  if (field->is_packed()) {
    if (data_size > 0) {
      // Packed fields get serialized like a string, not their native type.
      // Technically this doesn't really matter; the size only changes if it's
      // a GROUP
      our_size += TagSize(field->number(), FieldDescriptor::TYPE_STRING);
      our_size += io::CodedOutputStream::VarintSize32(data_size);
    }
  } else {
    our_size += count * TagSize(field->number(), field->type());
  }
  return our_size;
}

static size_t MapKeyDataOnlyByteSize(const FieldDescriptor* field,
                                     const MapKey& value) {
  GOOGLE_DCHECK_EQ(FieldDescriptor::TypeToCppType(field->type()), value.type());
  switch (field->type()) {
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_FLOAT:
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_BYTES:
    case FieldDescriptor::TYPE_ENUM:
      GOOGLE_LOG(FATAL) << "Unsupported";
      return 0;
#define CASE_TYPE(FieldType, CamelFieldType, CamelCppType) \
  case FieldDescriptor::TYPE_##FieldType:                  \
    return WireFormatLite::CamelFieldType##Size(           \
        value.Get##CamelCppType##Value());

#define FIXED_CASE_TYPE(FieldType, CamelFieldType) \
  case FieldDescriptor::TYPE_##FieldType:          \
    return WireFormatLite::k##CamelFieldType##Size;

      CASE_TYPE(INT32, Int32, Int32);
      CASE_TYPE(INT64, Int64, Int64);
      CASE_TYPE(UINT32, UInt32, UInt32);
      CASE_TYPE(UINT64, UInt64, UInt64);
      CASE_TYPE(SINT32, SInt32, Int32);
      CASE_TYPE(SINT64, SInt64, Int64);
      CASE_TYPE(STRING, String, String);
      FIXED_CASE_TYPE(FIXED32, Fixed32);
      FIXED_CASE_TYPE(FIXED64, Fixed64);
      FIXED_CASE_TYPE(SFIXED32, SFixed32);
      FIXED_CASE_TYPE(SFIXED64, SFixed64);
      FIXED_CASE_TYPE(BOOL, Bool);

#undef CASE_TYPE
#undef FIXED_CASE_TYPE
  }
  GOOGLE_LOG(FATAL) << "Cannot get here";
  return 0;
}

static size_t MapValueRefDataOnlyByteSize(const FieldDescriptor* field,
                                          const MapValueRef& value) {
  switch (field->type()) {
    case FieldDescriptor::TYPE_GROUP:
      GOOGLE_LOG(FATAL) << "Unsupported";
      return 0;
#define CASE_TYPE(FieldType, CamelFieldType, CamelCppType) \
  case FieldDescriptor::TYPE_##FieldType:                  \
    return WireFormatLite::CamelFieldType##Size(           \
        value.Get##CamelCppType##Value());

#define FIXED_CASE_TYPE(FieldType, CamelFieldType) \
  case FieldDescriptor::TYPE_##FieldType:          \
    return WireFormatLite::k##CamelFieldType##Size;

      CASE_TYPE(INT32, Int32, Int32);
      CASE_TYPE(INT64, Int64, Int64);
      CASE_TYPE(UINT32, UInt32, UInt32);
      CASE_TYPE(UINT64, UInt64, UInt64);
      CASE_TYPE(SINT32, SInt32, Int32);
      CASE_TYPE(SINT64, SInt64, Int64);
      CASE_TYPE(STRING, String, String);
      CASE_TYPE(BYTES, Bytes, String);
      CASE_TYPE(ENUM, Enum, Enum);
      CASE_TYPE(MESSAGE, Message, Message);
      FIXED_CASE_TYPE(FIXED32, Fixed32);
      FIXED_CASE_TYPE(FIXED64, Fixed64);
      FIXED_CASE_TYPE(SFIXED32, SFixed32);
      FIXED_CASE_TYPE(SFIXED64, SFixed64);
      FIXED_CASE_TYPE(DOUBLE, Double);
      FIXED_CASE_TYPE(FLOAT, Float);
      FIXED_CASE_TYPE(BOOL, Bool);

#undef CASE_TYPE
#undef FIXED_CASE_TYPE
  }
  GOOGLE_LOG(FATAL) << "Cannot get here";
  return 0;
}

size_t WireFormat::FieldDataOnlyByteSize(const FieldDescriptor* field,
                                         const Message& message) {
  const Reflection* message_reflection = message.GetReflection();

  size_t data_size = 0;

  if (field->is_map()) {
    const MapFieldBase* map_field =
        message_reflection->GetMapData(message, field);
    if (map_field->IsMapValid()) {
      MapIterator iter(const_cast<Message*>(&message), field);
      MapIterator end(const_cast<Message*>(&message), field);
      const FieldDescriptor* key_field = field->message_type()->field(0);
      const FieldDescriptor* value_field = field->message_type()->field(1);
      for (map_field->MapBegin(&iter), map_field->MapEnd(&end); iter != end;
           ++iter) {
        size_t size = kMapEntryTagByteSize;
        size += MapKeyDataOnlyByteSize(key_field, iter.GetKey());
        size += MapValueRefDataOnlyByteSize(value_field, iter.GetValueRef());
        data_size += WireFormatLite::LengthDelimitedSize(size);
      }
      return data_size;
    }
  }

  size_t count = 0;
  if (field->is_repeated()) {
    count =
        internal::FromIntSize(message_reflection->FieldSize(message, field));
  } else if (field->containing_type()->options().map_entry()) {
    // Map entry fields always need to be serialized.
    count = 1;
  } else if (message_reflection->HasField(message, field)) {
    count = 1;
  }

  switch (field->type()) {
#define HANDLE_TYPE(TYPE, TYPE_METHOD, CPPTYPE_METHOD)                      \
  case FieldDescriptor::TYPE_##TYPE:                                        \
    if (field->is_repeated()) {                                             \
      for (int j = 0; j < count; j++) {                                     \
        data_size += WireFormatLite::TYPE_METHOD##Size(                     \
            message_reflection->GetRepeated##CPPTYPE_METHOD(message, field, \
                                                            j));            \
      }                                                                     \
    } else {                                                                \
      data_size += WireFormatLite::TYPE_METHOD##Size(                       \
          message_reflection->Get##CPPTYPE_METHOD(message, field));         \
    }                                                                       \
    break;

#define HANDLE_FIXED_TYPE(TYPE, TYPE_METHOD)                   \
  case FieldDescriptor::TYPE_##TYPE:                           \
    data_size += count * WireFormatLite::k##TYPE_METHOD##Size; \
    break;

    HANDLE_TYPE(INT32, Int32, Int32)
    HANDLE_TYPE(INT64, Int64, Int64)
    HANDLE_TYPE(SINT32, SInt32, Int32)
    HANDLE_TYPE(SINT64, SInt64, Int64)
    HANDLE_TYPE(UINT32, UInt32, UInt32)
    HANDLE_TYPE(UINT64, UInt64, UInt64)

    HANDLE_FIXED_TYPE(FIXED32, Fixed32)
    HANDLE_FIXED_TYPE(FIXED64, Fixed64)
    HANDLE_FIXED_TYPE(SFIXED32, SFixed32)
    HANDLE_FIXED_TYPE(SFIXED64, SFixed64)

    HANDLE_FIXED_TYPE(FLOAT, Float)
    HANDLE_FIXED_TYPE(DOUBLE, Double)

    HANDLE_FIXED_TYPE(BOOL, Bool)

    HANDLE_TYPE(GROUP, Group, Message)
    HANDLE_TYPE(MESSAGE, Message, Message)
#undef HANDLE_TYPE
#undef HANDLE_FIXED_TYPE

    case FieldDescriptor::TYPE_ENUM: {
      if (field->is_repeated()) {
        for (int j = 0; j < count; j++) {
          data_size += WireFormatLite::EnumSize(
              message_reflection->GetRepeatedEnum(message, field, j)->number());
        }
      } else {
        data_size += WireFormatLite::EnumSize(
            message_reflection->GetEnum(message, field)->number());
      }
      break;
    }

    // Handle strings separately so that we can get string references
    // instead of copying.
    case FieldDescriptor::TYPE_STRING:
    case FieldDescriptor::TYPE_BYTES: {
      for (int j = 0; j < count; j++) {
        std::string scratch;
        const std::string& value =
            field->is_repeated()
                ? message_reflection->GetRepeatedStringReference(message, field,
                                                                 j, &scratch)
                : message_reflection->GetStringReference(message, field,
                                                         &scratch);
        data_size += WireFormatLite::StringSize(value);
      }
      break;
    }
  }
  return data_size;
}

size_t WireFormat::MessageSetItemByteSize(const FieldDescriptor* field,
                                          const Message& message) {
  const Reflection* message_reflection = message.GetReflection();

  size_t our_size = WireFormatLite::kMessageSetItemTagsSize;

  // type_id
  our_size += io::CodedOutputStream::VarintSize32(field->number());

  // message
  const Message& sub_message = message_reflection->GetMessage(message, field);
  size_t message_size = sub_message.ByteSizeLong();

  our_size += io::CodedOutputStream::VarintSize32(message_size);
  our_size += message_size;

  return our_size;
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
