// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_CACHE_H_
#define V8_UNICODE_CACHE_H_

#include "src/base/macros.h"
#include "src/unicode-decoder.h"
#include "src/unicode.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Caching predicates used by scanners.
class UnicodeCache {
 public:
  UnicodeCache() = default;
  typedef unibrow::Utf8Decoder<512> Utf8Decoder;

  StaticResource<Utf8Decoder>* utf8_decoder() { return &utf8_decoder_; }

 private:
  StaticResource<Utf8Decoder> utf8_decoder_;

  DISALLOW_COPY_AND_ASSIGN(UnicodeCache);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNICODE_CACHE_H_
