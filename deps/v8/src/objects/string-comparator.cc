// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/string-comparator.h"

#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

void StringComparator::State::Init(
    Tagged<String> string,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  Tagged<ConsString> cons_string =
      String::VisitFlat(this, string, 0, access_guard);
  iter_.Reset(cons_string);
  if (!cons_string.is_null()) {
    int offset;
    string = iter_.Next(&offset);
    // We are resetting the iterator with zero offset, so we should never have
    // a per-segment offset.
    DCHECK_EQ(offset, 0);
    String::VisitFlat(this, string, 0, access_guard);
  }
}

void StringComparator::State::Advance(
    int consumed, const SharedStringAccessGuardIfNeeded& access_guard) {
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
  Tagged<String> next = iter_.Next(&offset);
  DCHECK_EQ(0, offset);
  DCHECK(!next.is_null());
  String::VisitFlat(this, next, 0, access_guard);
}

bool StringComparator::Equals(
    Tagged<String> string_1, Tagged<String> string_2,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  int length = string_1->length();
  state_1_.Init(string_1, access_guard);
  state_2_.Init(string_2, access_guard);
  while (true) {
    int to_check = std::min(state_1_.length_, state_2_.length_);
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
    state_1_.Advance(to_check, access_guard);
    state_2_.Advance(to_check, access_guard);
  }
}

}  // namespace internal
}  // namespace v8
