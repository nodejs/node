// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_JSON_PARSER_H_
#define V8_JSON_PARSER_H_

#include "v8.h"

#include "char-predicates-inl.h"
#include "v8conversions.h"
#include "messages.h"
#include "spaces-inl.h"
#include "token.h"

namespace v8 {
namespace internal {

// A simple json parser.
template <bool seq_ascii>
class JsonParser BASE_EMBEDDED {
 public:
  static Handle<Object> Parse(Handle<String> source, Zone* zone) {
    return JsonParser().ParseJson(source, zone);
  }

  static const int kEndOfString = -1;

 private:
  // Parse a string containing a single JSON value.
  Handle<Object> ParseJson(Handle<String> source, Zone* zone);

  inline void Advance() {
    position_++;
    if (position_ >= source_length_) {
      c0_ = kEndOfString;
    } else if (seq_ascii) {
      c0_ = seq_source_->SeqOneByteStringGet(position_);
    } else {
      c0_ = source_->Get(position_);
    }
  }

  // The JSON lexical grammar is specified in the ECMAScript 5 standard,
  // section 15.12.1.1. The only allowed whitespace characters between tokens
  // are tab, carriage-return, newline and space.

  inline void AdvanceSkipWhitespace() {
    do {
      Advance();
    } while (c0_ == ' ' || c0_ == '\t' || c0_ == '\n' || c0_ == '\r');
  }

  inline void SkipWhitespace() {
    while (c0_ == ' ' || c0_ == '\t' || c0_ == '\n' || c0_ == '\r') {
      Advance();
    }
  }

  inline uc32 AdvanceGetChar() {
    Advance();
    return c0_;
  }

  // Checks that current charater is c.
  // If so, then consume c and skip whitespace.
  inline bool MatchSkipWhiteSpace(uc32 c) {
    if (c0_ == c) {
      AdvanceSkipWhitespace();
      return true;
    }
    return false;
  }

  // A JSON string (production JSONString) is subset of valid JavaScript string
  // literals. The string must only be double-quoted (not single-quoted), and
  // the only allowed backslash-escapes are ", /, \, b, f, n, r, t and
  // four-digit hex escapes (uXXXX). Any other use of backslashes is invalid.
  Handle<String> ParseJsonString() {
    return ScanJsonString<false>();
  }
  Handle<String> ParseJsonSymbol() {
    return ScanJsonString<true>();
  }
  template <bool is_symbol>
  Handle<String> ScanJsonString();
  // Creates a new string and copies prefix[start..end] into the beginning
  // of it. Then scans the rest of the string, adding characters after the
  // prefix. Called by ScanJsonString when reaching a '\' or non-ASCII char.
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
  inline Factory* factory() { return factory_; }
  inline Handle<JSFunction> object_constructor() { return object_constructor_; }
  inline Zone* zone() const { return zone_; }

  static const int kInitialSpecialStringLength = 1024;
  static const int kPretenureTreshold = 100 * 1024;


 private:
  Handle<String> source_;
  int source_length_;
  Handle<SeqOneByteString> seq_source_;

  PretenureFlag pretenure_;
  Isolate* isolate_;
  Factory* factory_;
  Handle<JSFunction> object_constructor_;
  uc32 c0_;
  int position_;
  Zone* zone_;
};

template <bool seq_ascii>
Handle<Object> JsonParser<seq_ascii>::ParseJson(Handle<String> source,
                                                Zone* zone) {
  isolate_ = source->map()->GetHeap()->isolate();
  factory_ = isolate_->factory();
  object_constructor_ = Handle<JSFunction>(
      isolate()->native_context()->object_function(), isolate());
  zone_ = zone;
  FlattenString(source);
  source_ = source;
  source_length_ = source_->length();
  pretenure_ = (source_length_ >= kPretenureTreshold) ? TENURED : NOT_TENURED;

  // Optimized fast case where we only have ASCII characters.
  if (seq_ascii) {
    seq_source_ = Handle<SeqOneByteString>::cast(source_);
  }

  // Set initial position right before the string.
  position_ = -1;
  // Advance to the first character (possibly EOS)
  AdvanceSkipWhitespace();
  Handle<Object> result = ParseJsonValue();
  if (result.is_null() || c0_ != kEndOfString) {
    // Some exception (for example stack overflow) is already pending.
    if (isolate_->has_pending_exception()) return Handle<Object>::null();

    // Parse failed. Current character is the unexpected token.
    const char* message;
    Factory* factory = this->factory();
    Handle<JSArray> array;

    switch (c0_) {
      case kEndOfString:
        message = "unexpected_eos";
        array = factory->NewJSArray(0);
        break;
      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        message = "unexpected_token_number";
        array = factory->NewJSArray(0);
        break;
      case '"':
        message = "unexpected_token_string";
        array = factory->NewJSArray(0);
        break;
      default:
        message = "unexpected_token";
        Handle<Object> name = LookupSingleCharacterStringFromCode(c0_);
        Handle<FixedArray> element = factory->NewFixedArray(1);
        element->set(0, *name);
        array = factory->NewJSArrayWithElements(element);
        break;
    }

    MessageLocation location(factory->NewScript(source),
                             position_,
                             position_ + 1);
    Handle<Object> result = factory->NewSyntaxError(message, array);
    isolate()->Throw(*result, &location);
    return Handle<Object>::null();
  }
  return result;
}


// Parse any JSON value.
template <bool seq_ascii>
Handle<Object> JsonParser<seq_ascii>::ParseJsonValue() {
  StackLimitCheck stack_check(isolate_);
  if (stack_check.HasOverflowed()) {
    isolate_->StackOverflow();
    return Handle<Object>::null();
  }

  if (c0_ == '"') return ParseJsonString();
  if ((c0_ >= '0' && c0_ <= '9') || c0_ == '-') return ParseJsonNumber();
  if (c0_ == '{') return ParseJsonObject();
  if (c0_ == '[') return ParseJsonArray();
  if (c0_ == 'f') {
    if (AdvanceGetChar() == 'a' && AdvanceGetChar() == 'l' &&
        AdvanceGetChar() == 's' && AdvanceGetChar() == 'e') {
      AdvanceSkipWhitespace();
      return factory()->false_value();
    }
    return ReportUnexpectedCharacter();
  }
  if (c0_ == 't') {
    if (AdvanceGetChar() == 'r' && AdvanceGetChar() == 'u' &&
        AdvanceGetChar() == 'e') {
      AdvanceSkipWhitespace();
      return factory()->true_value();
    }
    return ReportUnexpectedCharacter();
  }
  if (c0_ == 'n') {
    if (AdvanceGetChar() == 'u' && AdvanceGetChar() == 'l' &&
        AdvanceGetChar() == 'l') {
      AdvanceSkipWhitespace();
      return factory()->null_value();
    }
    return ReportUnexpectedCharacter();
  }
  return ReportUnexpectedCharacter();
}


// Parse a JSON object. Position must be right at '{'.
template <bool seq_ascii>
Handle<Object> JsonParser<seq_ascii>::ParseJsonObject() {
  Handle<Object> prototype;
  Handle<JSObject> json_object =
      factory()->NewJSObject(object_constructor(), pretenure_);
  ASSERT_EQ(c0_, '{');

  AdvanceSkipWhitespace();
  if (c0_ != '}') {
    do {
      if (c0_ != '"') return ReportUnexpectedCharacter();

      int start_position = position_;
      Advance();

      uint32_t index = 0;
      if (c0_ >= '0' && c0_ <= '9') {
        // Maybe an array index, try to parse it.
        if (c0_ == '0') {
          // With a leading zero, the string has to be "0" only to be an index.
          Advance();
        } else {
          do {
            int d = c0_ - '0';
            if (index > 429496729U - ((d > 5) ? 1 : 0)) break;
            index = (index * 10) + d;
            Advance();
          } while (c0_ >= '0' && c0_ <= '9');
        }

        if (c0_ == '"') {
          // Successfully parsed index, parse and store element.
          AdvanceSkipWhitespace();

          if (c0_ != ':') return ReportUnexpectedCharacter();
          AdvanceSkipWhitespace();
          Handle<Object> value = ParseJsonValue();
          if (value.is_null()) return ReportUnexpectedCharacter();

          JSObject::SetOwnElement(json_object, index, value, kNonStrictMode);
          continue;
        }
        // Not an index, fallback to the slow path.
      }

      position_ = start_position;
#ifdef DEBUG
      c0_ = '"';
#endif

      Handle<String> key = ParseJsonSymbol();
      if (key.is_null() || c0_ != ':') return ReportUnexpectedCharacter();

      AdvanceSkipWhitespace();
      Handle<Object> value = ParseJsonValue();
      if (value.is_null()) return ReportUnexpectedCharacter();

      if (key->Equals(isolate()->heap()->Proto_symbol())) {
        prototype = value;
      } else {
        if (JSObject::TryTransitionToField(json_object, key)) {
          int index = json_object->LastAddedFieldIndex();
          json_object->FastPropertyAtPut(index, *value);
        } else {
          JSObject::SetLocalPropertyIgnoreAttributes(
              json_object, key, value, NONE);
        }
      }
    } while (MatchSkipWhiteSpace(','));
    if (c0_ != '}') {
      return ReportUnexpectedCharacter();
    }
    if (!prototype.is_null()) SetPrototype(json_object, prototype);
  }
  AdvanceSkipWhitespace();
  return json_object;
}

// Parse a JSON array. Position must be right at '['.
template <bool seq_ascii>
Handle<Object> JsonParser<seq_ascii>::ParseJsonArray() {
  ZoneScope zone_scope(zone(), DELETE_ON_EXIT);
  ZoneList<Handle<Object> > elements(4, zone());
  ASSERT_EQ(c0_, '[');

  AdvanceSkipWhitespace();
  if (c0_ != ']') {
    do {
      Handle<Object> element = ParseJsonValue();
      if (element.is_null()) return ReportUnexpectedCharacter();
      elements.Add(element, zone());
    } while (MatchSkipWhiteSpace(','));
    if (c0_ != ']') {
      return ReportUnexpectedCharacter();
    }
  }
  AdvanceSkipWhitespace();
  // Allocate a fixed array with all the elements.
  Handle<FixedArray> fast_elements =
      factory()->NewFixedArray(elements.length(), pretenure_);
  for (int i = 0, n = elements.length(); i < n; i++) {
    fast_elements->set(i, *elements[i]);
  }
  return factory()->NewJSArrayWithElements(
      fast_elements, FAST_ELEMENTS, pretenure_);
}


template <bool seq_ascii>
Handle<Object> JsonParser<seq_ascii>::ParseJsonNumber() {
  bool negative = false;
  int beg_pos = position_;
  if (c0_ == '-') {
    Advance();
    negative = true;
  }
  if (c0_ == '0') {
    Advance();
    // Prefix zero is only allowed if it's the only digit before
    // a decimal point or exponent.
    if ('0' <= c0_ && c0_ <= '9') return ReportUnexpectedCharacter();
  } else {
    int i = 0;
    int digits = 0;
    if (c0_ < '1' || c0_ > '9') return ReportUnexpectedCharacter();
    do {
      i = i * 10 + c0_ - '0';
      digits++;
      Advance();
    } while (c0_ >= '0' && c0_ <= '9');
    if (c0_ != '.' && c0_ != 'e' && c0_ != 'E' && digits < 10) {
      SkipWhitespace();
      return Handle<Smi>(Smi::FromInt((negative ? -i : i)), isolate());
    }
  }
  if (c0_ == '.') {
    Advance();
    if (c0_ < '0' || c0_ > '9') return ReportUnexpectedCharacter();
    do {
      Advance();
    } while (c0_ >= '0' && c0_ <= '9');
  }
  if (AsciiAlphaToLower(c0_) == 'e') {
    Advance();
    if (c0_ == '-' || c0_ == '+') Advance();
    if (c0_ < '0' || c0_ > '9') return ReportUnexpectedCharacter();
    do {
      Advance();
    } while (c0_ >= '0' && c0_ <= '9');
  }
  int length = position_ - beg_pos;
  double number;
  if (seq_ascii) {
    Vector<const char> chars(seq_source_->GetChars() +  beg_pos, length);
    number = StringToDouble(isolate()->unicode_cache(),
                             chars,
                             NO_FLAGS,  // Hex, octal or trailing junk.
                             OS::nan_value());
  } else {
    Vector<char> buffer = Vector<char>::New(length);
    String::WriteToFlat(*source_, buffer.start(), beg_pos, position_);
    Vector<const char> result =
        Vector<const char>(reinterpret_cast<const char*>(buffer.start()),
        length);
    number = StringToDouble(isolate()->unicode_cache(),
                             result,
                             NO_FLAGS,  // Hex, octal or trailing junk.
                             0.0);
    buffer.Dispose();
  }
  SkipWhitespace();
  return factory()->NewNumber(number, pretenure_);
}


template <typename StringType>
inline void SeqStringSet(Handle<StringType> seq_str, int i, uc32 c);

template <>
inline void SeqStringSet(Handle<SeqTwoByteString> seq_str, int i, uc32 c) {
  seq_str->SeqTwoByteStringSet(i, c);
}

template <>
inline void SeqStringSet(Handle<SeqOneByteString> seq_str, int i, uc32 c) {
  seq_str->SeqOneByteStringSet(i, c);
}

template <typename StringType>
inline Handle<StringType> NewRawString(Factory* factory,
                                       int length,
                                       PretenureFlag pretenure);

template <>
inline Handle<SeqTwoByteString> NewRawString(Factory* factory,
                                             int length,
                                             PretenureFlag pretenure) {
  return factory->NewRawTwoByteString(length, pretenure);
}

template <>
inline Handle<SeqOneByteString> NewRawString(Factory* factory,
                                           int length,
                                           PretenureFlag pretenure) {
  return factory->NewRawOneByteString(length, pretenure);
}


// Scans the rest of a JSON string starting from position_ and writes
// prefix[start..end] along with the scanned characters into a
// sequential string of type StringType.
template <bool seq_ascii>
template <typename StringType, typename SinkChar>
Handle<String> JsonParser<seq_ascii>::SlowScanJsonString(
    Handle<String> prefix, int start, int end) {
  int count = end - start;
  int max_length = count + source_length_ - position_;
  int length = Min(max_length, Max(kInitialSpecialStringLength, 2 * count));
  Handle<StringType> seq_str =
      NewRawString<StringType>(factory(), length, pretenure_);
  // Copy prefix into seq_str.
  SinkChar* dest = seq_str->GetChars();
  String::WriteToFlat(*prefix, dest, start, end);

  while (c0_ != '"') {
    // Check for control character (0x00-0x1f) or unterminated string (<0).
    if (c0_ < 0x20) return Handle<String>::null();
    if (count >= length) {
      // We need to create a longer sequential string for the result.
      return SlowScanJsonString<StringType, SinkChar>(seq_str, 0, count);
    }
    if (c0_ != '\\') {
      // If the sink can contain UC16 characters, or source_ contains only
      // ASCII characters, there's no need to test whether we can store the
      // character. Otherwise check whether the UC16 source character can fit
      // in the ASCII sink.
      if (sizeof(SinkChar) == kUC16Size ||
          seq_ascii ||
          c0_ <= kMaxAsciiCharCode) {
        SeqStringSet(seq_str, count++, c0_);
        Advance();
      } else {
        // StringType is SeqOneByteString and we just read a non-ASCII char.
        return SlowScanJsonString<SeqTwoByteString, uc16>(seq_str, 0, count);
      }
    } else {
      Advance();  // Advance past the \.
      switch (c0_) {
        case '"':
        case '\\':
        case '/':
          SeqStringSet(seq_str, count++, c0_);
          break;
        case 'b':
          SeqStringSet(seq_str, count++, '\x08');
          break;
        case 'f':
          SeqStringSet(seq_str, count++, '\x0c');
          break;
        case 'n':
          SeqStringSet(seq_str, count++, '\x0a');
          break;
        case 'r':
          SeqStringSet(seq_str, count++, '\x0d');
          break;
        case 't':
          SeqStringSet(seq_str, count++, '\x09');
          break;
        case 'u': {
          uc32 value = 0;
          for (int i = 0; i < 4; i++) {
            Advance();
            int digit = HexValue(c0_);
            if (digit < 0) {
              return Handle<String>::null();
            }
            value = value * 16 + digit;
          }
          if (sizeof(SinkChar) == kUC16Size || value <= kMaxAsciiCharCode) {
            SeqStringSet(seq_str, count++, value);
            break;
          } else {
            // StringType is SeqOneByteString and we just read a non-ASCII char.
            position_ -= 6;  // Rewind position_ to \ in \uxxxx.
            Advance();
            return SlowScanJsonString<SeqTwoByteString, uc16>(seq_str,
                                                              0,
                                                              count);
          }
        }
        default:
          return Handle<String>::null();
      }
      Advance();
    }
  }
  // Shrink seq_string length to count.
  if (isolate()->heap()->InNewSpace(*seq_str)) {
    isolate()->heap()->new_space()->
        template ShrinkStringAtAllocationBoundary<StringType>(
            *seq_str, count);
  } else {
    int string_size = StringType::SizeFor(count);
    int allocated_string_size = StringType::SizeFor(length);
    int delta = allocated_string_size - string_size;
    Address start_filler_object = seq_str->address() + string_size;
    seq_str->set_length(count);
    isolate()->heap()->CreateFillerObjectAt(start_filler_object, delta);
  }
  ASSERT_EQ('"', c0_);
  // Advance past the last '"'.
  AdvanceSkipWhitespace();
  return seq_str;
}


template <bool seq_ascii>
template <bool is_symbol>
Handle<String> JsonParser<seq_ascii>::ScanJsonString() {
  ASSERT_EQ('"', c0_);
  Advance();
  if (c0_ == '"') {
    AdvanceSkipWhitespace();
    return factory()->empty_string();
  }

  if (seq_ascii && is_symbol) {
    // Fast path for existing symbols.  If the the string being parsed is not
    // a known symbol, contains backslashes or unexpectedly reaches the end of
    // string, return with an empty handle.
    uint32_t running_hash = isolate()->heap()->HashSeed();
    int position = position_;
    uc32 c0 = c0_;
    do {
      if (c0 == '\\') {
        c0_ = c0;
        int beg_pos = position_;
        position_ = position;
        return SlowScanJsonString<SeqOneByteString, char>(source_,
                                                        beg_pos,
                                                        position_);
      }
      if (c0 < 0x20) return Handle<String>::null();
      running_hash = StringHasher::AddCharacterCore(running_hash, c0);
      position++;
      if (position >= source_length_) return Handle<String>::null();
      c0 = seq_source_->SeqOneByteStringGet(position);
    } while (c0 != '"');
    int length = position - position_;
    uint32_t hash = (length <= String::kMaxHashCalcLength)
        ? StringHasher::GetHashCore(running_hash) : length;
    Vector<const char> string_vector(
        seq_source_->GetChars() + position_, length);
    SymbolTable* symbol_table = isolate()->heap()->symbol_table();
    uint32_t capacity = symbol_table->Capacity();
    uint32_t entry = SymbolTable::FirstProbe(hash, capacity);
    uint32_t count = 1;
    while (true) {
      Object* element = symbol_table->KeyAt(entry);
      if (element == isolate()->heap()->undefined_value()) {
        // Lookup failure.
        break;
      }
      if (element != isolate()->heap()->the_hole_value() &&
          String::cast(element)->IsAsciiEqualTo(string_vector)) {
        // Lookup success, update the current position.
        position_ = position;
        // Advance past the last '"'.
        AdvanceSkipWhitespace();
        return Handle<String>(String::cast(element), isolate());
      }
      entry = SymbolTable::NextProbe(entry, count++, capacity);
    }
  }

  int beg_pos = position_;
  // Fast case for ASCII only without escape characters.
  do {
    // Check for control character (0x00-0x1f) or unterminated string (<0).
    if (c0_ < 0x20) return Handle<String>::null();
    if (c0_ != '\\') {
      if (seq_ascii || c0_ <= kMaxAsciiCharCode) {
        Advance();
      } else {
        return SlowScanJsonString<SeqTwoByteString, uc16>(source_,
                                                          beg_pos,
                                                          position_);
      }
    } else {
      return SlowScanJsonString<SeqOneByteString, char>(source_,
                                                      beg_pos,
                                                      position_);
    }
  } while (c0_ != '"');
  int length = position_ - beg_pos;
  Handle<String> result;
  if (seq_ascii && is_symbol) {
    result = factory()->LookupAsciiSymbol(seq_source_,
                                          beg_pos,
                                          length);
  } else {
    result = factory()->NewRawOneByteString(length, pretenure_);
    char* dest = SeqOneByteString::cast(*result)->GetChars();
    String::WriteToFlat(*source_, dest, beg_pos, position_);
  }
  ASSERT_EQ('"', c0_);
  // Advance past the last '"'.
  AdvanceSkipWhitespace();
  return result;
}

} }  // namespace v8::internal

#endif  // V8_JSON_PARSER_H_
