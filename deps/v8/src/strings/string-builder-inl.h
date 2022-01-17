// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_STRING_BUILDER_INL_H_
#define V8_STRINGS_STRING_BUILDER_INL_H_

#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/objects/string-inl.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

const int kStringBuilderConcatHelperLengthBits = 11;
const int kStringBuilderConcatHelperPositionBits = 19;

using StringBuilderSubstringLength =
    base::BitField<int, 0, kStringBuilderConcatHelperLengthBits>;
using StringBuilderSubstringPosition =
    base::BitField<int, kStringBuilderConcatHelperLengthBits,
                   kStringBuilderConcatHelperPositionBits>;

template <typename sinkchar>
void StringBuilderConcatHelper(String special, sinkchar* sink,
                               FixedArray fixed_array, int array_length);

// Returns the result length of the concatenation.
// On illegal argument, -1 is returned.
int StringBuilderConcatLength(int special_length, FixedArray fixed_array,
                              int array_length, bool* one_byte);

class FixedArrayBuilder {
 public:
  explicit FixedArrayBuilder(Isolate* isolate, int initial_capacity);
  explicit FixedArrayBuilder(Handle<FixedArray> backing_store);

  bool HasCapacity(int elements);
  void EnsureCapacity(Isolate* isolate, int elements);

  void Add(Object value);
  void Add(Smi value);

  Handle<FixedArray> array() { return array_; }

  int length() { return length_; }

  int capacity();

  Handle<JSArray> ToJSArray(Handle<JSArray> target_array);

 private:
  Handle<FixedArray> array_;
  int length_;
  bool has_non_smi_elements_;
};

class ReplacementStringBuilder {
 public:
  ReplacementStringBuilder(Heap* heap, Handle<String> subject,
                           int estimated_part_count);

  // Caution: Callers must ensure the builder has enough capacity.
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

  void AddSubjectSlice(int from, int to) {
    EnsureCapacity(2);  // Subject slices are encoded with up to two smis.
    AddSubjectSlice(&array_builder_, from, to);
    IncrementCharacterCount(to - from);
  }

  void AddString(Handle<String> string);

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
  void AddElement(Handle<Object> element);
  void EnsureCapacity(int elements);

  Heap* heap_;
  FixedArrayBuilder array_builder_;
  Handle<String> subject_;
  int character_count_;
  bool is_one_byte_;
};

class IncrementalStringBuilder {
 public:
  explicit IncrementalStringBuilder(Isolate* isolate);

  V8_INLINE String::Encoding CurrentEncoding() { return encoding_; }

  template <typename SrcChar, typename DestChar>
  V8_INLINE void Append(SrcChar c);

  V8_INLINE void AppendCharacter(uint8_t c) {
    if (encoding_ == String::ONE_BYTE_ENCODING) {
      Append<uint8_t, uint8_t>(c);
    } else {
      Append<uint8_t, base::uc16>(c);
    }
  }

  V8_INLINE void AppendCString(const char* s) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(s);
    if (encoding_ == String::ONE_BYTE_ENCODING) {
      while (*u != '\0') Append<uint8_t, uint8_t>(*(u++));
    } else {
      while (*u != '\0') Append<uint8_t, base::uc16>(*(u++));
    }
  }

  V8_INLINE void AppendCString(const base::uc16* s) {
    if (encoding_ == String::ONE_BYTE_ENCODING) {
      while (*s != '\0') Append<base::uc16, uint8_t>(*(s++));
    } else {
      while (*s != '\0') Append<base::uc16, base::uc16>(*(s++));
    }
  }

  V8_INLINE void AppendInt(int i) {
    char buffer[kIntToCStringBufferSize];
    const char* str =
        IntToCString(i, base::Vector<char>(buffer, kIntToCStringBufferSize));
    AppendCString(str);
  }

  V8_INLINE bool CurrentPartCanFit(int length) {
    return part_length_ - current_index_ > length;
  }

  // We make a rough estimate to find out if the current string can be
  // serialized without allocating a new string part. The worst case length of
  // an escaped character is 6. Shifting the remaining string length right by 3
  // is a more pessimistic estimate, but faster to calculate.
  V8_INLINE int EscapedLengthIfCurrentPartFits(int length) {
    if (length > kMaxPartLength) return 0;
    STATIC_ASSERT((kMaxPartLength << 3) <= String::kMaxLength);
    // This shift will not overflow because length is already less than the
    // maximum part length.
    int worst_case_length = length << 3;
    return CurrentPartCanFit(worst_case_length) ? worst_case_length : 0;
  }

  void AppendString(Handle<String> string);

  MaybeHandle<String> Finish();

  V8_INLINE bool HasOverflowed() const { return overflowed_; }

  int Length() const;

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
    NoExtend(Handle<String> string, int offset,
             const DisallowGarbageCollection& no_gc) {
      DCHECK(string->IsSeqOneByteString() || string->IsSeqTwoByteString());
      if (sizeof(DestChar) == 1) {
        start_ = reinterpret_cast<DestChar*>(
            Handle<SeqOneByteString>::cast(string)->GetChars(no_gc) + offset);
      } else {
        start_ = reinterpret_cast<DestChar*>(
            Handle<SeqTwoByteString>::cast(string)->GetChars(no_gc) + offset);
      }
      cursor_ = start_;
    }

    V8_INLINE void Append(DestChar c) { *(cursor_++) = c; }
    V8_INLINE void AppendCString(const char* s) {
      const uint8_t* u = reinterpret_cast<const uint8_t*>(s);
      while (*u != '\0') Append(*(u++));
    }

    int written() { return static_cast<int>(cursor_ - start_); }

   private:
    DestChar* start_;
    DestChar* cursor_;
    DISALLOW_GARBAGE_COLLECTION(no_gc_)
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
    NoExtendBuilder(IncrementalStringBuilder* builder, int required_length,
                    const DisallowGarbageCollection& no_gc)
        : NoExtend<DestChar>(builder->current_part(), builder->current_index_,
                             no_gc),
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

  V8_INLINE Handle<String> accumulator() { return accumulator_; }

  V8_INLINE void set_accumulator(Handle<String> string) {
    accumulator_.PatchValue(*string);
  }

  V8_INLINE Handle<String> current_part() { return current_part_; }

  V8_INLINE void set_current_part(Handle<String> string) {
    current_part_.PatchValue(*string);
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

  void AppendStringByCopy(Handle<String> string);
  bool CanAppendByCopy(Handle<String> string);

  static const int kInitialPartLength = 32;
  static const int kMaxPartLength = 16 * 1024;
  static const int kPartLengthGrowthFactor = 2;
  static const int kIntToCStringBufferSize = 100;

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
        .SeqOneByteStringSet(current_index_++, c);
  } else {
    DCHECK_EQ(String::TWO_BYTE_ENCODING, encoding_);
    SeqTwoByteString::cast(*current_part_)
        .SeqTwoByteStringSet(current_index_++, c);
  }
  if (current_index_ == part_length_) Extend();
}
}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_STRING_BUILDER_INL_H_
