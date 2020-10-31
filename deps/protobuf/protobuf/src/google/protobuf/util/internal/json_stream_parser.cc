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

#include <google/protobuf/util/internal/json_stream_parser.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/util/internal/object_writer.h>
#include <google/protobuf/util/internal/json_escaping.h>
#include <google/protobuf/stubs/mathlimits.h>


namespace google {
namespace protobuf {
namespace util {

// Allow these symbols to be referenced as util::Status, util::error::* in
// this file.
using util::Status;
namespace error {
using util::error::CANCELLED;
using util::error::INTERNAL;
using util::error::INVALID_ARGUMENT;
}  // namespace error

namespace converter {

// Number of digits in an escaped UTF-16 code unit ('\\' 'u' X X X X)
static const int kUnicodeEscapedLength = 6;

static const int kDefaultMaxRecursionDepth = 100;

// Length of the true, false, and null literals.
static const int true_len = strlen("true");
static const int false_len = strlen("false");
static const int null_len = strlen("null");

inline bool IsLetter(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_') ||
         (c == '$');
}

inline bool IsAlphanumeric(char c) {
  return IsLetter(c) || ('0' <= c && c <= '9');
}

static bool ConsumeKey(StringPiece* input, StringPiece* key) {
  if (input->empty() || !IsLetter((*input)[0])) return false;
  int len = 1;
  for (; len < input->size(); ++len) {
    if (!IsAlphanumeric((*input)[len])) {
      break;
    }
  }
  *key = StringPiece(input->data(), len);
  *input = StringPiece(input->data() + len, input->size() - len);
  return true;
}

static bool MatchKey(StringPiece input) {
  return !input.empty() && IsLetter(input[0]);
}

JsonStreamParser::JsonStreamParser(ObjectWriter* ow)
    : ow_(ow),
      stack_(),
      leftover_(),
      json_(),
      p_(),
      key_(),
      key_storage_(),
      finishing_(false),
      parsed_(),
      parsed_storage_(),
      string_open_(0),
      chunk_storage_(),
      coerce_to_utf8_(false),
      allow_empty_null_(false),
      loose_float_number_conversion_(false),
      recursion_depth_(0),
      max_recursion_depth_(kDefaultMaxRecursionDepth) {
  // Initialize the stack with a single value to be parsed.
  stack_.push(VALUE);
}

JsonStreamParser::~JsonStreamParser() {}


util::Status JsonStreamParser::Parse(StringPiece json) {
  StringPiece chunk = json;
  // If we have leftovers from a previous chunk, append the new chunk to it
  // and create a new StringPiece pointing at the string's data. This could
  // be large but we rely on the chunks to be small, assuming they are
  // fragments of a Cord.
  if (!leftover_.empty()) {
    // Don't point chunk to leftover_ because leftover_ will be updated in
    // ParseChunk(chunk).
    chunk_storage_.swap(leftover_);
    StrAppend(&chunk_storage_, json);
    chunk = StringPiece(chunk_storage_);
  }

  // Find the structurally valid UTF8 prefix and parse only that.
  int n = internal::UTF8SpnStructurallyValid(chunk);
  if (n > 0) {
    util::Status status = ParseChunk(chunk.substr(0, n));

    // Any leftover characters are stashed in leftover_ for later parsing when
    // there is more data available.
    StrAppend(&leftover_, chunk.substr(n));
    return status;
  } else {
    leftover_.assign(chunk.data(), chunk.size());
    return util::Status();
  }
}

util::Status JsonStreamParser::FinishParse() {
  // If we do not expect anything and there is nothing left to parse we're all
  // done.
  if (stack_.empty() && leftover_.empty()) {
    return util::Status();
  }

  // Storage for UTF8-coerced string.
  std::unique_ptr<char[]> utf8;
  if (coerce_to_utf8_) {
    utf8.reset(new char[leftover_.size()]);
    char* coerced = internal::UTF8CoerceToStructurallyValid(leftover_, utf8.get(), ' ');
    p_ = json_ = StringPiece(coerced, leftover_.size());
  } else {
    p_ = json_ = leftover_;
    if (!internal::IsStructurallyValidUTF8(leftover_)) {
      return ReportFailure("Encountered non UTF-8 code points.");
    }
  }

  // Parse the remainder in finishing mode, which reports errors for things like
  // unterminated strings or unknown tokens that would normally be retried.
  finishing_ = true;
  util::Status result = RunParser();
  if (result.ok()) {
    SkipWhitespace();
    if (!p_.empty()) {
      result = ReportFailure("Parsing terminated before end of input.");
    }
  }
  return result;
}

util::Status JsonStreamParser::ParseChunk(StringPiece chunk) {
  // Do not do any work if the chunk is empty.
  if (chunk.empty()) return util::Status();

  p_ = json_ = chunk;

  finishing_ = false;
  util::Status result = RunParser();
  if (!result.ok()) return result;

  SkipWhitespace();
  if (p_.empty()) {
    // If we parsed everything we had, clear the leftover.
    leftover_.clear();
  } else {
    // If we do not expect anything i.e. stack is empty, and we have non-empty
    // string left to parse, we report an error.
    if (stack_.empty()) {
      return ReportFailure("Parsing terminated before end of input.");
    }
    // If we expect future data i.e. stack is non-empty, and we have some
    // unparsed data left, we save it for later parse.
    leftover_ = std::string(p_);
  }
  return util::Status();
}

util::Status JsonStreamParser::RunParser() {
  while (!stack_.empty()) {
    ParseType type = stack_.top();
    TokenType t = (string_open_ == 0) ? GetNextTokenType() : BEGIN_STRING;
    stack_.pop();
    util::Status result;
    switch (type) {
      case VALUE:
        result = ParseValue(t);
        break;

      case OBJ_MID:
        result = ParseObjectMid(t);
        break;

      case ENTRY:
        result = ParseEntry(t);
        break;

      case ENTRY_MID:
        result = ParseEntryMid(t);
        break;

      case ARRAY_VALUE:
        result = ParseArrayValue(t);
        break;

      case ARRAY_MID:
        result = ParseArrayMid(t);
        break;

      default:
        result = util::Status(util::error::INTERNAL,
                              StrCat("Unknown parse type: ", type));
        break;
    }
    if (!result.ok()) {
      // If we were cancelled, save our state and try again later.
      if (!finishing_ &&
          result == util::Status(util::error::CANCELLED, "")) {
        stack_.push(type);
        // If we have a key we still need to render, make sure to save off the
        // contents in our own storage.
        if (!key_.empty() && key_storage_.empty()) {
          StrAppend(&key_storage_, key_);
          key_ = StringPiece(key_storage_);
        }
        result = util::Status();
      }
      return result;
    }
  }
  return util::Status();
}

util::Status JsonStreamParser::ParseValue(TokenType type) {
  switch (type) {
    case BEGIN_OBJECT:
      return HandleBeginObject();
    case BEGIN_ARRAY:
      return HandleBeginArray();
    case BEGIN_STRING:
      return ParseString();
    case BEGIN_NUMBER:
      return ParseNumber();
    case BEGIN_TRUE:
      return ParseTrue();
    case BEGIN_FALSE:
      return ParseFalse();
    case BEGIN_NULL:
      return ParseNull();
    case UNKNOWN:
      return ReportUnknown("Expected a value.");
    default: {
      if (allow_empty_null_ && IsEmptyNullAllowed(type)) {
        return ParseEmptyNull();
      }

      // Special case for having been cut off while parsing, wait for more data.
      // This handles things like 'fals' being at the end of the string, we
      // don't know if the next char would be e, completing it, or something
      // else, making it invalid.
      if (!finishing_ && p_.length() < false_len) {
        return util::Status(util::error::CANCELLED, "");
      }
      return ReportFailure("Unexpected token.");
    }
  }
}

util::Status JsonStreamParser::ParseString() {
  util::Status result = ParseStringHelper();
  if (result.ok()) {
    ow_->RenderString(key_, parsed_);
    key_ = StringPiece();
    parsed_ = StringPiece();
    parsed_storage_.clear();
  }
  return result;
}

util::Status JsonStreamParser::ParseStringHelper() {
  // If we haven't seen the start quote, grab it and remember it for later.
  if (string_open_ == 0) {
    string_open_ = *p_.data();
    GOOGLE_DCHECK(string_open_ == '\"' || string_open_ == '\'');
    Advance();
  }
  // Track where we last copied data from so we can minimize copying.
  const char* last = p_.data();
  while (!p_.empty()) {
    const char* data = p_.data();
    if (*data == '\\') {
      // We're about to handle an escape, copy all bytes from last to data.
      if (last < data) {
        parsed_storage_.append(last, data - last);
      }
      // If we ran out of string after the \, cancel or report an error
      // depending on if we expect more data later.
      if (p_.length() == 1) {
        if (!finishing_) {
          return util::Status(util::error::CANCELLED, "");
        }
        return ReportFailure("Closing quote expected in string.");
      }
      // Parse a unicode escape if we found \u in the string.
      if (data[1] == 'u') {
        util::Status result = ParseUnicodeEscape();
        if (!result.ok()) {
          return result;
        }
        // Move last pointer past the unicode escape and continue.
        last = p_.data();
        continue;
      }
      // Handle the standard set of backslash-escaped characters.
      switch (data[1]) {
        case 'b':
          parsed_storage_.push_back('\b');
          break;
        case 'f':
          parsed_storage_.push_back('\f');
          break;
        case 'n':
          parsed_storage_.push_back('\n');
          break;
        case 'r':
          parsed_storage_.push_back('\r');
          break;
        case 't':
          parsed_storage_.push_back('\t');
          break;
        case 'v':
          parsed_storage_.push_back('\v');
          break;
        default:
          parsed_storage_.push_back(data[1]);
      }
      // We handled two characters, so advance past them and continue.
      p_.remove_prefix(2);
      last = p_.data();
      continue;
    }
    // If we found the closing quote note it, advance past it, and return.
    if (*data == string_open_) {
      // If we didn't copy anything, reuse the input buffer.
      if (parsed_storage_.empty()) {
        parsed_ = StringPiece(last, data - last);
      } else {
        if (last < data) {
          parsed_storage_.append(last, data - last);
        }
        parsed_ = StringPiece(parsed_storage_);
      }
      // Clear the quote char so next time we try to parse a string we'll
      // start fresh.
      string_open_ = 0;
      Advance();
      return util::Status();
    }
    // Normal character, just advance past it.
    Advance();
  }
  // If we ran out of characters, copy over what we have so far.
  if (last < p_.data()) {
    parsed_storage_.append(last, p_.data() - last);
  }
  // If we didn't find the closing quote but we expect more data, cancel for now
  if (!finishing_) {
    return util::Status(util::error::CANCELLED, "");
  }
  // End of string reached without a closing quote, report an error.
  string_open_ = 0;
  return ReportFailure("Closing quote expected in string.");
}

// Converts a unicode escaped character to a decimal value stored in a char32
// for use in UTF8 encoding utility.  We assume that str begins with \uhhhh and
// convert that from the hex number to a decimal value.
//
// There are some security exploits with UTF-8 that we should be careful of:
//   - http://www.unicode.org/reports/tr36/#UTF-8_Exploit
//   - http://sites/intl-eng/design-guide/core-application
util::Status JsonStreamParser::ParseUnicodeEscape() {
  if (p_.length() < kUnicodeEscapedLength) {
    if (!finishing_) {
      return util::Status(util::error::CANCELLED, "");
    }
    return ReportFailure("Illegal hex string.");
  }
  GOOGLE_DCHECK_EQ('\\', p_.data()[0]);
  GOOGLE_DCHECK_EQ('u', p_.data()[1]);
  uint32 code = 0;
  for (int i = 2; i < kUnicodeEscapedLength; ++i) {
    if (!isxdigit(p_.data()[i])) {
      return ReportFailure("Invalid escape sequence.");
    }
    code = (code << 4) + hex_digit_to_int(p_.data()[i]);
  }
  if (code >= JsonEscaping::kMinHighSurrogate &&
      code <= JsonEscaping::kMaxHighSurrogate) {
    if (p_.length() < 2 * kUnicodeEscapedLength) {
      if (!finishing_) {
        return util::Status(util::error::CANCELLED, "");
      }
      if (!coerce_to_utf8_) {
        return ReportFailure("Missing low surrogate.");
      }
    } else if (p_.data()[kUnicodeEscapedLength] == '\\' &&
               p_.data()[kUnicodeEscapedLength + 1] == 'u') {
      uint32 low_code = 0;
      for (int i = kUnicodeEscapedLength + 2; i < 2 * kUnicodeEscapedLength;
           ++i) {
        if (!isxdigit(p_.data()[i])) {
          return ReportFailure("Invalid escape sequence.");
        }
        low_code = (low_code << 4) + hex_digit_to_int(p_.data()[i]);
      }
      if (low_code >= JsonEscaping::kMinLowSurrogate &&
          low_code <= JsonEscaping::kMaxLowSurrogate) {
        // Convert UTF-16 surrogate pair to 21-bit Unicode codepoint.
        code = (((code & 0x3FF) << 10) | (low_code & 0x3FF)) +
               JsonEscaping::kMinSupplementaryCodePoint;
        // Advance past the first code unit escape.
        p_.remove_prefix(kUnicodeEscapedLength);
      } else if (!coerce_to_utf8_) {
        return ReportFailure("Invalid low surrogate.");
      }
    } else if (!coerce_to_utf8_) {
      return ReportFailure("Missing low surrogate.");
    }
  }
  if (!coerce_to_utf8_ && !IsValidCodePoint(code)) {
    return ReportFailure("Invalid unicode code point.");
  }
  char buf[UTFmax];
  int len = EncodeAsUTF8Char(code, buf);
  // Advance past the [final] code unit escape.
  p_.remove_prefix(kUnicodeEscapedLength);
  parsed_storage_.append(buf, len);
  return util::Status();
}

util::Status JsonStreamParser::ParseNumber() {
  NumberResult number;
  util::Status result = ParseNumberHelper(&number);
  if (result.ok()) {
    switch (number.type) {
      case NumberResult::DOUBLE:
        ow_->RenderDouble(key_, number.double_val);
        key_ = StringPiece();
        break;

      case NumberResult::INT:
        ow_->RenderInt64(key_, number.int_val);
        key_ = StringPiece();
        break;

      case NumberResult::UINT:
        ow_->RenderUint64(key_, number.uint_val);
        key_ = StringPiece();
        break;

      default:
        return ReportFailure("Unable to parse number.");
    }
  }
  return result;
}

util::Status JsonStreamParser::ParseDoubleHelper(const std::string& number,
                                                 NumberResult* result) {
  if (!safe_strtod(number, &result->double_val)) {
    return ReportFailure("Unable to parse number.");
  }
  if (!loose_float_number_conversion_ &&
      !MathLimits<double>::IsFinite(result->double_val)) {
    return ReportFailure("Number exceeds the range of double.");
  }
  result->type = NumberResult::DOUBLE;
  return util::Status();
}

util::Status JsonStreamParser::ParseNumberHelper(NumberResult* result) {
  const char* data = p_.data();
  int length = p_.length();

  // Look for the first non-numeric character, or the end of the string.
  int index = 0;
  bool floating = false;
  bool negative = data[index] == '-';
  // Find the first character that cannot be part of the number. Along the way
  // detect if the number needs to be parsed as a double.
  // Note that this restricts numbers to the JSON specification, so for example
  // we do not support hex or octal notations.
  for (; index < length; ++index) {
    char c = data[index];
    if (isdigit(c)) continue;
    if (c == '.' || c == 'e' || c == 'E') {
      floating = true;
      continue;
    }
    if (c == '+' || c == '-' || c == 'x') continue;
    // Not a valid number character, break out.
    break;
  }

  // If the entire input is a valid number, and we may have more content in the
  // future, we abort for now and resume when we know more.
  if (index == length && !finishing_) {
    return util::Status(util::error::CANCELLED, "");
  }

  // Create a string containing just the number, so we can use safe_strtoX
  std::string number = std::string(p_.substr(0, index));

  // Floating point number, parse as a double.
  if (floating) {
    util::Status status = ParseDoubleHelper(number, result);
    if (status.ok()) {
      p_.remove_prefix(index);
    }
    return status;
  }

  // Positive non-floating point number, parse as a uint64.
  if (!negative) {
    // Octal/Hex numbers are not valid JSON values.
    if (number.length() >= 2 && number[0] == '0') {
      return ReportFailure("Octal/hex numbers are not valid JSON values.");
    }
    if (safe_strtou64(number, &result->uint_val)) {
      result->type = NumberResult::UINT;
      p_.remove_prefix(index);
      return util::Status();
    } else {
      // If the value is too large, parse it as double.
      util::Status status = ParseDoubleHelper(number, result);
      if (status.ok()) {
        p_.remove_prefix(index);
      }
      return status;
    }
  }

  // Octal/Hex numbers are not valid JSON values.
  if (number.length() >= 3 && number[1] == '0') {
    return ReportFailure("Octal/hex numbers are not valid JSON values.");
  }
  // Negative non-floating point number, parse as an int64.
  if (safe_strto64(number, &result->int_val)) {
    result->type = NumberResult::INT;
    p_.remove_prefix(index);
    return util::Status();
  } else {
    // If the value is too large, parse it as double.
    util::Status status = ParseDoubleHelper(number, result);
    if (status.ok()) {
      p_.remove_prefix(index);
    }
    return status;
  }
}

util::Status JsonStreamParser::HandleBeginObject() {
  GOOGLE_DCHECK_EQ('{', *p_.data());
  Advance();
  ow_->StartObject(key_);
  auto status = IncrementRecursionDepth(key_);
  if (!status.ok()) {
    return status;
  }
  key_ = StringPiece();
  stack_.push(ENTRY);
  return util::Status();
}

util::Status JsonStreamParser::ParseObjectMid(TokenType type) {
  if (type == UNKNOWN) {
    return ReportUnknown("Expected , or } after key:value pair.");
  }

  // Object is complete, advance past the comma and render the EndObject.
  if (type == END_OBJECT) {
    Advance();
    ow_->EndObject();
    --recursion_depth_;
    return util::Status();
  }
  // Found a comma, advance past it and get ready for an entry.
  if (type == VALUE_SEPARATOR) {
    Advance();
    stack_.push(ENTRY);
    return util::Status();
  }
  // Illegal token after key:value pair.
  return ReportFailure("Expected , or } after key:value pair.");
}

util::Status JsonStreamParser::ParseEntry(TokenType type) {
  if (type == UNKNOWN) {
    return ReportUnknown("Expected an object key or }.");
  }

  // Close the object and return. This allows for trailing commas.
  if (type == END_OBJECT) {
    ow_->EndObject();
    Advance();
    --recursion_depth_;
    return util::Status();
  }

  util::Status result;
  if (type == BEGIN_STRING) {
    // Key is a string (standard JSON), parse it and store the string.
    result = ParseStringHelper();
    if (result.ok()) {
      key_storage_.clear();
      if (!parsed_storage_.empty()) {
        parsed_storage_.swap(key_storage_);
        key_ = StringPiece(key_storage_);
      } else {
        key_ = parsed_;
      }
      parsed_ = StringPiece();
    }
  } else if (type == BEGIN_KEY) {
    // Key is a bare key (back compat), create a StringPiece pointing to it.
    result = ParseKey();
  } else {
    // Unknown key type, report an error.
    result = ReportFailure("Expected an object key or }.");
  }
  // On success we next expect an entry mid ':' then an object mid ',' or '}'
  if (result.ok()) {
    stack_.push(OBJ_MID);
    stack_.push(ENTRY_MID);
  }
  return result;
}

util::Status JsonStreamParser::ParseEntryMid(TokenType type) {
  if (type == UNKNOWN) {
    return ReportUnknown("Expected : between key:value pair.");
  }
  if (type == ENTRY_SEPARATOR) {
    Advance();
    stack_.push(VALUE);
    return util::Status();
  }
  return ReportFailure("Expected : between key:value pair.");
}

util::Status JsonStreamParser::HandleBeginArray() {
  GOOGLE_DCHECK_EQ('[', *p_.data());
  Advance();
  ow_->StartList(key_);
  key_ = StringPiece();
  stack_.push(ARRAY_VALUE);
  return util::Status();
}

util::Status JsonStreamParser::ParseArrayValue(TokenType type) {
  if (type == UNKNOWN) {
    return ReportUnknown("Expected a value or ] within an array.");
  }

  if (type == END_ARRAY) {
    ow_->EndList();
    Advance();
    return util::Status();
  }

  // The ParseValue call may push something onto the stack so we need to make
  // sure an ARRAY_MID is after it, so we push it on now. Also, the parsing of
  // empty-null array value is relying on this ARRAY_MID token.
  stack_.push(ARRAY_MID);
  util::Status result = ParseValue(type);
  if (result == util::Status(util::error::CANCELLED, "")) {
    // If we were cancelled, pop back off the ARRAY_MID so we don't try to
    // push it on again when we try over.
    stack_.pop();
  }
  return result;
}

util::Status JsonStreamParser::ParseArrayMid(TokenType type) {
  if (type == UNKNOWN) {
    return ReportUnknown("Expected , or ] after array value.");
  }

  if (type == END_ARRAY) {
    ow_->EndList();
    Advance();
    return util::Status();
  }

  // Found a comma, advance past it and expect an array value next.
  if (type == VALUE_SEPARATOR) {
    Advance();
    stack_.push(ARRAY_VALUE);
    return util::Status();
  }
  // Illegal token after array value.
  return ReportFailure("Expected , or ] after array value.");
}

util::Status JsonStreamParser::ParseTrue() {
  ow_->RenderBool(key_, true);
  key_ = StringPiece();
  p_.remove_prefix(true_len);
  return util::Status();
}

util::Status JsonStreamParser::ParseFalse() {
  ow_->RenderBool(key_, false);
  key_ = StringPiece();
  p_.remove_prefix(false_len);
  return util::Status();
}

util::Status JsonStreamParser::ParseNull() {
  ow_->RenderNull(key_);
  key_ = StringPiece();
  p_.remove_prefix(null_len);
  return util::Status();
}

util::Status JsonStreamParser::ParseEmptyNull() {
  ow_->RenderNull(key_);
  key_ = StringPiece();
  return util::Status();
}

bool JsonStreamParser::IsEmptyNullAllowed(TokenType type) {
  if (stack_.empty()) return false;
  return (stack_.top() == ARRAY_MID && type == VALUE_SEPARATOR) ||
         stack_.top() == OBJ_MID;
}

util::Status JsonStreamParser::ReportFailure(StringPiece message) {
  static const int kContextLength = 20;
  const char* p_start = p_.data();
  const char* json_start = json_.data();
  const char* begin = std::max(p_start - kContextLength, json_start);
  const char* end =
      std::min(p_start + kContextLength, json_start + json_.size());
  StringPiece segment(begin, end - begin);
  std::string location(p_start - begin, ' ');
  location.push_back('^');
  return util::Status(util::error::INVALID_ARGUMENT,
                      StrCat(message, "\n", segment, "\n", location));
}

util::Status JsonStreamParser::ReportUnknown(StringPiece message) {
  // If we aren't finishing the parse, cancel parsing and try later.
  if (!finishing_) {
    return util::Status(util::error::CANCELLED, "");
  }
  if (p_.empty()) {
    return ReportFailure(StrCat("Unexpected end of string. ", message));
  }
  return ReportFailure(message);
}

util::Status JsonStreamParser::IncrementRecursionDepth(
    StringPiece key) const {
  if (++recursion_depth_ > max_recursion_depth_) {
    return Status(
        util::error::INVALID_ARGUMENT,
        StrCat("Message too deep. Max recursion depth reached for key '",
                     key, "'"));
  }
  return util::Status();
}

void JsonStreamParser::SkipWhitespace() {
  while (!p_.empty() && ascii_isspace(*p_.data())) {
    Advance();
  }
}

void JsonStreamParser::Advance() {
  // Advance by moving one UTF8 character while making sure we don't go beyond
  // the length of StringPiece.
  p_.remove_prefix(std::min<int>(
      p_.length(), UTF8FirstLetterNumBytes(p_.data(), p_.length())));
}

util::Status JsonStreamParser::ParseKey() {
  StringPiece original = p_;
  if (!ConsumeKey(&p_, &key_)) {
    return ReportFailure("Invalid key or variable name.");
  }
  // If we consumed everything but expect more data, reset p_ and cancel since
  // we can't know if the key was complete or not.
  if (!finishing_ && p_.empty()) {
    p_ = original;
    return util::Status(util::error::CANCELLED, "");
  }
  // Since we aren't using the key storage, clear it out.
  key_storage_.clear();
  return util::Status();
}

JsonStreamParser::TokenType JsonStreamParser::GetNextTokenType() {
  SkipWhitespace();

  int size = p_.size();
  if (size == 0) {
    // If we ran out of data, report unknown and we'll place the previous parse
    // type onto the stack and try again when we have more data.
    return UNKNOWN;
  }
  // TODO(sven): Split this method based on context since different contexts
  // support different tokens. Would slightly speed up processing?
  const char* data = p_.data();
  if (*data == '\"' || *data == '\'') return BEGIN_STRING;
  if (*data == '-' || ('0' <= *data && *data <= '9')) {
    return BEGIN_NUMBER;
  }
  if (size >= true_len && !strncmp(data, "true", true_len)) {
    return BEGIN_TRUE;
  }
  if (size >= false_len && !strncmp(data, "false", false_len)) {
    return BEGIN_FALSE;
  }
  if (size >= null_len && !strncmp(data, "null", null_len)) {
    return BEGIN_NULL;
  }
  if (*data == '{') return BEGIN_OBJECT;
  if (*data == '}') return END_OBJECT;
  if (*data == '[') return BEGIN_ARRAY;
  if (*data == ']') return END_ARRAY;
  if (*data == ':') return ENTRY_SEPARATOR;
  if (*data == ',') return VALUE_SEPARATOR;
  if (MatchKey(p_)) {
    return BEGIN_KEY;
  }

  // We don't know that we necessarily have an invalid token here, just that we
  // can't parse what we have so far. So we don't report an error and just
  // return UNKNOWN so we can try again later when we have more data, or if we
  // finish and we have leftovers.
  return UNKNOWN;
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
