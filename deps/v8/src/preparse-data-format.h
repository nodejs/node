// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PREPARSE_DATA_FORMAT_H_
#define V8_PREPARSE_DATA_FORMAT_H_

namespace v8 {
namespace internal {

// Generic and general data used by preparse data recorders and readers.

struct PreparseDataConstants {
 public:
  // Layout and constants of the preparse data exchange format.
  static const unsigned kMagicNumber = 0xBadDead;
  static const unsigned kCurrentVersion = 11;

  static const int kMagicOffset = 0;
  static const int kVersionOffset = 1;
  static const int kHasErrorOffset = 2;
  static const int kFunctionsSizeOffset = 3;
  static const int kSizeOffset = 4;
  static const int kHeaderSize = 5;

  // If encoding a message, the following positions are fixed.
  static const int kMessageStartPos = 0;
  static const int kMessageEndPos = 1;
  static const int kMessageArgCountPos = 2;
  static const int kParseErrorTypePos = 3;
  static const int kMessageTemplatePos = 4;
  static const int kMessageArgPos = 5;

  static const unsigned char kNumberTerminator = 0x80u;
};


} }  // namespace v8::internal.

#endif  // V8_PREPARSE_DATA_FORMAT_H_
