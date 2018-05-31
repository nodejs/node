// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ARRAY_GEN_H_
#define V8_BUILTINS_BUILTINS_ARRAY_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ArrayBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ArrayBuiltinsAssembler(compiler::CodeAssemblerState* state);

  typedef std::function<void(ArrayBuiltinsAssembler* masm)>
      BuiltinResultGenerator;

  typedef std::function<Node*(ArrayBuiltinsAssembler* masm, Node* k_value,
                              Node* k)>
      CallResultProcessor;

  typedef std::function<void(ArrayBuiltinsAssembler* masm)> PostLoopAction;

  enum class MissingPropertyMode { kSkip, kUseUndefined };

  void FindResultGenerator();

  Node* FindProcessor(Node* k_value, Node* k);

  void FindIndexResultGenerator();

  Node* FindIndexProcessor(Node* k_value, Node* k);

  void ForEachResultGenerator();

  Node* ForEachProcessor(Node* k_value, Node* k);

  void SomeResultGenerator();

  Node* SomeProcessor(Node* k_value, Node* k);

  void EveryResultGenerator();

  Node* EveryProcessor(Node* k_value, Node* k);

  void ReduceResultGenerator();

  Node* ReduceProcessor(Node* k_value, Node* k);

  void ReducePostLoopAction();

  void FilterResultGenerator();

  Node* FilterProcessor(Node* k_value, Node* k);

  void MapResultGenerator();

  void TypedArrayMapResultGenerator();

  Node* SpecCompliantMapProcessor(Node* k_value, Node* k);

  Node* FastMapProcessor(Node* k_value, Node* k);

  // See tc39.github.io/ecma262/#sec-%typedarray%.prototype.map.
  Node* TypedArrayMapProcessor(Node* k_value, Node* k);

  void NullPostLoopAction();

 protected:
  TNode<Context> context() { return context_; }
  TNode<Object> receiver() { return receiver_; }
  Node* new_target() { return new_target_; }
  TNode<IntPtrT> argc() { return argc_; }
  TNode<JSReceiver> o() { return o_; }
  TNode<Number> len() { return len_; }
  Node* callbackfn() { return callbackfn_; }
  Node* this_arg() { return this_arg_; }
  Node* k() { return k_.value(); }
  Node* a() { return a_.value(); }

  void ReturnFromBuiltin(Node* value);

  void InitIteratingArrayBuiltinBody(TNode<Context> context,
                                     TNode<Object> receiver, Node* callbackfn,
                                     Node* this_arg, Node* new_target,
                                     TNode<IntPtrT> argc);

  void GenerateIteratingArrayBuiltinBody(
      const char* name, const BuiltinResultGenerator& generator,
      const CallResultProcessor& processor, const PostLoopAction& action,
      const Callable& slow_case_continuation,
      MissingPropertyMode missing_property_mode,
      ForEachDirection direction = ForEachDirection::kForward);
  void InitIteratingArrayBuiltinLoopContinuation(
      TNode<Context> context, TNode<Object> receiver, Node* callbackfn,
      Node* this_arg, Node* a, TNode<JSReceiver> o, Node* initial_k,
      TNode<Number> len, Node* to);

  void GenerateIteratingTypedArrayBuiltinBody(
      const char* name, const BuiltinResultGenerator& generator,
      const CallResultProcessor& processor, const PostLoopAction& action,
      ForEachDirection direction = ForEachDirection::kForward);

  void GenerateIteratingArrayBuiltinLoopContinuation(
      const CallResultProcessor& processor, const PostLoopAction& action,
      MissingPropertyMode missing_property_mode,
      ForEachDirection direction = ForEachDirection::kForward);

 private:
  static ElementsKind ElementsKindForInstanceType(InstanceType type);

  void VisitAllTypedArrayElements(Node* array_buffer,
                                  const CallResultProcessor& processor,
                                  Label* detached, ForEachDirection direction,
                                  TNode<JSTypedArray> typed_array);

  void VisitAllFastElementsOneKind(ElementsKind kind,
                                   const CallResultProcessor& processor,
                                   Label* array_changed, ParameterMode mode,
                                   ForEachDirection direction,
                                   MissingPropertyMode missing_property_mode,
                                   TNode<Smi> length);

  void HandleFastElements(const CallResultProcessor& processor,
                          const PostLoopAction& action, Label* slow,
                          ForEachDirection direction,
                          MissingPropertyMode missing_property_mode);

  // Perform ArraySpeciesCreate (ES6 #sec-arrayspeciescreate).
  // This version is specialized to create a zero length array
  // of the elements kind of the input array.
  void GenerateArraySpeciesCreate();

  // Perform ArraySpeciesCreate (ES6 #sec-arrayspeciescreate).
  void GenerateArraySpeciesCreate(TNode<Number> len);

  Node* callbackfn_ = nullptr;
  TNode<JSReceiver> o_;
  Node* this_arg_ = nullptr;
  TNode<Number> len_;
  TNode<Context> context_;
  TNode<Object> receiver_;
  Node* new_target_ = nullptr;
  TNode<IntPtrT> argc_;
  Node* fast_typed_array_target_ = nullptr;
  const char* name_ = nullptr;
  Variable k_;
  Variable a_;
  Variable to_;
  Label fully_spec_compliant_;
  ElementsKind source_elements_kind_ = ElementsKind::NO_ELEMENTS;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ARRAY_GEN_H_
