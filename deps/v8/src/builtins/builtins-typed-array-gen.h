// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_
#define V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

class TypedArrayBuiltinsAssembler : public CodeStubAssembler {
 public:
  using ElementsInfo = TorqueStructTypedArrayElementsInfo;
  explicit TypedArrayBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void GenerateTypedArrayPrototypeIterationMethod(TNode<Context> context,
                                                  TNode<Object> receiver,
                                                  const char* method_name,
                                                  IterationKind iteration_kind);

  void SetupTypedArrayEmbedderFields(TNode<JSTypedArray> holder);
  void AttachBuffer(TNode<JSTypedArray> holder, TNode<JSArrayBuffer> buffer,
                    TNode<Map> map, TNode<Smi> length,
                    TNode<UintPtrT> byte_offset);

  TNode<JSArrayBuffer> AllocateEmptyOnHeapBuffer(TNode<Context> context,
                                                 TNode<UintPtrT> byte_length);

  TNode<Map> LoadMapForType(TNode<JSTypedArray> array);
  TNode<BoolT> IsMockArrayBufferAllocatorFlag();
  TNode<UintPtrT> CalculateExternalPointer(TNode<UintPtrT> backing_store,
                                           TNode<UintPtrT> byte_offset);

  // Returns true if kind is either UINT8_ELEMENTS or UINT8_CLAMPED_ELEMENTS.
  TNode<BoolT> IsUint8ElementsKind(TNode<Int32T> kind);

  // Returns true if kind is either BIGINT64_ELEMENTS or BIGUINT64_ELEMENTS.
  TNode<BoolT> IsBigInt64ElementsKind(TNode<Int32T> kind);

  // Returns the byte size of an element for a TypedArray elements kind.
  TNode<IntPtrT> GetTypedArrayElementSize(TNode<Int32T> elements_kind);

  // Returns information (byte size and map) about a TypedArray's elements.
  ElementsInfo GetTypedArrayElementsInfo(TNode<JSTypedArray> typed_array);
  ElementsInfo GetTypedArrayElementsInfo(TNode<Map> map);

  TNode<JSFunction> GetDefaultConstructor(TNode<Context> context,
                                          TNode<JSTypedArray> exemplar);

  TNode<JSArrayBuffer> GetBuffer(TNode<Context> context,
                                 TNode<JSTypedArray> array);

  TNode<JSTypedArray> ValidateTypedArray(TNode<Context> context,
                                         TNode<Object> obj,
                                         const char* method_name);

  void CallCMemmove(TNode<RawPtrT> dest_ptr, TNode<RawPtrT> src_ptr,
                    TNode<UintPtrT> byte_length);

  void CallCMemcpy(TNode<RawPtrT> dest_ptr, TNode<RawPtrT> src_ptr,
                   TNode<UintPtrT> byte_length);

  void CallCMemset(TNode<RawPtrT> dest_ptr, TNode<IntPtrT> value,
                   TNode<UintPtrT> length);

  void CallCCopyFastNumberJSArrayElementsToTypedArray(
      TNode<Context> context, TNode<JSArray> source, TNode<JSTypedArray> dest,
      TNode<UintPtrT> source_length, TNode<UintPtrT> offset);

  void CallCCopyTypedArrayElementsToTypedArray(TNode<JSTypedArray> source,
                                               TNode<JSTypedArray> dest,
                                               TNode<UintPtrT> source_length,
                                               TNode<UintPtrT> offset);

  void CallCCopyTypedArrayElementsSlice(TNode<JSTypedArray> source,
                                        TNode<JSTypedArray> dest,
                                        TNode<UintPtrT> start,
                                        TNode<UintPtrT> end);

  using TypedArraySwitchCase = std::function<void(ElementsKind, int, int)>;

  void DispatchTypedArrayByElementsKind(
      TNode<Word32T> elements_kind, const TypedArraySwitchCase& case_function);

  TNode<BoolT> IsSharedArrayBuffer(TNode<JSArrayBuffer> buffer);

  void SetJSTypedArrayOnHeapDataPtr(TNode<JSTypedArray> holder,
                                    TNode<ByteArray> base,
                                    TNode<UintPtrT> offset);
  void SetJSTypedArrayOffHeapDataPtr(TNode<JSTypedArray> holder,
                                     TNode<RawPtrT> base,
                                     TNode<UintPtrT> offset);
  void StoreJSTypedArrayElementFromNumeric(TNode<Context> context,
                                           TNode<JSTypedArray> typed_array,
                                           TNode<UintPtrT> index_node,
                                           TNode<Numeric> value,
                                           ElementsKind elements_kind);
  void StoreJSTypedArrayElementFromTagged(TNode<Context> context,
                                          TNode<JSTypedArray> typed_array,
                                          TNode<UintPtrT> index_node,
                                          TNode<Object> value,
                                          ElementsKind elements_kind,
                                          Label* if_detached);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_TYPED_ARRAY_GEN_H_
