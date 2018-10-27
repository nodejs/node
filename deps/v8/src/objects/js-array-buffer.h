// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_BUFFER_H_
#define V8_OBJECTS_JS_ARRAY_BUFFER_H_

#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Whether a JSArrayBuffer is a SharedArrayBuffer or not.
enum class SharedFlag : uint32_t { kNotShared, kShared };

class JSArrayBuffer : public JSObject {
 public:
// The maximum length for JSArrayBuffer's supported by V8.
// On 32-bit architectures we limit this to 2GiB, so that
// we can continue to use CheckBounds with the Unsigned31
// restriction for the length.
#if V8_HOST_ARCH_32_BIT
  static constexpr size_t kMaxByteLength = kMaxInt;
#else
  static constexpr size_t kMaxByteLength = kMaxSafeInteger;
#endif

  // [byte_length]: length in bytes
  DECL_PRIMITIVE_ACCESSORS(byte_length, size_t)

  // [backing_store]: backing memory for this array
  DECL_ACCESSORS(backing_store, void)

  // For non-wasm, allocation_length and allocation_base are byte_length and
  // backing_store, respectively.
  inline size_t allocation_length() const;
  inline void* allocation_base() const;

  // [bit_field]: boolean flags
  DECL_PRIMITIVE_ACCESSORS(bit_field, uint32_t)

// Bit positions for [bit_field].
#define JS_ARRAY_BUFFER_BIT_FIELD_FIELDS(V, _) \
  V(IsExternalBit, bool, 1, _)                 \
  V(IsNeuterableBit, bool, 1, _)               \
  V(WasNeuteredBit, bool, 1, _)                \
  V(IsSharedBit, bool, 1, _)                   \
  V(IsGrowableBit, bool, 1, _)                 \
  V(IsWasmMemoryBit, bool, 1, _)
  DEFINE_BIT_FIELDS(JS_ARRAY_BUFFER_BIT_FIELD_FIELDS)
#undef JS_ARRAY_BUFFER_BIT_FIELD_FIELDS

  // [is_external]: true indicates that the embedder is in charge of freeing the
  // backing_store, while is_external == false means that v8 will free the
  // memory block once all ArrayBuffers referencing it are collected by the GC.
  DECL_BOOLEAN_ACCESSORS(is_external)

  // [is_neuterable]: false indicates that this buffer cannot be detached.
  DECL_BOOLEAN_ACCESSORS(is_neuterable)

  // [was_neutered]: true if the buffer was previously detached.
  DECL_BOOLEAN_ACCESSORS(was_neutered)

  // [is_shared]: tells whether this is an ArrayBuffer or a SharedArrayBuffer.
  DECL_BOOLEAN_ACCESSORS(is_shared)

  // [is_growable]: indicates whether it's possible to grow this buffer.
  DECL_BOOLEAN_ACCESSORS(is_growable)

  // [is_wasm_memory]: whether the buffer is tracked by the WasmMemoryTracker.
  DECL_BOOLEAN_ACCESSORS(is_wasm_memory)

  DECL_CAST(JSArrayBuffer)

  void Neuter();

  struct Allocation {
    Allocation(void* allocation_base, size_t length, void* backing_store,
               bool is_wasm_memory)
        : allocation_base(allocation_base),
          length(length),
          backing_store(backing_store),
          is_wasm_memory(is_wasm_memory) {}

    void* allocation_base;
    size_t length;
    void* backing_store;
    bool is_wasm_memory;
  };

  void FreeBackingStoreFromMainThread();
  static void FreeBackingStore(Isolate* isolate, Allocation allocation);

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* data, size_t allocated_length,
      SharedFlag shared_flag = SharedFlag::kNotShared,
      bool is_wasm_memory = false);

  // Returns false if array buffer contents could not be allocated.
  // In this case, |array_buffer| will not be set up.
  static bool SetupAllocatingData(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
      size_t allocated_length, bool initialize = true,
      SharedFlag shared_flag = SharedFlag::kNotShared) V8_WARN_UNUSED_RESULT;

  // Dispatched behavior.
  DECL_PRINTER(JSArrayBuffer)
  DECL_VERIFIER(JSArrayBuffer)

  // The fields are not pointers into our heap, so they are not iterated over in
  // objects-body-descriptors-inl.h.
  static const int kByteLengthOffset = JSObject::kHeaderSize;
  static const int kBackingStoreOffset = kByteLengthOffset + kUIntptrSize;
  static const int kBitFieldSlot = kBackingStoreOffset + kPointerSize;
#if V8_TARGET_LITTLE_ENDIAN || !V8_HOST_ARCH_64_BIT
  static const int kBitFieldOffset = kBitFieldSlot;
#else
  static const int kBitFieldOffset = kBitFieldSlot + kInt32Size;
#endif
  static const int kSize = kBitFieldSlot + kPointerSize;

  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBuffer::kEmbedderFieldCount * kPointerSize;

  // Iterates all fields in the object including internal ones except
  // kBackingStoreOffset and kBitFieldSlot.
  class BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBuffer);
};

class JSArrayBufferView : public JSObject {
 public:
  // [buffer]: ArrayBuffer that this typed array views.
  DECL_ACCESSORS(buffer, Object)

  // [byte_offset]: offset of typed array in bytes.
  DECL_PRIMITIVE_ACCESSORS(byte_offset, size_t)

  // [byte_length]: length of typed array in bytes.
  DECL_PRIMITIVE_ACCESSORS(byte_length, size_t)

  DECL_CAST(JSArrayBufferView)

  DECL_VERIFIER(JSArrayBufferView)

  inline bool WasNeutered() const;

  static const int kBufferOffset = JSObject::kHeaderSize;
  static const int kByteOffsetOffset = kBufferOffset + kPointerSize;
  static const int kByteLengthOffset = kByteOffsetOffset + kUIntptrSize;
  static const int kHeaderSize = kByteLengthOffset + kUIntptrSize;

  // Iterates all fields in the object including internal ones except
  // kByteOffset and kByteLengthOffset.
  class BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBufferView);
};

class JSTypedArray : public JSArrayBufferView {
 public:
  // [length]: length of typed array in elements.
  DECL_ACCESSORS(length, Object)
  inline size_t length_value() const;

  // ES6 9.4.5.3
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSTypedArray> o, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  DECL_CAST(JSTypedArray)

  ExternalArrayType type();
  V8_EXPORT_PRIVATE size_t element_size();

  Handle<JSArrayBuffer> GetBuffer();

  // Whether the buffer's backing store is on-heap or off-heap.
  inline bool is_on_heap() const;

  static inline MaybeHandle<JSTypedArray> Validate(Isolate* isolate,
                                                   Handle<Object> receiver,
                                                   const char* method_name);

  // Dispatched behavior.
  DECL_PRINTER(JSTypedArray)
  DECL_VERIFIER(JSTypedArray)

  static const int kLengthOffset = JSArrayBufferView::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;
  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBufferView::kEmbedderFieldCount * kPointerSize;

 private:
  static Handle<JSArrayBuffer> MaterializeArrayBuffer(
      Handle<JSTypedArray> typed_array);
#ifdef VERIFY_HEAP
  DECL_ACCESSORS(raw_length, Object)
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSTypedArray);
};

class JSDataView : public JSArrayBufferView {
 public:
  DECL_CAST(JSDataView)

  // Dispatched behavior.
  DECL_PRINTER(JSDataView)
  DECL_VERIFIER(JSDataView)

  static const int kSize = JSArrayBufferView::kHeaderSize;
  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBufferView::kEmbedderFieldCount * kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataView);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_H_
