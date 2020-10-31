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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_STREAM_PARSER_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_STREAM_PARSER_H__

#include <stack>
#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/stubs/status.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class ObjectWriter;

// A JSON parser that can parse a stream of JSON chunks rather than needing the
// entire JSON string up front. It is a modified version of the parser in
// //net/proto/json/json-parser.h that has been changed in the following ways:
// - Changed from recursion to an explicit stack to allow resumption
// - Added support for int64 and uint64 numbers
// - Removed support for octal and decimal escapes
// - Removed support for numeric keys
// - Removed support for functions (javascript)
// - Removed some lax-comma support (but kept trailing comma support)
// - Writes directly to an ObjectWriter rather than using subclassing
//
// Here is an example usage:
// JsonStreamParser parser(ow_.get());
// util::Status result = parser.Parse(chunk1);
// result.Update(parser.Parse(chunk2));
// result.Update(parser.FinishParse());
// GOOGLE_DCHECK(result.ok()) << "Failed to parse JSON";
//
// This parser is thread-compatible as long as only one thread is calling a
// Parse() method at a time.
class PROTOBUF_EXPORT JsonStreamParser {
 public:
  // Creates a JsonStreamParser that will write to the given ObjectWriter.
  explicit JsonStreamParser(ObjectWriter* ow);
  virtual ~JsonStreamParser();

  // Parses a UTF-8 encoded JSON string from a StringPiece.
  util::Status Parse(StringPiece json);


  // Finish parsing the JSON string.
  util::Status FinishParse();


  // Sets the max recursion depth of JSON message to be deserialized. JSON
  // messages over this depth will fail to be deserialized.
  // Default value is 100.
  void set_max_recursion_depth(int max_depth) {
    max_recursion_depth_ = max_depth;
  }

 private:
  friend class JsonStreamParserTest;
  // Return the current recursion depth.
  int recursion_depth() { return recursion_depth_; }

  enum TokenType {
    BEGIN_STRING,     // " or '
    BEGIN_NUMBER,     // - or digit
    BEGIN_TRUE,       // true
    BEGIN_FALSE,      // false
    BEGIN_NULL,       // null
    BEGIN_OBJECT,     // {
    END_OBJECT,       // }
    BEGIN_ARRAY,      // [
    END_ARRAY,        // ]
    ENTRY_SEPARATOR,  // :
    VALUE_SEPARATOR,  // ,
    BEGIN_KEY,        // letter, _, $ or digit.  Must begin with non-digit
    UNKNOWN           // Unknown token or we ran out of the stream.
  };

  enum ParseType {
    VALUE,        // Expects a {, [, true, false, null, string or number
    OBJ_MID,      // Expects a ',' or }
    ENTRY,        // Expects a key or }
    ENTRY_MID,    // Expects a :
    ARRAY_VALUE,  // Expects a value or ]
    ARRAY_MID     // Expects a ',' or ]
  };

  // Holds the result of parsing a number
  struct NumberResult {
    enum Type { DOUBLE, INT, UINT };
    Type type;
    union {
      double double_val;
      int64 int_val;
      uint64 uint_val;
    };
  };

  // Parses a single chunk of JSON, returning an error if the JSON was invalid.
  util::Status ParseChunk(StringPiece json);

  // Runs the parser based on stack_ and p_, until the stack is empty or p_ runs
  // out of data. If we unexpectedly run out of p_ we push the latest back onto
  // the stack and return.
  util::Status RunParser();

  // Parses a value from p_ and writes it to ow_.
  // A value may be an object, array, true, false, null, string or number.
  util::Status ParseValue(TokenType type);

  // Parses a string and writes it out to the ow_.
  util::Status ParseString();

  // Parses a string, storing the result in parsed_.
  util::Status ParseStringHelper();

  // This function parses unicode escape sequences in strings. It returns an
  // error when there's a parsing error, either the size is not the expected
  // size or a character is not a hex digit.  When it returns str will contain
  // what has been successfully parsed so far.
  util::Status ParseUnicodeEscape();

  // Expects p_ to point to a JSON number, writes the number to the writer using
  // the appropriate Render method based on the type of number.
  util::Status ParseNumber();

  // Parse a number into a NumberResult, reporting an error if no number could
  // be parsed. This method will try to parse into a uint64, int64, or double
  // based on whether the number was positive or negative or had a decimal
  // component.
  util::Status ParseNumberHelper(NumberResult* result);

  // Parse a number as double into a NumberResult.
  util::Status ParseDoubleHelper(const std::string& number,
                                   NumberResult* result);

  // Handles a { during parsing of a value.
  util::Status HandleBeginObject();

  // Parses from the ENTRY state.
  util::Status ParseEntry(TokenType type);

  // Parses from the ENTRY_MID state.
  util::Status ParseEntryMid(TokenType type);

  // Parses from the OBJ_MID state.
  util::Status ParseObjectMid(TokenType type);

  // Handles a [ during parsing of a value.
  util::Status HandleBeginArray();

  // Parses from the ARRAY_VALUE state.
  util::Status ParseArrayValue(TokenType type);

  // Parses from the ARRAY_MID state.
  util::Status ParseArrayMid(TokenType type);

  // Expects p_ to point to an unquoted literal
  util::Status ParseTrue();
  util::Status ParseFalse();
  util::Status ParseNull();
  util::Status ParseEmptyNull();

  // Whether an empty-null is allowed in the current state.
  bool IsEmptyNullAllowed(TokenType type);

  // Report a failure as a util::Status.
  util::Status ReportFailure(StringPiece message);

  // Report a failure due to an UNKNOWN token type. We check if we hit the
  // end of the stream and if we're finishing or not to detect what type of
  // status to return in this case.
  util::Status ReportUnknown(StringPiece message);

  // Helper function to check recursion depth and increment it. It will return
  // Status::OK if the current depth is allowed. Otherwise an error is returned.
  // key is used for error reporting.
  util::Status IncrementRecursionDepth(StringPiece key) const;

  // Advance p_ past all whitespace or until the end of the string.
  void SkipWhitespace();

  // Advance p_ one UTF-8 character
  void Advance();

  // Expects p_ to point to the beginning of a key.
  util::Status ParseKey();

  // Return the type of the next token at p_.
  TokenType GetNextTokenType();

  // The object writer to write parse events to.
  ObjectWriter* ow_;

  // The stack of parsing we still need to do. When the stack runs empty we will
  // have parsed a single value from the root (e.g. an object or list).
  std::stack<ParseType> stack_;

  // Contains any leftover text from a previous chunk that we weren't able to
  // fully parse, for example the start of a key or number.
  std::string leftover_;

  // The current chunk of JSON being parsed. Primarily used for providing
  // context during error reporting.
  StringPiece json_;

  // A pointer within the current JSON being parsed, used to track location.
  StringPiece p_;

  // Stores the last key read, as we separate parsing of keys and values.
  StringPiece key_;

  // Storage for key_ if we need to keep ownership, for example between chunks
  // or if the key was unescaped from a JSON string.
  std::string key_storage_;

  // True during the FinishParse() call, so we know that any errors are fatal.
  // For example an unterminated string will normally result in cancelling and
  // trying during the next chunk, but during FinishParse() it is an error.
  bool finishing_;

  // String we parsed during a call to ParseStringHelper().
  StringPiece parsed_;

  // Storage for the string we parsed. This may be empty if the string was able
  // to be parsed directly from the input.
  std::string parsed_storage_;

  // The character that opened the string, either ' or ".
  // A value of 0 indicates that string parsing is not in process.
  char string_open_;

  // Storage for the chunk that are being parsed in ParseChunk().
  std::string chunk_storage_;

  // Whether to allow non UTF-8 encoded input and replace invalid code points.
  bool coerce_to_utf8_;

  // Whether allows empty string represented null array value or object entry
  // value.
  bool allow_empty_null_;

  // Whether allows out-of-range floating point numbers or reject them.
  bool loose_float_number_conversion_;

  // Tracks current recursion depth.
  mutable int recursion_depth_;

  // Maximum allowed recursion depth.
  int max_recursion_depth_;

  GOOGLE_DISALLOW_IMPLICIT_CONSTRUCTORS(JsonStreamParser);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_STREAM_PARSER_H__
