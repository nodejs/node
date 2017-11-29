// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROXY_HELPERS_GEN_H_
#define V8_BUILTINS_BUILTINS_PROXY_HELPERS_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {
using compiler::Node;

class ProxyAssembler : public CodeStubAssembler {
 public:
  explicit ProxyAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void BranchIfAccessorPair(Node* value, Label* if_accessor_pair,
                            Label* if_not_accessor_pair) {
    GotoIf(TaggedIsSmi(value), if_not_accessor_pair);
    Branch(IsAccessorPair(value), if_accessor_pair, if_not_accessor_pair);
  }

  // ES6 section 9.5.8 [[Get]] ( P, Receiver )
  Node* ProxyGetProperty(Node* context, Node* proxy, Node* name,
                         Node* receiver);

 protected:
  void CheckGetTrapResult(Node* context, Node* target, Node* proxy, Node* name,
                          Node* trap_result, Label* if_not_found,
                          Label* if_bailout);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROXY_HELPERS_GEN_H_
