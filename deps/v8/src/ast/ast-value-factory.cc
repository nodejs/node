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

#include "src/ast/ast-value-factory.h"

#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/strings/char-predicates-inl.h"
#include "src/strings/string-hasher.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

namespace {

// For using StringToIndex.
class OneByteStringStream {
 public:
  explicit OneByteStringStream(Vector<const byte> lb) :
      literal_bytes_(lb), pos_(0) {}

  bool HasMore() { return pos_ < literal_bytes_.length(); }
  uint16_t GetNext() { return literal_bytes_[pos_++]; }

 private:
  Vector<const byte> literal_bytes_;
  int pos_;
};

}  // namespace

void AstRawString::Internalize(Isolate* isolate) {
  DCHECK(!has_string_);
  if (literal_bytes_.length() == 0) {
    set_string(isolate->factory()->empty_string());
  } else if (is_one_byte()) {
    OneByteStringKey key(hash_field_, literal_bytes_);
    set_string(StringTable::LookupKey(isolate, &key));
  } else {
    TwoByteStringKey key(hash_field_,
                         Vector<const uint16_t>::cast(literal_bytes_));
    set_string(StringTable::LookupKey(isolate, &key));
  }
}

bool AstRawString::AsArrayIndex(uint32_t* index) const {
  // The StringHasher will set up the hash in such a way that we can use it to
  // figure out whether the string is convertible to an array index.
  if ((hash_field_ & Name::kIsNotArrayIndexMask) != 0) return false;
  if (length() <= Name::kMaxCachedArrayIndexLength) {
    *index = Name::ArrayIndexValueBits::decode(hash_field_);
  } else {
    OneByteStringStream stream(literal_bytes_);
    CHECK(StringToIndex(&stream, index));
  }
  return true;
}

bool AstRawString::IsIntegerIndex() const {
  return (hash_field_ & Name::kIsNotIntegerIndexMask) == 0;
}

bool AstRawString::IsOneByteEqualTo(const char* data) const {
  if (!is_one_byte()) return false;

  size_t length = static_cast<size_t>(literal_bytes_.length());
  if (length != strlen(data)) return false;

  return 0 == strncmp(reinterpret_cast<const char*>(literal_bytes_.begin()),
                      data, length);
}

uint16_t AstRawString::FirstCharacter() const {
  if (is_one_byte()) return literal_bytes_[0];
  const uint16_t* c = reinterpret_cast<const uint16_t*>(literal_bytes_.begin());
  return *c;
}

bool AstRawString::Compare(void* a, void* b) {
  const AstRawString* lhs = static_cast<AstRawString*>(a);
  const AstRawString* rhs = static_cast<AstRawString*>(b);
  DCHECK_EQ(lhs->Hash(), rhs->Hash());

  if (lhs->length() != rhs->length()) return false;
  if (lhs->length() == 0) return true;
  const unsigned char* l = lhs->raw_data();
  const unsigned char* r = rhs->raw_data();
  size_t length = rhs->length();
  if (lhs->is_one_byte()) {
    if (rhs->is_one_byte()) {
      return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(l),
                                  reinterpret_cast<const uint8_t*>(r),
                                  length) == 0;
    } else {
      return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(l),
                                  reinterpret_cast<const uint16_t*>(r),
                                  length) == 0;
    }
  } else {
    if (rhs->is_one_byte()) {
      return CompareCharsUnsigned(reinterpret_cast<const uint16_t*>(l),
                                  reinterpret_cast<const uint8_t*>(r),
                                  length) == 0;
    } else {
      return CompareCharsUnsigned(reinterpret_cast<const uint16_t*>(l),
                                  reinterpret_cast<const uint16_t*>(r),
                                  length) == 0;
    }
  }
}

void AstConsString::Internalize(Isolate* isolate) {
  if (IsEmpty()) {
    set_string(isolate->factory()->empty_string());
    return;
  }
  // AstRawStrings are internalized before AstConsStrings, so
  // AstRawString::string() will just work.
  Handle<String> tmp(segment_.string->string());
  for (AstConsString::Segment* current = segment_.next; current != nullptr;
       current = current->next) {
    tmp = isolate->factory()
              ->NewConsString(current->string->string(), tmp)
              .ToHandleChecked();
  }
  set_string(tmp);
}

std::forward_list<const AstRawString*> AstConsString::ToRawStrings() const {
  std::forward_list<const AstRawString*> result;
  if (IsEmpty()) {
    return result;
  }

  result.emplace_front(segment_.string);
  for (AstConsString::Segment* current = segment_.next; current != nullptr;
       current = current->next) {
    result.emplace_front(current->string);
  }
  return result;
}

AstStringConstants::AstStringConstants(Isolate* isolate, uint64_t hash_seed)
    : zone_(isolate->allocator(), ZONE_NAME),
      string_table_(AstRawString::Compare),
      hash_seed_(hash_seed) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
#define F(name, str)                                                       \
  {                                                                        \
    const char* data = str;                                                \
    Vector<const uint8_t> literal(reinterpret_cast<const uint8_t*>(data),  \
                                  static_cast<int>(strlen(data)));         \
    uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(     \
        literal.begin(), literal.length(), hash_seed_);                    \
    name##_string_ = new (&zone_) AstRawString(true, literal, hash_field); \
    /* The Handle returned by the factory is located on the roots */       \
    /* array, not on the temporary HandleScope, so this is safe.  */       \
    name##_string_->set_string(isolate->factory()->name##_string());       \
    base::HashMap::Entry* entry =                                          \
        string_table_.InsertNew(name##_string_, name##_string_->Hash());   \
    DCHECK_NULL(entry->value);                                             \
    entry->value = reinterpret_cast<void*>(1);                             \
  }
  AST_STRING_CONSTANTS(F)
#undef F
}

AstRawString* AstValueFactory::GetOneByteStringInternal(
    Vector<const uint8_t> literal) {
  if (literal.length() == 1 && literal[0] < kMaxOneCharStringValue) {
    int key = literal[0];
    if (V8_UNLIKELY(one_character_strings_[key] == nullptr)) {
      uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(
          literal.begin(), literal.length(), hash_seed_);
      one_character_strings_[key] = GetString(hash_field, true, literal);
    }
    return one_character_strings_[key];
  }
  uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(
      literal.begin(), literal.length(), hash_seed_);
  return GetString(hash_field, true, literal);
}

AstRawString* AstValueFactory::GetTwoByteStringInternal(
    Vector<const uint16_t> literal) {
  uint32_t hash_field = StringHasher::HashSequentialString<uint16_t>(
      literal.begin(), literal.length(), hash_seed_);
  return GetString(hash_field, false, Vector<const byte>::cast(literal));
}

const AstRawString* AstValueFactory::GetString(Handle<String> literal) {
  AstRawString* result = nullptr;
  DisallowHeapAllocation no_gc;
  String::FlatContent content = literal->GetFlatContent(no_gc);
  if (content.IsOneByte()) {
    result = GetOneByteStringInternal(content.ToOneByteVector());
  } else {
    DCHECK(content.IsTwoByte());
    result = GetTwoByteStringInternal(content.ToUC16Vector());
  }
  return result;
}

const AstRawString* AstValueFactory::CloneFromOtherFactory(
    const AstRawString* raw_string) {
  const AstRawString* result = GetString(
      raw_string->hash_field(), raw_string->is_one_byte(),
      Vector<const byte>(raw_string->raw_data(), raw_string->byte_length()));
  return result;
}

AstConsString* AstValueFactory::NewConsString() {
  AstConsString* new_string = new (zone_) AstConsString;
  DCHECK_NOT_NULL(new_string);
  AddConsString(new_string);
  return new_string;
}

AstConsString* AstValueFactory::NewConsString(const AstRawString* str) {
  return NewConsString()->AddString(zone_, str);
}

AstConsString* AstValueFactory::NewConsString(const AstRawString* str1,
                                              const AstRawString* str2) {
  return NewConsString()->AddString(zone_, str1)->AddString(zone_, str2);
}

const AstRawString* AstValueFactory::Flatten(const AstConsString* str) {
  if (str->IsEmpty()) return empty_string();
  AstConsString::Segment segment = str->segment_;
  if (!segment.next) return segment.string;

  int length = segment.string->length();
  bool is_one_byte = segment.string->is_one_byte();
  while (segment.next) {
    segment = *segment.next;
    length += segment.string->length();
    is_one_byte &= segment.string->is_one_byte();
  }

  if (is_one_byte) {
    Vector<byte> data(zone()->NewArray<byte>(length), length);
    segment = str->segment_;
    byte* p = data.begin();
    while (true) {
      CopyChars(p, segment.string->raw_data(), segment.string->length());
      p += segment.string->length();
      if (!segment.next) break;
      segment = *segment.next;
    }
    return GetOneByteString(data);
  }

  Vector<uint16_t> data(zone()->NewArray<uint16_t>(length), length);
  segment = str->segment_;
  uint16_t* p = data.begin();
  while (true) {
    if (segment.string->is_one_byte()) {
      CopyChars(p, segment.string->raw_data(), segment.string->length());
    } else {
      CopyChars(p,
                reinterpret_cast<const uint16_t*>(segment.string->raw_data()),
                segment.string->length());
    }
    p += segment.string->length();
    if (!segment.next) break;
    segment = *segment.next;
  }
  return GetTwoByteString(data);
}

void AstValueFactory::Internalize(Isolate* isolate) {
  // Strings need to be internalized before values, because values refer to
  // strings.
  for (AstRawString* current = strings_; current != nullptr;) {
    AstRawString* next = current->next();
    current->Internalize(isolate);
    current = next;
  }

  // AstConsStrings refer to AstRawStrings.
  for (AstConsString* current = cons_strings_; current != nullptr;) {
    AstConsString* next = current->next();
    current->Internalize(isolate);
    current = next;
  }

  ResetStrings();
}

AstRawString* AstValueFactory::GetString(uint32_t hash_field, bool is_one_byte,
                                         Vector<const byte> literal_bytes) {
  // literal_bytes here points to whatever the user passed, and this is OK
  // because we use vector_compare (which checks the contents) to compare
  // against the AstRawStrings which are in the string_table_. We should not
  // return this AstRawString.
  AstRawString key(is_one_byte, literal_bytes, hash_field);
  base::HashMap::Entry* entry = string_table_.LookupOrInsert(&key, key.Hash());
  if (entry->value == nullptr) {
    // Copy literal contents for later comparison.
    int length = literal_bytes.length();
    byte* new_literal_bytes = zone_->NewArray<byte>(length);
    memcpy(new_literal_bytes, literal_bytes.begin(), length);
    AstRawString* new_string = new (zone_) AstRawString(
        is_one_byte, Vector<const byte>(new_literal_bytes, length), hash_field);
    CHECK_NOT_NULL(new_string);
    AddString(new_string);
    entry->key = new_string;
    entry->value = reinterpret_cast<void*>(1);
  }
  return reinterpret_cast<AstRawString*>(entry->key);
}

}  // namespace internal
}  // namespace v8
