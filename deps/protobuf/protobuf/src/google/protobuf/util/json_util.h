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

// Utility functions to convert between protobuf binary format and proto3 JSON
// format.
#ifndef GOOGLE_PROTOBUF_UTIL_JSON_UTIL_H__
#define GOOGLE_PROTOBUF_UTIL_JSON_UTIL_H__

#include <google/protobuf/message.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/stubs/bytestream.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace io {
class ZeroCopyInputStream;
class ZeroCopyOutputStream;
}  // namespace io
namespace util {

struct JsonParseOptions {
  // Whether to ignore unknown JSON fields during parsing
  bool ignore_unknown_fields;

  // If true, when a lowercase enum value fails to parse, try convert it to
  // UPPER_CASE and see if it matches a valid enum.
  // WARNING: This option exists only to preserve legacy behavior. Avoid using
  // this option. If your enum needs to support different casing, consider using
  // allow_alias instead.
  bool case_insensitive_enum_parsing;

  JsonParseOptions()
      : ignore_unknown_fields(false),
        case_insensitive_enum_parsing(false) {}
};

struct JsonPrintOptions {
  // Whether to add spaces, line breaks and indentation to make the JSON output
  // easy to read.
  bool add_whitespace;
  // Whether to always print primitive fields. By default proto3 primitive
  // fields with default values will be omitted in JSON output. For example, an
  // int32 field set to 0 will be omitted. Set this flag to true will override
  // the default behavior and print primitive fields regardless of their values.
  bool always_print_primitive_fields;
  // Whether to always print enums as ints. By default they are rendered as
  // strings.
  bool always_print_enums_as_ints;
  // Whether to preserve proto field names
  bool preserve_proto_field_names;

  JsonPrintOptions()
      : add_whitespace(false),
        always_print_primitive_fields(false),
        always_print_enums_as_ints(false),
        preserve_proto_field_names(false) {}
};

// DEPRECATED. Use JsonPrintOptions instead.
typedef JsonPrintOptions JsonOptions;

// Converts from protobuf message to JSON and appends it to |output|. This is a
// simple wrapper of BinaryToJsonString(). It will use the DescriptorPool of the
// passed-in message to resolve Any types.
PROTOBUF_EXPORT util::Status MessageToJsonString(const Message& message,
                                                   std::string* output,
                                                   const JsonOptions& options);

inline util::Status MessageToJsonString(const Message& message,
                                          std::string* output) {
  return MessageToJsonString(message, output, JsonOptions());
}

// Converts from JSON to protobuf message. This is a simple wrapper of
// JsonStringToBinary(). It will use the DescriptorPool of the passed-in
// message to resolve Any types.
PROTOBUF_EXPORT util::Status JsonStringToMessage(
    StringPiece input, Message* message, const JsonParseOptions& options);

inline util::Status JsonStringToMessage(StringPiece input,
                                          Message* message) {
  return JsonStringToMessage(input, message, JsonParseOptions());
}

// Converts protobuf binary data to JSON.
// The conversion will fail if:
//   1. TypeResolver fails to resolve a type.
//   2. input is not valid protobuf wire format, or conflicts with the type
//      information returned by TypeResolver.
// Note that unknown fields will be discarded silently.
PROTOBUF_EXPORT util::Status BinaryToJsonStream(
    TypeResolver* resolver, const std::string& type_url,
    io::ZeroCopyInputStream* binary_input,
    io::ZeroCopyOutputStream* json_output, const JsonPrintOptions& options);

inline util::Status BinaryToJsonStream(
    TypeResolver* resolver, const std::string& type_url,
    io::ZeroCopyInputStream* binary_input,
    io::ZeroCopyOutputStream* json_output) {
  return BinaryToJsonStream(resolver, type_url, binary_input, json_output,
                            JsonPrintOptions());
}

PROTOBUF_EXPORT util::Status BinaryToJsonString(
    TypeResolver* resolver, const std::string& type_url,
    const std::string& binary_input, std::string* json_output,
    const JsonPrintOptions& options);

inline util::Status BinaryToJsonString(TypeResolver* resolver,
                                         const std::string& type_url,
                                         const std::string& binary_input,
                                         std::string* json_output) {
  return BinaryToJsonString(resolver, type_url, binary_input, json_output,
                            JsonPrintOptions());
}

// Converts JSON data to protobuf binary format.
// The conversion will fail if:
//   1. TypeResolver fails to resolve a type.
//   2. input is not valid JSON format, or conflicts with the type
//      information returned by TypeResolver.
PROTOBUF_EXPORT util::Status JsonToBinaryStream(
    TypeResolver* resolver, const std::string& type_url,
    io::ZeroCopyInputStream* json_input,
    io::ZeroCopyOutputStream* binary_output, const JsonParseOptions& options);

inline util::Status JsonToBinaryStream(
    TypeResolver* resolver, const std::string& type_url,
    io::ZeroCopyInputStream* json_input,
    io::ZeroCopyOutputStream* binary_output) {
  return JsonToBinaryStream(resolver, type_url, json_input, binary_output,
                            JsonParseOptions());
}

PROTOBUF_EXPORT util::Status JsonToBinaryString(
    TypeResolver* resolver, const std::string& type_url,
    StringPiece json_input, std::string* binary_output,
    const JsonParseOptions& options);

inline util::Status JsonToBinaryString(TypeResolver* resolver,
                                         const std::string& type_url,
                                         StringPiece json_input,
                                         std::string* binary_output) {
  return JsonToBinaryString(resolver, type_url, json_input, binary_output,
                            JsonParseOptions());
}

namespace internal {
// Internal helper class. Put in the header so we can write unit-tests for it.
class PROTOBUF_EXPORT ZeroCopyStreamByteSink : public strings::ByteSink {
 public:
  explicit ZeroCopyStreamByteSink(io::ZeroCopyOutputStream* stream)
      : stream_(stream), buffer_(NULL), buffer_size_(0) {}
  ~ZeroCopyStreamByteSink();

  void Append(const char* bytes, size_t len) override;

 private:
  io::ZeroCopyOutputStream* stream_;
  void* buffer_;
  int buffer_size_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ZeroCopyStreamByteSink);
};
}  // namespace internal

}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_JSON_UTIL_H__
