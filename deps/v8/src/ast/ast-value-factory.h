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

#include "src/api.h"
#include "src/base/hashmap.h"
#include "src/utils.h"

// AstString, AstValue and AstValueFactory are for storing strings and values
// independent of the V8 heap and internalizing them later. During parsing,
// AstStrings and AstValues are created and stored outside the heap, in
// AstValueFactory. After parsing, the strings and values are internalized
// (moved into the V8 heap).
namespace v8 {
namespace internal {

class AstString : public ZoneObject {
 public:
  explicit AstString(bool is_raw)
      : next_(nullptr), bit_field_(IsRawStringBits::encode(is_raw)) {}

  int length() const;
  bool IsEmpty() const { return length() == 0; }

  // Puts the string into the V8 heap.
  void Internalize(Isolate* isolate);

  // This function can be called after internalizing.
  V8_INLINE Handle<String> string() const {
    DCHECK(!string_.is_null());
    return string_;
  }

  AstString** next_location() { return &next_; }
  AstString* next() const { return next_; }

 protected:
  // Handle<String>::null() until internalized.
  Handle<String> string_;
  AstString* next_;
  // Poor-man's virtual dispatch to AstRawString / AstConsString. Takes less
  // memory.
  class IsRawStringBits : public BitField<bool, 0, 1> {};
  int bit_field_;
};


class AstRawString final : public AstString {
 public:
  int length() const {
    if (is_one_byte()) return literal_bytes_.length();
    return literal_bytes_.length() / 2;
  }

  int byte_length() const { return literal_bytes_.length(); }

  void Internalize(Isolate* isolate);

  bool AsArrayIndex(uint32_t* index) const;

  // The string is not null-terminated, use length() to find out the length.
  const unsigned char* raw_data() const {
    return literal_bytes_.start();
  }

  bool is_one_byte() const { return IsOneByteBits::decode(bit_field_); }

  bool IsOneByteEqualTo(const char* data) const;
  uint16_t FirstCharacter() const {
    if (is_one_byte()) return literal_bytes_[0];
    const uint16_t* c =
        reinterpret_cast<const uint16_t*>(literal_bytes_.start());
    return *c;
  }

  // For storing AstRawStrings in a hash map.
  uint32_t hash() const {
    return hash_;
  }

 private:
  friend class AstValueFactory;
  friend class AstRawStringInternalizationKey;

  AstRawString(bool is_one_byte, const Vector<const byte>& literal_bytes,
               uint32_t hash)
      : AstString(true), hash_(hash), literal_bytes_(literal_bytes) {
    bit_field_ |= IsOneByteBits::encode(is_one_byte);
  }

  AstRawString() : AstString(true), hash_(0) {
    bit_field_ |= IsOneByteBits::encode(true);
  }

  class IsOneByteBits : public BitField<bool, IsRawStringBits::kNext, 1> {};

  uint32_t hash_;
  // Points to memory owned by Zone.
  Vector<const byte> literal_bytes_;
};


class AstConsString final : public AstString {
 public:
  AstConsString(const AstString* left, const AstString* right)
      : AstString(false),
        length_(left->length() + right->length()),
        left_(left),
        right_(right) {}

  int length() const { return length_; }

  void Internalize(Isolate* isolate);

 private:
  const int length_;
  const AstString* left_;
  const AstString* right_;
};


// AstValue is either a string, a number, a string array, a boolean, or a
// special value (null, undefined, the hole).
class AstValue : public ZoneObject {
 public:
  bool IsString() const {
    return type_ == STRING;
  }

  bool IsNumber() const {
    return type_ == NUMBER || type_ == NUMBER_WITH_DOT || type_ == SMI ||
           type_ == SMI_WITH_DOT;
  }

  bool ContainsDot() const {
    return type_ == NUMBER_WITH_DOT || type_ == SMI_WITH_DOT;
  }

  const AstRawString* AsString() const {
    CHECK_EQ(STRING, type_);
    return string_;
  }

  double AsNumber() const {
    if (type_ == NUMBER || type_ == NUMBER_WITH_DOT)
      return number_;
    if (type_ == SMI || type_ == SMI_WITH_DOT)
      return smi_;
    UNREACHABLE();
    return 0;
  }

  Smi* AsSmi() const {
    CHECK(type_ == SMI || type_ == SMI_WITH_DOT);
    return Smi::FromInt(smi_);
  }

  bool EqualsString(const AstRawString* string) const {
    return type_ == STRING && string_ == string;
  }

  bool IsPropertyName() const;

  bool BooleanValue() const;

  bool IsSmi() const { return type_ == SMI || type_ == SMI_WITH_DOT; }
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
    DCHECK(!value_.is_null());
    return value_;
  }
  AstValue* next() const { return next_; }
  void set_next(AstValue* next) { next_ = next; }

 private:
  friend class AstValueFactory;

  enum Type {
    STRING,
    SYMBOL,
    NUMBER,
    NUMBER_WITH_DOT,
    SMI,
    SMI_WITH_DOT,
    BOOLEAN,
    NULL_TYPE,
    UNDEFINED,
    THE_HOLE
  };

  explicit AstValue(const AstRawString* s) : type_(STRING), next_(nullptr) {
    string_ = s;
  }

  explicit AstValue(const char* name) : type_(SYMBOL), next_(nullptr) {
    symbol_name_ = name;
  }

  explicit AstValue(double n, bool with_dot) : next_(nullptr) {
    int int_value;
    if (DoubleToSmiInteger(n, &int_value)) {
      type_ = with_dot ? SMI_WITH_DOT : SMI;
      smi_ = int_value;
    } else {
      type_ = with_dot ? NUMBER_WITH_DOT : NUMBER;
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

  // Uninternalized value.
  union {
    const AstRawString* string_;
    double number_;
    int smi_;
    bool bool_;
    const AstRawString* strings_;
    const char* symbol_name_;
  };

  // Handle<String>::null() until internalized.
  Handle<Object> value_;
  AstValue* next_;
};


// For generating constants.
#define STRING_CONSTANTS(F)                     \
  F(anonymous_function, "(anonymous function)") \
  F(arguments, "arguments")                     \
  F(async, "async")                             \
  F(await, "await")                             \
  F(constructor, "constructor")                 \
  F(default, "default")                         \
  F(done, "done")                               \
  F(dot, ".")                                   \
  F(dot_class_field_init, ".class-field-init")  \
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
  F(native, "native")                           \
  F(new_target, ".new.target")                  \
  F(next, "next")                               \
  F(proto, "__proto__")                         \
  F(prototype, "prototype")                     \
  F(return, "return")                           \
  F(set_space, "set ")                          \
  F(star_default_star, "*default*")             \
  F(this, "this")                               \
  F(this_function, ".this_function")            \
  F(throw, "throw")                             \
  F(undefined, "undefined")                     \
  F(use_asm, "use asm")                         \
  F(use_strict, "use strict")                   \
  F(value, "value")

#define OTHER_CONSTANTS(F) \
  F(true_value)            \
  F(false_value)           \
  F(null_value)            \
  F(undefined_value)       \
  F(the_hole_value)

class AstValueFactory {
 public:
  AstValueFactory(Zone* zone, uint32_t hash_seed)
      : string_table_(AstRawStringCompare),
        values_(nullptr),
        strings_end_(&strings_),
        zone_(zone),
        hash_seed_(hash_seed) {
    ResetStrings();
#define F(name, str) name##_string_ = NULL;
    STRING_CONSTANTS(F)
#undef F
#define F(name) name##_ = NULL;
    OTHER_CONSTANTS(F)
#undef F
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
  const AstConsString* NewConsString(const AstString* left,
                                     const AstString* right);
  const AstRawString* ConcatStrings(const AstRawString* left,
                                    const AstRawString* right);

  void Internalize(Isolate* isolate);

#define F(name, str)                                                    \
  const AstRawString* name##_string() {                                 \
    if (name##_string_ == NULL) {                                       \
      const char* data = str;                                           \
      name##_string_ = GetOneByteString(                                \
          Vector<const uint8_t>(reinterpret_cast<const uint8_t*>(data), \
                                static_cast<int>(strlen(data))));       \
    }                                                                   \
    return name##_string_;                                              \
  }
  STRING_CONSTANTS(F)
#undef F

  const AstValue* NewString(const AstRawString* string);
  // A JavaScript symbol (ECMA-262 edition 6).
  const AstValue* NewSymbol(const char* name);
  const AstValue* NewNumber(double number, bool with_dot = false);
  const AstValue* NewSmi(int number);
  const AstValue* NewBoolean(bool b);
  const AstValue* NewStringList(ZoneList<const AstRawString*>* strings);
  const AstValue* NewNull();
  const AstValue* NewUndefined();
  const AstValue* NewTheHole();

 private:
  AstValue* AddValue(AstValue* value) {
    value->set_next(values_);
    values_ = value;
    return value;
  }
  AstString* AddString(AstString* string) {
    *strings_end_ = string;
    strings_end_ = string->next_location();
    return string;
  }
  void ResetStrings() {
    strings_ = nullptr;
    strings_end_ = &strings_;
  }
  AstRawString* GetOneByteStringInternal(Vector<const uint8_t> literal);
  AstRawString* GetTwoByteStringInternal(Vector<const uint16_t> literal);
  AstRawString* GetString(uint32_t hash, bool is_one_byte,
                          Vector<const byte> literal_bytes);

  static bool AstRawStringCompare(void* a, void* b);

  // All strings are copied here, one after another (no NULLs inbetween).
  base::CustomMatcherHashMap string_table_;
  // For keeping track of all AstValues and AstRawStrings we've created (so that
  // they can be internalized later).
  AstValue* values_;
  // We need to keep track of strings_ in order, since cons strings require
  // their members to be internalized first.
  AstString* strings_;
  AstString** strings_end_;
  Zone* zone_;

  uint32_t hash_seed_;

#define F(name, str) const AstRawString* name##_string_;
  STRING_CONSTANTS(F)
#undef F

#define F(name) AstValue* name##_;
  OTHER_CONSTANTS(F)
#undef F
};
}  // namespace internal
}  // namespace v8

#undef STRING_CONSTANTS
#undef OTHER_CONSTANTS

#endif  // V8_AST_AST_VALUE_FACTORY_H_
