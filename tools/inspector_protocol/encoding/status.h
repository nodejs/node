// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_STATUS_H_
#define INSPECTOR_PROTOCOL_ENCODING_STATUS_H_

#include <cstdint>

namespace inspector_protocol {
// Error codes.
enum class Error {
  OK = 0,
  // JSON parsing errors - json_parser.{h,cc}.
  JSON_PARSER_UNPROCESSED_INPUT_REMAINS = 0x01,
  JSON_PARSER_STACK_LIMIT_EXCEEDED = 0x02,
  JSON_PARSER_NO_INPUT = 0x03,
  JSON_PARSER_INVALID_TOKEN = 0x04,
  JSON_PARSER_INVALID_NUMBER = 0x05,
  JSON_PARSER_INVALID_STRING = 0x06,
  JSON_PARSER_UNEXPECTED_ARRAY_END = 0x07,
  JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED = 0x08,
  JSON_PARSER_STRING_LITERAL_EXPECTED = 0x09,
  JSON_PARSER_COLON_EXPECTED = 0x0a,
  JSON_PARSER_UNEXPECTED_OBJECT_END = 0x0b,
  JSON_PARSER_COMMA_OR_OBJECT_END_EXPECTED = 0x0c,
  JSON_PARSER_VALUE_EXPECTED = 0x0d,

  CBOR_INVALID_INT32 = 0x0e,
  CBOR_INVALID_DOUBLE = 0x0f,
  CBOR_INVALID_ENVELOPE = 0x10,
  CBOR_INVALID_STRING8 = 0x11,
  CBOR_INVALID_STRING16 = 0x12,
  CBOR_INVALID_BINARY = 0x13,
  CBOR_UNSUPPORTED_VALUE = 0x14,
  CBOR_NO_INPUT = 0x15,
  CBOR_INVALID_START_BYTE = 0x16,
  CBOR_UNEXPECTED_EOF_EXPECTED_VALUE = 0x17,
  CBOR_UNEXPECTED_EOF_IN_ARRAY = 0x18,
  CBOR_UNEXPECTED_EOF_IN_MAP = 0x19,
  CBOR_INVALID_MAP_KEY = 0x1a,
  CBOR_STACK_LIMIT_EXCEEDED = 0x1b,
  CBOR_STRING8_MUST_BE_7BIT = 0x1c,
  CBOR_TRAILING_JUNK = 0x1d,
  CBOR_MAP_START_EXPECTED = 0x1e,
};

// A status value with position that can be copied. The default status
// is OK. Usually, error status values should come with a valid position.
struct Status {
  static constexpr std::ptrdiff_t npos() { return -1; }

  bool ok() const { return error == Error::OK; }

  Error error = Error::OK;
  std::ptrdiff_t pos = npos();
  Status(Error error, std::ptrdiff_t pos) : error(error), pos(pos) {}
  Status() = default;
};
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_STATUS_H_
