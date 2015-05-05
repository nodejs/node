// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_JSON_PARSER_H_
#define V8_JSON_PARSER_H_

#include "src/v8.h"

#include "src/char-predicates-inl.h"
#include "src/conversions.h"
#include "src/heap/spaces-inl.h"
#include "src/messages.h"
#include "src/token.h"

namespace v8 {
namespace internal {

// A simple json parser.
template <bool seq_one_byte>
class JsonParser BASE_EMBEDDED {
 public:
  MUST_USE_RESULT static MaybeHandle<Object> Parse(Handle<String> source) {
    return JsonParser(source).ParseJson();
  }

  static const int kEndOfString = -1;

 private:
  explicit JsonParser(Handle<String> source)
      : source_(source),
        source_length_(source->length()),
        isolate_(source->map()->GetHeap()->isolate()),
        factory_(isolate_->factory()),
        object_constructor_(isolate_->native_context()->object_function(),
                            isolate_),
        position_(-1) {
    source_ = String::Flatten(source_);
    pretenure_ = (source_length_ >= kPretenureTreshold) ? TENURED : NOT_TENURED;

    // Optimized fast case where we only have Latin1 characters.
    if (seq_one_byte) {
      seq_source_ = Handle<SeqOneByteString>::cast(source_);
    }
  }

  // Parse a string containing a single JSON value.
  MaybeHandle<Object> ParseJson();

  inline void Advance() {
    position_++;
    if (position_ >= source_length_) {
      c0_ = kEndOfString;
    } else if (seq_one_byte) {
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

  bool ParseJsonString(Handle<String> expected) {
    int length = expected->length();
    if (source_->length() - position_ - 1 > length) {
      DisallowHeapAllocation no_gc;
      String::FlatContent content = expected->GetFlatContent();
      if (content.IsOneByte()) {
        DCHECK_EQ('"', c0_);
        const uint8_t* input_chars = seq_source_->GetChars() + position_ + 1;
        const uint8_t* expected_chars = content.ToOneByteVector().start();
        for (int i = 0; i < length; i++) {
          uint8_t c0 = input_chars[i];
          if (c0 != expected_chars[i] || c0 == '"' || c0 < 0x20 || c0 == '\\') {
            return false;
          }
        }
        if (input_chars[length] == '"') {
          position_ = position_ + length + 1;
          AdvanceSkipWhitespace();
          return true;
        }
      }
    }
    return false;
  }

  Handle<String> ParseJsonInternalizedString() {
    return ScanJsonString<true>();
  }

  template <bool is_internalized>
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

  static const int kInitialSpecialStringLength = 1024;
  static const int kPretenureTreshold = 100 * 1024;


 private:
  Zone* zone() { return &zone_; }

  void CommitStateToJsonObject(Handle<JSObject> json_object, Handle<Map> map,
                               ZoneList<Handle<Object> >* properties);

  Handle<String> source_;
  int source_length_;
  Handle<SeqOneByteString> seq_source_;

  PretenureFlag pretenure_;
  Isolate* isolate_;
  Factory* factory_;
  Zone zone_;
  Handle<JSFunction> object_constructor_;
  uc32 c0_;
  int position_;
};

template <bool seq_one_byte>
MaybeHandle<Object> JsonParser<seq_one_byte>::ParseJson() {
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
        Handle<Object> name = factory->LookupSingleCharacterStringFromCode(c0_);
        Handle<FixedArray> element = factory->NewFixedArray(1);
        element->set(0, *name);
        array = factory->NewJSArrayWithElements(element);
        break;
    }

    MessageLocation location(factory->NewScript(source_),
                             position_,
                             position_ + 1);
    Handle<Object> error = factory->NewSyntaxError(message, array);
    return isolate()->template Throw<Object>(error, &location);
  }
  return result;
}


// Parse any JSON value.
template <bool seq_one_byte>
Handle<Object> JsonParser<seq_one_byte>::ParseJsonValue() {
  StackLimitCheck stack_check(isolate_);
  if (stack_check.HasOverflowed()) {
    isolate_->StackOverflow();
    return Handle<Object>::null();
  }

  if (isolate_->stack_guard()->InterruptRequested()) {
    ExecutionAccess access(isolate_);
    // Avoid blocking GC in long running parser (v8:3974).
    isolate_->stack_guard()->CheckAndHandleGCInterrupt();
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
template <bool seq_one_byte>
Handle<Object> JsonParser<seq_one_byte>::ParseJsonObject() {
  HandleScope scope(isolate());
  Handle<JSObject> json_object =
      factory()->NewJSObject(object_constructor(), pretenure_);
  Handle<Map> map(json_object->map());
  int descriptor = 0;
  ZoneList<Handle<Object> > properties(8, zone());
  DCHECK_EQ(c0_, '{');

  bool transitioning = true;

  AdvanceSkipWhitespace();
  if (c0_ != '}') {
    do {
      if (c0_ != '"') return ReportUnexpectedCharacter();

      int start_position = position_;
      Advance();

      uint32_t index = 0;
      if (IsDecimalDigit(c0_)) {
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
          } while (IsDecimalDigit(c0_));
        }

        if (c0_ == '"') {
          // Successfully parsed index, parse and store element.
          AdvanceSkipWhitespace();

          if (c0_ != ':') return ReportUnexpectedCharacter();
          AdvanceSkipWhitespace();
          Handle<Object> value = ParseJsonValue();
          if (value.is_null()) return ReportUnexpectedCharacter();

          JSObject::SetOwnElement(json_object, index, value, SLOPPY).Assert();
          continue;
        }
        // Not an index, fallback to the slow path.
      }

      position_ = start_position;
#ifdef DEBUG
      c0_ = '"';
#endif

      Handle<String> key;
      Handle<Object> value;

      // Try to follow existing transitions as long as possible. Once we stop
      // transitioning, no transition can be found anymore.
      if (transitioning) {
        // First check whether there is a single expected transition. If so, try
        // to parse it first.
        bool follow_expected = false;
        Handle<Map> target;
        if (seq_one_byte) {
          key = TransitionArray::ExpectedTransitionKey(map);
          follow_expected = !key.is_null() && ParseJsonString(key);
        }
        // If the expected transition hits, follow it.
        if (follow_expected) {
          target = TransitionArray::ExpectedTransitionTarget(map);
        } else {
          // If the expected transition failed, parse an internalized string and
          // try to find a matching transition.
          key = ParseJsonInternalizedString();
          if (key.is_null()) return ReportUnexpectedCharacter();

          target = TransitionArray::FindTransitionToField(map, key);
          // If a transition was found, follow it and continue.
          transitioning = !target.is_null();
        }
        if (c0_ != ':') return ReportUnexpectedCharacter();

        AdvanceSkipWhitespace();
        value = ParseJsonValue();
        if (value.is_null()) return ReportUnexpectedCharacter();

        if (transitioning) {
          PropertyDetails details =
              target->instance_descriptors()->GetDetails(descriptor);
          Representation expected_representation = details.representation();

          if (value->FitsRepresentation(expected_representation)) {
            if (expected_representation.IsHeapObject() &&
                !target->instance_descriptors()
                     ->GetFieldType(descriptor)
                     ->NowContains(value)) {
              Handle<HeapType> value_type(value->OptimalType(
                      isolate(), expected_representation));
              Map::GeneralizeFieldType(target, descriptor,
                                       expected_representation, value_type);
            }
            DCHECK(target->instance_descriptors()->GetFieldType(
                    descriptor)->NowContains(value));
            properties.Add(value, zone());
            map = target;
            descriptor++;
            continue;
          } else {
            transitioning = false;
          }
        }

        // Commit the intermediate state to the object and stop transitioning.
        CommitStateToJsonObject(json_object, map, &properties);
      } else {
        key = ParseJsonInternalizedString();
        if (key.is_null() || c0_ != ':') return ReportUnexpectedCharacter();

        AdvanceSkipWhitespace();
        value = ParseJsonValue();
        if (value.is_null()) return ReportUnexpectedCharacter();
      }

      Runtime::DefineObjectProperty(json_object, key, value, NONE).Check();
    } while (MatchSkipWhiteSpace(','));
    if (c0_ != '}') {
      return ReportUnexpectedCharacter();
    }

    // If we transitioned until the very end, transition the map now.
    if (transitioning) {
      CommitStateToJsonObject(json_object, map, &properties);
    }
  }
  AdvanceSkipWhitespace();
  return scope.CloseAndEscape(json_object);
}


template <bool seq_one_byte>
void JsonParser<seq_one_byte>::CommitStateToJsonObject(
    Handle<JSObject> json_object, Handle<Map> map,
    ZoneList<Handle<Object> >* properties) {
  JSObject::AllocateStorageForMap(json_object, map);
  DCHECK(!json_object->map()->is_dictionary_map());

  DisallowHeapAllocation no_gc;

  int length = properties->length();
  for (int i = 0; i < length; i++) {
    Handle<Object> value = (*properties)[i];
    json_object->WriteToField(i, *value);
  }
}


// Parse a JSON array. Position must be right at '['.
template <bool seq_one_byte>
Handle<Object> JsonParser<seq_one_byte>::ParseJsonArray() {
  HandleScope scope(isolate());
  ZoneList<Handle<Object> > elements(4, zone());
  DCHECK_EQ(c0_, '[');

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
  Handle<Object> json_array = factory()->NewJSArrayWithElements(
      fast_elements, FAST_ELEMENTS, pretenure_);
  return scope.CloseAndEscape(json_array);
}


template <bool seq_one_byte>
Handle<Object> JsonParser<seq_one_byte>::ParseJsonNumber() {
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
    if (IsDecimalDigit(c0_)) return ReportUnexpectedCharacter();
  } else {
    int i = 0;
    int digits = 0;
    if (c0_ < '1' || c0_ > '9') return ReportUnexpectedCharacter();
    do {
      i = i * 10 + c0_ - '0';
      digits++;
      Advance();
    } while (IsDecimalDigit(c0_));
    if (c0_ != '.' && c0_ != 'e' && c0_ != 'E' && digits < 10) {
      SkipWhitespace();
      return Handle<Smi>(Smi::FromInt((negative ? -i : i)), isolate());
    }
  }
  if (c0_ == '.') {
    Advance();
    if (!IsDecimalDigit(c0_)) return ReportUnexpectedCharacter();
    do {
      Advance();
    } while (IsDecimalDigit(c0_));
  }
  if (AsciiAlphaToLower(c0_) == 'e') {
    Advance();
    if (c0_ == '-' || c0_ == '+') Advance();
    if (!IsDecimalDigit(c0_)) return ReportUnexpectedCharacter();
    do {
      Advance();
    } while (IsDecimalDigit(c0_));
  }
  int length = position_ - beg_pos;
  double number;
  if (seq_one_byte) {
    Vector<const uint8_t> chars(seq_source_->GetChars() +  beg_pos, length);
    number = StringToDouble(isolate()->unicode_cache(), chars,
                            NO_FLAGS,  // Hex, octal or trailing junk.
                            std::numeric_limits<double>::quiet_NaN());
  } else {
    Vector<uint8_t> buffer = Vector<uint8_t>::New(length);
    String::WriteToFlat(*source_, buffer.start(), beg_pos, position_);
    Vector<const uint8_t> result =
        Vector<const uint8_t>(buffer.start(), length);
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
  return factory->NewRawTwoByteString(length, pretenure).ToHandleChecked();
}

template <>
inline Handle<SeqOneByteString> NewRawString(Factory* factory,
                                           int length,
                                           PretenureFlag pretenure) {
  return factory->NewRawOneByteString(length, pretenure).ToHandleChecked();
}


// Scans the rest of a JSON string starting from position_ and writes
// prefix[start..end] along with the scanned characters into a
// sequential string of type StringType.
template <bool seq_one_byte>
template <typename StringType, typename SinkChar>
Handle<String> JsonParser<seq_one_byte>::SlowScanJsonString(
    Handle<String> prefix, int start, int end) {
  int count = end - start;
  int max_length = count + source_length_ - position_;
  int length = Min(max_length, Max(kInitialSpecialStringLength, 2 * count));
  Handle<StringType> seq_string =
      NewRawString<StringType>(factory(), length, pretenure_);
  // Copy prefix into seq_str.
  SinkChar* dest = seq_string->GetChars();
  String::WriteToFlat(*prefix, dest, start, end);

  while (c0_ != '"') {
    // Check for control character (0x00-0x1f) or unterminated string (<0).
    if (c0_ < 0x20) return Handle<String>::null();
    if (count >= length) {
      // We need to create a longer sequential string for the result.
      return SlowScanJsonString<StringType, SinkChar>(seq_string, 0, count);
    }
    if (c0_ != '\\') {
      // If the sink can contain UC16 characters, or source_ contains only
      // Latin1 characters, there's no need to test whether we can store the
      // character. Otherwise check whether the UC16 source character can fit
      // in the Latin1 sink.
      if (sizeof(SinkChar) == kUC16Size || seq_one_byte ||
          c0_ <= String::kMaxOneByteCharCode) {
        SeqStringSet(seq_string, count++, c0_);
        Advance();
      } else {
        // StringType is SeqOneByteString and we just read a non-Latin1 char.
        return SlowScanJsonString<SeqTwoByteString, uc16>(seq_string, 0, count);
      }
    } else {
      Advance();  // Advance past the \.
      switch (c0_) {
        case '"':
        case '\\':
        case '/':
          SeqStringSet(seq_string, count++, c0_);
          break;
        case 'b':
          SeqStringSet(seq_string, count++, '\x08');
          break;
        case 'f':
          SeqStringSet(seq_string, count++, '\x0c');
          break;
        case 'n':
          SeqStringSet(seq_string, count++, '\x0a');
          break;
        case 'r':
          SeqStringSet(seq_string, count++, '\x0d');
          break;
        case 't':
          SeqStringSet(seq_string, count++, '\x09');
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
          if (sizeof(SinkChar) == kUC16Size ||
              value <= String::kMaxOneByteCharCode) {
            SeqStringSet(seq_string, count++, value);
            break;
          } else {
            // StringType is SeqOneByteString and we just read a non-Latin1
            // char.
            position_ -= 6;  // Rewind position_ to \ in \uxxxx.
            Advance();
            return SlowScanJsonString<SeqTwoByteString, uc16>(seq_string,
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

  DCHECK_EQ('"', c0_);
  // Advance past the last '"'.
  AdvanceSkipWhitespace();

  // Shrink seq_string length to count and return.
  return SeqString::Truncate(seq_string, count);
}


template <bool seq_one_byte>
template <bool is_internalized>
Handle<String> JsonParser<seq_one_byte>::ScanJsonString() {
  DCHECK_EQ('"', c0_);
  Advance();
  if (c0_ == '"') {
    AdvanceSkipWhitespace();
    return factory()->empty_string();
  }

  if (seq_one_byte && is_internalized) {
    // Fast path for existing internalized strings.  If the the string being
    // parsed is not a known internalized string, contains backslashes or
    // unexpectedly reaches the end of string, return with an empty handle.
    uint32_t running_hash = isolate()->heap()->HashSeed();
    int position = position_;
    uc32 c0 = c0_;
    do {
      if (c0 == '\\') {
        c0_ = c0;
        int beg_pos = position_;
        position_ = position;
        return SlowScanJsonString<SeqOneByteString, uint8_t>(source_,
                                                             beg_pos,
                                                             position_);
      }
      if (c0 < 0x20) return Handle<String>::null();
      if (static_cast<uint32_t>(c0) >
          unibrow::Utf16::kMaxNonSurrogateCharCode) {
        running_hash =
            StringHasher::AddCharacterCore(running_hash,
                                           unibrow::Utf16::LeadSurrogate(c0));
        running_hash =
            StringHasher::AddCharacterCore(running_hash,
                                           unibrow::Utf16::TrailSurrogate(c0));
      } else {
        running_hash = StringHasher::AddCharacterCore(running_hash, c0);
      }
      position++;
      if (position >= source_length_) return Handle<String>::null();
      c0 = seq_source_->SeqOneByteStringGet(position);
    } while (c0 != '"');
    int length = position - position_;
    uint32_t hash = (length <= String::kMaxHashCalcLength)
                        ? StringHasher::GetHashCore(running_hash)
                        : static_cast<uint32_t>(length);
    Vector<const uint8_t> string_vector(
        seq_source_->GetChars() + position_, length);
    StringTable* string_table = isolate()->heap()->string_table();
    uint32_t capacity = string_table->Capacity();
    uint32_t entry = StringTable::FirstProbe(hash, capacity);
    uint32_t count = 1;
    Handle<String> result;
    while (true) {
      Object* element = string_table->KeyAt(entry);
      if (element == isolate()->heap()->undefined_value()) {
        // Lookup failure.
        result = factory()->InternalizeOneByteString(
            seq_source_, position_, length);
        break;
      }
      if (element != isolate()->heap()->the_hole_value() &&
          String::cast(element)->IsOneByteEqualTo(string_vector)) {
        result = Handle<String>(String::cast(element), isolate());
#ifdef DEBUG
        uint32_t hash_field =
            (hash << String::kHashShift) | String::kIsNotArrayIndexMask;
        DCHECK_EQ(static_cast<int>(result->Hash()),
                  static_cast<int>(hash_field >> String::kHashShift));
#endif
        break;
      }
      entry = StringTable::NextProbe(entry, count++, capacity);
    }
    position_ = position;
    // Advance past the last '"'.
    AdvanceSkipWhitespace();
    return result;
  }

  int beg_pos = position_;
  // Fast case for Latin1 only without escape characters.
  do {
    // Check for control character (0x00-0x1f) or unterminated string (<0).
    if (c0_ < 0x20) return Handle<String>::null();
    if (c0_ != '\\') {
      if (seq_one_byte || c0_ <= String::kMaxOneByteCharCode) {
        Advance();
      } else {
        return SlowScanJsonString<SeqTwoByteString, uc16>(source_,
                                                          beg_pos,
                                                          position_);
      }
    } else {
      return SlowScanJsonString<SeqOneByteString, uint8_t>(source_,
                                                           beg_pos,
                                                           position_);
    }
  } while (c0_ != '"');
  int length = position_ - beg_pos;
  Handle<String> result =
      factory()->NewRawOneByteString(length, pretenure_).ToHandleChecked();
  uint8_t* dest = SeqOneByteString::cast(*result)->GetChars();
  String::WriteToFlat(*source_, dest, beg_pos, position_);

  DCHECK_EQ('"', c0_);
  // Advance past the last '"'.
  AdvanceSkipWhitespace();
  return result;
}

} }  // namespace v8::internal

#endif  // V8_JSON_PARSER_H_
