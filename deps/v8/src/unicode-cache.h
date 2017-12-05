// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_CACHE_H_
#define V8_UNICODE_CACHE_H_

#include "src/base/macros.h"
#include "src/char-predicates.h"
#include "src/unicode.h"
#include "src/unicode-decoder.h"

namespace v8 {
namespace internal {

// Caching predicates used by scanners.
class UnicodeCache {
 public:
  UnicodeCache() {}
  typedef unibrow::Utf8Decoder<512> Utf8Decoder;

  StaticResource<Utf8Decoder>* utf8_decoder() { return &utf8_decoder_; }

  inline bool IsIdentifierStart(unibrow::uchar c);
  inline bool IsIdentifierPart(unibrow::uchar c);
  inline bool IsLineTerminator(unibrow::uchar c);
  inline bool IsLineTerminatorSequence(unibrow::uchar c, unibrow::uchar next);

  inline bool IsWhiteSpace(unibrow::uchar c);
  inline bool IsWhiteSpaceOrLineTerminator(unibrow::uchar c);

 private:
  unibrow::Predicate<IdentifierStart, 128> kIsIdentifierStart;
  unibrow::Predicate<IdentifierPart, 128> kIsIdentifierPart;
  unibrow::Predicate<WhiteSpace, 128> kIsWhiteSpace;
  unibrow::Predicate<WhiteSpaceOrLineTerminator, 128>
      kIsWhiteSpaceOrLineTerminator;
  StaticResource<Utf8Decoder> utf8_decoder_;

  DISALLOW_COPY_AND_ASSIGN(UnicodeCache);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNICODE_CACHE_H_
