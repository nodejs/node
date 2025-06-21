// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_DOTPRINTER_H_
#define V8_REGEXP_REGEXP_DOTPRINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

class RegExpNode;

class DotPrinter final : public AllStatic {
 public:
  static void DotPrint(const char* label, RegExpNode* node);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_DOTPRINTER_H_
