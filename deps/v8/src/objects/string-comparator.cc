// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/string-comparator.h"

#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

void StringComparator::State::Init(String string) {
  ConsString cons_string = String::VisitFlat(this, string);
  iter_.Reset(cons_string);
  if (!cons_string.is_null()) {
    int offset;
    string = iter_.Next(&offset);
    String::VisitFlat(this, string, offset);
  }
}

void StringComparator::State::Advance(int consumed) {
  DCHECK(consumed <= length_);
  // Still in buffer.
  if (length_ != consumed) {
    if (is_one_byte_) {
      buffer8_ += consumed;
    } else {
      buffer16_ += consumed;
    }
    length_ -= consumed;
    return;
  }
  // Advance state.
  int offset;
  String next = iter_.Next(&offset);
  DCHECK_EQ(0, offset);
  DCHECK(!next.is_null());
  String::VisitFlat(this, next);
}

bool StringComparator::Equals(String string_1, String string_2) {
  int length = string_1->length();
  state_1_.Init(string_1);
  state_2_.Init(string_2);
  while (true) {
    int to_check = Min(state_1_.length_, state_2_.length_);
    DCHECK(to_check > 0 && to_check <= length);
    bool is_equal;
    if (state_1_.is_one_byte_) {
      if (state_2_.is_one_byte_) {
        is_equal = Equals<uint8_t, uint8_t>(&state_1_, &state_2_, to_check);
      } else {
        is_equal = Equals<uint8_t, uint16_t>(&state_1_, &state_2_, to_check);
      }
    } else {
      if (state_2_.is_one_byte_) {
        is_equal = Equals<uint16_t, uint8_t>(&state_1_, &state_2_, to_check);
      } else {
        is_equal = Equals<uint16_t, uint16_t>(&state_1_, &state_2_, to_check);
      }
    }
    // Looping done.
    if (!is_equal) return false;
    length -= to_check;
    // Exit condition. Strings are equal.
    if (length == 0) return true;
    state_1_.Advance(to_check);
    state_2_.Advance(to_check);
  }
}

}  // namespace internal
}  // namespace v8
