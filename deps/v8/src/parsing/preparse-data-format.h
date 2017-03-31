// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_FORMAT_H_
#define V8_PARSING_PREPARSE_DATA_FORMAT_H_

namespace v8 {
namespace internal {

// Generic and general data used by preparse data recorders and readers.

struct PreparseDataConstants {
 public:
  // Layout and constants of the preparse data exchange format.
  static const unsigned kMagicNumber = 0xBadDead;
  static const unsigned kCurrentVersion = 14;

  static const int kMagicOffset = 0;
  static const int kVersionOffset = 1;
  static const int kFunctionsSizeOffset = 2;
  static const int kSizeOffset = 3;
  static const int kHeaderSize = 4;

  static const unsigned char kNumberTerminator = 0x80u;
};


}  // namespace internal
}  // namespace v8.

#endif  // V8_PARSING_PREPARSE_DATA_FORMAT_H_
