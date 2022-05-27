// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-graph-assembler.h"

#include "src/wasm/object-access.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
CallDescriptor* GetBuiltinCallDescriptor(Builtin name, Zone* zone,
                                         StubCallMode stub_mode,
                                         bool needs_frame_state,
                                         Operator::Properties properties) {
  CallInterfaceDescriptor interface_descriptor =
      Builtins::CallInterfaceDescriptorFor(name);
  return Linkage::GetStubCallDescriptor(
      zone,                                           // zone
      interface_descriptor,                           // descriptor
      interface_descriptor.GetStackParameterCount(),  // stack parameter count
      needs_frame_state ? CallDescriptor::kNeedsFrameState
                        : CallDescriptor::kNoFlags,  // flags
      properties,                                    // properties
      stub_mode);                                    // stub call mode
}

// static
ObjectAccess ObjectAccessForGCStores(wasm::ValueType type) {
  return ObjectAccess(
      MachineType::TypeForRepresentation(type.machine_representation(),
                                         !type.is_packed()),
      type.is_reference() ? kFullWriteBarrier : kNoWriteBarrier);
}

// Sets {true_node} and {false_node} to their corresponding Branch outputs.
// Returns the Branch node. Does not change control().
Node* WasmGraphAssembler::Branch(Node* cond, Node** true_node,
                                 Node** false_node, BranchHint hint) {
  DCHECK_NOT_NULL(cond);
  Node* branch =
      graph()->NewNode(mcgraph()->common()->Branch(hint), cond, control());
  *true_node = graph()->NewNode(mcgraph()->common()->IfTrue(), branch);
  *false_node = graph()->NewNode(mcgraph()->common()->IfFalse(), branch);
  return branch;
}

// Helper functions for dealing with HeapObjects.
// Rule of thumb: if access to a given field in an object is required in
// at least two places, put a helper function here.

Node* WasmGraphAssembler::Allocate(int size) {
  AllowLargeObjects allow_large = size < kMaxRegularHeapObjectSize
                                      ? AllowLargeObjects::kFalse
                                      : AllowLargeObjects::kTrue;
  return Allocate(Int32Constant(size), allow_large);
}

Node* WasmGraphAssembler::Allocate(Node* size, AllowLargeObjects allow_large) {
  return AddNode(graph()->NewNode(
      simplified_.AllocateRaw(Type::Any(), AllocationType::kYoung, allow_large),
      size, effect(), control()));
}

Node* WasmGraphAssembler::LoadFromObject(MachineType type, Node* base,
                                         Node* offset) {
  return AddNode(graph()->NewNode(
      simplified_.LoadFromObject(ObjectAccess(type, kNoWriteBarrier)), base,
      offset, effect(), control()));
}

Node* WasmGraphAssembler::LoadImmutableFromObject(MachineType type, Node* base,
                                                  Node* offset) {
  return AddNode(graph()->NewNode(
      simplified_.LoadImmutableFromObject(ObjectAccess(type, kNoWriteBarrier)),
      base, offset, effect(), control()));
}

Node* WasmGraphAssembler::LoadImmutable(LoadRepresentation rep, Node* base,
                                        Node* offset) {
  return AddNode(
      graph()->NewNode(mcgraph()->machine()->LoadImmutable(rep), base, offset));
}

Node* WasmGraphAssembler::StoreToObject(ObjectAccess access, Node* base,
                                        Node* offset, Node* value) {
  return AddNode(graph()->NewNode(simplified_.StoreToObject(access), base,
                                  offset, value, effect(), control()));
}

Node* WasmGraphAssembler::InitializeImmutableInObject(ObjectAccess access,
                                                      Node* base, Node* offset,
                                                      Node* value) {
  return AddNode(
      graph()->NewNode(simplified_.InitializeImmutableInObject(access), base,
                       offset, value, effect(), control()));
}

Node* WasmGraphAssembler::IsI31(Node* object) {
  if (COMPRESS_POINTERS_BOOL) {
    return Word32Equal(Word32And(object, Int32Constant(kSmiTagMask)),
                       Int32Constant(kSmiTag));
  } else {
    return WordEqual(WordAnd(object, IntPtrConstant(kSmiTagMask)),
                     IntPtrConstant(kSmiTag));
  }
}

// Maps and their contents.
Node* WasmGraphAssembler::LoadMap(Node* object) {
  Node* map_word =
      LoadImmutableFromObject(MachineType::TaggedPointer(), object,
                              HeapObject::kMapOffset - kHeapObjectTag);
#ifdef V8_MAP_PACKING
  return UnpackMapWord(map_word);
#else
  return map_word;
#endif
}

void WasmGraphAssembler::StoreMap(Node* heap_object, Node* map) {
  ObjectAccess access(MachineType::TaggedPointer(), kMapWriteBarrier);
#ifdef V8_MAP_PACKING
  map = PackMapWord(TNode<Map>::UncheckedCast(map));
#endif
  InitializeImmutableInObject(access, heap_object,
                              HeapObject::kMapOffset - kHeapObjectTag, map);
}

Node* WasmGraphAssembler::LoadInstanceType(Node* map) {
  return LoadImmutableFromObject(
      MachineType::Uint16(), map,
      wasm::ObjectAccess::ToTagged(Map::kInstanceTypeOffset));
}
Node* WasmGraphAssembler::LoadWasmTypeInfo(Node* map) {
  int offset = Map::kConstructorOrBackPointerOrNativeContextOffset;
  return LoadImmutableFromObject(MachineType::TaggedPointer(), map,
                                 wasm::ObjectAccess::ToTagged(offset));
}

Node* WasmGraphAssembler::LoadSupertypes(Node* wasm_type_info) {
  return LoadImmutableFromObject(
      MachineType::TaggedPointer(), wasm_type_info,
      wasm::ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset));
}

// FixedArrays.

Node* WasmGraphAssembler::LoadFixedArrayLengthAsSmi(Node* fixed_array) {
  return LoadImmutableFromObject(
      MachineType::TaggedSigned(), fixed_array,
      wasm::ObjectAccess::ToTagged(FixedArray::kLengthOffset));
}

Node* WasmGraphAssembler::LoadFixedArrayElement(Node* fixed_array,
                                                Node* index_intptr,
                                                MachineType type) {
  Node* offset = IntAdd(
      IntMul(index_intptr, IntPtrConstant(kTaggedSize)),
      IntPtrConstant(wasm::ObjectAccess::ToTagged(FixedArray::kHeaderSize)));
  return LoadFromObject(type, fixed_array, offset);
}

Node* WasmGraphAssembler::LoadImmutableFixedArrayElement(Node* fixed_array,
                                                         Node* index_intptr,
                                                         MachineType type) {
  Node* offset = IntAdd(
      IntMul(index_intptr, IntPtrConstant(kTaggedSize)),
      IntPtrConstant(wasm::ObjectAccess::ToTagged(FixedArray::kHeaderSize)));
  return LoadImmutableFromObject(type, fixed_array, offset);
}

Node* WasmGraphAssembler::LoadFixedArrayElement(Node* array, int index,
                                                MachineType type) {
  return LoadFromObject(
      type, array, wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(index));
}

Node* WasmGraphAssembler::StoreFixedArrayElement(Node* array, int index,
                                                 Node* value,
                                                 ObjectAccess access) {
  return StoreToObject(
      access, array, wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(index),
      value);
}

// Functions, SharedFunctionInfos, FunctionData.

Node* WasmGraphAssembler::LoadSharedFunctionInfo(Node* js_function) {
  return LoadFromObject(
      MachineType::TaggedPointer(), js_function,
      wasm::ObjectAccess::SharedFunctionInfoOffsetInTaggedJSFunction());
}
Node* WasmGraphAssembler::LoadContextFromJSFunction(Node* js_function) {
  return LoadFromObject(MachineType::TaggedPointer(), js_function,
                        wasm::ObjectAccess::ContextOffsetInTaggedJSFunction());
}

Node* WasmGraphAssembler::LoadFunctionDataFromJSFunction(Node* js_function) {
  Node* shared = LoadSharedFunctionInfo(js_function);
  return LoadFromObject(
      MachineType::TaggedPointer(), shared,
      wasm::ObjectAccess::ToTagged(SharedFunctionInfo::kFunctionDataOffset));
}

Node* WasmGraphAssembler::LoadExportedFunctionIndexAsSmi(
    Node* exported_function_data) {
  return LoadImmutableFromObject(
      MachineType::TaggedSigned(), exported_function_data,
      wasm::ObjectAccess::ToTagged(
          WasmExportedFunctionData::kFunctionIndexOffset));
}
Node* WasmGraphAssembler::LoadExportedFunctionInstance(
    Node* exported_function_data) {
  return LoadImmutableFromObject(
      MachineType::TaggedPointer(), exported_function_data,
      wasm::ObjectAccess::ToTagged(WasmExportedFunctionData::kInstanceOffset));
}

// JavaScript objects.

Node* WasmGraphAssembler::LoadJSArrayElements(Node* js_array) {
  return LoadFromObject(
      MachineType::AnyTagged(), js_array,
      wasm::ObjectAccess::ToTagged(JSObject::kElementsOffset));
}

// WasmGC objects.

Node* WasmGraphAssembler::FieldOffset(const wasm::StructType* type,
                                      uint32_t field_index) {
  return IntPtrConstant(wasm::ObjectAccess::ToTagged(
      WasmStruct::kHeaderSize + type->field_offset(field_index)));
}

Node* WasmGraphAssembler::StoreStructField(Node* struct_object,
                                           const wasm::StructType* type,
                                           uint32_t field_index, Node* value) {
  ObjectAccess access = ObjectAccessForGCStores(type->field(field_index));
  return type->mutability(field_index)
             ? StoreToObject(access, struct_object,
                             FieldOffset(type, field_index), value)
             : InitializeImmutableInObject(access, struct_object,
                                           FieldOffset(type, field_index),
                                           value);
}

Node* WasmGraphAssembler::WasmArrayElementOffset(Node* index,
                                                 wasm::ValueType element_type) {
  Node* index_intptr =
      mcgraph()->machine()->Is64() ? ChangeInt32ToInt64(index) : index;
  return IntAdd(
      IntPtrConstant(wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize)),
      IntMul(index_intptr, IntPtrConstant(element_type.value_kind_size())));
}

Node* WasmGraphAssembler::LoadWasmArrayLength(Node* array) {
  return LoadImmutableFromObject(
      MachineType::Uint32(), array,
      wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset));
}

Node* WasmGraphAssembler::IsDataRefMap(Node* map) {
  Node* instance_type = LoadInstanceType(map);
  // We're going to test a range of WasmObject instance types with a single
  // unsigned comparison.
  Node* comparison_value =
      Int32Sub(instance_type, Int32Constant(FIRST_WASM_OBJECT_TYPE));
  return Uint32LessThanOrEqual(
      comparison_value,
      Int32Constant(LAST_WASM_OBJECT_TYPE - FIRST_WASM_OBJECT_TYPE));
}

// Generic HeapObject helpers.

Node* WasmGraphAssembler::HasInstanceType(Node* heap_object,
                                          InstanceType type) {
  Node* map = LoadMap(heap_object);
  Node* instance_type = LoadInstanceType(map);
  return Word32Equal(instance_type, Int32Constant(type));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
