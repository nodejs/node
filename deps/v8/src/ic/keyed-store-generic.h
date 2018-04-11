// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_KEYED_STORE_GENERIC_H_
#define V8_IC_KEYED_STORE_GENERIC_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

class KeyedStoreGenericGenerator {
 public:
  static void Generate(compiler::CodeAssemblerState* state);
};

class StoreICUninitializedGenerator {
 public:
  static void Generate(compiler::CodeAssemblerState* state);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_KEYED_STORE_GENERIC_H_
