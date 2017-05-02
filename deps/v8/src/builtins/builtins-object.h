
// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ObjectBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ObjectBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  std::tuple<Node*, Node*, Node*> EmitForInPrepare(Node* object, Node* context,
                                                   Label* call_runtime,
                                                   Label* nothing_to_iterate);

 protected:
  void IsString(Node* object, Label* if_string, Label* if_notstring);
  void ReturnToStringFormat(Node* context, Node* string);
};

}  // namespace internal
}  // namespace v8
