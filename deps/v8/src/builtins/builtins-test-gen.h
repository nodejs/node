// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_TEST_GEN_H_
#define V8_BUILTINS_BUILTINS_TEST_GEN_H_

#include "torque-generated/builtins-base-from-dsl-gen.h"

namespace v8 {
namespace internal {

class TestBuiltinsAssembler : public BaseBuiltinsFromDSLAssembler {
 public:
  explicit TestBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : BaseBuiltinsFromDSLAssembler(state) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_TEST_GEN_H_
