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
  DECL_ACCESSORS(backing_store, void*)

  // For non-wasm, allocation_length and allocation_base are byte_length and
  // backing_store, respectively.
  inline size_t allocation_length() const;
  inline void* allocation_base() const;

  // [bit_field]: boolean flags
  DECL_PRIMITIVE_ACCESSORS(bit_field, uint32_t)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic. Depending on the V8 build mode there could be no padding.
  V8_INLINE void clear_padding();

// Bit positions for [bit_field].
#define JS_ARRAY_BUFFER_BIT_FIELD_FIELDS(V, _) \
  V(IsExternalBit, bool, 1, _)                 \
  V(IsDetachableBit, bool, 1, _)               \
  V(WasDetachedBit, bool, 1, _)                \
  V(IsSharedBit, bool, 1, _)                   \
  V(IsWasmMemoryBit, bool, 1, _)
  DEFINE_BIT_FIELDS(JS_ARRAY_BUFFER_BIT_FIELD_FIELDS)
#undef JS_ARRAY_BUFFER_BIT_FIELD_FIELDS

  // [is_external]: true indicates that the embedder is in charge of freeing the
  // backing_store, while is_external == false means that v8 will free the
  // memory block once all ArrayBuffers referencing it are collected by the GC.
  DECL_BOOLEAN_ACCESSORS(is_external)

  // [is_detachable]: false indicates that this buffer cannot be detached.
  DECL_BOOLEAN_ACCESSORS(is_detachable)

  // [was_detached]: true if the buffer was previously detached.
  DECL_BOOLEAN_ACCESSORS(was_detached)

  // [is_shared]: tells whether this is an ArrayBuffer or a SharedArrayBuffer.
  DECL_BOOLEAN_ACCESSORS(is_shared)

  // [is_wasm_memory]: whether the buffer is tracked by the WasmMemoryTracker.
  DECL_BOOLEAN_ACCESSORS(is_wasm_memory)

  DECL_CAST(JSArrayBuffer)

  void Detach();

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

  V8_EXPORT_PRIVATE void FreeBackingStoreFromMainThread();
  V8_EXPORT_PRIVATE static void FreeBackingStore(Isolate* isolate,
                                                 Allocation allocation);

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* data, size_t allocated_length,
      SharedFlag shared_flag = SharedFlag::kNotShared,
      bool is_wasm_memory = false);

  // Initialize the object as empty one to avoid confusing heap verifier if
  // the failure happened in the middle of JSArrayBuffer construction.
  V8_EXPORT_PRIVATE static void SetupAsEmpty(Handle<JSArrayBuffer> array_buffer,
                                             Isolate* isolate);

  // Returns false if array buffer contents could not be allocated.
  // In this case, |array_buffer| will not be set up.
  V8_EXPORT_PRIVATE static bool SetupAllocatingData(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
      size_t allocated_length, bool initialize = true,
      SharedFlag shared_flag = SharedFlag::kNotShared) V8_WARN_UNUSED_RESULT;

  // Dispatched behavior.
  DECL_PRINTER(JSArrayBuffer)
  DECL_VERIFIER(JSArrayBuffer)

// Layout description.
#define JS_ARRAY_BUFFER_FIELDS(V)                                           \
  V(kEndOfTaggedFieldsOffset, 0)                                            \
  /* Raw data fields. */                                                    \
  V(kByteLengthOffset, kUIntptrSize)                                        \
  V(kBackingStoreOffset, kSystemPointerSize)                                \
  V(kBitFieldOffset, kInt32Size)                                            \
  /* Pads header size to be a multiple of kTaggedSize. */                   \
  V(kOptionalPaddingOffset, OBJECT_POINTER_PADDING(kOptionalPaddingOffset)) \
  /* Header size. */                                                        \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_ARRAY_BUFFER_FIELDS)
#undef JS_ARRAY_BUFFER_FIELDS

  static const int kSizeWithEmbedderFields =
      kHeaderSize +
      v8::ArrayBuffer::kEmbedderFieldCount * kEmbedderDataSlotSize;

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(JSArrayBuffer, JSObject);
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

  inline bool WasDetached() const;

// Layout description.
#define JS_ARRAY_BUFFER_VIEW_FIELDS(V) \
  V(kBufferOffset, kTaggedSize)        \
  V(kEndOfTaggedFieldsOffset, 0)       \
  /* Raw data fields. */               \
  V(kByteOffsetOffset, kUIntptrSize)   \
  V(kByteLengthOffset, kUIntptrSize)   \
  /* Header size. */                   \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_ARRAY_BUFFER_VIEW_FIELDS)
#undef JS_ARRAY_BUFFER_VIEW_FIELDS

  STATIC_ASSERT(IsAligned(kByteOffsetOffset, kUIntptrSize));
  STATIC_ASSERT(IsAligned(kByteLengthOffset, kUIntptrSize));

  OBJECT_CONSTRUCTORS(JSArrayBufferView, JSObject);
};

class JSTypedArray : public JSArrayBufferView {
 public:
  // TODO(v8:4153): This should be equal to JSArrayBuffer::kMaxByteLength
  // eventually.
  static constexpr size_t kMaxLength = v8::TypedArray::kMaxLength;

  // [length]: length of typed array in elements.
  DECL_PRIMITIVE_ACCESSORS(length, size_t)

  // [external_pointer]: TODO(v8:4153)
  DECL_PRIMITIVE_ACCESSORS(external_pointer, void*)

  // [base_pointer]: TODO(v8:4153)
  DECL_ACCESSORS(base_pointer, Object)

  // ES6 9.4.5.3
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSTypedArray> o, Handle<Object> key,
      PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw);

  DECL_CAST(JSTypedArray)

  ExternalArrayType type();
  V8_EXPORT_PRIVATE size_t element_size();

  V8_EXPORT_PRIVATE Handle<JSArrayBuffer> GetBuffer();

  // Use with care: returns raw pointer into heap.
  inline void* DataPtr();

  // Whether the buffer's backing store is on-heap or off-heap.
  inline bool is_on_heap() const;

  static inline void* ExternalPointerForOnHeapArray();

  static inline MaybeHandle<JSTypedArray> Validate(Isolate* isolate,
                                                   Handle<Object> receiver,
                                                   const char* method_name);

  // Dispatched behavior.
  DECL_PRINTER(JSTypedArray)
  DECL_VERIFIER(JSTypedArray)

// Layout description.
#define JS_TYPED_ARRAY_FIELDS(V)                \
  /* Raw data fields. */                        \
  V(kLengthOffset, kUIntptrSize)                \
  V(kExternalPointerOffset, kSystemPointerSize) \
  V(kBasePointerOffset, kTaggedSize)            \
  /* Header size. */                            \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSArrayBufferView::kHeaderSize,
                                JS_TYPED_ARRAY_FIELDS)
#undef JS_TYPED_ARRAY_FIELDS

  STATIC_ASSERT(IsAligned(kLengthOffset, kUIntptrSize));
  STATIC_ASSERT(IsAligned(kExternalPointerOffset, kSystemPointerSize));

  static const int kSizeWithEmbedderFields =
      kHeaderSize +
      v8::ArrayBufferView::kEmbedderFieldCount * kEmbedderDataSlotSize;

  class BodyDescriptor;

 private:
  static Handle<JSArrayBuffer> MaterializeArrayBuffer(
      Handle<JSTypedArray> typed_array);

  OBJECT_CONSTRUCTORS(JSTypedArray, JSArrayBufferView);
};

class JSDataView : public JSArrayBufferView {
 public:
  // [data_pointer]: pointer to the actual data.
  DECL_PRIMITIVE_ACCESSORS(data_pointer, void*)

  DECL_CAST(JSDataView)

  // Dispatched behavior.
  DECL_PRINTER(JSDataView)
  DECL_VERIFIER(JSDataView)

  // Layout description.
#define JS_DATA_VIEW_FIELDS(V)       \
  /* Raw data fields. */             \
  V(kDataPointerOffset, kIntptrSize) \
  /* Header size. */                 \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSArrayBufferView::kHeaderSize,
                                JS_DATA_VIEW_FIELDS)
#undef JS_DATA_VIEW_FIELDS

  STATIC_ASSERT(IsAligned(kDataPointerOffset, kUIntptrSize));

  static const int kSizeWithEmbedderFields =
      kHeaderSize +
      v8::ArrayBufferView::kEmbedderFieldCount * kEmbedderDataSlotSize;

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(JSDataView, JSArrayBufferView);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_H_
