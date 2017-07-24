// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

class TypedArrayBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit TypedArrayBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void GenerateTypedArrayPrototypeGetter(Node* context, Node* receiver,
                                         const char* method_name,
                                         int object_offset);
  void GenerateTypedArrayPrototypeIterationMethod(Node* context, Node* receiver,
                                                  const char* method_name,
                                                  IterationKind iteration_kind);

  void LoadMapAndElementsSize(Node* const array, Variable* typed_map,
                              Variable* size);

  Node* CalculateExternalPointer(Node* const backing_store,
                                 Node* const byte_offset);
  void DoInitialize(Node* const holder, Node* length, Node* const maybe_buffer,
                    Node* const byte_offset, Node* byte_length,
                    Node* const initialize, Node* const context);
  void InitializeBasedOnLength(Node* const holder, Node* const length,
                               Node* const element_size,
                               Node* const byte_offset, Node* const initialize,
                               Node* const context);
  Node* LoadDataPtr(Node* typed_array);
  Node* ByteLengthIsValid(Node* byte_length);
};

void TypedArrayBuiltinsAssembler::LoadMapAndElementsSize(Node* const array,
                                                         Variable* typed_map,
                                                         Variable* size) {
  Label unreachable(this), done(this);
  Label uint8_elements(this), uint8_clamped_elements(this), int8_elements(this),
      uint16_elements(this), int16_elements(this), uint32_elements(this),
      int32_elements(this), float32_elements(this), float64_elements(this);
  Label* elements_kind_labels[] = {
      &uint8_elements,  &uint8_clamped_elements, &int8_elements,
      &uint16_elements, &int16_elements,         &uint32_elements,
      &int32_elements,  &float32_elements,       &float64_elements};
  int32_t elements_kinds[] = {
      UINT8_ELEMENTS,  UINT8_CLAMPED_ELEMENTS, INT8_ELEMENTS,
      UINT16_ELEMENTS, INT16_ELEMENTS,         UINT32_ELEMENTS,
      INT32_ELEMENTS,  FLOAT32_ELEMENTS,       FLOAT64_ELEMENTS};
  const size_t kTypedElementsKindCount = LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND -
                                         FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND +
                                         1;
  DCHECK_EQ(kTypedElementsKindCount, arraysize(elements_kinds));
  DCHECK_EQ(kTypedElementsKindCount, arraysize(elements_kind_labels));

  Node* array_map = LoadMap(array);
  Node* elements_kind = LoadMapElementsKind(array_map);
  Switch(elements_kind, &unreachable, elements_kinds, elements_kind_labels,
         kTypedElementsKindCount);

  for (int i = 0; i < static_cast<int>(kTypedElementsKindCount); i++) {
    Bind(elements_kind_labels[i]);
    {
      ElementsKind kind = static_cast<ElementsKind>(elements_kinds[i]);
      ExternalArrayType type =
          isolate()->factory()->GetArrayTypeFromElementsKind(kind);
      Handle<Map> map(isolate()->heap()->MapForFixedTypedArray(type));
      typed_map->Bind(HeapConstant(map));
      size->Bind(SmiConstant(static_cast<int>(
          isolate()->factory()->GetExternalArrayElementSize(type))));
      Goto(&done);
    }
  }

  BIND(&unreachable);
  { Unreachable(); }
  BIND(&done);
}

// The byte_offset can be higher than Smi range, in which case to perform the
// pointer arithmetic necessary to calculate external_pointer, converting
// byte_offset to an intptr is more difficult. The max byte_offset is 8 * MaxSmi
// on the particular platform. 32 bit platforms are self-limiting, because we
// can't allocate an array bigger than our 32-bit arithmetic range anyway. 64
// bit platforms could theoretically have an offset up to 2^35 - 1, so we may
// need to convert the float heap number to an intptr.
compiler::Node* TypedArrayBuiltinsAssembler::CalculateExternalPointer(
    Node* const backing_store, Node* const byte_offset) {
  return IntPtrAdd(backing_store, ChangeNumberToIntPtr(byte_offset));
}

void TypedArrayBuiltinsAssembler::DoInitialize(Node* const holder, Node* length,
                                               Node* const maybe_buffer,
                                               Node* const byte_offset,
                                               Node* byte_length,
                                               Node* const initialize,
                                               Node* const context) {
  static const int32_t fta_base_data_offset =
      FixedTypedArrayBase::kDataOffset - kHeapObjectTag;

  Label setup_holder(this), alloc_array_buffer(this), aligned(this),
      allocate_elements(this), attach_buffer(this), done(this);
  VARIABLE(fixed_typed_map, MachineRepresentation::kTagged);
  VARIABLE(element_size, MachineRepresentation::kTagged);
  VARIABLE(total_size, MachineType::PointerRepresentation());

  // Make sure length is a Smi. The caller guarantees this is the case.
  length = ToInteger(context, length, CodeStubAssembler::kTruncateMinusZero);
  CSA_ASSERT(this, TaggedIsSmi(length));

  // byte_length can be -0, get rid of it.
  byte_length =
      ToInteger(context, byte_length, CodeStubAssembler::kTruncateMinusZero);

  GotoIfNot(IsNull(maybe_buffer), &setup_holder);
  // If the buffer is null, then we need a Smi byte_length. The caller
  // guarantees this is the case, because when byte_length >
  // TypedArrayMaxSizeInHeap, a buffer is allocated and passed in here.
  CSA_ASSERT(this, TaggedIsSmi(byte_length));
  Goto(&setup_holder);

  BIND(&setup_holder);
  {
    LoadMapAndElementsSize(holder, &fixed_typed_map, &element_size);
    // Setup the holder (JSArrayBufferView).
    //  - Set the length.
    //  - Set the byte_offset.
    //  - Set the byte_length.
    //  - Set EmbedderFields to 0.
    StoreObjectField(holder, JSTypedArray::kLengthOffset, length);
    StoreObjectField(holder, JSArrayBufferView::kByteOffsetOffset, byte_offset);
    StoreObjectField(holder, JSArrayBufferView::kByteLengthOffset, byte_length);
    for (int offset = JSTypedArray::kSize;
         offset < JSTypedArray::kSizeWithEmbedderFields;
         offset += kPointerSize) {
      StoreObjectField(holder, offset, SmiConstant(Smi::kZero));
    }

    Branch(IsNull(maybe_buffer), &alloc_array_buffer, &attach_buffer);
  }

  BIND(&alloc_array_buffer);
  {
    // Allocate a new ArrayBuffer and initialize it with empty properties and
    // elements.
    Node* const native_context = LoadNativeContext(context);
    Node* const map =
        LoadContextElement(native_context, Context::ARRAY_BUFFER_MAP_INDEX);
    Node* empty_fixed_array = LoadRoot(Heap::kEmptyFixedArrayRootIndex);

    Node* const buffer = Allocate(JSArrayBuffer::kSizeWithEmbedderFields);
    StoreMapNoWriteBarrier(buffer, map);
    StoreObjectFieldNoWriteBarrier(buffer, JSArray::kPropertiesOffset,
                                   empty_fixed_array);
    StoreObjectFieldNoWriteBarrier(buffer, JSArray::kElementsOffset,
                                   empty_fixed_array);
    // Setup the ArrayBuffer.
    //  - Set BitField to 0.
    //  - Set IsExternal and IsNeuterable bits of BitFieldSlot.
    //  - Set the byte_length field to byte_length.
    //  - Set backing_store to null/Smi(0).
    //  - Set all embedder fields to Smi(0).
    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBitFieldSlot,
                                   SmiConstant(Smi::kZero));
    int32_t bitfield_value = (1 << JSArrayBuffer::IsExternal::kShift) |
                             (1 << JSArrayBuffer::IsNeuterable::kShift);
    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBitFieldOffset,
                                   Int32Constant(bitfield_value),
                                   MachineRepresentation::kWord32);

    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kByteLengthOffset,
                                   byte_length);
    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBackingStoreOffset,
                                   SmiConstant(Smi::kZero));
    for (int i = 0; i < v8::ArrayBuffer::kEmbedderFieldCount; i++) {
      int offset = JSArrayBuffer::kSize + i * kPointerSize;
      StoreObjectFieldNoWriteBarrier(buffer, offset, SmiConstant(Smi::kZero));
    }

    StoreObjectField(holder, JSArrayBufferView::kBufferOffset, buffer);

    // Check the alignment.
    GotoIf(SmiEqual(SmiMod(element_size.value(), SmiConstant(kObjectAlignment)),
                    SmiConstant(0)),
           &aligned);

    // Fix alignment if needed.
    DCHECK_EQ(0, FixedTypedArrayBase::kHeaderSize & kObjectAlignmentMask);
    Node* aligned_header_size =
        IntPtrConstant(FixedTypedArrayBase::kHeaderSize + kObjectAlignmentMask);
    Node* size = IntPtrAdd(SmiToWord(byte_length), aligned_header_size);
    total_size.Bind(WordAnd(size, IntPtrConstant(~kObjectAlignmentMask)));
    Goto(&allocate_elements);
  }

  BIND(&aligned);
  {
    Node* header_size = IntPtrConstant(FixedTypedArrayBase::kHeaderSize);
    total_size.Bind(IntPtrAdd(SmiToWord(byte_length), header_size));
    Goto(&allocate_elements);
  }

  BIND(&allocate_elements);
  {
    // Allocate a FixedTypedArray and set the length, base pointer and external
    // pointer.
    CSA_ASSERT(this, IsRegularHeapObjectSize(total_size.value()));

    Node* elements;
    int heap_alignment =
        ElementSizeLog2Of(MachineType::PointerRepresentation());

    if (UnalignedLoadSupported(MachineType::Float64(), heap_alignment) &&
        UnalignedStoreSupported(MachineType::Float64(), heap_alignment)) {
      elements = AllocateInNewSpace(total_size.value());
    } else {
      elements = AllocateInNewSpace(total_size.value(), kDoubleAlignment);
    }

    StoreMapNoWriteBarrier(elements, fixed_typed_map.value());
    StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
    StoreObjectFieldNoWriteBarrier(
        elements, FixedTypedArrayBase::kBasePointerOffset, elements);
    StoreObjectFieldNoWriteBarrier(elements,
                                   FixedTypedArrayBase::kExternalPointerOffset,
                                   IntPtrConstant(fta_base_data_offset),
                                   MachineType::PointerRepresentation());

    StoreObjectField(holder, JSObject::kElementsOffset, elements);

    GotoIf(IsFalse(initialize), &done);
    // Initialize the backing store by filling it with 0s.
    Node* backing_store = IntPtrAdd(BitcastTaggedToWord(elements),
                                    IntPtrConstant(fta_base_data_offset));
    // Call out to memset to perform initialization.
    Node* memset =
        ExternalConstant(ExternalReference::libc_memset_function(isolate()));
    CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                   MachineType::IntPtr(), MachineType::UintPtr(), memset,
                   backing_store, IntPtrConstant(0), SmiToWord(byte_length));
    Goto(&done);
  }

  BIND(&attach_buffer);
  {
    StoreObjectField(holder, JSArrayBufferView::kBufferOffset, maybe_buffer);

    Node* elements = Allocate(FixedTypedArrayBase::kHeaderSize);
    StoreMapNoWriteBarrier(elements, fixed_typed_map.value());
    StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
    StoreObjectFieldNoWriteBarrier(
        elements, FixedTypedArrayBase::kBasePointerOffset, SmiConstant(0));

    Node* backing_store =
        LoadObjectField(maybe_buffer, JSArrayBuffer::kBackingStoreOffset,
                        MachineType::Pointer());

    Node* external_pointer =
        CalculateExternalPointer(backing_store, byte_offset);
    StoreObjectFieldNoWriteBarrier(
        elements, FixedTypedArrayBase::kExternalPointerOffset, external_pointer,
        MachineType::PointerRepresentation());

    StoreObjectField(holder, JSObject::kElementsOffset, elements);
    Goto(&done);
  }

  BIND(&done);
}

TF_BUILTIN(TypedArrayInitialize, TypedArrayBuiltinsAssembler) {
  Node* const holder = Parameter(Descriptor::kHolder);
  Node* length = Parameter(Descriptor::kLength);
  Node* const maybe_buffer = Parameter(Descriptor::kBuffer);
  Node* const byte_offset = Parameter(Descriptor::kByteOffset);
  Node* byte_length = Parameter(Descriptor::kByteLength);
  Node* const initialize = Parameter(Descriptor::kInitialize);
  Node* const context = Parameter(Descriptor::kContext);

  DoInitialize(holder, length, maybe_buffer, byte_offset, byte_length,
               initialize, context);
  Return(UndefinedConstant());
}

// Small buffers with byte_length <= typed_array_max_size_in_heap are allocated
// on the heap, but larger buffer must be externally allocated with the
// ArrayBuffer constructor. This helper allocates the buffer externally if
// necessary, and then calls into DoInitialize, which will allocate small
// on-heap buffers.
void TypedArrayBuiltinsAssembler::InitializeBasedOnLength(
    Node* const holder, Node* const length, Node* const element_size,
    Node* const byte_offset, Node* const initialize, Node* const context) {
  Label allocate_buffer(this), allocate_buffer_noinit(this), do_init(this);

  VARIABLE(maybe_buffer, MachineRepresentation::kTagged, NullConstant());

  // SmiMul returns a heap number in case of Smi overflow.
  Node* byte_length = SmiMul(length, element_size);
  GotoIf(TaggedIsNotSmi(byte_length), &allocate_buffer);
  GotoIf(SmiLessThanOrEqual(byte_length,
                            SmiConstant(FLAG_typed_array_max_size_in_heap)),
         &do_init);
  Branch(IsTrue(initialize), &allocate_buffer, &allocate_buffer_noinit);

  BIND(&allocate_buffer);
  {
    Node* const buffer_constructor = LoadContextElement(
        LoadNativeContext(context), Context::ARRAY_BUFFER_FUN_INDEX);
    maybe_buffer.Bind(ConstructJS(CodeFactory::Construct(isolate()), context,
                                  buffer_constructor, byte_length));
    Goto(&do_init);
  }

  Bind(&allocate_buffer_noinit);
  {
    Node* const buffer_constructor_noinit = LoadContextElement(
        LoadNativeContext(context), Context::ARRAY_BUFFER_NOINIT_FUN_INDEX);
    maybe_buffer.Bind(CallJS(CodeFactory::Call(isolate()), context,
                             buffer_constructor_noinit, UndefinedConstant(),
                             byte_length));
    Goto(&do_init);
  }

  Bind(&do_init);
  {
    DoInitialize(holder, length, maybe_buffer.value(), byte_offset, byte_length,
                 initialize, context);
  }
}

// ES6 #sec-typedarray-length
TF_BUILTIN(TypedArrayConstructByLength, TypedArrayBuiltinsAssembler) {
  // We know that holder cannot be an object if this builtin was called.
  Node* holder = Parameter(Descriptor::kHolder);
  Node* length = Parameter(Descriptor::kLength);
  Node* element_size = Parameter(Descriptor::kElementSize);
  Node* context = Parameter(Descriptor::kContext);

  Node* byte_offset = SmiConstant(0);
  Node* initialize = BooleanConstant(true);

  Label invalid_length(this);

  length = ToInteger(context, length, CodeStubAssembler::kTruncateMinusZero);
  // The maximum length of a TypedArray is MaxSmi().
  // Note: this is not per spec, but rather a constraint of our current
  // representation (which uses smi's).
  GotoIf(TaggedIsNotSmi(length), &invalid_length);
  GotoIf(SmiLessThan(length, SmiConstant(0)), &invalid_length);

  InitializeBasedOnLength(holder, length, element_size, byte_offset, initialize,
                          context);
  Return(UndefinedConstant());

  BIND(&invalid_length);
  {
    CallRuntime(Runtime::kThrowRangeError, context,
                SmiConstant(MessageTemplate::kInvalidTypedArrayLength), length);
    Unreachable();
  }
}

// ES6 #sec-typedarray-buffer-byteoffset-length
TF_BUILTIN(TypedArrayConstructByArrayBuffer, TypedArrayBuiltinsAssembler) {
  Node* const holder = Parameter(Descriptor::kHolder);
  Node* const buffer = Parameter(Descriptor::kBuffer);
  Node* const byte_offset = Parameter(Descriptor::kByteOffset);
  Node* const length = Parameter(Descriptor::kLength);
  Node* const element_size = Parameter(Descriptor::kElementSize);
  CSA_ASSERT(this, TaggedIsSmi(element_size));
  Node* const context = Parameter(Descriptor::kContext);
  Node* const initialize = BooleanConstant(true);

  VARIABLE(new_byte_length, MachineRepresentation::kTagged, SmiConstant(0));
  VARIABLE(offset, MachineRepresentation::kTagged, SmiConstant(0));

  Label start_offset_error(this, Label::kDeferred),
      byte_length_error(this, Label::kDeferred),
      invalid_offset_error(this, Label::kDeferred);
  Label offset_is_smi(this), offset_not_smi(this, Label::kDeferred),
      check_length(this), call_init(this), invalid_length(this),
      length_undefined(this), length_defined(this);

  Callable add = CodeFactory::Add(isolate());
  Callable div = CodeFactory::Divide(isolate());
  Callable equal = CodeFactory::Equal(isolate());
  Callable greater_than = CodeFactory::GreaterThan(isolate());
  Callable less_than = CodeFactory::LessThan(isolate());
  Callable mod = CodeFactory::Modulus(isolate());
  Callable sub = CodeFactory::Subtract(isolate());

  GotoIf(IsUndefined(byte_offset), &check_length);

  offset.Bind(
      ToInteger(context, byte_offset, CodeStubAssembler::kTruncateMinusZero));
  Branch(TaggedIsSmi(offset.value()), &offset_is_smi, &offset_not_smi);

  // Check that the offset is a multiple of the element size.
  BIND(&offset_is_smi);
  {
    GotoIf(SmiEqual(offset.value(), SmiConstant(0)), &check_length);
    GotoIf(SmiLessThan(offset.value(), SmiConstant(0)), &invalid_length);
    Node* remainder = SmiMod(offset.value(), element_size);
    Branch(SmiEqual(remainder, SmiConstant(0)), &check_length,
           &start_offset_error);
  }
  BIND(&offset_not_smi);
  {
    GotoIf(IsTrue(CallStub(less_than, context, offset.value(), SmiConstant(0))),
           &invalid_length);
    Node* remainder = CallStub(mod, context, offset.value(), element_size);
    // Remainder can be a heap number.
    Branch(IsTrue(CallStub(equal, context, remainder, SmiConstant(0))),
           &check_length, &start_offset_error);
  }

  BIND(&check_length);
  // TODO(petermarshall): Throw on detached typedArray.
  Branch(IsUndefined(length), &length_undefined, &length_defined);

  BIND(&length_undefined);
  {
    Node* buffer_byte_length =
        LoadObjectField(buffer, JSArrayBuffer::kByteLengthOffset);

    Node* remainder = CallStub(mod, context, buffer_byte_length, element_size);
    // Remainder can be a heap number.
    GotoIf(IsFalse(CallStub(equal, context, remainder, SmiConstant(0))),
           &byte_length_error);

    new_byte_length.Bind(
        CallStub(sub, context, buffer_byte_length, offset.value()));

    Branch(IsTrue(CallStub(less_than, context, new_byte_length.value(),
                           SmiConstant(0))),
           &invalid_offset_error, &call_init);
  }

  BIND(&length_defined);
  {
    Node* new_length = ToSmiIndex(length, context, &invalid_length);
    new_byte_length.Bind(SmiMul(new_length, element_size));
    // Reading the byte length must come after the ToIndex operation, which
    // could cause the buffer to become detached.
    Node* buffer_byte_length =
        LoadObjectField(buffer, JSArrayBuffer::kByteLengthOffset);

    Node* end = CallStub(add, context, offset.value(), new_byte_length.value());

    Branch(IsTrue(CallStub(greater_than, context, end, buffer_byte_length)),
           &invalid_length, &call_init);
  }

  BIND(&call_init);
  {
    Node* new_length =
        CallStub(div, context, new_byte_length.value(), element_size);
    // Force the result into a Smi, or throw a range error if it doesn't fit.
    new_length = ToSmiIndex(new_length, context, &invalid_length);

    DoInitialize(holder, new_length, buffer, offset.value(),
                 new_byte_length.value(), initialize, context);
    Return(UndefinedConstant());
  }

  BIND(&invalid_offset_error);
  {
    CallRuntime(Runtime::kThrowRangeError, context,
                SmiConstant(MessageTemplate::kInvalidOffset), byte_offset);
    Unreachable();
  }

  BIND(&start_offset_error);
  {
    Node* holder_map = LoadMap(holder);
    Node* problem_string = HeapConstant(
        factory()->NewStringFromAsciiChecked("start offset", TENURED));
    CallRuntime(Runtime::kThrowInvalidTypedArrayAlignment, context, holder_map,
                problem_string);

    Unreachable();
  }

  BIND(&byte_length_error);
  {
    Node* holder_map = LoadMap(holder);
    Node* problem_string = HeapConstant(
        factory()->NewStringFromAsciiChecked("byte length", TENURED));
    CallRuntime(Runtime::kThrowInvalidTypedArrayAlignment, context, holder_map,
                problem_string);

    Unreachable();
  }

  BIND(&invalid_length);
  {
    CallRuntime(Runtime::kThrowRangeError, context,
                SmiConstant(MessageTemplate::kInvalidTypedArrayLength), length);
    Unreachable();
  }
}

compiler::Node* TypedArrayBuiltinsAssembler::LoadDataPtr(Node* typed_array) {
  CSA_ASSERT(this, IsJSTypedArray(typed_array));
  Node* elements = LoadElements(typed_array);
  CSA_ASSERT(this, IsFixedTypedArray(elements));
  Node* base_pointer = BitcastTaggedToWord(
      LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset));
  Node* external_pointer = BitcastTaggedToWord(
      LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset));
  return IntPtrAdd(base_pointer, external_pointer);
}

compiler::Node* TypedArrayBuiltinsAssembler::ByteLengthIsValid(
    Node* byte_length) {
  Label smi(this), done(this);
  VARIABLE(is_valid, MachineRepresentation::kWord32);
  GotoIf(TaggedIsSmi(byte_length), &smi);

  CSA_ASSERT(this, IsHeapNumber(byte_length));
  Node* float_value = LoadHeapNumberValue(byte_length);
  Node* max_byte_length_double =
      Float64Constant(FixedTypedArrayBase::kMaxByteLength);
  is_valid.Bind(Float64LessThanOrEqual(float_value, max_byte_length_double));
  Goto(&done);

  BIND(&smi);
  Node* max_byte_length = IntPtrConstant(FixedTypedArrayBase::kMaxByteLength);
  is_valid.Bind(UintPtrLessThanOrEqual(SmiUntag(byte_length), max_byte_length));
  Goto(&done);

  BIND(&done);
  return is_valid.value();
}

TF_BUILTIN(TypedArrayConstructByArrayLike, TypedArrayBuiltinsAssembler) {
  Node* const holder = Parameter(Descriptor::kHolder);
  Node* const array_like = Parameter(Descriptor::kArrayLike);
  Node* initial_length = Parameter(Descriptor::kLength);
  Node* const element_size = Parameter(Descriptor::kElementSize);
  CSA_ASSERT(this, TaggedIsSmi(element_size));
  Node* const context = Parameter(Descriptor::kContext);

  Node* byte_offset = SmiConstant(0);
  Node* initialize = BooleanConstant(false);

  Label invalid_length(this), fill(this), fast_copy(this);

  // The caller has looked up length on array_like, which is observable.
  Node* length = ToSmiLength(initial_length, context, &invalid_length);

  InitializeBasedOnLength(holder, length, element_size, byte_offset, initialize,
                          context);
  GotoIf(SmiNotEqual(length, SmiConstant(0)), &fill);
  Return(UndefinedConstant());

  BIND(&fill);
  Node* holder_kind = LoadMapElementsKind(LoadMap(holder));
  Node* source_kind = LoadMapElementsKind(LoadMap(array_like));
  GotoIf(Word32Equal(holder_kind, source_kind), &fast_copy);

  // Call to JS to copy the contents of the array in.
  Callable callable = CodeFactory::Call(isolate());
  Node* copy_array_contents = LoadContextElement(
      LoadNativeContext(context), Context::TYPED_ARRAY_SET_FROM_ARRAY_LIKE);
  CallJS(callable, context, copy_array_contents, UndefinedConstant(), holder,
         array_like, length, SmiConstant(0));
  Return(UndefinedConstant());

  BIND(&fast_copy);
  {
    Node* holder_data_ptr = LoadDataPtr(holder);
    Node* source_data_ptr = LoadDataPtr(array_like);

    // Calculate the byte length. We shouldn't be trying to copy if the typed
    // array was neutered.
    CSA_ASSERT(this, SmiNotEqual(length, SmiConstant(0)));
    CSA_ASSERT(this, Word32Equal(IsDetachedBuffer(LoadObjectField(
                                     array_like, JSTypedArray::kBufferOffset)),
                                 Int32Constant(0)));

    Node* byte_length = SmiMul(length, element_size);
    CSA_ASSERT(this, ByteLengthIsValid(byte_length));
    Node* byte_length_intptr = ChangeNumberToIntPtr(byte_length);
    CSA_ASSERT(this, UintPtrLessThanOrEqual(
                         byte_length_intptr,
                         IntPtrConstant(FixedTypedArrayBase::kMaxByteLength)));

    Node* memcpy =
        ExternalConstant(ExternalReference::libc_memcpy_function(isolate()));
    CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                   MachineType::Pointer(), MachineType::UintPtr(), memcpy,
                   holder_data_ptr, source_data_ptr, byte_length_intptr);
    Return(UndefinedConstant());
  }

  BIND(&invalid_length);
  {
    CallRuntime(Runtime::kThrowRangeError, context,
                SmiConstant(MessageTemplate::kInvalidTypedArrayLength),
                initial_length);
    Unreachable();
  }
}

void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeGetter(
    Node* context, Node* receiver, const char* method_name, int object_offset) {
  // Check if the {receiver} is actually a JSTypedArray.
  Label receiver_is_incompatible(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &receiver_is_incompatible);
  GotoIfNot(HasInstanceType(receiver, JS_TYPED_ARRAY_TYPE),
            &receiver_is_incompatible);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(this, Label::kDeferred);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_receiverisneutered);
  Return(LoadObjectField(receiver, object_offset));

  BIND(&if_receiverisneutered);
  {
    // The {receiver}s buffer was neutered, default to zero.
    Return(SmiConstant(0));
  }

  BIND(&receiver_is_incompatible);
  {
    // The {receiver} is not a valid JSTypedArray.
    CallRuntime(Runtime::kThrowIncompatibleMethodReceiver, context,
                HeapConstant(
                    factory()->NewStringFromAsciiChecked(method_name, TENURED)),
                receiver);
    Unreachable();
  }
}

// ES6 #sec-get-%typedarray%.prototype.bytelength
TF_BUILTIN(TypedArrayPrototypeByteLength, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeGetter(context, receiver,
                                    "get TypedArray.prototype.byteLength",
                                    JSTypedArray::kByteLengthOffset);
}

// ES6 #sec-get-%typedarray%.prototype.byteoffset
TF_BUILTIN(TypedArrayPrototypeByteOffset, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeGetter(context, receiver,
                                    "get TypedArray.prototype.byteOffset",
                                    JSTypedArray::kByteOffsetOffset);
}

// ES6 #sec-get-%typedarray%.prototype.length
TF_BUILTIN(TypedArrayPrototypeLength, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeGetter(context, receiver,
                                    "get TypedArray.prototype.length",
                                    JSTypedArray::kLengthOffset);
}

void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeIterationMethod(
    Node* context, Node* receiver, const char* method_name,
    IterationKind iteration_kind) {
  Label throw_bad_receiver(this, Label::kDeferred);
  Label throw_typeerror(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &throw_bad_receiver);

  Node* map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(map);
  GotoIf(Word32NotEqual(instance_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
         &throw_bad_receiver);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(this, Label::kDeferred);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_receiverisneutered);

  Return(CreateArrayIterator(receiver, map, instance_type, context,
                             iteration_kind));

  VARIABLE(var_message, MachineRepresentation::kTagged);
  BIND(&throw_bad_receiver);
  var_message.Bind(SmiConstant(MessageTemplate::kNotTypedArray));
  Goto(&throw_typeerror);

  BIND(&if_receiverisneutered);
  var_message.Bind(
      SmiConstant(Smi::FromInt(MessageTemplate::kDetachedOperation)));
  Goto(&throw_typeerror);

  BIND(&throw_typeerror);
  {
    Node* method_arg = HeapConstant(
        isolate()->factory()->NewStringFromAsciiChecked(method_name, TENURED));
    Node* result = CallRuntime(Runtime::kThrowTypeError, context,
                               var_message.value(), method_arg);
    Return(result);
  }
}

// ES6 #sec-%typedarray%.prototype.values
TF_BUILTIN(TypedArrayPrototypeValues, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.values()",
                                             IterationKind::kValues);
}

// ES6 #sec-%typedarray%.prototype.entries
TF_BUILTIN(TypedArrayPrototypeEntries, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.entries()",
                                             IterationKind::kEntries);
}

// ES6 #sec-%typedarray%.prototype.keys
TF_BUILTIN(TypedArrayPrototypeKeys, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeIterationMethod(
      context, receiver, "%TypedArray%.prototype.keys()", IterationKind::kKeys);
}

}  // namespace internal
}  // namespace v8
