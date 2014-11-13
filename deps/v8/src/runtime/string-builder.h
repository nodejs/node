// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_STRING_BUILDER_H_
#define V8_RUNTIME_STRING_BUILDER_H_

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
      int encoded_slice = Smi::cast(element)->value();
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
        pos = Smi::cast(obj)->value();
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
      int smi_value = Smi::cast(elt)->value();
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
        pos = Smi::cast(next_smi)->value();
        if (pos < 0) return -1;
      }
      DCHECK(pos >= 0);
      DCHECK(len >= 0);
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
    DCHECK(initial_capacity > 0);
  }

  explicit FixedArrayBuilder(Handle<FixedArray> backing_store)
      : array_(backing_store), length_(0), has_non_smi_elements_(false) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    DCHECK(backing_store->length() > 0);
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
    DCHECK(estimated_part_count > 0);
  }

  static inline void AddSubjectSlice(FixedArrayBuilder* builder, int from,
                                     int to) {
    DCHECK(from >= 0);
    int length = to - from;
    DCHECK(length > 0);
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
    DCHECK(length > 0);
    AddElement(*string);
    if (!string->IsOneByteRepresentation()) {
      is_one_byte_ = false;
    }
    IncrementCharacterCount(length);
  }


  MaybeHandle<String> ToString() {
    Isolate* isolate = heap_->isolate();
    if (array_builder_.length() == 0) {
      return isolate->factory()->empty_string();
    }

    Handle<String> joined_string;
    if (is_one_byte_) {
      Handle<SeqOneByteString> seq;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, seq,
          isolate->factory()->NewRawOneByteString(character_count_), String);

      DisallowHeapAllocation no_gc;
      uint8_t* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_, char_buffer, *array_builder_.array(),
                                array_builder_.length());
      joined_string = Handle<String>::cast(seq);
    } else {
      // Two-byte.
      Handle<SeqTwoByteString> seq;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, seq,
          isolate->factory()->NewRawTwoByteString(character_count_), String);

      DisallowHeapAllocation no_gc;
      uc16* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_, char_buffer, *array_builder_.array(),
                                array_builder_.length());
      joined_string = Handle<String>::cast(seq);
    }
    return joined_string;
  }


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
}
}  // namespace v8::internal

#endif  // V8_RUNTIME_STRING_BUILDER_H_
