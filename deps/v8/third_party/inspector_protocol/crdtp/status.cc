// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

namespace v8_crdtp {
// =============================================================================
// Status and Error codes
// =============================================================================

std::string Status::ToASCIIString() const {
  switch (error) {
    case Error::OK:
      return "OK";
    case Error::JSON_PARSER_UNPROCESSED_INPUT_REMAINS:
      return ToASCIIString("JSON: unprocessed input remains");
    case Error::JSON_PARSER_STACK_LIMIT_EXCEEDED:
      return ToASCIIString("JSON: stack limit exceeded");
    case Error::JSON_PARSER_NO_INPUT:
      return ToASCIIString("JSON: no input");
    case Error::JSON_PARSER_INVALID_TOKEN:
      return ToASCIIString("JSON: invalid token");
    case Error::JSON_PARSER_INVALID_NUMBER:
      return ToASCIIString("JSON: invalid number");
    case Error::JSON_PARSER_INVALID_STRING:
      return ToASCIIString("JSON: invalid string");
    case Error::JSON_PARSER_UNEXPECTED_ARRAY_END:
      return ToASCIIString("JSON: unexpected array end");
    case Error::JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED:
      return ToASCIIString("JSON: comma or array end expected");
    case Error::JSON_PARSER_STRING_LITERAL_EXPECTED:
      return ToASCIIString("JSON: string literal expected");
    case Error::JSON_PARSER_COLON_EXPECTED:
      return ToASCIIString("JSON: colon expected");
    case Error::JSON_PARSER_UNEXPECTED_MAP_END:
      return ToASCIIString("JSON: unexpected map end");
    case Error::JSON_PARSER_COMMA_OR_MAP_END_EXPECTED:
      return ToASCIIString("JSON: comma or map end expected");
    case Error::JSON_PARSER_VALUE_EXPECTED:
      return ToASCIIString("JSON: value expected");

    case Error::CBOR_INVALID_INT32:
      return ToASCIIString("CBOR: invalid int32");
    case Error::CBOR_INVALID_DOUBLE:
      return ToASCIIString("CBOR: invalid double");
    case Error::CBOR_INVALID_ENVELOPE:
      return ToASCIIString("CBOR: invalid envelope");
    case Error::CBOR_ENVELOPE_CONTENTS_LENGTH_MISMATCH:
      return ToASCIIString("CBOR: envelope contents length mismatch");
    case Error::CBOR_MAP_OR_ARRAY_EXPECTED_IN_ENVELOPE:
      return ToASCIIString("CBOR: map or array expected in envelope");
    case Error::CBOR_INVALID_STRING8:
      return ToASCIIString("CBOR: invalid string8");
    case Error::CBOR_INVALID_STRING16:
      return ToASCIIString("CBOR: invalid string16");
    case Error::CBOR_INVALID_BINARY:
      return ToASCIIString("CBOR: invalid binary");
    case Error::CBOR_UNSUPPORTED_VALUE:
      return ToASCIIString("CBOR: unsupported value");
    case Error::CBOR_NO_INPUT:
      return ToASCIIString("CBOR: no input");
    case Error::CBOR_INVALID_START_BYTE:
      return ToASCIIString("CBOR: invalid start byte");
    case Error::CBOR_UNEXPECTED_EOF_EXPECTED_VALUE:
      return ToASCIIString("CBOR: unexpected eof expected value");
    case Error::CBOR_UNEXPECTED_EOF_IN_ARRAY:
      return ToASCIIString("CBOR: unexpected eof in array");
    case Error::CBOR_UNEXPECTED_EOF_IN_MAP:
      return ToASCIIString("CBOR: unexpected eof in map");
    case Error::CBOR_INVALID_MAP_KEY:
      return ToASCIIString("CBOR: invalid map key");
    case Error::CBOR_STACK_LIMIT_EXCEEDED:
      return ToASCIIString("CBOR: stack limit exceeded");
    case Error::CBOR_TRAILING_JUNK:
      return ToASCIIString("CBOR: trailing junk");
    case Error::CBOR_MAP_START_EXPECTED:
      return ToASCIIString("CBOR: map start expected");
    case Error::CBOR_MAP_STOP_EXPECTED:
      return ToASCIIString("CBOR: map stop expected");
    case Error::CBOR_ARRAY_START_EXPECTED:
      return ToASCIIString("CBOR: array start expected");
    case Error::CBOR_ENVELOPE_SIZE_LIMIT_EXCEEDED:
      return ToASCIIString("CBOR: envelope size limit exceeded");

    case Error::BINDINGS_MANDATORY_FIELD_MISSING:
      return ToASCIIString("BINDINGS: mandatory field missing");
    case Error::BINDINGS_BOOL_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: bool value expected");
    case Error::BINDINGS_INT32_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: int32 value expected");
    case Error::BINDINGS_DOUBLE_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: double value expected");
    case Error::BINDINGS_STRING_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: string value expected");
    case Error::BINDINGS_STRING8_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: string8 value expected");
    case Error::BINDINGS_BINARY_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: binary value expected");
  }
  // Some compilers can't figure out that we can't get here.
  return "INVALID ERROR CODE";
}

std::string Status::ToASCIIString(const char* msg) const {
  return std::string(msg) + " at position " + std::to_string(pos);
}
}  // namespace v8_crdtp
