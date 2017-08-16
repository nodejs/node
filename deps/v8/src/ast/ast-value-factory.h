// Copyright 2014 the V8 project authors. All rights reserved.
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

#ifndef V8_AST_AST_VALUE_FACTORY_H_
#define V8_AST_AST_VALUE_FACTORY_H_

#include "src/base/hashmap.h"
#include "src/conversions.h"
#include "src/factory.h"
#include "src/globals.h"
#include "src/isolate.h"
#include "src/utils.h"

// Ast(Raw|Cons)String, AstValue and AstValueFactory are for storing strings and
// values independent of the V8 heap and internalizing them later. During
// parsing, they are created and stored outside the heap, in AstValueFactory.
// After parsing, the strings and values are internalized (moved into the V8
// heap).
namespace v8 {
namespace internal {

class AstRawString final : public ZoneObject {
 public:
  bool IsEmpty() const { return literal_bytes_.length() == 0; }
  int length() const {
    return is_one_byte() ? literal_bytes_.length()
                         : literal_bytes_.length() / 2;
  }
  bool AsArrayIndex(uint32_t* index) const;
  bool IsOneByteEqualTo(const char* data) const;
  uint16_t FirstCharacter() const;

  void Internalize(Isolate* isolate);

  // Access the physical representation:
  bool is_one_byte() const { return is_one_byte_; }
  int byte_length() const { return literal_bytes_.length(); }
  const unsigned char* raw_data() const {
    return literal_bytes_.start();
  }

  // For storing AstRawStrings in a hash map.
  uint32_t hash() const {
    return hash_;
  }

  // This function can be called after internalizing.
  V8_INLINE Handle<String> string() const {
    DCHECK_NOT_NULL(string_);
    DCHECK(has_string_);
    return Handle<String>(string_);
  }

 private:
  friend class AstRawStringInternalizationKey;
  friend class AstStringConstants;
  friend class AstValueFactory;

  // Members accessed only by the AstValueFactory & related classes:
  static bool Compare(void* a, void* b);
  AstRawString(bool is_one_byte, const Vector<const byte>& literal_bytes,
               uint32_t hash)
      : next_(nullptr),
        literal_bytes_(literal_bytes),
        hash_(hash),
        is_one_byte_(is_one_byte) {}
  AstRawString* next() {
    DCHECK(!has_string_);
    return next_;
  }
  AstRawString** next_location() {
    DCHECK(!has_string_);
    return &next_;
  }

  void set_string(Handle<String> string) {
    DCHECK(!string.is_null());
    DCHECK(!has_string_);
    string_ = string.location();
#ifdef DEBUG
    has_string_ = true;
#endif
  }

  // {string_} is stored as String** instead of a Handle<String> so it can be
  // stored in a union with {next_}.
  union {
    AstRawString* next_;
    String** string_;
  };

  Vector<const byte> literal_bytes_;  // Memory owned by Zone.
  uint32_t hash_;
  bool is_one_byte_;
#ifdef DEBUG
  // (Debug-only:) Verify the object life-cylce: Some functions may only be
  // called after internalization (that is, after a v8::internal::String has
  // been set); some only before.
  bool has_string_ = false;
#endif
};

class AstConsString final : public ZoneObject {
 public:
  AstConsString* AddString(Zone* zone, const AstRawString* s) {
    if (s->IsEmpty()) return this;
    if (!IsEmpty()) {
      // We're putting the new string to the head of the list, meaning
      // the string segments will be in reverse order.
      Segment* tmp = new (zone->New(sizeof(Segment))) Segment;
      *tmp = segment_;
      segment_.next = tmp;
    }
    segment_.string = s;
    return this;
  }

  bool IsEmpty() const {
    DCHECK_IMPLIES(segment_.string == nullptr, segment_.next == nullptr);
    DCHECK_IMPLIES(segment_.string != nullptr, !segment_.string->IsEmpty());
    return segment_.string == nullptr;
  }

  void Internalize(Isolate* isolate);

  V8_INLINE Handle<String> string() const {
    DCHECK_NOT_NULL(string_);
    return Handle<String>(string_);
  }

 private:
  friend class AstValueFactory;

  AstConsString() : next_(nullptr), segment_({nullptr, nullptr}) {}

  AstConsString* next() const { return next_; }
  AstConsString** next_location() { return &next_; }

  // {string_} is stored as String** instead of a Handle<String> so it can be
  // stored in a union with {next_}.
  void set_string(Handle<String> string) { string_ = string.location(); }
  union {
    AstConsString* next_;
    String** string_;
  };

  struct Segment {
    const AstRawString* string;
    AstConsString::Segment* next;
  };
  Segment segment_;
};

enum class AstSymbol : uint8_t { kHomeObjectSymbol };

// AstValue is either a string, a symbol, a number, a string array, a boolean,
// or a special value (null, undefined, the hole).
class AstValue : public ZoneObject {
 public:
  bool IsString() const {
    return type_ == STRING;
  }

  bool IsSymbol() const { return type_ == SYMBOL; }

  bool IsNumber() const { return IsSmi() || IsHeapNumber(); }

  const AstRawString* AsString() const {
    CHECK_EQ(STRING, type_);
    return string_;
  }

  AstSymbol AsSymbol() const {
    CHECK_EQ(SYMBOL, type_);
    return symbol_;
  }

  double AsNumber() const {
    if (IsHeapNumber()) return number_;
    if (IsSmi()) return smi_;
    UNREACHABLE();
    return 0;
  }

  Smi* AsSmi() const {
    CHECK(IsSmi());
    return Smi::FromInt(smi_);
  }

  bool ToUint32(uint32_t* value) const {
    if (IsSmi()) {
      int num = smi_;
      if (num < 0) return false;
      *value = static_cast<uint32_t>(num);
      return true;
    }
    if (IsHeapNumber()) {
      return DoubleToUint32IfEqualToSelf(number_, value);
    }
    return false;
  }

  bool EqualsString(const AstRawString* string) const {
    return type_ == STRING && string_ == string;
  }

  bool IsPropertyName() const;

  bool BooleanValue() const;

  bool IsSmi() const { return type_ == SMI; }
  bool IsHeapNumber() const { return type_ == NUMBER; }
  bool IsFalse() const { return type_ == BOOLEAN && !bool_; }
  bool IsTrue() const { return type_ == BOOLEAN && bool_; }
  bool IsUndefined() const { return type_ == UNDEFINED; }
  bool IsTheHole() const { return type_ == THE_HOLE; }
  bool IsNull() const { return type_ == NULL_TYPE; }

  void Internalize(Isolate* isolate);

  // Can be called after Internalize has been called.
  V8_INLINE Handle<Object> value() const {
    if (type_ == STRING) {
      return string_->string();
    }
    DCHECK_NOT_NULL(value_);
    return Handle<Object>(value_);
  }
  AstValue* next() const { return next_; }
  void set_next(AstValue* next) { next_ = next; }

 private:
  void set_value(Handle<Object> object) { value_ = object.location(); }
  friend class AstValueFactory;

  enum Type {
    STRING,
    SYMBOL,
    NUMBER,
    SMI,
    BOOLEAN,
    NULL_TYPE,
    UNDEFINED,
    THE_HOLE
  };

  explicit AstValue(const AstRawString* s) : type_(STRING), next_(nullptr) {
    string_ = s;
  }

  explicit AstValue(AstSymbol symbol) : type_(SYMBOL), next_(nullptr) {
    symbol_ = symbol;
  }

  explicit AstValue(double n) : next_(nullptr) {
    int int_value;
    if (DoubleToSmiInteger(n, &int_value)) {
      type_ = SMI;
      smi_ = int_value;
    } else {
      type_ = NUMBER;
      number_ = n;
    }
  }

  AstValue(Type t, int i) : type_(t), next_(nullptr) {
    DCHECK(type_ == SMI);
    smi_ = i;
  }

  explicit AstValue(bool b) : type_(BOOLEAN), next_(nullptr) { bool_ = b; }

  explicit AstValue(Type t) : type_(t), next_(nullptr) {
    DCHECK(t == NULL_TYPE || t == UNDEFINED || t == THE_HOLE);
  }

  Type type_;

  // {value_} is stored as Object** instead of a Handle<Object> so it can be
  // stored in a union with {next_}.
  union {
    Object** value_;  // if internalized
    AstValue* next_;  // if !internalized
  };

  // Uninternalized value.
  union {
    const AstRawString* string_;
    double number_;
    int smi_;
    bool bool_;
    AstSymbol symbol_;
  };
};

// For generating constants.
#define STRING_CONSTANTS(F)                     \
  F(anonymous_function, "(anonymous function)") \
  F(arguments, "arguments")                     \
  F(async, "async")                             \
  F(await, "await")                             \
  F(boolean, "boolean")                         \
  F(constructor, "constructor")                 \
  F(default, "default")                         \
  F(done, "done")                               \
  F(dot, ".")                                   \
  F(dot_for, ".for")                            \
  F(dot_generator_object, ".generator_object")  \
  F(dot_iterator, ".iterator")                  \
  F(dot_result, ".result")                      \
  F(dot_switch_tag, ".switch_tag")              \
  F(dot_catch, ".catch")                        \
  F(empty, "")                                  \
  F(eval, "eval")                               \
  F(function, "function")                       \
  F(get_space, "get ")                          \
  F(length, "length")                           \
  F(let, "let")                                 \
  F(name, "name")                               \
  F(native, "native")                           \
  F(new_target, ".new.target")                  \
  F(next, "next")                               \
  F(number, "number")                           \
  F(object, "object")                           \
  F(proto, "__proto__")                         \
  F(prototype, "prototype")                     \
  F(return, "return")                           \
  F(set_space, "set ")                          \
  F(star_default_star, "*default*")             \
  F(string, "string")                           \
  F(symbol, "symbol")                           \
  F(this, "this")                               \
  F(this_function, ".this_function")            \
  F(throw, "throw")                             \
  F(undefined, "undefined")                     \
  F(use_asm, "use asm")                         \
  F(use_strict, "use strict")                   \
  F(value, "value")

class AstStringConstants final {
 public:
  AstStringConstants(Isolate* isolate, uint32_t hash_seed)
      : zone_(isolate->allocator(), ZONE_NAME),
        string_table_(AstRawString::Compare),
        hash_seed_(hash_seed) {
    DCHECK(ThreadId::Current().Equals(isolate->thread_id()));
#define F(name, str)                                                      \
  {                                                                       \
    const char* data = str;                                               \
    Vector<const uint8_t> literal(reinterpret_cast<const uint8_t*>(data), \
                                  static_cast<int>(strlen(data)));        \
    uint32_t hash = StringHasher::HashSequentialString<uint8_t>(          \
        literal.start(), literal.length(), hash_seed_);                   \
    name##_string_ = new (&zone_) AstRawString(true, literal, hash);      \
    /* The Handle returned by the factory is located on the roots */      \
    /* array, not on the temporary HandleScope, so this is safe.  */      \
    name##_string_->set_string(isolate->factory()->name##_string());      \
    base::HashMap::Entry* entry =                                         \
        string_table_.InsertNew(name##_string_, name##_string_->hash());  \
    DCHECK(entry->value == nullptr);                                      \
    entry->value = reinterpret_cast<void*>(1);                            \
  }
    STRING_CONSTANTS(F)
#undef F
  }

#define F(name, str) \
  const AstRawString* name##_string() const { return name##_string_; }
  STRING_CONSTANTS(F)
#undef F

  uint32_t hash_seed() const { return hash_seed_; }
  const base::CustomMatcherHashMap* string_table() const {
    return &string_table_;
  }

 private:
  Zone zone_;
  base::CustomMatcherHashMap string_table_;
  uint32_t hash_seed_;

#define F(name, str) AstRawString* name##_string_;
  STRING_CONSTANTS(F)
#undef F

  DISALLOW_COPY_AND_ASSIGN(AstStringConstants);
};

#define OTHER_CONSTANTS(F) \
  F(true_value)            \
  F(false_value)           \
  F(null_value)            \
  F(undefined_value)       \
  F(the_hole_value)

class AstValueFactory {
 public:
  AstValueFactory(Zone* zone, const AstStringConstants* string_constants,
                  uint32_t hash_seed)
      : string_table_(string_constants->string_table()),
        values_(nullptr),
        strings_(nullptr),
        strings_end_(&strings_),
        cons_strings_(nullptr),
        cons_strings_end_(&cons_strings_),
        string_constants_(string_constants),
        empty_cons_string_(nullptr),
        zone_(zone),
        hash_seed_(hash_seed) {
#define F(name) name##_ = nullptr;
    OTHER_CONSTANTS(F)
#undef F
    DCHECK_EQ(hash_seed, string_constants->hash_seed());
    std::fill(smis_, smis_ + arraysize(smis_), nullptr);
    std::fill(one_character_strings_,
              one_character_strings_ + arraysize(one_character_strings_),
              nullptr);
    empty_cons_string_ = NewConsString();
  }

  Zone* zone() const { return zone_; }

  const AstRawString* GetOneByteString(Vector<const uint8_t> literal) {
    return GetOneByteStringInternal(literal);
  }
  const AstRawString* GetOneByteString(const char* string) {
    return GetOneByteString(Vector<const uint8_t>(
        reinterpret_cast<const uint8_t*>(string), StrLength(string)));
  }
  const AstRawString* GetTwoByteString(Vector<const uint16_t> literal) {
    return GetTwoByteStringInternal(literal);
  }
  const AstRawString* GetString(Handle<String> literal);
  V8_EXPORT_PRIVATE AstConsString* NewConsString();
  AstConsString* NewConsString(const AstRawString* str);
  AstConsString* NewConsString(const AstRawString* str1,
                               const AstRawString* str2);

  V8_EXPORT_PRIVATE void Internalize(Isolate* isolate);

#define F(name, str)                           \
  const AstRawString* name##_string() const {  \
    return string_constants_->name##_string(); \
  }
  STRING_CONSTANTS(F)
#undef F
  const AstConsString* empty_cons_string() const { return empty_cons_string_; }

  V8_EXPORT_PRIVATE const AstValue* NewString(const AstRawString* string);
  // A JavaScript symbol (ECMA-262 edition 6).
  const AstValue* NewSymbol(AstSymbol symbol);
  V8_EXPORT_PRIVATE const AstValue* NewNumber(double number);
  const AstValue* NewSmi(uint32_t number);
  const AstValue* NewBoolean(bool b);
  const AstValue* NewStringList(ZoneList<const AstRawString*>* strings);
  const AstValue* NewNull();
  const AstValue* NewUndefined();
  const AstValue* NewTheHole();

 private:
  static const uint32_t kMaxCachedSmi = 1 << 10;

  STATIC_ASSERT(kMaxCachedSmi <= Smi::kMaxValue);

  AstValue* AddValue(AstValue* value) {
    value->set_next(values_);
    values_ = value;
    return value;
  }
  AstRawString* AddString(AstRawString* string) {
    *strings_end_ = string;
    strings_end_ = string->next_location();
    return string;
  }
  AstConsString* AddConsString(AstConsString* string) {
    *cons_strings_end_ = string;
    cons_strings_end_ = string->next_location();
    return string;
  }
  void ResetStrings() {
    strings_ = nullptr;
    strings_end_ = &strings_;
    cons_strings_ = nullptr;
    cons_strings_end_ = &cons_strings_;
  }
  V8_EXPORT_PRIVATE AstRawString* GetOneByteStringInternal(
      Vector<const uint8_t> literal);
  AstRawString* GetTwoByteStringInternal(Vector<const uint16_t> literal);
  AstRawString* GetString(uint32_t hash, bool is_one_byte,
                          Vector<const byte> literal_bytes);

  // All strings are copied here, one after another (no NULLs inbetween).
  base::CustomMatcherHashMap string_table_;
  // For keeping track of all AstValues and AstRawStrings we've created (so that
  // they can be internalized later).
  AstValue* values_;

  // We need to keep track of strings_ in order since cons strings require their
  // members to be internalized first.
  AstRawString* strings_;
  AstRawString** strings_end_;
  AstConsString* cons_strings_;
  AstConsString** cons_strings_end_;

  // Holds constant string values which are shared across the isolate.
  const AstStringConstants* string_constants_;
  const AstConsString* empty_cons_string_;

  // Caches for faster access: small numbers, one character lowercase strings
  // (for minified code).
  AstValue* smis_[kMaxCachedSmi + 1];
  AstRawString* one_character_strings_[26];

  Zone* zone_;

  uint32_t hash_seed_;

#define F(name) AstValue* name##_;
  OTHER_CONSTANTS(F)
#undef F
};
}  // namespace internal
}  // namespace v8

#undef STRING_CONSTANTS
#undef OTHER_CONSTANTS

#endif  // V8_AST_AST_VALUE_FACTORY_H_
