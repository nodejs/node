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

#include "src/ast-value-factory.h"

#include "src/api.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

namespace {

// For using StringToArrayIndex.
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

}

class AstRawStringInternalizationKey : public HashTableKey {
 public:
  explicit AstRawStringInternalizationKey(const AstRawString* string)
      : string_(string) {}

  virtual bool IsMatch(Object* other) V8_OVERRIDE {
    if (string_->is_one_byte_)
      return String::cast(other)->IsOneByteEqualTo(string_->literal_bytes_);
    return String::cast(other)->IsTwoByteEqualTo(
        Vector<const uint16_t>::cast(string_->literal_bytes_));
  }

  virtual uint32_t Hash() V8_OVERRIDE {
    return string_->hash() >> Name::kHashShift;
  }

  virtual uint32_t HashForObject(Object* key) V8_OVERRIDE {
    return String::cast(key)->Hash();
  }

  virtual Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    if (string_->is_one_byte_)
      return isolate->factory()->NewOneByteInternalizedString(
          string_->literal_bytes_, string_->hash());
    return isolate->factory()->NewTwoByteInternalizedString(
        Vector<const uint16_t>::cast(string_->literal_bytes_), string_->hash());
  }

 private:
  const AstRawString* string_;
};


void AstRawString::Internalize(Isolate* isolate) {
  if (!string_.is_null()) return;
  if (literal_bytes_.length() == 0) {
    string_ = isolate->factory()->empty_string();
  } else {
    AstRawStringInternalizationKey key(this);
    string_ = StringTable::LookupKey(isolate, &key);
  }
}


bool AstRawString::AsArrayIndex(uint32_t* index) const {
  if (!string_.is_null())
    return string_->AsArrayIndex(index);
  if (!is_one_byte_ || literal_bytes_.length() == 0 ||
      literal_bytes_.length() > String::kMaxArrayIndexSize)
    return false;
  OneByteStringStream stream(literal_bytes_);
  return StringToArrayIndex(&stream, index);
}


bool AstRawString::IsOneByteEqualTo(const char* data) const {
  int length = static_cast<int>(strlen(data));
  if (is_one_byte_ && literal_bytes_.length() == length) {
    const char* token = reinterpret_cast<const char*>(literal_bytes_.start());
    return !strncmp(token, data, length);
  }
  return false;
}


bool AstRawString::Compare(void* a, void* b) {
  AstRawString* string1 = reinterpret_cast<AstRawString*>(a);
  AstRawString* string2 = reinterpret_cast<AstRawString*>(b);
  if (string1->is_one_byte_ != string2->is_one_byte_) return false;
  if (string1->hash_ != string2->hash_) return false;
  int length = string1->literal_bytes_.length();
  if (string2->literal_bytes_.length() != length) return false;
  return memcmp(string1->literal_bytes_.start(),
                string2->literal_bytes_.start(), length) == 0;
}


void AstConsString::Internalize(Isolate* isolate) {
  // AstRawStrings are internalized before AstConsStrings so left and right are
  // already internalized.
  string_ = isolate->factory()
                ->NewConsString(left_->string(), right_->string())
                .ToHandleChecked();
}


bool AstValue::IsPropertyName() const {
  if (type_ == STRING) {
    uint32_t index;
    return !string_->AsArrayIndex(&index);
  }
  return false;
}


bool AstValue::BooleanValue() const {
  switch (type_) {
    case STRING:
      DCHECK(string_ != NULL);
      return !string_->IsEmpty();
    case SYMBOL:
      UNREACHABLE();
      break;
    case NUMBER:
      return DoubleToBoolean(number_);
    case SMI:
      return smi_ != 0;
    case STRING_ARRAY:
      UNREACHABLE();
      break;
    case BOOLEAN:
      return bool_;
    case NULL_TYPE:
      return false;
    case THE_HOLE:
      UNREACHABLE();
      break;
    case UNDEFINED:
      return false;
  }
  UNREACHABLE();
  return false;
}


void AstValue::Internalize(Isolate* isolate) {
  switch (type_) {
    case STRING:
      DCHECK(string_ != NULL);
      // Strings are already internalized.
      DCHECK(!string_->string().is_null());
      break;
    case SYMBOL:
      value_ = Object::GetProperty(
                   isolate, handle(isolate->native_context()->builtins()),
                   symbol_name_).ToHandleChecked();
      break;
    case NUMBER:
      value_ = isolate->factory()->NewNumber(number_, TENURED);
      break;
    case SMI:
      value_ = handle(Smi::FromInt(smi_), isolate);
      break;
    case BOOLEAN:
      if (bool_) {
        value_ = isolate->factory()->true_value();
      } else {
        value_ = isolate->factory()->false_value();
      }
      break;
    case STRING_ARRAY: {
      DCHECK(strings_ != NULL);
      Factory* factory = isolate->factory();
      int len = strings_->length();
      Handle<FixedArray> elements = factory->NewFixedArray(len, TENURED);
      for (int i = 0; i < len; i++) {
        const AstRawString* string = (*strings_)[i];
        Handle<Object> element = string->string();
        // Strings are already internalized.
        DCHECK(!element.is_null());
        elements->set(i, *element);
      }
      value_ =
          factory->NewJSArrayWithElements(elements, FAST_ELEMENTS, TENURED);
      break;
    }
    case NULL_TYPE:
      value_ = isolate->factory()->null_value();
      break;
    case THE_HOLE:
      value_ = isolate->factory()->the_hole_value();
      break;
    case UNDEFINED:
      value_ = isolate->factory()->undefined_value();
      break;
  }
}


const AstRawString* AstValueFactory::GetOneByteString(
    Vector<const uint8_t> literal) {
  uint32_t hash = StringHasher::HashSequentialString<uint8_t>(
      literal.start(), literal.length(), hash_seed_);
  return GetString(hash, true, literal);
}


const AstRawString* AstValueFactory::GetTwoByteString(
    Vector<const uint16_t> literal) {
  uint32_t hash = StringHasher::HashSequentialString<uint16_t>(
      literal.start(), literal.length(), hash_seed_);
  return GetString(hash, false, Vector<const byte>::cast(literal));
}


const AstRawString* AstValueFactory::GetString(Handle<String> literal) {
  DisallowHeapAllocation no_gc;
  String::FlatContent content = literal->GetFlatContent();
  if (content.IsAscii()) {
    return GetOneByteString(content.ToOneByteVector());
  }
  DCHECK(content.IsTwoByte());
  return GetTwoByteString(content.ToUC16Vector());
}


const AstConsString* AstValueFactory::NewConsString(
    const AstString* left, const AstString* right) {
  // This Vector will be valid as long as the Collector is alive (meaning that
  // the AstRawString will not be moved).
  AstConsString* new_string = new (zone_) AstConsString(left, right);
  strings_.Add(new_string);
  if (isolate_) {
    new_string->Internalize(isolate_);
  }
  return new_string;
}


void AstValueFactory::Internalize(Isolate* isolate) {
  if (isolate_) {
    // Everything is already internalized.
    return;
  }
  // Strings need to be internalized before values, because values refer to
  // strings.
  for (int i = 0; i < strings_.length(); ++i) {
    strings_[i]->Internalize(isolate);
  }
  for (int i = 0; i < values_.length(); ++i) {
    values_[i]->Internalize(isolate);
  }
  isolate_ = isolate;
}


const AstValue* AstValueFactory::NewString(const AstRawString* string) {
  AstValue* value = new (zone_) AstValue(string);
  DCHECK(string != NULL);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewSymbol(const char* name) {
  AstValue* value = new (zone_) AstValue(name);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewNumber(double number) {
  AstValue* value = new (zone_) AstValue(number);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewSmi(int number) {
  AstValue* value =
      new (zone_) AstValue(AstValue::SMI, number);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewBoolean(bool b) {
  AstValue* value = new (zone_) AstValue(b);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewStringList(
    ZoneList<const AstRawString*>* strings) {
  AstValue* value = new (zone_) AstValue(strings);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewNull() {
  AstValue* value = new (zone_) AstValue(AstValue::NULL_TYPE);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewUndefined() {
  AstValue* value = new (zone_) AstValue(AstValue::UNDEFINED);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstValue* AstValueFactory::NewTheHole() {
  AstValue* value = new (zone_) AstValue(AstValue::THE_HOLE);
  if (isolate_) {
    value->Internalize(isolate_);
  }
  values_.Add(value);
  return value;
}


const AstRawString* AstValueFactory::GetString(
    uint32_t hash, bool is_one_byte, Vector<const byte> literal_bytes) {
  // literal_bytes here points to whatever the user passed, and this is OK
  // because we use vector_compare (which checks the contents) to compare
  // against the AstRawStrings which are in the string_table_. We should not
  // return this AstRawString.
  AstRawString key(is_one_byte, literal_bytes, hash);
  HashMap::Entry* entry = string_table_.Lookup(&key, hash, true);
  if (entry->value == NULL) {
    // Copy literal contents for later comparison.
    int length = literal_bytes.length();
    byte* new_literal_bytes = zone_->NewArray<byte>(length);
    memcpy(new_literal_bytes, literal_bytes.start(), length);
    AstRawString* new_string = new (zone_) AstRawString(
        is_one_byte, Vector<const byte>(new_literal_bytes, length), hash);
    entry->key = new_string;
    strings_.Add(new_string);
    if (isolate_) {
      new_string->Internalize(isolate_);
    }
    entry->value = reinterpret_cast<void*>(1);
  }
  return reinterpret_cast<AstRawString*>(entry->key);
}


} }  // namespace v8::internal
