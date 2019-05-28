// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_JSON_PARSER_H_
#define V8_JSON_PARSER_H_

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

enum ParseElementResult { kElementFound, kElementNotFound, kNullHandle };

class JsonParseInternalizer {
 public:
  static MaybeHandle<Object> Internalize(Isolate* isolate,
                                         Handle<Object> object,
                                         Handle<Object> reviver);

 private:
  JsonParseInternalizer(Isolate* isolate, Handle<JSReceiver> reviver)
      : isolate_(isolate), reviver_(reviver) {}

  MaybeHandle<Object> InternalizeJsonProperty(Handle<JSReceiver> holder,
                                              Handle<String> key);

  bool RecurseAndApply(Handle<JSReceiver> holder, Handle<String> name);

  Isolate* isolate_;
  Handle<JSReceiver> reviver_;
};

// A simple json parser.
template <bool seq_one_byte>
class JsonParser {
 public:
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Parse(
      Isolate* isolate, Handle<String> source, Handle<Object> reviver) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               JsonParser(isolate, source).ParseJson(), Object);
    if (reviver->IsCallable()) {
      return JsonParseInternalizer::Internalize(isolate, result, reviver);
    }
    return result;
  }

  static const int kEndOfString = -1;

 private:
  JsonParser(Isolate* isolate, Handle<String> source);

  // Parse a string containing a single JSON value.
  MaybeHandle<Object> ParseJson();

  V8_INLINE void Advance();

  // The JSON lexical grammar is specified in the ECMAScript 5 standard,
  // section 15.12.1.1. The only allowed whitespace characters between tokens
  // are tab, carriage-return, newline and space.

  V8_INLINE void AdvanceSkipWhitespace();
  V8_INLINE void SkipWhitespace();
  V8_INLINE uc32 AdvanceGetChar();

  // Checks that current charater is c.
  // If so, then consume c and skip whitespace.
  V8_INLINE bool MatchSkipWhiteSpace(uc32 c);

  // A JSON string (production JSONString) is subset of valid JavaScript string
  // literals. The string must only be double-quoted (not single-quoted), and
  // the only allowed backslash-escapes are ", /, \, b, f, n, r, t and
  // four-digit hex escapes (uXXXX). Any other use of backslashes is invalid.
  bool ParseJsonString(Handle<String> expected);

  Handle<String> ParseJsonString() {
    Handle<String> result = ScanJsonString();
    if (result.is_null()) return result;
    return factory()->InternalizeString(result);
  }

  Handle<String> ScanJsonString();
  // Creates a new string and copies prefix[start..end] into the beginning
  // of it. Then scans the rest of the string, adding characters after the
  // prefix. Called by ScanJsonString when reaching a '\' or non-Latin1 char.
  template <typename StringType, typename SinkChar>
  Handle<String> SlowScanJsonString(Handle<String> prefix, int start, int end);

  // A JSON number (production JSONNumber) is a subset of the valid JavaScript
  // decimal number literals.
  // It includes an optional minus sign, must have at least one
  // digit before and after a decimal point, may not have prefixed zeros (unless
  // the integer part is zero), and may include an exponent part (e.g., "e-10").
  // Hexadecimal and octal numbers are not allowed.
  Handle<Object> ParseJsonNumber();

  // Parse a single JSON value from input (grammar production JSONValue).
  // A JSON value is either a (double-quoted) string literal, a number literal,
  // one of "true", "false", or "null", or an object or array literal.
  Handle<Object> ParseJsonValue();

  // Parse a JSON object literal (grammar production JSONObject).
  // An object literal is a squiggly-braced and comma separated sequence
  // (possibly empty) of key/value pairs, where the key is a JSON string
  // literal, the value is a JSON value, and the two are separated by a colon.
  // A JSON array doesn't allow numbers and identifiers as keys, like a
  // JavaScript array.
  Handle<Object> ParseJsonObject();

  // Helper for ParseJsonObject. Parses the form "123": obj, which is recorded
  // as an element, not a property.
  ParseElementResult ParseElement(Handle<JSObject> json_object);

  // Parses a JSON array literal (grammar production JSONArray). An array
  // literal is a square-bracketed and comma separated sequence (possibly empty)
  // of JSON values.
  // A JSON array doesn't allow leaving out values from the sequence, nor does
  // it allow a terminal comma, like a JavaScript array does.
  Handle<Object> ParseJsonArray();


  // Mark that a parsing error has happened at the current token, and
  // return a null handle. Primarily for readability.
  inline Handle<Object> ReportUnexpectedCharacter() {
    return Handle<Object>::null();
  }

  inline Isolate* isolate() { return isolate_; }
  inline Factory* factory() { return isolate_->factory(); }
  inline Handle<JSFunction> object_constructor() { return object_constructor_; }

  static const int kInitialSpecialStringLength = 32;
  static const int kPretenureTreshold = 100 * 1024;

 private:
  Zone* zone() { return &zone_; }

  void CommitStateToJsonObject(Handle<JSObject> json_object, Handle<Map> map,
                               Vector<const Handle<Object>> properties);

  Handle<String> source_;
  int source_length_;
  Handle<SeqOneByteString> seq_source_;

  AllocationType allocation_;
  Isolate* isolate_;
  Zone zone_;
  Handle<JSFunction> object_constructor_;
  uc32 c0_;
  int position_;

  // Property handles are stored here inside ParseJsonObject.
  ZoneVector<Handle<Object>> properties_;
};

// Explicit instantiation declarations.
extern template class JsonParser<true>;
extern template class JsonParser<false>;

}  // namespace internal
}  // namespace v8

#endif  // V8_JSON_PARSER_H_
