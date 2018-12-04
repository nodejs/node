// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/unicode-helpers.h"

int Ucs2CharLength(unibrow::uchar c) {
  if (c == unibrow::Utf8::kIncomplete || c == unibrow::Utf8::kBufferEmpty) {
    return 0;
  } else if (c < 0xFFFF) {
    return 1;
  } else {
    return 2;
  }
}

int Utf8LengthHelper(const char* s) {
  unibrow::Utf8::Utf8IncrementalBuffer buffer(unibrow::Utf8::kBufferEmpty);
  unibrow::Utf8::State state = unibrow::Utf8::State::kAccept;

  int length = 0;
  size_t i = 0;
  while (s[i] != '\0') {
    unibrow::uchar tmp =
        unibrow::Utf8::ValueOfIncremental(s[i], &i, &state, &buffer);
    length += Ucs2CharLength(tmp);
  }
  unibrow::uchar tmp = unibrow::Utf8::ValueOfIncrementalFinish(&state);
  length += Ucs2CharLength(tmp);
  return length;
}
