// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_BUILDER_H_
#define V8_STRING_BUILDER_H_

#include "src/assert-scope.h"
#include "src/handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

const int kStringBuilderConcatHelperLengthBits = 11;
const int kStringBuilderConcatHelperPositionBits = 19;

typedef BitField<int, 0, kStringBuilderConcatHelperLengthBits>
    StringBuilderSubstringLength;
typedef BitField<int, kStringBuilderConcatHelperLengthBits,
                 kStringBuilderConcatHelperPositionBits>
    StringBuilderSubstringPosition;


template <typename sinkchar>
static inline void StringBuilderConcatHelper(String* special, sinkchar* sink,
                                             FixedArray* fixed_array,
                                             int array_length) {
  DisallowHeapAllocation no_gc;
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    Object* element = fixed_array->get(i);
    if (element->IsSmi()) {
      // Smi encoding of position and length.
      int encoded_slice = Smi::ToInt(element);
      int pos;
      int len;
      if (encoded_slice > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(encoded_slice);
        len = StringBuilderSubstringLength::decode(encoded_slice);
      } else {
        // Position and length encoded in two smis.
        Object* obj = fixed_array->get(++i);
        DCHECK(obj->IsSmi());
        pos = Smi::ToInt(obj);
        len = -encoded_slice;
      }
      String::WriteToFlat(special, sink + position, pos, pos + len);
      position += len;
    } else {
      String* string = String::cast(element);
      int element_length = string->length();
      String::WriteToFlat(string, sink + position, 0, element_length);
      position += element_length;
    }
  }
}


// Returns the result length of the concatenation.
// On illegal argument, -1 is returned.
static inline int StringBuilderConcatLength(int special_length,
                                            FixedArray* fixed_array,
                                            int array_length, bool* one_byte) {
  DisallowHeapAllocation no_gc;
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    int increment = 0;
    Object* elt = fixed_array->get(i);
    if (elt->IsSmi()) {
      // Smi encoding of position and length.
      int smi_value = Smi::ToInt(elt);
      int pos;
      int len;
      if (smi_value > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(smi_value);
        len = StringBuilderSubstringLength::decode(smi_value);
      } else {
        // Position and length encoded in two smis.
        len = -smi_value;
        // Get the position and check that it is a positive smi.
        i++;
        if (i >= array_length) return -1;
        Object* next_smi = fixed_array->get(i);
        if (!next_smi->IsSmi()) return -1;
        pos = Smi::ToInt(next_smi);
        if (pos < 0) return -1;
      }
      DCHECK_GE(pos, 0);
      DCHECK_GE(len, 0);
      if (pos > special_length || len > special_length - pos) return -1;
      increment = len;
    } else if (elt->IsString()) {
      String* element = String::cast(elt);
      int element_length = element->length();
      increment = element_length;
      if (*one_byte && !element->HasOnlyOneByteChars()) {
        *one_byte = false;
      }
    } else {
      return -1;
    }
    if (increment > String::kMaxLength - position) {
      return kMaxInt;  // Provoke throw on allocation.
    }
    position += increment;
  }
  return position;
}


class FixedArrayBuilder {
 public:
  explicit FixedArrayBuilder(Isolate* isolate, int initial_capacity)
      : array_(isolate->factory()->NewFixedArrayWithHoles(initial_capacity)),
        length_(0),
        has_non_smi_elements_(false) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    DCHECK_GT(initial_capacity, 0);
  }

  explicit FixedArrayBuilder(Handle<FixedArray> backing_store)
      : array_(backing_store), length_(0), has_non_smi_elements_(false) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    DCHECK_GT(backing_store->length(), 0);
  }

  bool HasCapacity(int elements) {
    int length = array_->length();
    int required_length = length_ + elements;
    return (length >= required_length);
  }

  void EnsureCapacity(int elements) {
    int length = array_->length();
    int required_length = length_ + elements;
    if (length < required_length) {
      int new_length = length;
      do {
        new_length *= 2;
      } while (new_length < required_length);
      Handle<FixedArray> extended_array =
          array_->GetIsolate()->factory()->NewFixedArrayWithHoles(new_length);
      array_->CopyTo(0, *extended_array, 0, length_);
      array_ = extended_array;
    }
  }

  void Add(Object* value) {
    DCHECK(!value->IsSmi());
    DCHECK(length_ < capacity());
    array_->set(length_, value);
    length_++;
    has_non_smi_elements_ = true;
  }

  void Add(Smi* value) {
    DCHECK(value->IsSmi());
    DCHECK(length_ < capacity());
    array_->set(length_, value);
    length_++;
  }

  Handle<FixedArray> array() { return array_; }

  int length() { return length_; }

  int capacity() { return array_->length(); }

  Handle<JSArray> ToJSArray(Handle<JSArray> target_array) {
    JSArray::SetContent(target_array, array_);
    target_array->set_length(Smi::FromInt(length_));
    return target_array;
  }

 private:
  Handle<FixedArray> array_;
  int length_;
  bool has_non_smi_elements_;
};


class ReplacementStringBuilder {
 public:
  ReplacementStringBuilder(Heap* heap, Handle<String> subject,
                           int estimated_part_count)
      : heap_(heap),
        array_builder_(heap->isolate(), estimated_part_count),
        subject_(subject),
        character_count_(0),
        is_one_byte_(subject->IsOneByteRepresentation()) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    DCHECK_GT(estimated_part_count, 0);
  }

  static inline void AddSubjectSlice(FixedArrayBuilder* builder, int from,
                                     int to) {
    DCHECK_GE(from, 0);
    int length = to - from;
    DCHECK_GT(length, 0);
    if (StringBuilderSubstringLength::is_valid(length) &&
        StringBuilderSubstringPosition::is_valid(from)) {
      int encoded_slice = StringBuilderSubstringLength::encode(length) |
                          StringBuilderSubstringPosition::encode(from);
      builder->Add(Smi::FromInt(encoded_slice));
    } else {
      // Otherwise encode as two smis.
      builder->Add(Smi::FromInt(-length));
      builder->Add(Smi::FromInt(from));
    }
  }


  void EnsureCapacity(int elements) { array_builder_.EnsureCapacity(elements); }


  void AddSubjectSlice(int from, int to) {
    AddSubjectSlice(&array_builder_, from, to);
    IncrementCharacterCount(to - from);
  }


  void AddString(Handle<String> string) {
    int length = string->length();
    DCHECK_GT(length, 0);
    AddElement(*string);
    if (!string->IsOneByteRepresentation()) {
      is_one_byte_ = false;
    }
    IncrementCharacterCount(length);
  }


  MaybeHandle<String> ToString();


  void IncrementCharacterCount(int by) {
    if (character_count_ > String::kMaxLength - by) {
      STATIC_ASSERT(String::kMaxLength < kMaxInt);
      character_count_ = kMaxInt;
    } else {
      character_count_ += by;
    }
  }

 private:
  void AddElement(Object* element) {
    DCHECK(element->IsSmi() || element->IsString());
    DCHECK(array_builder_.capacity() > array_builder_.length());
    array_builder_.Add(element);
  }

  Heap* heap_;
  FixedArrayBuilder array_builder_;
  Handle<String> subject_;
  int character_count_;
  bool is_one_byte_;
};


class IncrementalStringBuilder {
 public:
  explicit IncrementalStringBuilder(Isolate* isolate);

  INLINE(String::Encoding CurrentEncoding()) { return encoding_; }

  template <typename SrcChar, typename DestChar>
  INLINE(void Append(SrcChar c));

  INLINE(void AppendCharacter(uint8_t c)) {
    if (encoding_ == String::ONE_BYTE_ENCODING) {
      Append<uint8_t, uint8_t>(c);
    } else {
      Append<uint8_t, uc16>(c);
    }
  }

  INLINE(void AppendCString(const char* s)) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(s);
    if (encoding_ == String::ONE_BYTE_ENCODING) {
      while (*u != '\0') Append<uint8_t, uint8_t>(*(u++));
    } else {
      while (*u != '\0') Append<uint8_t, uc16>(*(u++));
    }
  }

  INLINE(void AppendCString(const uc16* s)) {
    if (encoding_ == String::ONE_BYTE_ENCODING) {
      while (*s != '\0') Append<uc16, uint8_t>(*(s++));
    } else {
      while (*s != '\0') Append<uc16, uc16>(*(s++));
    }
  }

  INLINE(bool CurrentPartCanFit(int length)) {
    return part_length_ - current_index_ > length;
  }

  // We make a rough estimate to find out if the current string can be
  // serialized without allocating a new string part. The worst case length of
  // an escaped character is 6. Shifting the remaining string length right by 3
  // is a more pessimistic estimate, but faster to calculate.
  INLINE(int EscapedLengthIfCurrentPartFits(int length)) {
    if (length > kMaxPartLength) return 0;
    STATIC_ASSERT((kMaxPartLength << 3) <= String::kMaxLength);
    // This shift will not overflow because length is already less than the
    // maximum part length.
    int worst_case_length = length << 3;
    return CurrentPartCanFit(worst_case_length) ? worst_case_length : 0;
  }

  void AppendString(Handle<String> string);

  MaybeHandle<String> Finish();

  INLINE(bool HasOverflowed()) const { return overflowed_; }

  INLINE(int Length()) const { return accumulator_->length() + current_index_; }

  // Change encoding to two-byte.
  void ChangeEncoding() {
    DCHECK_EQ(String::ONE_BYTE_ENCODING, encoding_);
    ShrinkCurrentPart();
    encoding_ = String::TWO_BYTE_ENCODING;
    Extend();
  }

  template <typename DestChar>
  class NoExtend {
   public:
    explicit NoExtend(Handle<String> string, int offset) {
      DCHECK(string->IsSeqOneByteString() || string->IsSeqTwoByteString());
      if (sizeof(DestChar) == 1) {
        start_ = reinterpret_cast<DestChar*>(
            Handle<SeqOneByteString>::cast(string)->GetChars() + offset);
      } else {
        start_ = reinterpret_cast<DestChar*>(
            Handle<SeqTwoByteString>::cast(string)->GetChars() + offset);
      }
      cursor_ = start_;
    }

    INLINE(void Append(DestChar c)) { *(cursor_++) = c; }
    INLINE(void AppendCString(const char* s)) {
      const uint8_t* u = reinterpret_cast<const uint8_t*>(s);
      while (*u != '\0') Append(*(u++));
    }

    int written() { return static_cast<int>(cursor_ - start_); }

   private:
    DestChar* start_;
    DestChar* cursor_;
    DisallowHeapAllocation no_gc_;
  };

  template <typename DestChar>
  class NoExtendString : public NoExtend<DestChar> {
   public:
    NoExtendString(Handle<String> string, int required_length)
        : NoExtend<DestChar>(string, 0), string_(string) {
      DCHECK(string->length() >= required_length);
    }

    Handle<String> Finalize() {
      Handle<SeqString> string = Handle<SeqString>::cast(string_);
      int length = NoExtend<DestChar>::written();
      Handle<String> result = SeqString::Truncate(string, length);
      string_ = Handle<String>();
      return result;
    }

   private:
    Handle<String> string_;
  };

  template <typename DestChar>
  class NoExtendBuilder : public NoExtend<DestChar> {
   public:
    NoExtendBuilder(IncrementalStringBuilder* builder, int required_length)
        : NoExtend<DestChar>(builder->current_part(), builder->current_index_),
          builder_(builder) {
      DCHECK(builder->CurrentPartCanFit(required_length));
    }

    ~NoExtendBuilder() {
      builder_->current_index_ += NoExtend<DestChar>::written();
    }

   private:
    IncrementalStringBuilder* builder_;
  };

 private:
  Factory* factory() { return isolate_->factory(); }

  INLINE(Handle<String> accumulator()) { return accumulator_; }

  INLINE(void set_accumulator(Handle<String> string)) {
    *accumulator_.location() = *string;
  }

  INLINE(Handle<String> current_part()) { return current_part_; }

  INLINE(void set_current_part(Handle<String> string)) {
    *current_part_.location() = *string;
  }

  // Add the current part to the accumulator.
  void Accumulate(Handle<String> new_part);

  // Finish the current part and allocate a new part.
  void Extend();

  // Shrink current part to the right size.
  void ShrinkCurrentPart() {
    DCHECK(current_index_ < part_length_);
    set_current_part(SeqString::Truncate(
        Handle<SeqString>::cast(current_part()), current_index_));
  }

  static const int kInitialPartLength = 32;
  static const int kMaxPartLength = 16 * 1024;
  static const int kPartLengthGrowthFactor = 2;

  Isolate* isolate_;
  String::Encoding encoding_;
  bool overflowed_;
  int part_length_;
  int current_index_;
  Handle<String> accumulator_;
  Handle<String> current_part_;
};


template <typename SrcChar, typename DestChar>
void IncrementalStringBuilder::Append(SrcChar c) {
  DCHECK_EQ(encoding_ == String::ONE_BYTE_ENCODING, sizeof(DestChar) == 1);
  if (sizeof(DestChar) == 1) {
    DCHECK_EQ(String::ONE_BYTE_ENCODING, encoding_);
    SeqOneByteString::cast(*current_part_)
        ->SeqOneByteStringSet(current_index_++, c);
  } else {
    DCHECK_EQ(String::TWO_BYTE_ENCODING, encoding_);
    SeqTwoByteString::cast(*current_part_)
        ->SeqTwoByteStringSet(current_index_++, c);
  }
  if (current_index_ == part_length_) Extend();
}
}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_BUILDER_H_
