// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_JSON_JSON_PARSER_H_
#define V8_JSON_JSON_PARSER_H_

#include "src/base/small-vector.h"
#include "src/base/strings.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

enum ParseElementResult { kElementFound, kElementNotFound };

class JsonString final {
 public:
  JsonString()
      : start_(0),
        length_(0),
        needs_conversion_(false),
        internalize_(false),
        has_escape_(false),
        is_index_(false) {}

  explicit JsonString(uint32_t index)
      : index_(index),
        length_(0),
        needs_conversion_(false),
        internalize_(false),
        has_escape_(false),
        is_index_(true) {}

  JsonString(int start, int length, bool needs_conversion,
             bool needs_internalization, bool has_escape)
      : start_(start),
        length_(length),
        needs_conversion_(needs_conversion),
        internalize_(needs_internalization ||
                     length_ <= kMaxInternalizedStringValueLength),
        has_escape_(has_escape),
        is_index_(false) {}

  bool internalize() const {
    DCHECK(!is_index_);
    return internalize_;
  }

  bool needs_conversion() const {
    DCHECK(!is_index_);
    return needs_conversion_;
  }

  bool has_escape() const {
    DCHECK(!is_index_);
    return has_escape_;
  }

  int start() const {
    DCHECK(!is_index_);
    return start_;
  }

  int length() const {
    DCHECK(!is_index_);
    return length_;
  }

  uint32_t index() const {
    DCHECK(is_index_);
    return index_;
  }

  bool is_index() const { return is_index_; }

 private:
  static const int kMaxInternalizedStringValueLength = 10;

  union {
    const int start_;
    const uint32_t index_;
  };
  const int length_;
  const bool needs_conversion_ : 1;
  const bool internalize_ : 1;
  const bool has_escape_ : 1;
  const bool is_index_ : 1;
};

struct JsonProperty {
  JsonProperty() { UNREACHABLE(); }
  explicit JsonProperty(const JsonString& string) : string(string) {}

  JsonString string;
  Handle<Object> value;
};

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

enum class JsonToken : uint8_t {
  NUMBER,
  STRING,
  LBRACE,
  RBRACE,
  LBRACK,
  RBRACK,
  TRUE_LITERAL,
  FALSE_LITERAL,
  NULL_LITERAL,
  WHITESPACE,
  COLON,
  COMMA,
  ILLEGAL,
  EOS
};

// A simple json parser.
template <typename Char>
class JsonParser final {
 public:
  using SeqString = typename CharTraits<Char>::String;
  using SeqExternalString = typename CharTraits<Char>::ExternalString;

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

  static constexpr base::uc32 kEndOfString = static_cast<base::uc32>(-1);
  static constexpr base::uc32 kInvalidUnicodeCharacter =
      static_cast<base::uc32>(-1);

 private:
  template <typename T>
  using SmallVector = base::SmallVector<T, 16>;
  struct JsonContinuation {
    enum Type : uint8_t { kReturn, kObjectProperty, kArrayElement };
    JsonContinuation(Isolate* isolate, Type type, size_t index)
        : scope(isolate),
          type_(type),
          index(static_cast<uint32_t>(index)),
          max_index(0),
          elements(0) {}

    Type type() const { return static_cast<Type>(type_); }
    void set_type(Type type) { type_ = static_cast<uint8_t>(type); }

    HandleScope scope;
    // Unfortunately GCC doesn't like packing Type in two bits.
    uint32_t type_ : 2;
    uint32_t index : 30;
    uint32_t max_index;
    uint32_t elements;
  };

  JsonParser(Isolate* isolate, Handle<String> source);
  ~JsonParser();

  // Parse a string containing a single JSON value.
  MaybeHandle<Object> ParseJson();

  void advance() { ++cursor_; }

  base::uc32 CurrentCharacter() {
    if (V8_UNLIKELY(is_at_end())) return kEndOfString;
    return *cursor_;
  }

  base::uc32 NextCharacter() {
    advance();
    return CurrentCharacter();
  }

  void AdvanceToNonDecimal();

  V8_INLINE JsonToken peek() const { return next_; }

  void Consume(JsonToken token) {
    DCHECK_EQ(peek(), token);
    advance();
  }

  void Expect(JsonToken token) {
    if (V8_LIKELY(peek() == token)) {
      advance();
    } else {
      ReportUnexpectedToken(peek());
    }
  }

  void ExpectNext(JsonToken token) {
    SkipWhitespace();
    Expect(token);
  }

  bool Check(JsonToken token) {
    SkipWhitespace();
    if (next_ != token) return false;
    advance();
    return true;
  }

  template <size_t N>
  void ScanLiteral(const char (&s)[N]) {
    DCHECK(!is_at_end());
    // There's at least 1 character, we always consume a character and compare
    // the next character. The first character was compared before we jumped
    // to ScanLiteral.
    STATIC_ASSERT(N > 2);
    size_t remaining = static_cast<size_t>(end_ - cursor_);
    if (V8_LIKELY(remaining >= N - 1 &&
                  CompareCharsEqual(s + 1, cursor_ + 1, N - 2))) {
      cursor_ += N - 1;
      return;
    }

    cursor_++;
    for (size_t i = 0; i < std::min(N - 2, remaining - 1); i++) {
      if (*(s + 1 + i) != *cursor_) {
        ReportUnexpectedCharacter(*cursor_);
        return;
      }
      cursor_++;
    }

    DCHECK(is_at_end());
    ReportUnexpectedToken(JsonToken::EOS);
  }

  // The JSON lexical grammar is specified in the ECMAScript 5 standard,
  // section 15.12.1.1. The only allowed whitespace characters between tokens
  // are tab, carriage-return, newline and space.
  void SkipWhitespace();

  // A JSON string (production JSONString) is subset of valid JavaScript string
  // literals. The string must only be double-quoted (not single-quoted), and
  // the only allowed backslash-escapes are ", /, \, b, f, n, r, t and
  // four-digit hex escapes (uXXXX). Any other use of backslashes is invalid.
  JsonString ScanJsonString(bool needs_internalization);
  JsonString ScanJsonPropertyKey(JsonContinuation* cont);
  base::uc32 ScanUnicodeCharacter();
  Handle<String> MakeString(const JsonString& string,
                            Handle<String> hint = Handle<String>());

  template <typename SinkChar>
  void DecodeString(SinkChar* sink, int start, int length);

  template <typename SinkSeqString>
  Handle<String> DecodeString(const JsonString& string,
                              Handle<SinkSeqString> intermediate,
                              Handle<String> hint);

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
  MaybeHandle<Object> ParseJsonValue();

  Handle<Object> BuildJsonObject(
      const JsonContinuation& cont,
      const SmallVector<JsonProperty>& property_stack, Handle<Map> feedback);
  Handle<Object> BuildJsonArray(
      const JsonContinuation& cont,
      const SmallVector<Handle<Object>>& element_stack);

  // Mark that a parsing error has happened at the current character.
  void ReportUnexpectedCharacter(base::uc32 c);
  // Mark that a parsing error has happened at the current token.
  void ReportUnexpectedToken(JsonToken token);

  inline Isolate* isolate() { return isolate_; }
  inline Factory* factory() { return isolate_->factory(); }
  inline Handle<JSFunction> object_constructor() { return object_constructor_; }

  static const int kInitialSpecialStringLength = 32;

  static void UpdatePointersCallback(v8::Isolate* v8_isolate, v8::GCType type,
                                     v8::GCCallbackFlags flags, void* parser) {
    reinterpret_cast<JsonParser<Char>*>(parser)->UpdatePointers();
  }

  void UpdatePointers() {
    DisallowGarbageCollection no_gc;
    const Char* chars = Handle<SeqString>::cast(source_)->GetChars(no_gc);
    if (chars_ != chars) {
      size_t position = cursor_ - chars_;
      size_t length = end_ - chars_;
      chars_ = chars;
      cursor_ = chars_ + position;
      end_ = chars_ + length;
    }
  }

 private:
  static const bool kIsOneByte = sizeof(Char) == 1;

  bool is_at_end() const {
    DCHECK_LE(cursor_, end_);
    return cursor_ == end_;
  }

  int position() const { return static_cast<int>(cursor_ - chars_); }

  Isolate* isolate_;
  const uint64_t hash_seed_;
  JsonToken next_;
  // Indicates whether the bytes underneath source_ can relocate during GC.
  bool chars_may_relocate_;
  Handle<JSFunction> object_constructor_;
  const Handle<String> original_source_;
  Handle<String> source_;

  // Cached pointer to the raw chars in source. In case source is on-heap, we
  // register an UpdatePointers callback. For this reason, chars_, cursor_ and
  // end_ should never be locally cached across a possible allocation. The scope
  // in which we cache chars has to be guarded by a DisallowGarbageCollection
  // scope.
  const Char* cursor_;
  const Char* end_;
  const Char* chars_;
};

// Explicit instantiation declarations.
extern template class JsonParser<uint8_t>;
extern template class JsonParser<uint16_t>;

}  // namespace internal
}  // namespace v8

#endif  // V8_JSON_JSON_PARSER_H_
