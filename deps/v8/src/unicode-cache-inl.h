// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_CACHE_INL_H_
#define V8_UNICODE_CACHE_INL_H_

#include "src/unicode-inl.h"
#include "src/unicode-cache.h"

namespace v8 {
namespace internal {

bool UnicodeCache::IsIdentifierStart(unibrow::uchar c) {
  return kIsIdentifierStart.get(c);
}


bool UnicodeCache::IsIdentifierPart(unibrow::uchar c) {
  return kIsIdentifierPart.get(c);
}


bool UnicodeCache::IsLineTerminator(unibrow::uchar c) {
  return kIsLineTerminator.get(c);
}


bool UnicodeCache::IsLineTerminatorSequence(unibrow::uchar c,
                                            unibrow::uchar next) {
  if (!IsLineTerminator(c)) return false;
  if (c == 0x000d && next == 0x000a) return false;  // CR with following LF.
  return true;
}


bool UnicodeCache::IsWhiteSpace(unibrow::uchar c) {
  return kIsWhiteSpace.get(c);
}


bool UnicodeCache::IsWhiteSpaceOrLineTerminator(unibrow::uchar c) {
  return kIsWhiteSpaceOrLineTerminator.get(c);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_UNICODE_CACHE_INL_H_
