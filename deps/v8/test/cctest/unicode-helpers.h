// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_UNICODE_HELPERS_H_
#define V8_CCTEST_UNICODE_HELPERS_H_

#include "src/unicode.h"

static int Ucs2CharLength(unibrow::uchar c) {
  if (c == unibrow::Utf8::kIncomplete || c == unibrow::Utf8::kBufferEmpty) {
    return 0;
  } else if (c < 0xffff) {
    return 1;
  } else {
    return 2;
  }
}

static int Utf8LengthHelper(const char* s) {
  unibrow::Utf8::Utf8IncrementalBuffer buffer(unibrow::Utf8::kBufferEmpty);
  int length = 0;
  for (; *s != '\0'; s++) {
    unibrow::uchar tmp = unibrow::Utf8::ValueOfIncremental(*s, &buffer);
    length += Ucs2CharLength(tmp);
  }
  unibrow::uchar tmp = unibrow::Utf8::ValueOfIncrementalFinish(&buffer);
  length += Ucs2CharLength(tmp);
  return length;
}

#endif  // V8_CCTEST_UNICODE_HELPERS_H_
