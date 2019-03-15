// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_
#define V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_

#include "src/code-stub-assembler.h"
#include "torque-generated/builtins-typed-array-from-dsl-gen.h"

namespace v8 {
namespace internal {

class TypedArrayBuiltinsAssembler : public CodeStubAssembler {
 public:
  using ElementsInfo =
      TypedArrayBuiltinsFromDSLAssembler::TypedArrayElementsInfo;
  explicit TypedArrayBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  template <class... TArgs>
  TNode<JSTypedArray> TypedArraySpeciesCreate(const char* method_name,
                                              TNode<Context> context,
                                              TNode<JSTypedArray> exemplar,
                                              TArgs... args);

  void GenerateTypedArrayPrototypeIterationMethod(TNode<Context> context,
                                                  TNode<Object> receiver,
                                                  const char* method_name,
                                                  IterationKind iteration_kind);

  void SetupTypedArray(TNode<JSTypedArray> holder, TNode<Smi> length,
                       TNode<UintPtrT> byte_offset,
                       TNode<UintPtrT> byte_length);
  void AttachBuffer(TNode<JSTypedArray> holder, TNode<JSArrayBuffer> buffer,
                    TNode<Map> map, TNode<Smi> length,
                    TNode<UintPtrT> byte_offset);

  TNode<JSArrayBuffer> AllocateEmptyOnHeapBuffer(TNode<Context> context,
                                                 TNode<JSTypedArray> holder,
                                                 TNode<UintPtrT> byte_length);

  TNode<FixedTypedArrayBase> AllocateOnHeapElements(TNode<Map> map,
                                                    TNode<IntPtrT> byte_length,
                                                    TNode<Number> length);

  TNode<Map> LoadMapForType(TNode<JSTypedArray> array);
  TNode<BoolT> IsMockArrayBufferAllocatorFlag();
  TNode<UintPtrT> CalculateExternalPointer(TNode<UintPtrT> backing_store,
                                           TNode<UintPtrT> byte_offset);
  TNode<RawPtrT> LoadDataPtr(TNode<JSTypedArray> typed_array);

  // Returns true if kind is either UINT8_ELEMENTS or UINT8_CLAMPED_ELEMENTS.
  TNode<Word32T> IsUint8ElementsKind(TNode<Word32T> kind);

  // Returns true if kind is either BIGINT64_ELEMENTS or BIGUINT64_ELEMENTS.
  TNode<Word32T> IsBigInt64ElementsKind(TNode<Word32T> kind);

  // Returns the byte size of an element for a TypedArray elements kind.
  TNode<IntPtrT> GetTypedArrayElementSize(TNode<Word32T> elements_kind);

  // Returns information (byte size and map) about a TypedArray's elements.
  ElementsInfo GetTypedArrayElementsInfo(TNode<JSTypedArray> typed_array);

  TNode<JSFunction> GetDefaultConstructor(TNode<Context> context,
                                          TNode<JSTypedArray> exemplar);

  TNode<JSTypedArray> TypedArrayCreateByLength(TNode<Context> context,
                                               TNode<Object> constructor,
                                               TNode<Smi> len,
                                               const char* method_name);

  void ThrowIfLengthLessThan(TNode<Context> context,
                             TNode<JSTypedArray> typed_array,
                             TNode<Smi> min_length);

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

  void CallCMemmove(TNode<RawPtrT> dest_ptr, TNode<RawPtrT> src_ptr,
                    TNode<UintPtrT> byte_length);

  void CallCMemcpy(TNode<RawPtrT> dest_ptr, TNode<RawPtrT> src_ptr,
                   TNode<UintPtrT> byte_length);

  void CallCMemset(TNode<RawPtrT> dest_ptr, TNode<IntPtrT> value,
                   TNode<UintPtrT> length);

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

  TNode<BoolT> IsSharedArrayBuffer(TNode<JSArrayBuffer> buffer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_
