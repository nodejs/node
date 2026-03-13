// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_OBJECT_GEN_H_
#define V8_BUILTINS_BUILTINS_OBJECT_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.1 Object Objects

class ObjectBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ObjectBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<Union<Undefined, JSObject>> FromPropertyDescriptor(TNode<Context>,
                                                           TNode<Object> desc);

 protected:
  void ReturnToStringFormat(TNode<Context> context, TNode<String> string);

  // TODO(v8:11167) remove |context| and |object| once OrderedNameDictionary
  // supported.
  void AddToDictionaryIf(TNode<BoolT> condition, TNode<Context> context,
                         TNode<Object> object,
                         TNode<HeapObject> name_dictionary, Handle<Name> name,
                         TNode<Object> value, Label* bailout);
  TNode<JSObject> FromPropertyDescriptor(TNode<Context> context,
                                         TNode<PropertyDescriptorObject> desc);
  TNode<JSObject> FromPropertyDetails(TNode<Context> context,
                                      TNode<Object> raw_value,
                                      TNode<Word32T> details,
                                      Label* if_bailout);
  TNode<PropertyDescriptorObject> DescriptorFromPropertyDetails(
      TNode<Context> context, TNode<Object> raw_value, TNode<Word32T> details,
      Label* if_bailout);
  TNode<JSObject> ConstructAccessorDescriptor(TNode<Context> context,
                                              TNode<Object> getter,
                                              TNode<Object> setter,
                                              TNode<BoolT> enumerable,
                                              TNode<BoolT> configurable);
  TNode<JSObject> ConstructDataDescriptor(TNode<Context> context,
                                          TNode<Object> value,
                                          TNode<BoolT> writable,
                                          TNode<BoolT> enumerable,
                                          TNode<BoolT> configurable);
  TNode<HeapObject> GetAccessorOrUndefined(TNode<HeapObject> accessor,
                                           Label* if_bailout);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_OBJECT_GEN_H_
