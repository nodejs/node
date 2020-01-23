// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ARGUMENTS_GEN_H_
#define V8_BUILTINS_BUILTINS_ARGUMENTS_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

// TODO(v8:9396): these declarations pollute the v8::internal scope.
using CodeAssemblerState = compiler::CodeAssemblerState;
using CodeAssemblerLabel = compiler::CodeAssemblerLabel;

class ArgumentsBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ArgumentsBuiltinsAssembler(CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<JSObject> EmitFastNewStrictArguments(TNode<Context> context,
                                             TNode<JSFunction> function);
  TNode<JSObject> EmitFastNewSloppyArguments(TNode<Context> context,
                                             TNode<JSFunction> function);
  TNode<JSObject> EmitFastNewRestParameter(TNode<Context> context,
                                           TNode<JSFunction> function);

 private:
  struct ArgumentsAllocationResult {
    TNode<JSObject> arguments_object;
    TNode<FixedArray> elements;
    TNode<FixedArray> parameter_map;
  };
  // Allocates an an arguments (either rest, strict or sloppy) together with the
  // FixedArray elements for the arguments and a parameter map (for sloppy
  // arguments only, or empty TNode<> otherwise).
  ArgumentsAllocationResult AllocateArgumentsObject(
      TNode<Map> map, TNode<BInt> arguments, TNode<BInt> mapped_arguments,
      int base_size);

  // For Rest parameters and Strict arguments, the copying of parameters from
  // the stack into the arguments object is straight-forward and shares much of
  // the same underlying logic, which is encapsulated by this function. It
  // allocates an arguments-like object of size |base_size| with the map |map|,
  // and then copies |rest_count| arguments from the stack frame pointed to by
  // |frame_ptr| starting from |first_arg|. |arg_count| == |first_arg| +
  // |rest_count|.
  TNode<JSObject> ConstructParametersObjectFromArgs(
      TNode<Map> map, TNode<RawPtrT> frame_ptr, TNode<BInt> arg_count,
      TNode<BInt> first_arg, TNode<BInt> rest_count, int base_size);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ARGUMENTS_GEN_H_
