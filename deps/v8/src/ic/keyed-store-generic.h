// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SRC_IC_KEYED_STORE_GENERIC_H_
#define V8_SRC_IC_KEYED_STORE_GENERIC_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class KeyedStoreGenericGenerator {
 public:
  static void Generate(CodeStubAssembler* assembler,
                       const CodeStubAssembler::StoreICParameters* p,
                       LanguageMode language_mode);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SRC_IC_KEYED_STORE_GENERIC_H_
