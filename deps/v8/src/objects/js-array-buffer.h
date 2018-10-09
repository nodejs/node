// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_BUFFER_H_
#define V8_OBJECTS_JS_ARRAY_BUFFER_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Whether a JSArrayBuffer is a SharedArrayBuffer or not.
enum class SharedFlag { kNotShared, kShared };

class JSArrayBuffer : public JSObject {
 public:
  // [byte_length]: length in bytes
  DECL_ACCESSORS(byte_length, Object)

  // [backing_store]: backing memory for this array
  DECL_ACCESSORS(backing_store, void)

  // For non-wasm, allocation_length and allocation_base are byte_length and
  // backing_store, respectively.
  inline size_t allocation_length() const;
  inline void* allocation_base() const;

  inline uint32_t bit_field() const;
  inline void set_bit_field(uint32_t bits);

  // [is_external]: true indicates that the embedder is in charge of freeing the
  // backing_store, while is_external == false means that v8 will free the
  // memory block once all ArrayBuffers referencing it are collected by the GC.
  inline bool is_external();
  inline void set_is_external(bool value);

  inline bool is_neuterable();
  inline void set_is_neuterable(bool value);

  inline bool was_neutered();
  inline void set_was_neutered(bool value);

  inline bool is_shared();
  inline void set_is_shared(bool value);

  inline bool is_growable();
  inline void set_is_growable(bool value);

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

  // Returns whether the buffer is tracked by the WasmMemoryTracker.
  inline bool is_wasm_memory() const;

  // Sets whether the buffer is tracked by the WasmMemoryTracker.
  void set_is_wasm_memory(bool is_wasm_memory);

  // Removes the backing store from the WasmMemoryTracker and sets
  // |is_wasm_memory| to false.
  void StopTrackingWasmMemory(Isolate* isolate);

  void FreeBackingStoreFromMainThread();
  static void FreeBackingStore(Isolate* isolate, Allocation allocation);

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* data, size_t allocated_length,
      SharedFlag shared = SharedFlag::kNotShared, bool is_wasm_memory = false);

  // Returns false if array buffer contents could not be allocated.
  // In this case, |array_buffer| will not be set up.
  static bool SetupAllocatingData(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
      size_t allocated_length, bool initialize = true,
      SharedFlag shared = SharedFlag::kNotShared) V8_WARN_UNUSED_RESULT;

  // Dispatched behavior.
  DECL_PRINTER(JSArrayBuffer)
  DECL_VERIFIER(JSArrayBuffer)

  static const int kByteLengthOffset = JSObject::kHeaderSize;
  // The rest of the fields are not JSObjects, so they are not iterated over in
  // objects-body-descriptors-inl.h.
  static const int kBackingStoreOffset = kByteLengthOffset + kPointerSize;
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
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  class IsExternal : public BitField<bool, 1, 1> {};
  class IsNeuterable : public BitField<bool, 2, 1> {};
  class WasNeutered : public BitField<bool, 3, 1> {};
  class IsShared : public BitField<bool, 4, 1> {};
  class IsGrowable : public BitField<bool, 5, 1> {};
  class IsWasmMemory : public BitField<bool, 6, 1> {};

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBuffer);
};

class JSArrayBufferView : public JSObject {
 public:
  // [buffer]: ArrayBuffer that this typed array views.
  DECL_ACCESSORS(buffer, Object)

  // [byte_offset]: offset of typed array in bytes.
  DECL_ACCESSORS(byte_offset, Object)

  // [byte_length]: length of typed array in bytes.
  DECL_ACCESSORS(byte_length, Object)

  DECL_CAST(JSArrayBufferView)

  DECL_VERIFIER(JSArrayBufferView)

  inline bool WasNeutered() const;

  static const int kBufferOffset = JSObject::kHeaderSize;
  static const int kByteOffsetOffset = kBufferOffset + kPointerSize;
  static const int kByteLengthOffset = kByteOffsetOffset + kPointerSize;
  static const int kViewSize = kByteLengthOffset + kPointerSize;

 private:
#ifdef VERIFY_HEAP
  DECL_ACCESSORS(raw_byte_offset, Object)
  DECL_ACCESSORS(raw_byte_length, Object)
#endif

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

  static const int kLengthOffset = kViewSize;
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

  static const int kSize = kViewSize;

  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBufferView::kEmbedderFieldCount * kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataView);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_H_
