// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/string-builder.h"

namespace v8 {
namespace internal {

MaybeHandle<String> ReplacementStringBuilder::ToString() {
  Isolate* isolate = heap_->isolate();
  if (array_builder_.length() == 0) {
    return isolate->factory()->empty_string();
  }

  Handle<String> joined_string;
  if (is_one_byte_) {
    Handle<SeqOneByteString> seq;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, seq, isolate->factory()->NewRawOneByteString(character_count_),
        String);

    DisallowHeapAllocation no_gc;
    uint8_t* char_buffer = seq->GetChars();
    StringBuilderConcatHelper(*subject_, char_buffer, *array_builder_.array(),
                              array_builder_.length());
    joined_string = Handle<String>::cast(seq);
  } else {
    // Two-byte.
    Handle<SeqTwoByteString> seq;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, seq, isolate->factory()->NewRawTwoByteString(character_count_),
        String);

    DisallowHeapAllocation no_gc;
    uc16* char_buffer = seq->GetChars();
    StringBuilderConcatHelper(*subject_, char_buffer, *array_builder_.array(),
                              array_builder_.length());
    joined_string = Handle<String>::cast(seq);
  }
  return joined_string;
}


IncrementalStringBuilder::IncrementalStringBuilder(Isolate* isolate)
    : isolate_(isolate),
      encoding_(String::ONE_BYTE_ENCODING),
      overflowed_(false),
      part_length_(kInitialPartLength),
      current_index_(0) {
  // Create an accumulator handle starting with the empty string.
  accumulator_ = Handle<String>(isolate->heap()->empty_string(), isolate);
  current_part_ =
      factory()->NewRawOneByteString(part_length_).ToHandleChecked();
}


void IncrementalStringBuilder::Accumulate() {
  // Only accumulate fully written strings. Shrink first if necessary.
  DCHECK_EQ(current_index_, current_part()->length());
  Handle<String> new_accumulator;
  if (accumulator()->length() + current_part()->length() > String::kMaxLength) {
    // Set the flag and carry on. Delay throwing the exception till the end.
    new_accumulator = factory()->empty_string();
    overflowed_ = true;
  } else {
    new_accumulator = factory()
                          ->NewConsString(accumulator(), current_part())
                          .ToHandleChecked();
  }
  set_accumulator(new_accumulator);
}


void IncrementalStringBuilder::Extend() {
  Accumulate();
  if (part_length_ <= kMaxPartLength / kPartLengthGrowthFactor) {
    part_length_ *= kPartLengthGrowthFactor;
  }
  Handle<String> new_part;
  if (encoding_ == String::ONE_BYTE_ENCODING) {
    new_part = factory()->NewRawOneByteString(part_length_).ToHandleChecked();
  } else {
    new_part = factory()->NewRawTwoByteString(part_length_).ToHandleChecked();
  }
  // Reuse the same handle to avoid being invalidated when exiting handle scope.
  set_current_part(new_part);
  current_index_ = 0;
}


MaybeHandle<String> IncrementalStringBuilder::Finish() {
  ShrinkCurrentPart();
  Accumulate();
  if (overflowed_) {
    THROW_NEW_ERROR(isolate_, NewInvalidStringLengthError(), String);
  }
  return accumulator();
}


void IncrementalStringBuilder::AppendString(Handle<String> string) {
  ShrinkCurrentPart();
  part_length_ = kInitialPartLength;  // Allocate conservatively.
  Extend();  // Attach current part and allocate new part.
  Handle<String> concat =
      factory()->NewConsString(accumulator(), string).ToHandleChecked();
  set_accumulator(concat);
}
}
}  // namespace v8::internal
