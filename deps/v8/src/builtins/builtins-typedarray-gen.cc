// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-typedarray-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/builtins/growable-fixed-array-gen.h"
#include "src/handles-inl.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;

// This is needed for gc_mole which will compile this file without the full set
// of GN defined macros.
#ifndef V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP
#define V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP 64
#endif

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

TNode<Map> TypedArrayBuiltinsAssembler::LoadMapForType(
    TNode<JSTypedArray> array) {
  TVARIABLE(Map, var_typed_map);
  TNode<Map> array_map = LoadMap(array);
  TNode<Int32T> elements_kind = LoadMapElementsKind(array_map);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind kind, int size, int typed_array_fun_index) {
        Handle<Map> map(isolate()->heap()->MapForFixedTypedArray(kind));
        var_typed_map = HeapConstant(map);
      });

  return var_typed_map.value();
}

// The byte_offset can be higher than Smi range, in which case to perform the
// pointer arithmetic necessary to calculate external_pointer, converting
// byte_offset to an intptr is more difficult. The max byte_offset is 8 * MaxSmi
// on the particular platform. 32 bit platforms are self-limiting, because we
// can't allocate an array bigger than our 32-bit arithmetic range anyway. 64
// bit platforms could theoretically have an offset up to 2^35 - 1, so we may
// need to convert the float heap number to an intptr.
TNode<UintPtrT> TypedArrayBuiltinsAssembler::CalculateExternalPointer(
    TNode<UintPtrT> backing_store, TNode<Number> byte_offset) {
  return Unsigned(
      IntPtrAdd(backing_store, ChangeNonnegativeNumberToUintPtr(byte_offset)));
}

// Setup the TypedArray which is under construction.
//  - Set the length.
//  - Set the byte_offset.
//  - Set the byte_length.
//  - Set EmbedderFields to 0.
void TypedArrayBuiltinsAssembler::SetupTypedArray(TNode<JSTypedArray> holder,
                                                  TNode<Smi> length,
                                                  TNode<Number> byte_offset,
                                                  TNode<Number> byte_length) {
  StoreObjectField(holder, JSTypedArray::kLengthOffset, length);
  StoreObjectField(holder, JSArrayBufferView::kByteOffsetOffset, byte_offset);
  StoreObjectField(holder, JSArrayBufferView::kByteLengthOffset, byte_length);
  for (int offset = JSTypedArray::kSize;
       offset < JSTypedArray::kSizeWithEmbedderFields; offset += kPointerSize) {
    StoreObjectField(holder, offset, SmiConstant(0));
  }
}

// Attach an off-heap buffer to a TypedArray.
void TypedArrayBuiltinsAssembler::AttachBuffer(TNode<JSTypedArray> holder,
                                               TNode<JSArrayBuffer> buffer,
                                               TNode<Map> map,
                                               TNode<Smi> length,
                                               TNode<Number> byte_offset) {
  StoreObjectField(holder, JSArrayBufferView::kBufferOffset, buffer);

  Node* elements = Allocate(FixedTypedArrayBase::kHeaderSize);
  StoreMapNoWriteBarrier(elements, map);
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
  StoreObjectFieldNoWriteBarrier(
      elements, FixedTypedArrayBase::kBasePointerOffset, SmiConstant(0));

  TNode<UintPtrT> backing_store =
      LoadObjectField<UintPtrT>(buffer, JSArrayBuffer::kBackingStoreOffset);

  TNode<UintPtrT> external_pointer =
      CalculateExternalPointer(backing_store, byte_offset);
  StoreObjectFieldNoWriteBarrier(
      elements, FixedTypedArrayBase::kExternalPointerOffset, external_pointer,
      MachineType::PointerRepresentation());

  StoreObjectField(holder, JSObject::kElementsOffset, elements);
}

TF_BUILTIN(TypedArrayInitializeWithBuffer, TypedArrayBuiltinsAssembler) {
  TNode<JSTypedArray> holder = CAST(Parameter(Descriptor::kHolder));
  TNode<Smi> length = CAST(Parameter(Descriptor::kLength));
  TNode<JSArrayBuffer> buffer = CAST(Parameter(Descriptor::kBuffer));
  TNode<Smi> element_size = CAST(Parameter(Descriptor::kElementSize));
  TNode<Number> byte_offset = CAST(Parameter(Descriptor::kByteOffset));

  TNode<Map> fixed_typed_map = LoadMapForType(holder);

  // SmiMul returns a heap number in case of Smi overflow.
  TNode<Number> byte_length = SmiMul(length, element_size);

  SetupTypedArray(holder, length, byte_offset, byte_length);
  AttachBuffer(holder, buffer, fixed_typed_map, length, byte_offset);
  Return(UndefinedConstant());
}

TF_BUILTIN(TypedArrayInitialize, TypedArrayBuiltinsAssembler) {
  TNode<JSTypedArray> holder = CAST(Parameter(Descriptor::kHolder));
  TNode<Smi> length = CAST(Parameter(Descriptor::kLength));
  TNode<Smi> element_size = CAST(Parameter(Descriptor::kElementSize));
  Node* initialize = Parameter(Descriptor::kInitialize);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CSA_ASSERT(this, TaggedIsPositiveSmi(length));
  CSA_ASSERT(this, TaggedIsPositiveSmi(element_size));
  CSA_ASSERT(this, IsBoolean(initialize));

  TNode<Smi> byte_offset = SmiConstant(0);

  static const int32_t fta_base_data_offset =
      FixedTypedArrayBase::kDataOffset - kHeapObjectTag;

  Label setup_holder(this), allocate_on_heap(this), aligned(this),
      allocate_elements(this), allocate_off_heap(this),
      allocate_off_heap_no_init(this), attach_buffer(this), done(this);
  TVARIABLE(IntPtrT, var_total_size);

  // SmiMul returns a heap number in case of Smi overflow.
  TNode<Number> byte_length = SmiMul(length, element_size);

  SetupTypedArray(holder, length, byte_offset, byte_length);

  TNode<Map> fixed_typed_map = LoadMapForType(holder);
  GotoIf(TaggedIsNotSmi(byte_length), &allocate_off_heap);
  // The goto above ensures that byte_length is a Smi.
  TNode<Smi> smi_byte_length = CAST(byte_length);
  GotoIf(SmiGreaterThan(smi_byte_length,
                        SmiConstant(V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP)),
         &allocate_off_heap);
  TNode<IntPtrT> word_byte_length = SmiToIntPtr(smi_byte_length);
  Goto(&allocate_on_heap);

  BIND(&allocate_on_heap);
  {
    CSA_ASSERT(this, TaggedIsPositiveSmi(byte_length));
    // Allocate a new ArrayBuffer and initialize it with empty properties and
    // elements.
    Node* native_context = LoadNativeContext(context);
    Node* map =
        LoadContextElement(native_context, Context::ARRAY_BUFFER_MAP_INDEX);
    Node* empty_fixed_array = LoadRoot(Heap::kEmptyFixedArrayRootIndex);

    Node* buffer = Allocate(JSArrayBuffer::kSizeWithEmbedderFields);
    StoreMapNoWriteBarrier(buffer, map);
    StoreObjectFieldNoWriteBarrier(buffer, JSArray::kPropertiesOrHashOffset,
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
                                   SmiConstant(0));
    int32_t bitfield_value = (1 << JSArrayBuffer::IsExternal::kShift) |
                             (1 << JSArrayBuffer::IsNeuterable::kShift);
    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBitFieldOffset,
                                   Int32Constant(bitfield_value),
                                   MachineRepresentation::kWord32);

    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kByteLengthOffset,
                                   byte_length);
    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBackingStoreOffset,
                                   SmiConstant(0));
    for (int i = 0; i < v8::ArrayBuffer::kEmbedderFieldCount; i++) {
      int offset = JSArrayBuffer::kSize + i * kPointerSize;
      StoreObjectFieldNoWriteBarrier(buffer, offset, SmiConstant(0));
    }

    StoreObjectField(holder, JSArrayBufferView::kBufferOffset, buffer);

    // Check the alignment.
    GotoIf(SmiEqual(SmiMod(element_size, SmiConstant(kObjectAlignment)),
                    SmiConstant(0)),
           &aligned);

    // Fix alignment if needed.
    DCHECK_EQ(0, FixedTypedArrayBase::kHeaderSize & kObjectAlignmentMask);
    TNode<IntPtrT> aligned_header_size =
        IntPtrConstant(FixedTypedArrayBase::kHeaderSize + kObjectAlignmentMask);
    TNode<IntPtrT> size = IntPtrAdd(word_byte_length, aligned_header_size);
    var_total_size = WordAnd(size, IntPtrConstant(~kObjectAlignmentMask));
    Goto(&allocate_elements);
  }

  BIND(&aligned);
  {
    TNode<IntPtrT> header_size =
        IntPtrConstant(FixedTypedArrayBase::kHeaderSize);
    var_total_size = IntPtrAdd(word_byte_length, header_size);
    Goto(&allocate_elements);
  }

  BIND(&allocate_elements);
  {
    // Allocate a FixedTypedArray and set the length, base pointer and external
    // pointer.
    CSA_ASSERT(this, IsRegularHeapObjectSize(var_total_size.value()));

    Node* elements;

    if (UnalignedLoadSupported(MachineRepresentation::kFloat64) &&
        UnalignedStoreSupported(MachineRepresentation::kFloat64)) {
      elements = AllocateInNewSpace(var_total_size.value());
    } else {
      elements = AllocateInNewSpace(var_total_size.value(), kDoubleAlignment);
    }

    StoreMapNoWriteBarrier(elements, fixed_typed_map);
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
                   backing_store, IntPtrConstant(0), word_byte_length);
    Goto(&done);
  }

  TVARIABLE(JSArrayBuffer, var_buffer);

  BIND(&allocate_off_heap);
  {
    GotoIf(IsFalse(initialize), &allocate_off_heap_no_init);

    Node* buffer_constructor = LoadContextElement(
        LoadNativeContext(context), Context::ARRAY_BUFFER_FUN_INDEX);
    var_buffer = CAST(ConstructJS(CodeFactory::Construct(isolate()), context,
                                  buffer_constructor, byte_length));
    Goto(&attach_buffer);
  }

  BIND(&allocate_off_heap_no_init);
  {
    Node* buffer_constructor_noinit = LoadContextElement(
        LoadNativeContext(context), Context::ARRAY_BUFFER_NOINIT_FUN_INDEX);
    var_buffer = CAST(CallJS(CodeFactory::Call(isolate()), context,
                             buffer_constructor_noinit, UndefinedConstant(),
                             byte_length));
    Goto(&attach_buffer);
  }

  BIND(&attach_buffer);
  {
    AttachBuffer(holder, var_buffer.value(), fixed_typed_map, length,
                 byte_offset);
    Goto(&done);
  }

  BIND(&done);
  Return(UndefinedConstant());
}

// ES6 #sec-typedarray-length
void TypedArrayBuiltinsAssembler::ConstructByLength(TNode<Context> context,
                                                    TNode<JSTypedArray> holder,
                                                    TNode<Object> length,
                                                    TNode<Smi> element_size) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(element_size));

  Label invalid_length(this, Label::kDeferred), done(this);

  TNode<Number> converted_length =
      ToInteger_Inline(context, length, CodeStubAssembler::kTruncateMinusZero);

  // The maximum length of a TypedArray is MaxSmi().
  // Note: this is not per spec, but rather a constraint of our current
  // representation (which uses Smis).
  GotoIf(TaggedIsNotSmi(converted_length), &invalid_length);
  // The goto above ensures that byte_length is a Smi.
  TNode<Smi> smi_converted_length = CAST(converted_length);
  GotoIf(SmiLessThan(smi_converted_length, SmiConstant(0)), &invalid_length);

  Node* initialize = TrueConstant();
  CallBuiltin(Builtins::kTypedArrayInitialize, context, holder,
              converted_length, element_size, initialize);
  Goto(&done);

  BIND(&invalid_length);
  {
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength,
                    converted_length);
  }

  BIND(&done);
}

// ES6 #sec-typedarray-buffer-byteoffset-length
void TypedArrayBuiltinsAssembler::ConstructByArrayBuffer(
    TNode<Context> context, TNode<JSTypedArray> holder,
    TNode<JSArrayBuffer> buffer, TNode<Object> byte_offset,
    TNode<Object> length, TNode<Smi> element_size) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(element_size));

  VARIABLE(new_byte_length, MachineRepresentation::kTagged, SmiConstant(0));
  VARIABLE(offset, MachineRepresentation::kTagged, SmiConstant(0));

  Label start_offset_error(this, Label::kDeferred),
      byte_length_error(this, Label::kDeferred),
      invalid_offset_error(this, Label::kDeferred);
  Label offset_is_smi(this), offset_not_smi(this, Label::kDeferred),
      check_length(this), call_init(this), invalid_length(this),
      length_undefined(this), length_defined(this), detached_error(this),
      done(this);

  GotoIf(IsUndefined(byte_offset), &check_length);

  offset.Bind(ToInteger_Inline(context, byte_offset,
                               CodeStubAssembler::kTruncateMinusZero));
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
    GotoIf(IsTrue(CallBuiltin(Builtins::kLessThan, context, offset.value(),
                              SmiConstant(0))),
           &invalid_length);
    Node* remainder =
        CallBuiltin(Builtins::kModulus, context, offset.value(), element_size);
    // Remainder can be a heap number.
    Branch(IsTrue(CallBuiltin(Builtins::kEqual, context, remainder,
                              SmiConstant(0))),
           &check_length, &start_offset_error);
  }

  BIND(&check_length);
  Branch(IsUndefined(length), &length_undefined, &length_defined);

  BIND(&length_undefined);
  {
    GotoIf(IsDetachedBuffer(buffer), &detached_error);
    Node* buffer_byte_length =
        LoadObjectField(buffer, JSArrayBuffer::kByteLengthOffset);

    Node* remainder = CallBuiltin(Builtins::kModulus, context,
                                  buffer_byte_length, element_size);
    // Remainder can be a heap number.
    GotoIf(IsFalse(CallBuiltin(Builtins::kEqual, context, remainder,
                               SmiConstant(0))),
           &byte_length_error);

    new_byte_length.Bind(CallBuiltin(Builtins::kSubtract, context,
                                     buffer_byte_length, offset.value()));

    Branch(IsTrue(CallBuiltin(Builtins::kLessThan, context,
                              new_byte_length.value(), SmiConstant(0))),
           &invalid_offset_error, &call_init);
  }

  BIND(&length_defined);
  {
    TNode<Smi> new_length = ToSmiIndex(length, context, &invalid_length);
    GotoIf(IsDetachedBuffer(buffer), &detached_error);
    new_byte_length.Bind(SmiMul(new_length, element_size));
    // Reading the byte length must come after the ToIndex operation, which
    // could cause the buffer to become detached.
    Node* buffer_byte_length =
        LoadObjectField(buffer, JSArrayBuffer::kByteLengthOffset);

    Node* end = CallBuiltin(Builtins::kAdd, context, offset.value(),
                            new_byte_length.value());

    Branch(IsTrue(CallBuiltin(Builtins::kGreaterThan, context, end,
                              buffer_byte_length)),
           &invalid_length, &call_init);
  }

  BIND(&call_init);
  {
    TNode<Object> raw_length = CallBuiltin(
        Builtins::kDivide, context, new_byte_length.value(), element_size);
    // Force the result into a Smi, or throw a range error if it doesn't fit.
    TNode<Smi> new_length = ToSmiIndex(raw_length, context, &invalid_length);

    CallBuiltin(Builtins::kTypedArrayInitializeWithBuffer, context, holder,
                new_length, buffer, element_size, offset.value());
    Goto(&done);
  }

  BIND(&invalid_offset_error);
  { ThrowRangeError(context, MessageTemplate::kInvalidOffset, byte_offset); }

  BIND(&start_offset_error);
  {
    Node* holder_map = LoadMap(holder);
    Node* problem_string = StringConstant("start offset");
    CallRuntime(Runtime::kThrowInvalidTypedArrayAlignment, context, holder_map,
                problem_string);

    Unreachable();
  }

  BIND(&byte_length_error);
  {
    Node* holder_map = LoadMap(holder);
    Node* problem_string = StringConstant("byte length");
    CallRuntime(Runtime::kThrowInvalidTypedArrayAlignment, context, holder_map,
                problem_string);

    Unreachable();
  }

  BIND(&invalid_length);
  {
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength, length);
  }

  BIND(&detached_error);
  { ThrowTypeError(context, MessageTemplate::kDetachedOperation, "Construct"); }

  BIND(&done);
}

void TypedArrayBuiltinsAssembler::ConstructByTypedArray(
    TNode<Context> context, TNode<JSTypedArray> holder,
    TNode<JSTypedArray> typed_array, TNode<Smi> element_size) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(element_size));

  TNode<JSFunction> const default_constructor = CAST(LoadContextElement(
      LoadNativeContext(context), Context::ARRAY_BUFFER_FUN_INDEX));

  Label construct(this), if_detached(this), if_notdetached(this),
      check_for_sab(this), if_buffernotshared(this), check_prototype(this),
      done(this);
  TVARIABLE(JSReceiver, buffer_constructor, default_constructor);

  TNode<JSArrayBuffer> source_buffer = LoadObjectField<JSArrayBuffer>(
      typed_array, JSArrayBufferView::kBufferOffset);
  Branch(IsDetachedBuffer(source_buffer), &if_detached, &if_notdetached);

  // TODO(petermarshall): Throw on detached typedArray.
  TVARIABLE(Smi, source_length);
  BIND(&if_detached);
  source_length = SmiConstant(0);
  Goto(&check_for_sab);

  BIND(&if_notdetached);
  source_length =
      CAST(LoadObjectField(typed_array, JSTypedArray::kLengthOffset));
  Goto(&check_for_sab);

  // The spec requires that constructing a typed array using a SAB-backed typed
  // array use the ArrayBuffer constructor, not the species constructor. See
  // https://tc39.github.io/ecma262/#sec-typedarray-typedarray.
  BIND(&check_for_sab);
  TNode<Uint32T> bitfield =
      LoadObjectField<Uint32T>(source_buffer, JSArrayBuffer::kBitFieldOffset);
  Branch(IsSetWord32<JSArrayBuffer::IsShared>(bitfield), &construct,
         &if_buffernotshared);

  BIND(&if_buffernotshared);
  {
    buffer_constructor =
        CAST(SpeciesConstructor(context, source_buffer, default_constructor));
    // TODO(petermarshall): Throw on detached typedArray.
    GotoIfNot(IsDetachedBuffer(source_buffer), &construct);
    source_length = SmiConstant(0);
    Goto(&construct);
  }

  BIND(&construct);
  {
    ConstructByArrayLike(context, holder, typed_array, source_length.value(),
                         element_size);
    TNode<Object> proto = GetProperty(context, buffer_constructor.value(),
                                      PrototypeStringConstant());
    // TODO(petermarshall): Correct for realm as per 9.1.14 step 4.
    TNode<JSArrayBuffer> buffer = LoadObjectField<JSArrayBuffer>(
        holder, JSArrayBufferView::kBufferOffset);
    CallRuntime(Runtime::kInternalSetPrototype, context, buffer, proto);

    Goto(&done);
  }

  BIND(&done);
}

Node* TypedArrayBuiltinsAssembler::LoadDataPtr(Node* typed_array) {
  CSA_ASSERT(this, IsJSTypedArray(typed_array));
  Node* elements = LoadElements(typed_array);
  CSA_ASSERT(this, IsFixedTypedArray(elements));
  return LoadFixedTypedArrayBackingStore(CAST(elements));
}

TNode<BoolT> TypedArrayBuiltinsAssembler::ByteLengthIsValid(
    TNode<Number> byte_length) {
  Label smi(this), done(this);
  TVARIABLE(BoolT, is_valid);
  GotoIf(TaggedIsSmi(byte_length), &smi);

  TNode<Float64T> float_value = LoadHeapNumberValue(CAST(byte_length));
  TNode<Float64T> max_byte_length_double =
      Float64Constant(FixedTypedArrayBase::kMaxByteLength);
  is_valid = Float64LessThanOrEqual(float_value, max_byte_length_double);
  Goto(&done);

  BIND(&smi);
  TNode<IntPtrT> max_byte_length =
      IntPtrConstant(FixedTypedArrayBase::kMaxByteLength);
  is_valid =
      UintPtrLessThanOrEqual(SmiUntag(CAST(byte_length)), max_byte_length);
  Goto(&done);

  BIND(&done);
  return is_valid.value();
}

void TypedArrayBuiltinsAssembler::ConstructByArrayLike(
    TNode<Context> context, TNode<JSTypedArray> holder,
    TNode<HeapObject> array_like, TNode<Object> initial_length,
    TNode<Smi> element_size) {
  Node* initialize = FalseConstant();

  Label invalid_length(this), fill(this), fast_copy(this), done(this);

  // The caller has looked up length on array_like, which is observable.
  TNode<Smi> length = ToSmiLength(initial_length, context, &invalid_length);

  CallBuiltin(Builtins::kTypedArrayInitialize, context, holder, length,
              element_size, initialize);
  GotoIf(SmiNotEqual(length, SmiConstant(0)), &fill);
  Goto(&done);

  BIND(&fill);
  TNode<Int32T> holder_kind = LoadMapElementsKind(LoadMap(holder));
  TNode<Int32T> source_kind = LoadMapElementsKind(LoadMap(array_like));
  GotoIf(Word32Equal(holder_kind, source_kind), &fast_copy);

  // Copy using the elements accessor.
  CallRuntime(Runtime::kTypedArrayCopyElements, context, holder, array_like,
              length);
  Goto(&done);

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

    TNode<Number> byte_length = SmiMul(length, element_size);
    CSA_ASSERT(this, ByteLengthIsValid(byte_length));
    TNode<UintPtrT> byte_length_intptr =
        ChangeNonnegativeNumberToUintPtr(byte_length);
    CSA_ASSERT(this, UintPtrLessThanOrEqual(
                         byte_length_intptr,
                         IntPtrConstant(FixedTypedArrayBase::kMaxByteLength)));

    Node* memcpy =
        ExternalConstant(ExternalReference::libc_memcpy_function(isolate()));
    CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                   MachineType::Pointer(), MachineType::UintPtr(), memcpy,
                   holder_data_ptr, source_data_ptr, byte_length_intptr);
    Goto(&done);
  }

  BIND(&invalid_length);
  {
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength,
                    initial_length);
  }

  BIND(&done);
}

void TypedArrayBuiltinsAssembler::ConstructByIterable(
    TNode<Context> context, TNode<JSTypedArray> holder,
    TNode<JSReceiver> iterable, TNode<Object> iterator_fn,
    TNode<Smi> element_size) {
  CSA_ASSERT(this, IsCallable(iterator_fn));
  Label fast_path(this), slow_path(this), done(this);

  TNode<JSArray> array_like = CAST(
      CallBuiltin(Builtins::kIterableToList, context, iterable, iterator_fn));
  TNode<Object> initial_length = LoadJSArrayLength(array_like);
  ConstructByArrayLike(context, holder, array_like, initial_length,
                       element_size);
}

TF_BUILTIN(TypedArrayBaseConstructor, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  ThrowTypeError(context, MessageTemplate::kConstructAbstractClass,
                 "TypedArray");
}

// ES #sec-typedarray-constructors
TF_BUILTIN(CreateTypedArray, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kTarget));
  TNode<JSReceiver> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Object> arg1 = CAST(Parameter(Descriptor::kArg1));
  TNode<Object> arg2 = CAST(Parameter(Descriptor::kArg2));
  TNode<Object> arg3 = CAST(Parameter(Descriptor::kArg3));

  CSA_ASSERT(this, IsConstructor(target));
  CSA_ASSERT(this, IsJSReceiver(new_target));

  Label if_arg1isbuffer(this), if_arg1istypedarray(this),
      if_arg1isreceiver(this), if_arg1isnumber(this), return_result(this);

  ConstructorBuiltinsAssembler constructor_assembler(this->state());
  TNode<JSTypedArray> result = CAST(
      constructor_assembler.EmitFastNewObject(context, target, new_target));

  TNode<Smi> element_size =
      SmiTag(GetTypedArrayElementSize(LoadElementsKind(result)));

  GotoIf(TaggedIsSmi(arg1), &if_arg1isnumber);
  GotoIf(IsJSArrayBuffer(arg1), &if_arg1isbuffer);
  GotoIf(IsJSTypedArray(arg1), &if_arg1istypedarray);
  GotoIf(IsJSReceiver(arg1), &if_arg1isreceiver);
  Goto(&if_arg1isnumber);

  // https://tc39.github.io/ecma262/#sec-typedarray-buffer-byteoffset-length
  BIND(&if_arg1isbuffer);
  {
    ConstructByArrayBuffer(context, result, CAST(arg1), arg2, arg3,
                           element_size);
    Goto(&return_result);
  }

  // https://tc39.github.io/ecma262/#sec-typedarray-typedarray
  BIND(&if_arg1istypedarray);
  {
    TNode<JSTypedArray> typed_array = CAST(arg1);
    ConstructByTypedArray(context, result, typed_array, element_size);
    Goto(&return_result);
  }

  // https://tc39.github.io/ecma262/#sec-typedarray-object
  BIND(&if_arg1isreceiver);
  {
    Label if_iteratorundefined(this), if_iteratornotcallable(this);
    // Get iterator symbol
    TNode<Object> iteratorFn =
        CAST(GetMethod(context, arg1, isolate()->factory()->iterator_symbol(),
                       &if_iteratorundefined));
    GotoIf(TaggedIsSmi(iteratorFn), &if_iteratornotcallable);
    GotoIfNot(IsCallable(iteratorFn), &if_iteratornotcallable);

    ConstructByIterable(context, result, CAST(arg1), iteratorFn, element_size);
    Goto(&return_result);

    BIND(&if_iteratorundefined);
    {
      TNode<HeapObject> array_like = CAST(arg1);
      TNode<Object> initial_length =
          GetProperty(context, arg1, LengthStringConstant());

      ConstructByArrayLike(context, result, array_like, initial_length,
                           element_size);
      Goto(&return_result);
    }

    BIND(&if_iteratornotcallable);
    { ThrowTypeError(context, MessageTemplate::kIteratorSymbolNonCallable); }
  }

  // The first argument was a number or fell through and is treated as
  // a number. https://tc39.github.io/ecma262/#sec-typedarray-length
  BIND(&if_arg1isnumber);
  {
    ConstructByLength(context, result, arg1, element_size);
    Goto(&return_result);
  }

  BIND(&return_result);
  Return(result);
}

TF_BUILTIN(TypedArrayConstructorLazyDeoptContinuation,
           TypedArrayBuiltinsAssembler) {
  Node* result = Parameter(Descriptor::kResult);
  Return(result);
}

// ES #sec-typedarray-constructors
TF_BUILTIN(TypedArrayConstructor, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                               MachineType::TaggedPointer());
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* arg1 = args.GetOptionalArgumentValue(0);
  Node* arg2 = args.GetOptionalArgumentValue(1);
  Node* arg3 = args.GetOptionalArgumentValue(2);

  // If NewTarget is undefined, throw a TypeError exception.
  // All the TypedArray constructors have this as the first step:
  // https://tc39.github.io/ecma262/#sec-typedarray-constructors
  Label throwtypeerror(this, Label::kDeferred);
  GotoIf(IsUndefined(new_target), &throwtypeerror);

  Node* result = CallBuiltin(Builtins::kCreateTypedArray, context, target,
                             new_target, arg1, arg2, arg3);
  args.PopAndReturn(result);

  BIND(&throwtypeerror);
  {
    Node* name = CallRuntime(Runtime::kGetFunctionName, context, target);
    ThrowTypeError(context, MessageTemplate::kConstructorNotFunction, name);
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
    ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                   StringConstant(method_name), receiver);
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

TNode<Word32T> TypedArrayBuiltinsAssembler::IsUint8ElementsKind(
    TNode<Word32T> kind) {
  return Word32Or(Word32Equal(kind, Int32Constant(UINT8_ELEMENTS)),
                  Word32Equal(kind, Int32Constant(UINT8_CLAMPED_ELEMENTS)));
}

TNode<Word32T> TypedArrayBuiltinsAssembler::IsBigInt64ElementsKind(
    TNode<Word32T> kind) {
  return Word32Or(Word32Equal(kind, Int32Constant(BIGINT64_ELEMENTS)),
                  Word32Equal(kind, Int32Constant(BIGUINT64_ELEMENTS)));
}

TNode<Word32T> TypedArrayBuiltinsAssembler::LoadElementsKind(
    TNode<JSTypedArray> typed_array) {
  return LoadMapElementsKind(LoadMap(typed_array));
}

TNode<IntPtrT> TypedArrayBuiltinsAssembler::GetTypedArrayElementSize(
    TNode<Word32T> elements_kind) {
  TVARIABLE(IntPtrT, element_size);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind el_kind, int size, int typed_array_fun_index) {
        element_size = IntPtrConstant(size);
      });

  return element_size.value();
}

TNode<Object> TypedArrayBuiltinsAssembler::GetDefaultConstructor(
    TNode<Context> context, TNode<JSTypedArray> exemplar) {
  TVARIABLE(IntPtrT, context_slot);
  TNode<Word32T> elements_kind = LoadElementsKind(exemplar);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind el_kind, int size, int typed_array_function_index) {
        context_slot = IntPtrConstant(typed_array_function_index);
      });

  return LoadContextElement(LoadNativeContext(context), context_slot.value());
}

TNode<Object> TypedArrayBuiltinsAssembler::TypedArraySpeciesConstructor(
    TNode<Context> context, TNode<JSTypedArray> exemplar) {
  TVARIABLE(Object, var_constructor);
  Label slow(this), done(this);

  // Let defaultConstructor be the intrinsic object listed in column one of
  // Table 52 for exemplar.[[TypedArrayName]].
  TNode<Object> default_constructor = GetDefaultConstructor(context, exemplar);

  var_constructor = default_constructor;
  Node* map = LoadMap(exemplar);
  GotoIfNot(IsPrototypeTypedArrayPrototype(context, map), &slow);
  Branch(IsTypedArraySpeciesProtectorCellInvalid(), &slow, &done);

  BIND(&slow);
  var_constructor = SpeciesConstructor(context, exemplar, default_constructor);
  Goto(&done);

  BIND(&done);
  return var_constructor.value();
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::SpeciesCreateByArrayBuffer(
    TNode<Context> context, TNode<JSTypedArray> exemplar,
    TNode<JSArrayBuffer> buffer, TNode<Number> byte_offset, TNode<Smi> len,
    const char* method_name) {
  // Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  TNode<Object> constructor = TypedArraySpeciesConstructor(context, exemplar);

  // Let newTypedArray be ? Construct(constructor, argumentList).
  TNode<Object> new_object =
      CAST(ConstructJS(CodeFactory::Construct(isolate()), context, constructor,
                       buffer, byte_offset, len));

  // Perform ? ValidateTypedArray(newTypedArray).
  return ValidateTypedArray(context, new_object, method_name);
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::SpeciesCreateByLength(
    TNode<Context> context, TNode<JSTypedArray> exemplar, TNode<Smi> len,
    const char* method_name) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(len));

  // Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  TNode<Object> constructor = TypedArraySpeciesConstructor(context, exemplar);
  CSA_ASSERT(this, IsJSFunction(constructor));

  return CreateByLength(context, constructor, len, method_name);
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::CreateByLength(
    TNode<Context> context, TNode<Object> constructor, TNode<Smi> len,
    const char* method_name) {
  // Let newTypedArray be ? Construct(constructor, argumentList).
  TNode<Object> new_object = CAST(ConstructJS(CodeFactory::Construct(isolate()),
                                              context, constructor, len));

  // Perform ? ValidateTypedArray(newTypedArray).
  TNode<JSTypedArray> new_typed_array =
      ValidateTypedArray(context, new_object, method_name);

  // If newTypedArray.[[ArrayLength]] < argumentList[0], throw a TypeError
  // exception.
  Label if_length_is_not_short(this);
  TNode<Smi> new_length =
      LoadObjectField<Smi>(new_typed_array, JSTypedArray::kLengthOffset);
  GotoIfNot(SmiLessThan(new_length, len), &if_length_is_not_short);
  ThrowTypeError(context, MessageTemplate::kTypedArrayTooShort);

  BIND(&if_length_is_not_short);
  return new_typed_array;
}

TNode<JSArrayBuffer> TypedArrayBuiltinsAssembler::GetBuffer(
    TNode<Context> context, TNode<JSTypedArray> array) {
  Label call_runtime(this), done(this);
  TVARIABLE(Object, var_result);

  TNode<Object> buffer = LoadObjectField(array, JSTypedArray::kBufferOffset);
  GotoIf(IsDetachedBuffer(buffer), &call_runtime);
  TNode<UintPtrT> backing_store = LoadObjectField<UintPtrT>(
      CAST(buffer), JSArrayBuffer::kBackingStoreOffset);
  GotoIf(WordEqual(backing_store, IntPtrConstant(0)), &call_runtime);
  var_result = buffer;
  Goto(&done);

  BIND(&call_runtime);
  {
    var_result = CallRuntime(Runtime::kTypedArrayGetBuffer, context, array);
    Goto(&done);
  }

  BIND(&done);
  return CAST(var_result.value());
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::ValidateTypedArray(
    TNode<Context> context, TNode<Object> obj, const char* method_name) {
  Label validation_done(this);

  // If it is not a typed array, throw
  ThrowIfNotInstanceType(context, obj, JS_TYPED_ARRAY_TYPE, method_name);

  // If the typed array's buffer is detached, throw
  TNode<Object> buffer =
      LoadObjectField(CAST(obj), JSTypedArray::kBufferOffset);
  GotoIfNot(IsDetachedBuffer(buffer), &validation_done);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation, method_name);

  BIND(&validation_done);
  return CAST(obj);
}

void TypedArrayBuiltinsAssembler::SetTypedArraySource(
    TNode<Context> context, TNode<JSTypedArray> source,
    TNode<JSTypedArray> target, TNode<IntPtrT> offset, Label* call_runtime,
    Label* if_source_too_large) {
  CSA_ASSERT(this, Word32BinaryNot(IsDetachedBuffer(
                       LoadObjectField(source, JSTypedArray::kBufferOffset))));
  CSA_ASSERT(this, Word32BinaryNot(IsDetachedBuffer(
                       LoadObjectField(target, JSTypedArray::kBufferOffset))));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(offset, IntPtrConstant(0)));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(offset, IntPtrConstant(Smi::kMaxValue)));

  // Check for possible range errors.

  TNode<IntPtrT> source_length =
      LoadAndUntagObjectField(source, JSTypedArray::kLengthOffset);
  TNode<IntPtrT> target_length =
      LoadAndUntagObjectField(target, JSTypedArray::kLengthOffset);
  TNode<IntPtrT> required_target_length = IntPtrAdd(source_length, offset);

  GotoIf(IntPtrGreaterThan(required_target_length, target_length),
         if_source_too_large);

  // Grab pointers and byte lengths we need later on.

  TNode<IntPtrT> target_data_ptr = UncheckedCast<IntPtrT>(LoadDataPtr(target));
  TNode<IntPtrT> source_data_ptr = UncheckedCast<IntPtrT>(LoadDataPtr(source));

  TNode<Word32T> source_el_kind = LoadElementsKind(source);
  TNode<Word32T> target_el_kind = LoadElementsKind(target);

  TNode<IntPtrT> source_el_size = GetTypedArrayElementSize(source_el_kind);
  TNode<IntPtrT> target_el_size = GetTypedArrayElementSize(target_el_kind);

  // A note on byte lengths: both source- and target byte lengths must be valid,
  // i.e. it must be possible to allocate an array of the given length. That
  // means we're safe from overflows in the following multiplication.
  TNode<IntPtrT> source_byte_length = IntPtrMul(source_length, source_el_size);
  CSA_ASSERT(this,
             UintPtrGreaterThanOrEqual(source_byte_length, IntPtrConstant(0)));

  Label call_memmove(this), fast_c_call(this), out(this), exception(this);

  // A fast memmove call can be used when the source and target types are are
  // the same or either Uint8 or Uint8Clamped.
  GotoIf(Word32Equal(source_el_kind, target_el_kind), &call_memmove);
  GotoIfNot(IsUint8ElementsKind(source_el_kind), &fast_c_call);
  Branch(IsUint8ElementsKind(target_el_kind), &call_memmove, &fast_c_call);

  BIND(&call_memmove);
  {
    TNode<IntPtrT> target_start =
        IntPtrAdd(target_data_ptr, IntPtrMul(offset, target_el_size));
    CallCMemmove(target_start, source_data_ptr, source_byte_length);
    Goto(&out);
  }

  BIND(&fast_c_call);
  {
    CSA_ASSERT(
        this, UintPtrGreaterThanOrEqual(
                  IntPtrMul(target_length, target_el_size), IntPtrConstant(0)));

    GotoIf(Word32NotEqual(IsBigInt64ElementsKind(source_el_kind),
                          IsBigInt64ElementsKind(target_el_kind)),
           &exception);

    TNode<IntPtrT> source_length =
        LoadAndUntagObjectField(source, JSTypedArray::kLengthOffset);
    CallCCopyTypedArrayElementsToTypedArray(source, target, source_length,
                                            offset);
    Goto(&out);
  }

  BIND(&exception);
  ThrowTypeError(context, MessageTemplate::kBigIntMixedTypes);

  BIND(&out);
}

void TypedArrayBuiltinsAssembler::SetJSArraySource(
    TNode<Context> context, TNode<JSArray> source, TNode<JSTypedArray> target,
    TNode<IntPtrT> offset, Label* call_runtime, Label* if_source_too_large) {
  CSA_ASSERT(this, IsFastJSArray(source, context));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(offset, IntPtrConstant(0)));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(offset, IntPtrConstant(Smi::kMaxValue)));

  TNode<IntPtrT> source_length = SmiUntag(LoadFastJSArrayLength(source));
  TNode<IntPtrT> target_length =
      LoadAndUntagObjectField(target, JSTypedArray::kLengthOffset);

  // Maybe out of bounds?
  GotoIf(IntPtrGreaterThan(IntPtrAdd(source_length, offset), target_length),
         if_source_too_large);

  // Nothing to do if {source} is empty.
  Label out(this), fast_c_call(this);
  GotoIf(IntPtrEqual(source_length, IntPtrConstant(0)), &out);

  // Dispatch based on the source elements kind.
  {
    // These are the supported elements kinds in TryCopyElementsFastNumber.
    int32_t values[] = {
        PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS, PACKED_DOUBLE_ELEMENTS,
        HOLEY_DOUBLE_ELEMENTS,
    };
    Label* labels[] = {
        &fast_c_call, &fast_c_call, &fast_c_call, &fast_c_call,
    };
    STATIC_ASSERT(arraysize(values) == arraysize(labels));

    TNode<Int32T> source_elements_kind = LoadMapElementsKind(LoadMap(source));
    Switch(source_elements_kind, call_runtime, values, labels,
           arraysize(values));
  }

  BIND(&fast_c_call);
  GotoIf(IsBigInt64ElementsKind(LoadElementsKind(target)), call_runtime);
  CallCCopyFastNumberJSArrayElementsToTypedArray(context, source, target,
                                                 source_length, offset);
  Goto(&out);
  BIND(&out);
}

void TypedArrayBuiltinsAssembler::CallCMemmove(TNode<IntPtrT> dest_ptr,
                                               TNode<IntPtrT> src_ptr,
                                               TNode<IntPtrT> byte_length) {
  TNode<ExternalReference> memmove =
      ExternalConstant(ExternalReference::libc_memmove_function(isolate()));
  CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                 MachineType::Pointer(), MachineType::UintPtr(), memmove,
                 dest_ptr, src_ptr, byte_length);
}

void TypedArrayBuiltinsAssembler::
    CallCCopyFastNumberJSArrayElementsToTypedArray(TNode<Context> context,
                                                   TNode<JSArray> source,
                                                   TNode<JSTypedArray> dest,
                                                   TNode<IntPtrT> source_length,
                                                   TNode<IntPtrT> offset) {
  CSA_ASSERT(this, Word32Not(IsBigInt64ElementsKind(LoadElementsKind(dest))));
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_fast_number_jsarray_elements_to_typed_array(
          isolate()));
  CallCFunction5(MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::UintPtr(), MachineType::UintPtr(), f, context,
                 source, dest, source_length, offset);
}

void TypedArrayBuiltinsAssembler::CallCCopyTypedArrayElementsToTypedArray(
    TNode<JSTypedArray> source, TNode<JSTypedArray> dest,
    TNode<IntPtrT> source_length, TNode<IntPtrT> offset) {
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_typed_array_elements_to_typed_array(isolate()));
  CallCFunction4(MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::AnyTagged(), MachineType::UintPtr(),
                 MachineType::UintPtr(), f, source, dest, source_length,
                 offset);
}

void TypedArrayBuiltinsAssembler::CallCCopyTypedArrayElementsSlice(
    TNode<JSTypedArray> source, TNode<JSTypedArray> dest, TNode<IntPtrT> start,
    TNode<IntPtrT> end) {
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_typed_array_elements_slice(isolate()));
  CallCFunction4(MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::AnyTagged(), MachineType::UintPtr(),
                 MachineType::UintPtr(), f, source, dest, start, end);
}

void TypedArrayBuiltinsAssembler::DispatchTypedArrayByElementsKind(
    TNode<Word32T> elements_kind, const TypedArraySwitchCase& case_function) {
  Label next(this), if_unknown_type(this, Label::kDeferred);

  int32_t elements_kinds[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) TYPE##_ELEMENTS,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  Label if_##type##array(this);
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  Label* elements_kind_labels[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) &if_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  STATIC_ASSERT(arraysize(elements_kinds) == arraysize(elements_kind_labels));

  Switch(elements_kind, &if_unknown_type, elements_kinds, elements_kind_labels,
         arraysize(elements_kinds));

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                    \
  BIND(&if_##type##array);                                                 \
  {                                                                        \
    case_function(TYPE##_ELEMENTS, size, Context::TYPE##_ARRAY_FUN_INDEX); \
    Goto(&next);                                                           \
  }
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  BIND(&if_unknown_type);
  Unreachable();

  BIND(&next);
}

// ES #sec-get-%typedarray%.prototype.set
TF_BUILTIN(TypedArrayPrototypeSet, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));
  CodeStubArguments args(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));

  Label if_source_is_typed_array(this), if_source_is_fast_jsarray(this),
      if_offset_is_out_of_bounds(this, Label::kDeferred),
      if_source_too_large(this, Label::kDeferred),
      if_typed_array_is_neutered(this, Label::kDeferred),
      if_receiver_is_not_typedarray(this, Label::kDeferred);

  // Check the receiver is a typed array.
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &if_receiver_is_not_typedarray);
  GotoIfNot(IsJSTypedArray(receiver), &if_receiver_is_not_typedarray);

  // Normalize offset argument (using ToInteger) and handle heap number cases.
  TNode<Object> offset = args.GetOptionalArgumentValue(1, SmiConstant(0));
  TNode<Number> offset_num =
      ToInteger_Inline(context, offset, kTruncateMinusZero);

  // Since ToInteger always returns a Smi if the given value is within Smi
  // range, and the only corner case of -0.0 has already been truncated to 0.0,
  // we can simply throw unless the offset is a non-negative Smi.
  // TODO(jgruber): It's an observable spec violation to throw here if
  // {offset_num} is a positive number outside the Smi range. Per spec, we need
  // to check for detached buffers and call the observable ToObject/ToLength
  // operations first.
  GotoIfNot(TaggedIsPositiveSmi(offset_num), &if_offset_is_out_of_bounds);
  TNode<Smi> offset_smi = CAST(offset_num);

  // Check the receiver is not neutered.
  TNode<Object> receiver_buffer =
      LoadObjectField(CAST(receiver), JSTypedArray::kBufferOffset);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_typed_array_is_neutered);

  // Check the source argument is valid and whether a fast path can be taken.
  Label call_runtime(this);
  TNode<Object> source = args.GetOptionalArgumentValue(0);
  GotoIf(TaggedIsSmi(source), &call_runtime);
  GotoIf(IsJSTypedArray(source), &if_source_is_typed_array);
  BranchIfFastJSArray(source, context, &if_source_is_fast_jsarray,
                      &call_runtime);

  // Fast path for a typed array source argument.
  BIND(&if_source_is_typed_array);
  {
    // Check the source argument is not neutered.
    TNode<Object> source_buffer =
        LoadObjectField(CAST(source), JSTypedArray::kBufferOffset);
    GotoIf(IsDetachedBuffer(source_buffer), &if_typed_array_is_neutered);

    SetTypedArraySource(context, CAST(source), CAST(receiver),
                        SmiUntag(offset_smi), &call_runtime,
                        &if_source_too_large);
    args.PopAndReturn(UndefinedConstant());
  }

  // Fast path for a fast JSArray source argument.
  BIND(&if_source_is_fast_jsarray);
  {
    SetJSArraySource(context, CAST(source), CAST(receiver),
                     SmiUntag(offset_smi), &call_runtime, &if_source_too_large);
    args.PopAndReturn(UndefinedConstant());
  }

  BIND(&call_runtime);
  args.PopAndReturn(CallRuntime(Runtime::kTypedArraySet, context, receiver,
                                source, offset_smi));

  BIND(&if_offset_is_out_of_bounds);
  ThrowRangeError(context, MessageTemplate::kTypedArraySetOffsetOutOfBounds);

  BIND(&if_source_too_large);
  ThrowRangeError(context, MessageTemplate::kTypedArraySetSourceTooLarge);

  BIND(&if_typed_array_is_neutered);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation,
                 "%TypedArray%.prototype.set");

  BIND(&if_receiver_is_not_typedarray);
  ThrowTypeError(context, MessageTemplate::kNotTypedArray);
}

// ES %TypedArray%.prototype.slice
TF_BUILTIN(TypedArrayPrototypeSlice, TypedArrayBuiltinsAssembler) {
  const char* method_name = "%TypedArray%.prototype.slice";
  Label call_c(this), call_memmove(this), if_count_is_not_zero(this),
      if_typed_array_is_neutered(this, Label::kDeferred),
      if_bigint_mixed_types(this, Label::kDeferred);

  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));
  CodeStubArguments args(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));

  TNode<Object> receiver = args.GetReceiver();
  TNode<JSTypedArray> source =
      ValidateTypedArray(context, receiver, method_name);

  TNode<Smi> source_length =
      LoadObjectField<Smi>(source, JSTypedArray::kLengthOffset);

  // Convert start offset argument to integer, and calculate relative offset.
  TNode<Object> start = args.GetOptionalArgumentValue(0, SmiConstant(0));
  TNode<Smi> start_index =
      SmiTag(ConvertToRelativeIndex(context, start, SmiUntag(source_length)));

  // Convert end offset argument to integer, and calculate relative offset.
  // If end offset is not given or undefined is given, set source_length to
  // "end_index".
  TNode<Object> end = args.GetOptionalArgumentValue(1, UndefinedConstant());
  TNode<Smi> end_index =
      Select<Smi>(IsUndefined(end), [=] { return source_length; },
                  [=] {
                    return SmiTag(ConvertToRelativeIndex(
                        context, end, SmiUntag(source_length)));
                  });

  // Create a result array by invoking TypedArraySpeciesCreate.
  TNode<Smi> count = SmiMax(SmiSub(end_index, start_index), SmiConstant(0));
  TNode<JSTypedArray> result_array =
      SpeciesCreateByLength(context, source, count, method_name);

  // If count is zero, return early.
  GotoIf(SmiGreaterThan(count, SmiConstant(0)), &if_count_is_not_zero);
  args.PopAndReturn(result_array);

  BIND(&if_count_is_not_zero);
  // Check the source array is neutered or not. We don't need to check if the
  // result array is neutered or not since TypedArraySpeciesCreate checked it.
  CSA_ASSERT(this, Word32BinaryNot(IsDetachedBuffer(LoadObjectField(
                       result_array, JSTypedArray::kBufferOffset))));
  TNode<Object> receiver_buffer =
      LoadObjectField(CAST(receiver), JSTypedArray::kBufferOffset);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_typed_array_is_neutered);

  // result_array could be a different type from source or share the same
  // buffer with the source because of custom species constructor.
  // If the types of source and result array are the same and they are not
  // sharing the same buffer, use memmove.
  TNode<Word32T> source_el_kind = LoadElementsKind(source);
  TNode<Word32T> target_el_kind = LoadElementsKind(result_array);
  GotoIfNot(Word32Equal(source_el_kind, target_el_kind), &call_c);

  TNode<Object> target_buffer =
      LoadObjectField(result_array, JSTypedArray::kBufferOffset);
  Branch(WordEqual(receiver_buffer, target_buffer), &call_c, &call_memmove);

  BIND(&call_memmove);
  {
    GotoIfForceSlowPath(&call_c);

    TNode<IntPtrT> target_data_ptr =
        UncheckedCast<IntPtrT>(LoadDataPtr(result_array));
    TNode<IntPtrT> source_data_ptr =
        UncheckedCast<IntPtrT>(LoadDataPtr(source));

    TNode<IntPtrT> source_el_size = GetTypedArrayElementSize(source_el_kind);
    TNode<IntPtrT> source_start_bytes =
        IntPtrMul(SmiToIntPtr(start_index), source_el_size);
    TNode<IntPtrT> source_start =
        IntPtrAdd(source_data_ptr, source_start_bytes);

    TNode<IntPtrT> count_bytes = IntPtrMul(SmiToIntPtr(count), source_el_size);

#ifdef DEBUG
    TNode<IntPtrT> target_byte_length =
        LoadAndUntagObjectField(result_array, JSTypedArray::kByteLengthOffset);
    CSA_ASSERT(this, IntPtrLessThanOrEqual(count_bytes, target_byte_length));

    TNode<IntPtrT> source_byte_length =
        LoadAndUntagObjectField(source, JSTypedArray::kByteLengthOffset);
    TNode<IntPtrT> source_size_in_bytes =
        IntPtrSub(source_byte_length, source_start_bytes);
    CSA_ASSERT(this, IntPtrLessThanOrEqual(count_bytes, source_size_in_bytes));
#endif  // DEBUG

    CallCMemmove(target_data_ptr, source_start, count_bytes);
    args.PopAndReturn(result_array);
  }

  BIND(&call_c);
  {
    GotoIf(Word32NotEqual(IsBigInt64ElementsKind(source_el_kind),
                          IsBigInt64ElementsKind(target_el_kind)),
           &if_bigint_mixed_types);

    CallCCopyTypedArrayElementsSlice(
        source, result_array, SmiToIntPtr(start_index), SmiToIntPtr(end_index));
    args.PopAndReturn(result_array);
  }

  BIND(&if_typed_array_is_neutered);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation, method_name);

  BIND(&if_bigint_mixed_types);
  ThrowTypeError(context, MessageTemplate::kBigIntMixedTypes);
}

// ES %TypedArray%.prototype.subarray
TF_BUILTIN(TypedArrayPrototypeSubArray, TypedArrayBuiltinsAssembler) {
  const char* method_name = "%TypedArray%.prototype.subarray";
  Label offset_done(this);

  TVARIABLE(Smi, var_begin);
  TVARIABLE(Smi, var_end);

  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));
  CodeStubArguments args(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));

  // 1. Let O be the this value.
  // 3. If O does not have a [[TypedArrayName]] internal slot, throw a TypeError
  // exception.
  TNode<Object> receiver = args.GetReceiver();
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, method_name);

  TNode<JSTypedArray> source = CAST(receiver);

  // 5. Let buffer be O.[[ViewedArrayBuffer]].
  TNode<JSArrayBuffer> buffer = GetBuffer(context, source);
  // 6. Let srcLength be O.[[ArrayLength]].
  TNode<Smi> source_length =
      LoadObjectField<Smi>(source, JSTypedArray::kLengthOffset);

  // 7. Let relativeBegin be ? ToInteger(begin).
  // 8. If relativeBegin < 0, let beginIndex be max((srcLength + relativeBegin),
  // 0); else let beginIndex be min(relativeBegin, srcLength).
  TNode<Object> begin = args.GetOptionalArgumentValue(0, SmiConstant(0));
  var_begin =
      SmiTag(ConvertToRelativeIndex(context, begin, SmiUntag(source_length)));

  TNode<Object> end = args.GetOptionalArgumentValue(1, UndefinedConstant());
  // 9. If end is undefined, let relativeEnd be srcLength;
  var_end = source_length;
  GotoIf(IsUndefined(end), &offset_done);

  // else, let relativeEnd be ? ToInteger(end).
  // 10. If relativeEnd < 0, let endIndex be max((srcLength + relativeEnd), 0);
  // else let endIndex be min(relativeEnd, srcLength).
  var_end =
      SmiTag(ConvertToRelativeIndex(context, end, SmiUntag(source_length)));
  Goto(&offset_done);

  BIND(&offset_done);

  // 11. Let newLength be max(endIndex - beginIndex, 0).
  TNode<Smi> new_length =
      SmiMax(SmiSub(var_end.value(), var_begin.value()), SmiConstant(0));

  // 12. Let constructorName be the String value of O.[[TypedArrayName]].
  // 13. Let elementSize be the Number value of the Element Size value specified
  // in Table 52 for constructorName.
  TNode<Word32T> element_kind = LoadElementsKind(source);
  TNode<IntPtrT> element_size = GetTypedArrayElementSize(element_kind);

  // 14. Let srcByteOffset be O.[[ByteOffset]].
  TNode<Number> source_byte_offset =
      LoadObjectField<Number>(source, JSTypedArray::kByteOffsetOffset);

  // 15. Let beginByteOffset be srcByteOffset + beginIndex × elementSize.
  TNode<Number> offset = SmiMul(var_begin.value(), SmiFromIntPtr(element_size));
  TNode<Number> begin_byte_offset = NumberAdd(source_byte_offset, offset);

  // 16. Let argumentsList be « buffer, beginByteOffset, newLength ».
  // 17. Return ? TypedArraySpeciesCreate(O, argumentsList).
  args.PopAndReturn(SpeciesCreateByArrayBuffer(
      context, source, buffer, begin_byte_offset, new_length, method_name));
}

// ES #sec-get-%typedarray%.prototype-@@tostringtag
TF_BUILTIN(TypedArrayPrototypeToStringTag, TypedArrayBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Label if_receiverisheapobject(this), return_undefined(this);
  Branch(TaggedIsSmi(receiver), &return_undefined, &if_receiverisheapobject);

  // Dispatch on the elements kind, offset by
  // FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND.
  size_t const kTypedElementsKindCount = LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND -
                                         FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND +
                                         1;
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  Label return_##type##array(this);                     \
  BIND(&return_##type##array);                          \
  Return(StringConstant(#Type "Array"));
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  Label* elements_kind_labels[kTypedElementsKindCount] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) &return_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  int32_t elements_kinds[kTypedElementsKindCount] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  TYPE##_ELEMENTS - FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

  // We offset the dispatch by FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND, so
  // that this can be turned into a non-sparse table switch for ideal
  // performance.
  BIND(&if_receiverisheapobject);
  Node* elements_kind =
      Int32Sub(LoadMapElementsKind(LoadMap(receiver)),
               Int32Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));
  Switch(elements_kind, &return_undefined, elements_kinds, elements_kind_labels,
         kTypedElementsKindCount);

  BIND(&return_undefined);
  Return(UndefinedConstant());
}

void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeIterationMethod(
    Node* context, Node* receiver, const char* method_name,
    IterationKind kind) {
  Label throw_bad_receiver(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &throw_bad_receiver);
  GotoIfNot(IsJSTypedArray(receiver), &throw_bad_receiver);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(this, Label::kDeferred);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_receiverisneutered);

  Return(CreateArrayIterator(context, receiver, kind));

  BIND(&throw_bad_receiver);
  ThrowTypeError(context, MessageTemplate::kNotTypedArray, method_name);

  BIND(&if_receiverisneutered);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation, method_name);
}

// ES #sec-%typedarray%.prototype.values
TF_BUILTIN(TypedArrayPrototypeValues, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.values()",
                                             IterationKind::kValues);
}

// ES #sec-%typedarray%.prototype.entries
TF_BUILTIN(TypedArrayPrototypeEntries, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.entries()",
                                             IterationKind::kEntries);
}

// ES #sec-%typedarray%.prototype.keys
TF_BUILTIN(TypedArrayPrototypeKeys, TypedArrayBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  GenerateTypedArrayPrototypeIterationMethod(
      context, receiver, "%TypedArray%.prototype.keys()", IterationKind::kKeys);
}

// ES6 #sec-%typedarray%.of
TF_BUILTIN(TypedArrayOf, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));

  // 1. Let len be the actual number of arguments passed to this function.
  TNode<IntPtrT> length = ChangeInt32ToIntPtr(
      UncheckedCast<Int32T>(Parameter(BuiltinDescriptor::kArgumentsCount)));
  // 2. Let items be the List of arguments passed to this function.
  CodeStubArguments args(this, length, nullptr, INTPTR_PARAMETERS,
                         CodeStubArguments::ReceiverMode::kHasReceiver);

  Label if_not_constructor(this, Label::kDeferred),
      if_neutered(this, Label::kDeferred);

  // 3. Let C be the this value.
  // 4. If IsConstructor(C) is false, throw a TypeError exception.
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &if_not_constructor);
  GotoIfNot(IsConstructor(receiver), &if_not_constructor);

  // 5. Let newObj be ? TypedArrayCreate(C, len).
  TNode<JSTypedArray> new_typed_array =
      CreateByLength(context, receiver, SmiTag(length), "%TypedArray%.of");

  TNode<Word32T> elements_kind = LoadElementsKind(new_typed_array);

  // 6. Let k be 0.
  // 7. Repeat, while k < len
  //  a. Let kValue be items[k].
  //  b. Let Pk be ! ToString(k).
  //  c. Perform ? Set(newObj, Pk, kValue, true).
  //  d. Increase k by 1.
  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind kind, int size, int typed_array_fun_index) {
        TNode<FixedTypedArrayBase> elements =
            CAST(LoadElements(new_typed_array));
        BuildFastLoop(
            IntPtrConstant(0), length,
            [&](Node* index) {
              TNode<Object> item = args.AtIndex(index, INTPTR_PARAMETERS);
              TNode<IntPtrT> intptr_index = UncheckedCast<IntPtrT>(index);
              if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
                EmitBigTypedArrayElementStore(new_typed_array, elements,
                                              intptr_index, item, context,
                                              &if_neutered);
              } else {
                Node* value =
                    PrepareValueForWriteToTypedArray(item, kind, context);

                // ToNumber may execute JavaScript code, which could neuter
                // the array's buffer.
                Node* buffer = LoadObjectField(new_typed_array,
                                               JSTypedArray::kBufferOffset);
                GotoIf(IsDetachedBuffer(buffer), &if_neutered);

                // GC may move backing store in ToNumber, thus load backing
                // store everytime in this loop.
                TNode<RawPtrT> backing_store =
                    LoadFixedTypedArrayBackingStore(elements);
                StoreElement(backing_store, kind, index, value,
                             INTPTR_PARAMETERS);
              }
            },
            1, ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      });

  // 8. Return newObj.
  args.PopAndReturn(new_typed_array);

  BIND(&if_not_constructor);
  ThrowTypeError(context, MessageTemplate::kNotConstructor, receiver);

  BIND(&if_neutered);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation,
                 "%TypedArray%.of");
}

TF_BUILTIN(IterableToList, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> iterable = CAST(Parameter(Descriptor::kIterable));
  TNode<Object> iterator_fn = CAST(Parameter(Descriptor::kIteratorFn));

  Label fast_path(this), slow_path(this), done(this);

  TVARIABLE(JSArray, created_list);

  // This is a fast-path for ignoring the iterator.
  // TODO(petermarshall): Port to CSA.
  Node* elided =
      CallRuntime(Runtime::kIterableToListCanBeElided, context, iterable);
  CSA_ASSERT(this, IsBoolean(elided));
  Branch(IsTrue(elided), &fast_path, &slow_path);

  BIND(&fast_path);
  {
    created_list = CAST(iterable);
    Goto(&done);
  }

  BIND(&slow_path);
  {
    IteratorBuiltinsAssembler iterator_assembler(state());

    // 1. Let iteratorRecord be ? GetIterator(items, method).
    IteratorRecord iterator_record =
        iterator_assembler.GetIterator(context, iterable, iterator_fn);

    // 2. Let values be a new empty List.
    GrowableFixedArray values(state());

    Variable* vars[] = {values.var_array(), values.var_length(),
                        values.var_capacity()};
    Label loop_start(this, 3, vars), loop_end(this);
    Goto(&loop_start);
    // 3. Let next be true.
    // 4. Repeat, while next is not false
    BIND(&loop_start);
    {
      //  a. Set next to ? IteratorStep(iteratorRecord).
      TNode<Object> next = CAST(
          iterator_assembler.IteratorStep(context, iterator_record, &loop_end));
      //  b. If next is not false, then
      //   i. Let nextValue be ? IteratorValue(next).
      TNode<Object> next_value =
          CAST(iterator_assembler.IteratorValue(context, next));
      //   ii. Append nextValue to the end of the List values.
      values.Push(next_value);
      Goto(&loop_start);
    }
    BIND(&loop_end);

    // 5. Return values.
    TNode<JSArray> js_array_values = values.ToJSArray(context);
    created_list = js_array_values;
    Goto(&done);
  }

  BIND(&done);
  Return(created_list.value());
}

// ES6 #sec-%typedarray%.from
TF_BUILTIN(TypedArrayFrom, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));

  Label check_iterator(this), from_array_like(this), fast_path(this),
      slow_path(this), create_typed_array(this),
      if_not_constructor(this, Label::kDeferred),
      if_map_fn_not_callable(this, Label::kDeferred),
      if_iterator_fn_not_callable(this, Label::kDeferred),
      if_neutered(this, Label::kDeferred);

  CodeStubArguments args(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));
  TNode<Object> source = args.GetOptionalArgumentValue(0);

  // 5. If thisArg is present, let T be thisArg; else let T be undefined.
  TNode<Object> this_arg = args.GetOptionalArgumentValue(2);

  // 1. Let C be the this value.
  // 2. If IsConstructor(C) is false, throw a TypeError exception.
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &if_not_constructor);
  GotoIfNot(IsConstructor(receiver), &if_not_constructor);

  // 3. If mapfn is present and mapfn is not undefined, then
  TNode<Object> map_fn = args.GetOptionalArgumentValue(1);
  TVARIABLE(BoolT, mapping, Int32FalseConstant());
  GotoIf(IsUndefined(map_fn), &check_iterator);

  //  a. If IsCallable(mapfn) is false, throw a TypeError exception.
  //  b. Let mapping be true.
  // 4. Else, let mapping be false.
  GotoIf(TaggedIsSmi(map_fn), &if_map_fn_not_callable);
  GotoIfNot(IsCallable(map_fn), &if_map_fn_not_callable);
  mapping = Int32TrueConstant();
  Goto(&check_iterator);

  TVARIABLE(Object, final_source);
  TVARIABLE(Smi, final_length);

  // We split up this builtin differently to the way it is written in the spec.
  // We already have great code in the elements accessor for copying from a
  // JSArray into a TypedArray, so we use that when possible. We only avoid
  // calling into the elements accessor when we have a mapping function, because
  // we can't handle that. Here, presence of a mapping function is the slow
  // path. We also combine the two different loops in the specification
  // (starting at 7.e and 13) because they are essentially identical. We also
  // save on code-size this way.

  BIND(&check_iterator);
  {
    // 6. Let usingIterator be ? GetMethod(source, @@iterator).
    TNode<Object> iterator_fn =
        CAST(GetMethod(context, source, isolate()->factory()->iterator_symbol(),
                       &from_array_like));
    GotoIf(TaggedIsSmi(iterator_fn), &if_iterator_fn_not_callable);
    GotoIfNot(IsCallable(iterator_fn), &if_iterator_fn_not_callable);

    // We are using the iterator.
    Label if_length_not_smi(this, Label::kDeferred);
    // 7. If usingIterator is not undefined, then
    //  a. Let values be ? IterableToList(source, usingIterator).
    //  b. Let len be the number of elements in values.
    TNode<JSArray> values = CAST(
        CallBuiltin(Builtins::kIterableToList, context, source, iterator_fn));

    // This is not a spec'd limit, so it doesn't particularly matter when we
    // throw the range error for typed array length > MaxSmi.
    TNode<Object> raw_length = LoadJSArrayLength(values);
    GotoIfNot(TaggedIsSmi(raw_length), &if_length_not_smi);

    final_length = CAST(raw_length);
    final_source = values;
    Goto(&create_typed_array);

    BIND(&if_length_not_smi);
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength,
                    raw_length);
  }

  BIND(&from_array_like);
  {
    Label if_length_not_smi(this, Label::kDeferred);
    final_source = source;

    // 10. Let len be ? ToLength(? Get(arrayLike, "length")).
    TNode<Object> raw_length =
        GetProperty(context, final_source.value(), LengthStringConstant());
    final_length = ToSmiLength(raw_length, context, &if_length_not_smi);
    Goto(&create_typed_array);

    BIND(&if_length_not_smi);
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength,
                    raw_length);
  }

  TVARIABLE(JSTypedArray, target_obj);

  BIND(&create_typed_array);
  {
    // 7c/11. Let targetObj be ? TypedArrayCreate(C, «len»).
    target_obj = CreateByLength(context, receiver, final_length.value(),
                                "%TypedArray%.from");

    Branch(mapping.value(), &slow_path, &fast_path);
  }

  BIND(&fast_path);
  {
    Label done(this);
    GotoIf(SmiEqual(final_length.value(), SmiConstant(0)), &done);

    CallRuntime(Runtime::kTypedArrayCopyElements, context, target_obj.value(),
                final_source.value(), final_length.value());
    Goto(&done);

    BIND(&done);
    args.PopAndReturn(target_obj.value());
  }

  BIND(&slow_path);
  TNode<Word32T> elements_kind = LoadElementsKind(target_obj.value());

  // 7e/13 : Copy the elements
  TNode<FixedTypedArrayBase> elements = CAST(LoadElements(target_obj.value()));
  BuildFastLoop(
      SmiConstant(0), final_length.value(),
      [&](Node* index) {
        TNode<Object> const k_value =
            GetProperty(context, final_source.value(), index);

        TNode<Object> const mapped_value =
            CAST(CallJS(CodeFactory::Call(isolate()), context, map_fn, this_arg,
                        k_value, index));

        TNode<IntPtrT> intptr_index = SmiUntag(index);
        DispatchTypedArrayByElementsKind(
            elements_kind,
            [&](ElementsKind kind, int size, int typed_array_fun_index) {
              if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
                EmitBigTypedArrayElementStore(target_obj.value(), elements,
                                              intptr_index, mapped_value,
                                              context, &if_neutered);
              } else {
                Node* const final_value = PrepareValueForWriteToTypedArray(
                    mapped_value, kind, context);

                // ToNumber may execute JavaScript code, which could neuter
                // the array's buffer.
                Node* buffer = LoadObjectField(target_obj.value(),
                                               JSTypedArray::kBufferOffset);
                GotoIf(IsDetachedBuffer(buffer), &if_neutered);

                // GC may move backing store in map_fn, thus load backing
                // store in each iteration of this loop.
                TNode<RawPtrT> backing_store =
                    LoadFixedTypedArrayBackingStore(elements);
                StoreElement(backing_store, kind, index, final_value,
                             SMI_PARAMETERS);
              }
            });
      },
      1, ParameterMode::SMI_PARAMETERS, IndexAdvanceMode::kPost);

  args.PopAndReturn(target_obj.value());

  BIND(&if_not_constructor);
  ThrowTypeError(context, MessageTemplate::kNotConstructor, receiver);

  BIND(&if_map_fn_not_callable);
  ThrowTypeError(context, MessageTemplate::kCalledNonCallable, map_fn);

  BIND(&if_iterator_fn_not_callable);
  ThrowTypeError(context, MessageTemplate::kIteratorSymbolNonCallable);

  BIND(&if_neutered);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation,
                 "%TypedArray%.from");
}

// ES %TypedArray%.prototype.filter
TF_BUILTIN(TypedArrayPrototypeFilter, TypedArrayBuiltinsAssembler) {
  const char* method_name = "%TypedArray%.prototype.filter";

  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));
  CodeStubArguments args(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));

  Label if_callback_not_callable(this, Label::kDeferred),
      detached(this, Label::kDeferred);

  // 1. Let O be the this value.
  // 2. Perform ? ValidateTypedArray(O).
  TNode<Object> receiver = args.GetReceiver();
  TNode<JSTypedArray> source =
      ValidateTypedArray(context, receiver, method_name);

  // 3. Let len be O.[[ArrayLength]].
  TNode<Smi> length = LoadObjectField<Smi>(source, JSTypedArray::kLengthOffset);

  // 4. If IsCallable(callbackfn) is false, throw a TypeError exception.
  TNode<Object> callbackfn = args.GetOptionalArgumentValue(0);
  GotoIf(TaggedIsSmi(callbackfn), &if_callback_not_callable);
  GotoIfNot(IsCallable(callbackfn), &if_callback_not_callable);

  // 5. If thisArg is present, let T be thisArg; else let T be undefined.
  TNode<Object> this_arg = args.GetOptionalArgumentValue(1);

  TNode<JSArrayBuffer> source_buffer =
      LoadObjectField<JSArrayBuffer>(source, JSArrayBufferView::kBufferOffset);
  TNode<Word32T> elements_kind = LoadElementsKind(source);
  GrowableFixedArray values(state());
  VariableList vars(
      {values.var_array(), values.var_length(), values.var_capacity()}, zone());

  // 6. Let kept be a new empty List.
  // 7. Let k be 0.
  // 8. Let captured be 0.
  // 9. Repeat, while k < len
  BuildFastLoop(
      vars, SmiConstant(0), length,
      [&](Node* index) {
        GotoIf(IsDetachedBuffer(source_buffer), &detached);

        TVARIABLE(Numeric, value);
        // a. Let Pk be ! ToString(k).
        // b. Let kValue be ? Get(O, Pk).
        DispatchTypedArrayByElementsKind(
            elements_kind,
            [&](ElementsKind kind, int size, int typed_array_fun_index) {
              TNode<IntPtrT> backing_store =
                  UncheckedCast<IntPtrT>(LoadDataPtr(source));
              value = CAST(LoadFixedTypedArrayElementAsTagged(
                  backing_store, index, kind, ParameterMode::SMI_PARAMETERS));
            });

        // c. Let selected be ToBoolean(Call(callbackfn, T, kValue, k, O))
        Node* selected =
            CallJS(CodeFactory::Call(isolate()), context, callbackfn, this_arg,
                   value.value(), index, source);

        Label true_continue(this), false_continue(this);
        BranchIfToBooleanIsTrue(selected, &true_continue, &false_continue);

        BIND(&true_continue);
        // d. If selected is true, then
        //   i. Append kValue to the end of kept.
        //   ii. Increase captured by 1.
        values.Push(value.value());
        Goto(&false_continue);

        BIND(&false_continue);
      },
      1, ParameterMode::SMI_PARAMETERS, IndexAdvanceMode::kPost);

  TNode<JSArray> values_array = values.ToJSArray(context);
  TNode<Smi> captured = LoadFastJSArrayLength(values_array);

  // 10. Let A be ? TypedArraySpeciesCreate(O, captured).
  TNode<JSTypedArray> result_array =
      SpeciesCreateByLength(context, source, captured, method_name);

  // 11. Let n be 0.
  // 12. For each element e of kept, do
  //   a. Perform ! Set(A, ! ToString(n), e, true).
  //   b. Increment n by 1.
  CallRuntime(Runtime::kTypedArrayCopyElements, context, result_array,
              values_array, captured);

  // 13. Return A.
  args.PopAndReturn(result_array);

  BIND(&if_callback_not_callable);
  ThrowTypeError(context, MessageTemplate::kCalledNonCallable, callbackfn);

  BIND(&detached);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation, method_name);
}

#undef V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP

}  // namespace internal
}  // namespace v8
