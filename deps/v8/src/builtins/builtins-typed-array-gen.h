// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_
#define V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_

#include "torque-generated/builtins-base-from-dsl-gen.h"

namespace v8 {
namespace internal {

class TypedArrayBuiltinsAssembler : public BaseBuiltinsFromDSLAssembler {
 public:
  explicit TypedArrayBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : BaseBuiltinsFromDSLAssembler(state) {}

  TNode<JSTypedArray> SpeciesCreateByLength(TNode<Context> context,
                                            TNode<JSTypedArray> exemplar,
                                            TNode<Smi> len,
                                            const char* method_name);

 protected:
  void GenerateTypedArrayPrototypeIterationMethod(TNode<Context> context,
                                                  TNode<Object> receiver,
                                                  const char* method_name,
                                                  IterationKind iteration_kind);

  void ConstructByLength(TNode<Context> context, TNode<JSTypedArray> holder,
                         TNode<Object> length, TNode<Smi> element_size);
  void ConstructByArrayBuffer(TNode<Context> context,
                              TNode<JSTypedArray> holder,
                              TNode<JSArrayBuffer> buffer,
                              TNode<Object> byte_offset, TNode<Object> length,
                              TNode<Smi> element_size);
  void ConstructByTypedArray(TNode<Context> context, TNode<JSTypedArray> holder,
                             TNode<JSTypedArray> typed_array,
                             TNode<Smi> element_size);
  void ConstructByArrayLike(TNode<Context> context, TNode<JSTypedArray> holder,
                            TNode<HeapObject> array_like,
                            TNode<Object> initial_length,
                            TNode<Smi> element_size,
                            TNode<JSReceiver> buffer_constructor);
  void ConstructByIterable(TNode<Context> context, TNode<JSTypedArray> holder,
                           TNode<JSReceiver> iterable,
                           TNode<JSReceiver> iterator_fn,
                           TNode<Smi> element_size);

  void SetupTypedArray(TNode<JSTypedArray> holder, TNode<Smi> length,
                       TNode<UintPtrT> byte_offset,
                       TNode<UintPtrT> byte_length);
  void AttachBuffer(TNode<JSTypedArray> holder, TNode<JSArrayBuffer> buffer,
                    TNode<Map> map, TNode<Smi> length,
                    TNode<Number> byte_offset);

  TNode<Map> LoadMapForType(TNode<JSTypedArray> array);
  TNode<UintPtrT> CalculateExternalPointer(TNode<UintPtrT> backing_store,
                                           TNode<Number> byte_offset);
  Node* LoadDataPtr(Node* typed_array);
  TNode<BoolT> ByteLengthIsValid(TNode<Number> byte_length);

  // Returns true if kind is either UINT8_ELEMENTS or UINT8_CLAMPED_ELEMENTS.
  TNode<Word32T> IsUint8ElementsKind(TNode<Word32T> kind);

  // Returns true if kind is either BIGINT64_ELEMENTS or BIGUINT64_ELEMENTS.
  TNode<Word32T> IsBigInt64ElementsKind(TNode<Word32T> kind);

  // Returns the byte size of an element for a TypedArray elements kind.
  TNode<IntPtrT> GetTypedArrayElementSize(TNode<Word32T> elements_kind);

  TNode<JSArrayBuffer> LoadTypedArrayBuffer(TNode<JSTypedArray> typed_array) {
    return LoadObjectField<JSArrayBuffer>(typed_array,
                                          JSTypedArray::kBufferOffset);
  }

  TNode<Object> GetDefaultConstructor(TNode<Context> context,
                                      TNode<JSTypedArray> exemplar);

  TNode<Object> TypedArraySpeciesConstructor(TNode<Context> context,
                                             TNode<JSTypedArray> exemplar);

  TNode<JSTypedArray> SpeciesCreateByArrayBuffer(TNode<Context> context,
                                                 TNode<JSTypedArray> exemplar,
                                                 TNode<JSArrayBuffer> buffer,
                                                 TNode<Number> byte_offset,
                                                 TNode<Smi> len,
                                                 const char* method_name);

  TNode<JSTypedArray> CreateByLength(TNode<Context> context,
                                     TNode<Object> constructor, TNode<Smi> len,
                                     const char* method_name);

  TNode<JSArrayBuffer> GetBuffer(TNode<Context> context,
                                 TNode<JSTypedArray> array);

  TNode<JSTypedArray> ValidateTypedArray(TNode<Context> context,
                                         TNode<Object> obj,
                                         const char* method_name);

  // Fast path for setting a TypedArray (source) onto another TypedArray
  // (target) at an element offset.
  void SetTypedArraySource(TNode<Context> context, TNode<JSTypedArray> source,
                           TNode<JSTypedArray> target, TNode<IntPtrT> offset,
                           Label* call_runtime, Label* if_source_too_large);

  void SetJSArraySource(TNode<Context> context, TNode<JSArray> source,
                        TNode<JSTypedArray> target, TNode<IntPtrT> offset,
                        Label* call_runtime, Label* if_source_too_large);

  void CallCMemmove(TNode<IntPtrT> dest_ptr, TNode<IntPtrT> src_ptr,
                    TNode<IntPtrT> byte_length);

  void CallCCopyFastNumberJSArrayElementsToTypedArray(
      TNode<Context> context, TNode<JSArray> source, TNode<JSTypedArray> dest,
      TNode<IntPtrT> source_length, TNode<IntPtrT> offset);

  void CallCCopyTypedArrayElementsToTypedArray(TNode<JSTypedArray> source,
                                               TNode<JSTypedArray> dest,
                                               TNode<IntPtrT> source_length,
                                               TNode<IntPtrT> offset);

  void CallCCopyTypedArrayElementsSlice(TNode<JSTypedArray> source,
                                        TNode<JSTypedArray> dest,
                                        TNode<IntPtrT> start,
                                        TNode<IntPtrT> end);

  typedef std::function<void(ElementsKind, int, int)> TypedArraySwitchCase;

  void DispatchTypedArrayByElementsKind(
      TNode<Word32T> elements_kind, const TypedArraySwitchCase& case_function);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_
