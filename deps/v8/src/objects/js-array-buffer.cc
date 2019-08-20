// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-array-buffer.h"
#include "src/objects/js-array-buffer-inl.h"

#include "src/logging/counters.h"
#include "src/objects/property-descriptor.h"

namespace v8 {
namespace internal {

namespace {

bool CanonicalNumericIndexString(Isolate* isolate, Handle<Object> s,
                                 Handle<Object>* index) {
  DCHECK(s->IsString() || s->IsSmi());

  Handle<Object> result;
  if (s->IsSmi()) {
    result = s;
  } else {
    result = String::ToNumber(isolate, Handle<String>::cast(s));
    if (!result->IsMinusZero()) {
      Handle<String> str = Object::ToString(isolate, result).ToHandleChecked();
      // Avoid treating strings like "2E1" and "20" as the same key.
      if (!str->SameValue(*s)) return false;
    }
  }
  *index = result;
  return true;
}

inline int ConvertToMb(size_t size) {
  return static_cast<int>(size / static_cast<size_t>(MB));
}

}  // anonymous namespace

void JSArrayBuffer::Detach() {
  CHECK(is_detachable());
  CHECK(!was_detached());
  CHECK(is_external());
  set_backing_store(nullptr);
  set_byte_length(0);
  set_was_detached(true);
  set_is_detachable(false);
  // Invalidate the detaching protector.
  Isolate* const isolate = GetIsolate();
  if (isolate->IsArrayBufferDetachingIntact()) {
    isolate->InvalidateArrayBufferDetachingProtector();
  }
}

void JSArrayBuffer::FreeBackingStoreFromMainThread() {
  if (allocation_base() == nullptr) {
    return;
  }
  FreeBackingStore(GetIsolate(), {allocation_base(), allocation_length(),
                                  backing_store(), is_wasm_memory()});
  // Zero out the backing store and allocation base to avoid dangling
  // pointers.
  set_backing_store(nullptr);
}

// static
void JSArrayBuffer::FreeBackingStore(Isolate* isolate, Allocation allocation) {
  if (allocation.is_wasm_memory) {
    wasm::WasmMemoryTracker* memory_tracker =
        isolate->wasm_engine()->memory_tracker();
    memory_tracker->FreeWasmMemory(isolate, allocation.backing_store);
  } else {
    isolate->array_buffer_allocator()->Free(allocation.allocation_base,
                                            allocation.length);
  }
}

void JSArrayBuffer::Setup(Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
                          bool is_external, void* data, size_t byte_length,
                          SharedFlag shared_flag, bool is_wasm_memory) {
  DCHECK_EQ(array_buffer->GetEmbedderFieldCount(),
            v8::ArrayBuffer::kEmbedderFieldCount);
  DCHECK_LE(byte_length, JSArrayBuffer::kMaxByteLength);
  for (int i = 0; i < v8::ArrayBuffer::kEmbedderFieldCount; i++) {
    array_buffer->SetEmbedderField(i, Smi::kZero);
  }
  array_buffer->set_byte_length(byte_length);
  array_buffer->set_bit_field(0);
  array_buffer->clear_padding();
  array_buffer->set_is_external(is_external);
  array_buffer->set_is_detachable(shared_flag == SharedFlag::kNotShared);
  array_buffer->set_is_shared(shared_flag == SharedFlag::kShared);
  array_buffer->set_is_wasm_memory(is_wasm_memory);
  // Initialize backing store at last to avoid handling of |JSArrayBuffers| that
  // are currently being constructed in the |ArrayBufferTracker|. The
  // registration method below handles the case of registering a buffer that has
  // already been promoted.
  array_buffer->set_backing_store(data);

  if (data && !is_external) {
    isolate->heap()->RegisterNewArrayBuffer(*array_buffer);
  }
}

void JSArrayBuffer::SetupAsEmpty(Handle<JSArrayBuffer> array_buffer,
                                 Isolate* isolate) {
  Setup(array_buffer, isolate, false, nullptr, 0, SharedFlag::kNotShared);
}

bool JSArrayBuffer::SetupAllocatingData(Handle<JSArrayBuffer> array_buffer,
                                        Isolate* isolate,
                                        size_t allocated_length,
                                        bool initialize,
                                        SharedFlag shared_flag) {
  void* data;
  CHECK_NOT_NULL(isolate->array_buffer_allocator());
  if (allocated_length != 0) {
    if (allocated_length >= MB)
      isolate->counters()->array_buffer_big_allocations()->AddSample(
          ConvertToMb(allocated_length));
    if (shared_flag == SharedFlag::kShared)
      isolate->counters()->shared_array_allocations()->AddSample(
          ConvertToMb(allocated_length));
    if (initialize) {
      data = isolate->array_buffer_allocator()->Allocate(allocated_length);
    } else {
      data = isolate->array_buffer_allocator()->AllocateUninitialized(
          allocated_length);
    }
    if (data == nullptr) {
      isolate->counters()->array_buffer_new_size_failures()->AddSample(
          ConvertToMb(allocated_length));
      SetupAsEmpty(array_buffer, isolate);
      return false;
    }
  } else {
    data = nullptr;
  }

  const bool is_external = false;
  JSArrayBuffer::Setup(array_buffer, isolate, is_external, data,
                       allocated_length, shared_flag);
  return true;
}

Handle<JSArrayBuffer> JSTypedArray::MaterializeArrayBuffer(
    Handle<JSTypedArray> typed_array) {
  DCHECK(typed_array->is_on_heap());

  Isolate* isolate = typed_array->GetIsolate();

  DCHECK(IsTypedArrayElementsKind(typed_array->GetElementsKind()));

  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(typed_array->buffer()),
                               isolate);
  // This code does not know how to materialize from wasm buffers.
  DCHECK(!buffer->is_wasm_memory());

  void* backing_store =
      isolate->array_buffer_allocator()->AllocateUninitialized(
          typed_array->byte_length());
  if (backing_store == nullptr) {
    isolate->heap()->FatalProcessOutOfMemory(
        "JSTypedArray::MaterializeArrayBuffer");
  }
  buffer->set_is_external(false);
  DCHECK_EQ(buffer->byte_length(), typed_array->byte_length());
  // Initialize backing store at last to avoid handling of |JSArrayBuffers| that
  // are currently being constructed in the |ArrayBufferTracker|. The
  // registration method below handles the case of registering a buffer that has
  // already been promoted.
  buffer->set_backing_store(backing_store);
  // RegisterNewArrayBuffer expects a valid length for adjusting counters.
  isolate->heap()->RegisterNewArrayBuffer(*buffer);
  memcpy(buffer->backing_store(), typed_array->DataPtr(),
         typed_array->byte_length());

  typed_array->set_elements(ReadOnlyRoots(isolate).empty_byte_array());
  typed_array->set_external_pointer(backing_store);
  typed_array->set_base_pointer(Smi::kZero);
  DCHECK(!typed_array->is_on_heap());

  return buffer;
}

Handle<JSArrayBuffer> JSTypedArray::GetBuffer() {
  if (!is_on_heap()) {
    Handle<JSArrayBuffer> array_buffer(JSArrayBuffer::cast(buffer()),
                                       GetIsolate());
    return array_buffer;
  }
  Handle<JSTypedArray> self(*this, GetIsolate());
  return MaterializeArrayBuffer(self);
}

// ES#sec-integer-indexed-exotic-objects-defineownproperty-p-desc
// static
Maybe<bool> JSTypedArray::DefineOwnProperty(Isolate* isolate,
                                            Handle<JSTypedArray> o,
                                            Handle<Object> key,
                                            PropertyDescriptor* desc,
                                            Maybe<ShouldThrow> should_throw) {
  // 1. Assert: IsPropertyKey(P) is true.
  DCHECK(key->IsName() || key->IsNumber());
  // 2. Assert: O is an Object that has a [[ViewedArrayBuffer]] internal slot.
  // 3. If Type(P) is String, then
  if (key->IsString() || key->IsSmi()) {
    // 3a. Let numericIndex be ! CanonicalNumericIndexString(P)
    // 3b. If numericIndex is not undefined, then
    Handle<Object> numeric_index;
    if (CanonicalNumericIndexString(isolate, key, &numeric_index)) {
      // 3b i. If IsInteger(numericIndex) is false, return false.
      // 3b ii. If numericIndex = -0, return false.
      // 3b iii. If numericIndex < 0, return false.
      // FIXME: the standard allows up to 2^53 elements.
      uint32_t index;
      if (numeric_index->IsMinusZero() || !numeric_index->ToUint32(&index)) {
        RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                       NewTypeError(MessageTemplate::kInvalidTypedArrayIndex));
      }
      // 3b iv. Let length be O.[[ArrayLength]].
      size_t length = o->length();
      // 3b v. If numericIndex ≥ length, return false.
      if (o->WasDetached() || index >= length) {
        RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                       NewTypeError(MessageTemplate::kInvalidTypedArrayIndex));
      }
      // 3b vi. If IsAccessorDescriptor(Desc) is true, return false.
      if (PropertyDescriptor::IsAccessorDescriptor(desc)) {
        RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                       NewTypeError(MessageTemplate::kRedefineDisallowed, key));
      }
      // 3b vii. If Desc has a [[Configurable]] field and if
      //         Desc.[[Configurable]] is true, return false.
      // 3b viii. If Desc has an [[Enumerable]] field and if Desc.[[Enumerable]]
      //          is false, return false.
      // 3b ix. If Desc has a [[Writable]] field and if Desc.[[Writable]] is
      //        false, return false.
      if ((desc->has_configurable() && desc->configurable()) ||
          (desc->has_enumerable() && !desc->enumerable()) ||
          (desc->has_writable() && !desc->writable())) {
        RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                       NewTypeError(MessageTemplate::kRedefineDisallowed, key));
      }
      // 3b x. If Desc has a [[Value]] field, then
      //   3b x 1. Let value be Desc.[[Value]].
      //   3b x 2. Return ? IntegerIndexedElementSet(O, numericIndex, value).
      if (desc->has_value()) {
        if (!desc->has_configurable()) desc->set_configurable(false);
        if (!desc->has_enumerable()) desc->set_enumerable(true);
        if (!desc->has_writable()) desc->set_writable(true);
        Handle<Object> value = desc->value();
        RETURN_ON_EXCEPTION_VALUE(isolate,
                                  SetOwnElementIgnoreAttributes(
                                      o, index, value, desc->ToAttributes()),
                                  Nothing<bool>());
      }
      // 3b xi. Return true.
      return Just(true);
    }
  }
  // 4. Return ! OrdinaryDefineOwnProperty(O, P, Desc).
  return OrdinaryDefineOwnProperty(isolate, o, key, desc, should_throw);
}

ExternalArrayType JSTypedArray::type() {
  switch (map().elements_kind()) {
#define ELEMENTS_KIND_TO_ARRAY_TYPE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                                      \
    return kExternal##Type##Array;

    TYPED_ARRAYS(ELEMENTS_KIND_TO_ARRAY_TYPE)
#undef ELEMENTS_KIND_TO_ARRAY_TYPE

    default:
      UNREACHABLE();
  }
}

size_t JSTypedArray::element_size() {
  switch (map().elements_kind()) {
#define ELEMENTS_KIND_TO_ELEMENT_SIZE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                                        \
    return sizeof(ctype);

    TYPED_ARRAYS(ELEMENTS_KIND_TO_ELEMENT_SIZE)
#undef ELEMENTS_KIND_TO_ELEMENT_SIZE

    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
