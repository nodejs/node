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

#ifndef GOOGLE_PROTOBUF_EXTENSION_SET_INL_H__
#define GOOGLE_PROTOBUF_EXTENSION_SET_INL_H__

#include <google/protobuf/parse_context.h>
#include <google/protobuf/extension_set.h>

namespace google {
namespace protobuf {
namespace internal {

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
template <typename T>
const char* ExtensionSet::ParseFieldWithExtensionInfo(
    int number, bool was_packed_on_wire, const ExtensionInfo& extension,
    T* metadata, const char* ptr, internal::ParseContext* ctx) {
  if (was_packed_on_wire) {
    switch (extension.type) {
#define HANDLE_TYPE(UPPERCASE, CPP_CAMELCASE)                                \
  case WireFormatLite::TYPE_##UPPERCASE:                                     \
    return internal::Packed##CPP_CAMELCASE##Parser(                          \
        MutableRawRepeatedField(number, extension.type, extension.is_packed, \
                                extension.descriptor),                       \
        ptr, ctx);
      HANDLE_TYPE(INT32, Int32);
      HANDLE_TYPE(INT64, Int64);
      HANDLE_TYPE(UINT32, UInt32);
      HANDLE_TYPE(UINT64, UInt64);
      HANDLE_TYPE(SINT32, SInt32);
      HANDLE_TYPE(SINT64, SInt64);
      HANDLE_TYPE(FIXED32, Fixed32);
      HANDLE_TYPE(FIXED64, Fixed64);
      HANDLE_TYPE(SFIXED32, SFixed32);
      HANDLE_TYPE(SFIXED64, SFixed64);
      HANDLE_TYPE(FLOAT, Float);
      HANDLE_TYPE(DOUBLE, Double);
      HANDLE_TYPE(BOOL, Bool);
#undef HANDLE_TYPE

      case WireFormatLite::TYPE_ENUM:
        return internal::PackedEnumParserArg(
            MutableRawRepeatedField(number, extension.type, extension.is_packed,
                                    extension.descriptor),
            ptr, ctx, extension.enum_validity_check.func,
            extension.enum_validity_check.arg, metadata, number);
      case WireFormatLite::TYPE_STRING:
      case WireFormatLite::TYPE_BYTES:
      case WireFormatLite::TYPE_GROUP:
      case WireFormatLite::TYPE_MESSAGE:
        GOOGLE_LOG(FATAL) << "Non-primitive types can't be packed.";
        break;
    }
  } else {
    switch (extension.type) {
#define HANDLE_VARINT_TYPE(UPPERCASE, CPP_CAMELCASE)                        \
  case WireFormatLite::TYPE_##UPPERCASE: {                                  \
    uint64 value;                                                           \
    ptr = ParseVarint64(ptr, &value);                                       \
    GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);                                    \
    if (extension.is_repeated) {                                            \
      Add##CPP_CAMELCASE(number, WireFormatLite::TYPE_##UPPERCASE,          \
                         extension.is_packed, value, extension.descriptor); \
    } else {                                                                \
      Set##CPP_CAMELCASE(number, WireFormatLite::TYPE_##UPPERCASE, value,   \
                         extension.descriptor);                             \
    }                                                                       \
  } break

      HANDLE_VARINT_TYPE(INT32, Int32);
      HANDLE_VARINT_TYPE(INT64, Int64);
      HANDLE_VARINT_TYPE(UINT32, UInt32);
      HANDLE_VARINT_TYPE(UINT64, UInt64);
#undef HANDLE_VARINT_TYPE
#define HANDLE_SVARINT_TYPE(UPPERCASE, CPP_CAMELCASE, SIZE)                 \
  case WireFormatLite::TYPE_##UPPERCASE: {                                  \
    uint64 val;                                                             \
    ptr = ParseVarint64(ptr, &val);                                         \
    GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);                                    \
    auto value = WireFormatLite::ZigZagDecode##SIZE(val);                   \
    if (extension.is_repeated) {                                            \
      Add##CPP_CAMELCASE(number, WireFormatLite::TYPE_##UPPERCASE,          \
                         extension.is_packed, value, extension.descriptor); \
    } else {                                                                \
      Set##CPP_CAMELCASE(number, WireFormatLite::TYPE_##UPPERCASE, value,   \
                         extension.descriptor);                             \
    }                                                                       \
  } break

      HANDLE_SVARINT_TYPE(SINT32, Int32, 32);
      HANDLE_SVARINT_TYPE(SINT64, Int64, 64);
#undef HANDLE_SVARINT_TYPE
#define HANDLE_FIXED_TYPE(UPPERCASE, CPP_CAMELCASE, CPPTYPE)                \
  case WireFormatLite::TYPE_##UPPERCASE: {                                  \
    CPPTYPE value;                                                          \
    std::memcpy(&value, ptr, sizeof(CPPTYPE));                              \
    ptr += sizeof(CPPTYPE);                                                 \
    if (extension.is_repeated) {                                            \
      Add##CPP_CAMELCASE(number, WireFormatLite::TYPE_##UPPERCASE,          \
                         extension.is_packed, value, extension.descriptor); \
    } else {                                                                \
      Set##CPP_CAMELCASE(number, WireFormatLite::TYPE_##UPPERCASE, value,   \
                         extension.descriptor);                             \
    }                                                                       \
  } break

      HANDLE_FIXED_TYPE(FIXED32, UInt32, uint32);
      HANDLE_FIXED_TYPE(FIXED64, UInt64, uint64);
      HANDLE_FIXED_TYPE(SFIXED32, Int32, int32);
      HANDLE_FIXED_TYPE(SFIXED64, Int64, int64);
      HANDLE_FIXED_TYPE(FLOAT, Float, float);
      HANDLE_FIXED_TYPE(DOUBLE, Double, double);
      HANDLE_FIXED_TYPE(BOOL, Bool, bool);
#undef HANDLE_FIXED_TYPE

      case WireFormatLite::TYPE_ENUM: {
        uint64 val;
        ptr = ParseVarint64(ptr, &val);
        GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
        int value = val;

        if (!extension.enum_validity_check.func(
                extension.enum_validity_check.arg, value)) {
          WriteVarint(number, val, metadata->mutable_unknown_fields());
        } else if (extension.is_repeated) {
          AddEnum(number, WireFormatLite::TYPE_ENUM, extension.is_packed, value,
                  extension.descriptor);
        } else {
          SetEnum(number, WireFormatLite::TYPE_ENUM, value,
                  extension.descriptor);
        }
        break;
      }

      case WireFormatLite::TYPE_BYTES:
      case WireFormatLite::TYPE_STRING: {
        std::string* value =
            extension.is_repeated
                ? AddString(number, WireFormatLite::TYPE_STRING,
                            extension.descriptor)
                : MutableString(number, WireFormatLite::TYPE_STRING,
                                extension.descriptor);
        int size = ReadSize(&ptr);
        GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
        return ctx->ReadString(ptr, size, value);
      }

      case WireFormatLite::TYPE_GROUP: {
        MessageLite* value =
            extension.is_repeated
                ? AddMessage(number, WireFormatLite::TYPE_GROUP,
                             *extension.message_info.prototype,
                             extension.descriptor)
                : MutableMessage(number, WireFormatLite::TYPE_GROUP,
                                 *extension.message_info.prototype,
                                 extension.descriptor);
        uint32 tag = (number << 3) + WireFormatLite::WIRETYPE_START_GROUP;
        return ctx->ParseGroup(value, ptr, tag);
      }

      case WireFormatLite::TYPE_MESSAGE: {
        MessageLite* value =
            extension.is_repeated
                ? AddMessage(number, WireFormatLite::TYPE_MESSAGE,
                             *extension.message_info.prototype,
                             extension.descriptor)
                : MutableMessage(number, WireFormatLite::TYPE_MESSAGE,
                                 *extension.message_info.prototype,
                                 extension.descriptor);
        return ctx->ParseMessage(value, ptr);
      }
    }
  }
  return ptr;
}

template <typename Msg, typename Metadata>
const char* ExtensionSet::ParseMessageSetItemTmpl(const char* ptr,
                                                  const Msg* containing_type,
                                                  Metadata* metadata,
                                                  internal::ParseContext* ctx) {
  std::string payload;
  uint32 type_id = 0;
  while (!ctx->Done(&ptr)) {
    uint32 tag = static_cast<uint8>(*ptr++);
    if (tag == WireFormatLite::kMessageSetTypeIdTag) {
      uint64 tmp;
      ptr = ParseVarint64Inline(ptr, &tmp);
      GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
      type_id = tmp;
      if (!payload.empty()) {
        ExtensionInfo extension;
        bool was_packed_on_wire;
        if (!FindExtension(2, type_id, containing_type, ctx, &extension,
                           &was_packed_on_wire)) {
          WriteLengthDelimited(type_id, payload,
                               metadata->mutable_unknown_fields());
        } else {
          MessageLite* value =
              extension.is_repeated
                  ? AddMessage(type_id, WireFormatLite::TYPE_MESSAGE,
                               *extension.message_info.prototype,
                               extension.descriptor)
                  : MutableMessage(type_id, WireFormatLite::TYPE_MESSAGE,
                                   *extension.message_info.prototype,
                                   extension.descriptor);

          const char* p;
          // We can't use regular parse from string as we have to track
          // proper recursion depth and descriptor pools.
          ParseContext tmp_ctx(ctx->depth(), false, &p, payload);
          tmp_ctx.data().pool = ctx->data().pool;
          tmp_ctx.data().factory = ctx->data().factory;
          GOOGLE_PROTOBUF_PARSER_ASSERT(value->_InternalParse(p, &tmp_ctx) &&
                                         tmp_ctx.EndedAtLimit());
        }
        type_id = 0;
      }
    } else if (tag == WireFormatLite::kMessageSetMessageTag) {
      if (type_id != 0) {
        ptr = ParseFieldMaybeLazily(static_cast<uint64>(type_id) * 8 + 2, ptr,
                                    containing_type, metadata, ctx);
        GOOGLE_PROTOBUF_PARSER_ASSERT(ptr != nullptr);
        type_id = 0;
      } else {
        int32 size = ReadSize(&ptr);
        GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
        ptr = ctx->ReadString(ptr, size, &payload);
        GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
      }
    } else {
      if (tag >= 128) {
        // Parse remainder of tag varint
        uint32 tmp;
        ptr = VarintParse<4>(ptr, &tmp);
        GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
        tag += (tmp - 1) << 7;
      }
      if (tag == 0 || (tag & 7) == 4) {
        ctx->SetLastTag(tag);
        return ptr;
      }
      ptr = ParseField(tag, ptr, containing_type, metadata, ctx);
      GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
    }
  }
  return ptr;
}
#endif  // GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_EXTENSION_SET_INL_H__
