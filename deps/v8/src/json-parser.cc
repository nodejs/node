// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/json-parser.h"

#include "src/char-predicates-inl.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/field-type.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/parsing/token.h"
#include "src/property-descriptor.h"
#include "src/transitions.h"
#include "src/unicode-cache.h"

namespace v8 {
namespace internal {

MaybeHandle<Object> JsonParseInternalizer::Internalize(Isolate* isolate,
                                                       Handle<Object> object,
                                                       Handle<Object> reviver) {
  DCHECK(reviver->IsCallable());
  JsonParseInternalizer internalizer(isolate,
                                     Handle<JSReceiver>::cast(reviver));
  Handle<JSObject> holder =
      isolate->factory()->NewJSObject(isolate->object_function());
  Handle<String> name = isolate->factory()->empty_string();
  JSObject::AddProperty(holder, name, object, NONE);
  return internalizer.InternalizeJsonProperty(holder, name);
}

MaybeHandle<Object> JsonParseInternalizer::InternalizeJsonProperty(
    Handle<JSReceiver> holder, Handle<String> name) {
  HandleScope outer_scope(isolate_);
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, value, Object::GetPropertyOrElement(holder, name), Object);
  if (value->IsJSReceiver()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(value);
    Maybe<bool> is_array = Object::IsArray(object);
    if (is_array.IsNothing()) return MaybeHandle<Object>();
    if (is_array.FromJust()) {
      Handle<Object> length_object;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate_, length_object,
          Object::GetLengthFromArrayLike(isolate_, object), Object);
      double length = length_object->Number();
      for (double i = 0; i < length; i++) {
        HandleScope inner_scope(isolate_);
        Handle<Object> index = isolate_->factory()->NewNumber(i);
        Handle<String> name = isolate_->factory()->NumberToString(index);
        if (!RecurseAndApply(object, name)) return MaybeHandle<Object>();
      }
    } else {
      Handle<FixedArray> contents;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate_, contents,
          KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                                  ENUMERABLE_STRINGS,
                                  GetKeysConversion::kConvertToString),
          Object);
      for (int i = 0; i < contents->length(); i++) {
        HandleScope inner_scope(isolate_);
        Handle<String> name(String::cast(contents->get(i)), isolate_);
        if (!RecurseAndApply(object, name)) return MaybeHandle<Object>();
      }
    }
  }
  Handle<Object> argv[] = {name, value};
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, result, Execution::Call(isolate_, reviver_, holder, 2, argv),
      Object);
  return outer_scope.CloseAndEscape(result);
}

bool JsonParseInternalizer::RecurseAndApply(Handle<JSReceiver> holder,
                                            Handle<String> name) {
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, result, InternalizeJsonProperty(holder, name), false);
  Maybe<bool> change_result = Nothing<bool>();
  if (result->IsUndefined(isolate_)) {
    change_result = JSReceiver::DeletePropertyOrElement(holder, name, SLOPPY);
  } else {
    PropertyDescriptor desc;
    desc.set_value(result);
    desc.set_configurable(true);
    desc.set_enumerable(true);
    desc.set_writable(true);
    change_result = JSReceiver::DefineOwnProperty(isolate_, holder, name, &desc,
                                                  Object::DONT_THROW);
  }
  MAYBE_RETURN(change_result, false);
  return true;
}

template <bool seq_one_byte>
JsonParser<seq_one_byte>::JsonParser(Isolate* isolate, Handle<String> source)
    : source_(source),
      source_length_(source->length()),
      isolate_(isolate),
      factory_(isolate_->factory()),
      zone_(isolate_->allocator(), ZONE_NAME),
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

template <bool seq_one_byte>
MaybeHandle<Object> JsonParser<seq_one_byte>::ParseJson() {
  // Advance to the first character (possibly EOS)
  AdvanceSkipWhitespace();
  Handle<Object> result = ParseJsonValue();
  if (result.is_null() || c0_ != kEndOfString) {
    // Some exception (for example stack overflow) is already pending.
    if (isolate_->has_pending_exception()) return Handle<Object>::null();

    // Parse failed. Current character is the unexpected token.
    Factory* factory = this->factory();
    MessageTemplate::Template message;
    Handle<Object> arg1 = Handle<Smi>(Smi::FromInt(position_), isolate());
    Handle<Object> arg2;

    switch (c0_) {
      case kEndOfString:
        message = MessageTemplate::kJsonParseUnexpectedEOS;
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
        message = MessageTemplate::kJsonParseUnexpectedTokenNumber;
        break;
      case '"':
        message = MessageTemplate::kJsonParseUnexpectedTokenString;
        break;
      default:
        message = MessageTemplate::kJsonParseUnexpectedToken;
        arg2 = arg1;
        arg1 = factory->LookupSingleCharacterStringFromCode(c0_);
        break;
    }

    Handle<Script> script(factory->NewScript(source_));
    // We should sent compile error event because we compile JSON object in
    // separated source file.
    isolate()->debug()->OnCompileError(script);
    MessageLocation location(script, position_, position_ + 1);
    Handle<Object> error = factory->NewSyntaxError(message, arg1, arg2);
    return isolate()->template Throw<Object>(error, &location);
  }
  return result;
}

MaybeHandle<Object> InternalizeJsonProperty(Handle<JSObject> holder,
                                            Handle<String> key);

template <bool seq_one_byte>
void JsonParser<seq_one_byte>::Advance() {
  position_++;
  if (position_ >= source_length_) {
    c0_ = kEndOfString;
  } else if (seq_one_byte) {
    c0_ = seq_source_->SeqOneByteStringGet(position_);
  } else {
    c0_ = source_->Get(position_);
  }
}

template <bool seq_one_byte>
void JsonParser<seq_one_byte>::AdvanceSkipWhitespace() {
  do {
    Advance();
  } while (c0_ == ' ' || c0_ == '\t' || c0_ == '\n' || c0_ == '\r');
}

template <bool seq_one_byte>
void JsonParser<seq_one_byte>::SkipWhitespace() {
  while (c0_ == ' ' || c0_ == '\t' || c0_ == '\n' || c0_ == '\r') {
    Advance();
  }
}

template <bool seq_one_byte>
uc32 JsonParser<seq_one_byte>::AdvanceGetChar() {
  Advance();
  return c0_;
}

template <bool seq_one_byte>
bool JsonParser<seq_one_byte>::MatchSkipWhiteSpace(uc32 c) {
  if (c0_ == c) {
    AdvanceSkipWhitespace();
    return true;
  }
  return false;
}

template <bool seq_one_byte>
bool JsonParser<seq_one_byte>::ParseJsonString(Handle<String> expected) {
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

// Parse any JSON value.
template <bool seq_one_byte>
Handle<Object> JsonParser<seq_one_byte>::ParseJsonValue() {
  StackLimitCheck stack_check(isolate_);
  if (stack_check.HasOverflowed()) {
    isolate_->StackOverflow();
    return Handle<Object>::null();
  }

  if (stack_check.InterruptRequested() &&
      isolate_->stack_guard()->HandleInterrupts()->IsException(isolate_)) {
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

template <bool seq_one_byte>
ParseElementResult JsonParser<seq_one_byte>::ParseElement(
    Handle<JSObject> json_object) {
  uint32_t index = 0;
  // Maybe an array index, try to parse it.
  if (c0_ == '0') {
    // With a leading zero, the string has to be "0" only to be an index.
    Advance();
  } else {
    do {
      int d = c0_ - '0';
      if (index > 429496729U - ((d + 3) >> 3)) break;
      index = (index * 10) + d;
      Advance();
    } while (IsDecimalDigit(c0_));
  }

  if (c0_ == '"') {
    // Successfully parsed index, parse and store element.
    AdvanceSkipWhitespace();

    if (c0_ == ':') {
      AdvanceSkipWhitespace();
      Handle<Object> value = ParseJsonValue();
      if (!value.is_null()) {
        JSObject::SetOwnElementIgnoreAttributes(json_object, index, value, NONE)
            .Assert();
        return kElementFound;
      } else {
        return kNullHandle;
      }
    }
  }
  return kElementNotFound;
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

      if (IsDecimalDigit(c0_)) {
        ParseElementResult element_result = ParseElement(json_object);
        if (element_result == kNullHandle) return Handle<Object>::null();
        if (element_result == kElementFound) continue;
      }
      // Not an index, fallback to the slow path.

      position_ = start_position;
#ifdef DEBUG
      c0_ = '"';
#endif

      Handle<String> key;
      Handle<Object> value;

      // Try to follow existing transitions as long as possible. Once we stop
      // transitioning, no transition can be found anymore.
      DCHECK(transitioning);
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
            Handle<FieldType> value_type(
                value->OptimalType(isolate(), expected_representation));
            Map::GeneralizeField(target, descriptor, expected_representation,
                                 value_type);
          }
          DCHECK(target->instance_descriptors()
                     ->GetFieldType(descriptor)
                     ->NowContains(value));
          properties.Add(value, zone());
          map = target;
          descriptor++;
          continue;
        } else {
          transitioning = false;
        }
      }

      DCHECK(!transitioning);

      // Commit the intermediate state to the object and stop transitioning.
      CommitStateToJsonObject(json_object, map, &properties);

      JSObject::DefinePropertyOrElementIgnoreAttributes(json_object, key, value)
          .Check();
    } while (transitioning && MatchSkipWhiteSpace(','));

    // If we transitioned until the very end, transition the map now.
    if (transitioning) {
      CommitStateToJsonObject(json_object, map, &properties);
    } else {
      while (MatchSkipWhiteSpace(',')) {
        HandleScope local_scope(isolate());
        if (c0_ != '"') return ReportUnexpectedCharacter();

        int start_position = position_;
        Advance();

        if (IsDecimalDigit(c0_)) {
          ParseElementResult element_result = ParseElement(json_object);
          if (element_result == kNullHandle) return Handle<Object>::null();
          if (element_result == kElementFound) continue;
        }
        // Not an index, fallback to the slow path.

        position_ = start_position;
#ifdef DEBUG
        c0_ = '"';
#endif

        Handle<String> key;
        Handle<Object> value;

        key = ParseJsonInternalizedString();
        if (key.is_null() || c0_ != ':') return ReportUnexpectedCharacter();

        AdvanceSkipWhitespace();
        value = ParseJsonValue();
        if (value.is_null()) return ReportUnexpectedCharacter();

        JSObject::DefinePropertyOrElementIgnoreAttributes(json_object, key,
                                                          value)
            .Check();
      }
    }

    if (c0_ != '}') {
      return ReportUnexpectedCharacter();
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
    // Initializing store.
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
    Vector<const uint8_t> chars(seq_source_->GetChars() + beg_pos, length);
    number = StringToDouble(isolate()->unicode_cache(), chars,
                            NO_FLAGS,  // Hex, octal or trailing junk.
                            std::numeric_limits<double>::quiet_NaN());
  } else {
    Vector<uint8_t> buffer = Vector<uint8_t>::New(length);
    String::WriteToFlat(*source_, buffer.start(), beg_pos, position_);
    Vector<const uint8_t> result =
        Vector<const uint8_t>(buffer.start(), length);
    number = StringToDouble(isolate()->unicode_cache(), result,
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
inline Handle<StringType> NewRawString(Factory* factory, int length,
                                       PretenureFlag pretenure);

template <>
inline Handle<SeqTwoByteString> NewRawString(Factory* factory, int length,
                                             PretenureFlag pretenure) {
  return factory->NewRawTwoByteString(length, pretenure).ToHandleChecked();
}

template <>
inline Handle<SeqOneByteString> NewRawString(Factory* factory, int length,
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
            return SlowScanJsonString<SeqTwoByteString, uc16>(seq_string, 0,
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
        return SlowScanJsonString<SeqOneByteString, uint8_t>(source_, beg_pos,
                                                             position_);
      }
      if (c0 < 0x20) return Handle<String>::null();
      running_hash = StringHasher::AddCharacterCore(running_hash,
                                                    static_cast<uint16_t>(c0));
      position++;
      if (position >= source_length_) return Handle<String>::null();
      c0 = seq_source_->SeqOneByteStringGet(position);
    } while (c0 != '"');
    int length = position - position_;
    uint32_t hash = (length <= String::kMaxHashCalcLength)
                        ? StringHasher::GetHashCore(running_hash)
                        : static_cast<uint32_t>(length);
    Vector<const uint8_t> string_vector(seq_source_->GetChars() + position_,
                                        length);
    StringTable* string_table = isolate()->heap()->string_table();
    uint32_t capacity = string_table->Capacity();
    uint32_t entry = StringTable::FirstProbe(hash, capacity);
    uint32_t count = 1;
    Handle<String> result;
    while (true) {
      Object* element = string_table->KeyAt(entry);
      if (element->IsUndefined(isolate())) {
        // Lookup failure.
        result =
            factory()->InternalizeOneByteString(seq_source_, position_, length);
        break;
      }
      if (!element->IsTheHole(isolate()) &&
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
        return SlowScanJsonString<SeqTwoByteString, uc16>(source_, beg_pos,
                                                          position_);
      }
    } else {
      return SlowScanJsonString<SeqOneByteString, uint8_t>(source_, beg_pos,
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

// Explicit instantiation.
template class JsonParser<true>;
template class JsonParser<false>;

}  // namespace internal
}  // namespace v8
