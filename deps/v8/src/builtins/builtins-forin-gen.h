
// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ForInBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ForInBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  std::tuple<Node*, Node*, Node*> EmitForInPrepare(Node* object, Node* context,
                                                   Label* call_runtime,
                                                   Label* nothing_to_iterate);

  Node* ForInFilter(Node* key, Node* object, Node* context);

 private:
  // Get the enumerable length from |map| and return the result as a Smi.
  Node* EnumLength(Node* map);
  void CheckPrototypeEnumCache(Node* receiver, Node* map, Label* use_cache,
                               Label* use_runtime);
  // Check the cache validity for |receiver|. Branch to |use_cache| if
  // the cache is valid, otherwise branch to |use_runtime|.
  void CheckEnumCache(Node* receiver, Label* use_cache,
                      Label* nothing_to_iterate, Label* use_runtime);
};

}  // namespace internal
}  // namespace v8
