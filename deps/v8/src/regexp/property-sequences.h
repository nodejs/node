// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_PROPERTY_SEQUENCES_H_
#define V8_REGEXP_PROPERTY_SEQUENCES_H_

#ifdef V8_INTL_SUPPORT

#include "src/base/strings.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class UnicodePropertySequences : public AllStatic {
 public:
  static const base::uc32 kEmojiFlagSequences[];
  static const base::uc32 kEmojiTagSequences[];
  static const base::uc32 kEmojiZWJSequences[];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INTL_SUPPORT

#endif  // V8_REGEXP_PROPERTY_SEQUENCES_H_
