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

#include <forward_list>

#include "src/base/hashmap.h"
#include "src/conversions.h"
#include "src/globals.h"
#include "src/heap/factory.h"
#include "src/isolate.h"

// Ast(Raw|Cons)String and AstValueFactory are for storing strings and
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
  uint32_t hash_field() const { return hash_field_; }
  uint32_t Hash() const { return hash_field_ >> Name::kHashShift; }

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
               uint32_t hash_field)
      : next_(nullptr),
        literal_bytes_(literal_bytes),
        hash_field_(hash_field),
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

  // {string_} is stored as Address* instead of a Handle<String> so it can be
  // stored in a union with {next_}.
  union {
    AstRawString* next_;
    Address* string_;
  };

  Vector<const byte> literal_bytes_;  // Memory owned by Zone.
  uint32_t hash_field_;
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

  std::forward_list<const AstRawString*> ToRawStrings() const;

 private:
  friend class AstValueFactory;

  AstConsString() : next_(nullptr), segment_({nullptr, nullptr}) {}

  AstConsString* next() const { return next_; }
  AstConsString** next_location() { return &next_; }

  // {string_} is stored as Address* instead of a Handle<String> so it can be
  // stored in a union with {next_}.
  void set_string(Handle<String> string) { string_ = string.location(); }
  union {
    AstConsString* next_;
    Address* string_;
  };

  struct Segment {
    const AstRawString* string;
    AstConsString::Segment* next;
  };
  Segment segment_;
};

enum class AstSymbol : uint8_t { kHomeObjectSymbol };

class AstBigInt {
 public:
  // |bigint| must be a NUL-terminated string of ASCII characters
  // representing a BigInt (suitable for passing to BigIntLiteral()
  // from conversions.h).
  explicit AstBigInt(const char* bigint) : bigint_(bigint) {}

  const char* c_str() const { return bigint_; }

 private:
  const char* bigint_;
};

// For generating constants.
#define AST_STRING_CONSTANTS(F)                 \
  F(anonymous, "anonymous")                     \
  F(anonymous_function, "(anonymous function)") \
  F(arguments, "arguments")                     \
  F(as, "as")                                   \
  F(async, "async")                             \
  F(await, "await")                             \
  F(bigint, "bigint")                           \
  F(boolean, "boolean")                         \
  F(computed, "<computed>")                     \
  F(constructor, "constructor")                 \
  F(default, "default")                         \
  F(done, "done")                               \
  F(dot, ".")                                   \
  F(dot_default, ".default")                    \
  F(dot_for, ".for")                            \
  F(dot_generator_object, ".generator_object")  \
  F(dot_iterator, ".iterator")                  \
  F(dot_promise, ".promise")                    \
  F(dot_result, ".result")                      \
  F(dot_switch_tag, ".switch_tag")              \
  F(dot_catch, ".catch")                        \
  F(empty, "")                                  \
  F(eval, "eval")                               \
  F(from, "from")                               \
  F(function, "function")                       \
  F(get, "get")                                 \
  F(get_space, "get ")                          \
  F(length, "length")                           \
  F(let, "let")                                 \
  F(meta, "meta")                               \
  F(name, "name")                               \
  F(native, "native")                           \
  F(new_target, ".new.target")                  \
  F(next, "next")                               \
  F(number, "number")                           \
  F(object, "object")                           \
  F(of, "of")                                   \
  F(private_constructor, "#constructor")        \
  F(proto, "__proto__")                         \
  F(prototype, "prototype")                     \
  F(return, "return")                           \
  F(set, "set")                                 \
  F(set_space, "set ")                          \
  F(string, "string")                           \
  F(symbol, "symbol")                           \
  F(target, "target")                           \
  F(this, "this")                               \
  F(this_function, ".this_function")            \
  F(throw, "throw")                             \
  F(undefined, "undefined")                     \
  F(value, "value")

class AstStringConstants final {
 public:
  AstStringConstants(Isolate* isolate, uint64_t hash_seed);

#define F(name, str) \
  const AstRawString* name##_string() const { return name##_string_; }
  AST_STRING_CONSTANTS(F)
#undef F

  uint64_t hash_seed() const { return hash_seed_; }
  const base::CustomMatcherHashMap* string_table() const {
    return &string_table_;
  }

 private:
  Zone zone_;
  base::CustomMatcherHashMap string_table_;
  uint64_t hash_seed_;

#define F(name, str) AstRawString* name##_string_;
  AST_STRING_CONSTANTS(F)
#undef F

  DISALLOW_COPY_AND_ASSIGN(AstStringConstants);
};

class AstValueFactory {
 public:
  AstValueFactory(Zone* zone, const AstStringConstants* string_constants,
                  uint64_t hash_seed)
      : string_table_(string_constants->string_table()),
        strings_(nullptr),
        strings_end_(&strings_),
        cons_strings_(nullptr),
        cons_strings_end_(&cons_strings_),
        string_constants_(string_constants),
        empty_cons_string_(nullptr),
        zone_(zone),
        hash_seed_(hash_seed) {
    DCHECK_EQ(hash_seed, string_constants->hash_seed());
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

  // Clones an AstRawString from another ast value factory, adding it to this
  // factory and returning the clone.
  const AstRawString* CloneFromOtherFactory(const AstRawString* raw_string);

  V8_EXPORT_PRIVATE AstConsString* NewConsString();
  V8_EXPORT_PRIVATE AstConsString* NewConsString(const AstRawString* str);
  V8_EXPORT_PRIVATE AstConsString* NewConsString(const AstRawString* str1,
                                                 const AstRawString* str2);

  V8_EXPORT_PRIVATE void Internalize(Isolate* isolate);

#define F(name, str)                           \
  const AstRawString* name##_string() const {  \
    return string_constants_->name##_string(); \
  }
  AST_STRING_CONSTANTS(F)
#undef F
  const AstConsString* empty_cons_string() const { return empty_cons_string_; }

 private:
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

  // All strings are copied here, one after another (no zeroes inbetween).
  base::CustomMatcherHashMap string_table_;

  // We need to keep track of strings_ in order since cons strings require their
  // members to be internalized first.
  AstRawString* strings_;
  AstRawString** strings_end_;
  AstConsString* cons_strings_;
  AstConsString** cons_strings_end_;

  // Holds constant string values which are shared across the isolate.
  const AstStringConstants* string_constants_;
  const AstConsString* empty_cons_string_;

  // Caches one character lowercase strings (for minified code).
  static const int kMaxOneCharStringValue = 128;
  AstRawString* one_character_strings_[kMaxOneCharStringValue];

  Zone* zone_;

  uint64_t hash_seed_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_VALUE_FACTORY_H_
