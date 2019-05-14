// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ARGUMENTS_GEN_H_
#define V8_BUILTINS_BUILTINS_ARGUMENTS_GEN_H_

#include "src/code-stub-assembler.h"
#include "torque-generated/builtins-arguments-from-dsl-gen.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;
typedef compiler::CodeAssemblerState CodeAssemblerState;
typedef compiler::CodeAssemblerLabel CodeAssemblerLabel;

class ArgumentsBuiltinsAssembler : public CodeStubAssembler,
                                   public ArgumentsBuiltinsFromDSLAssembler {
 public:
  explicit ArgumentsBuiltinsAssembler(CodeAssemblerState* state)
      : CodeStubAssembler(state), ArgumentsBuiltinsFromDSLAssembler(state) {}

  Node* EmitFastNewStrictArguments(Node* context, Node* function);
  Node* EmitFastNewSloppyArguments(Node* context, Node* function);
  Node* EmitFastNewRestParameter(Node* context, Node* function);

 private:
  // Allocates an an arguments (either rest, strict or sloppy) together with the
  // FixedArray elements for the arguments and a parameter map (for sloppy
  // arguments only). A tuple is returned with pointers to the arguments object,
  // the elements and parameter map in the form:
  // <argument object, arguments FixedArray, parameter map or nullptr>
  std::tuple<Node*, Node*, Node*> AllocateArgumentsObject(
      Node* map, Node* arguments, Node* mapped_arguments,
      ParameterMode param_mode, int base_size);

  // For Rest parameters and Strict arguments, the copying of parameters from
  // the stack into the arguments object is straight-forward and shares much of
  // the same underlying logic, which is encapsulated by this function. It
  // allocates an arguments-like object of size |base_size| with the map |map|,
  // and then copies |rest_count| arguments from the stack frame pointed to by
  // |frame_ptr| starting from |first_arg|. |arg_count| == |first_arg| +
  // |rest_count|.
  Node* ConstructParametersObjectFromArgs(Node* map, Node* frame_ptr,
                                          Node* arg_count, Node* first_arg,
                                          Node* rest_count,
                                          ParameterMode param_mode,
                                          int base_size);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ARGUMENTS_GEN_H_
