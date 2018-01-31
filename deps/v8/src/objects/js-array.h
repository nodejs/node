// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_H_
#define V8_OBJECTS_JS_ARRAY_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// The JSArray describes JavaScript Arrays
//  Such an array can be in one of two modes:
//    - fast, backing storage is a FixedArray and length <= elements.length();
//       Please note: push and pop can be used to grow and shrink the array.
//    - slow, backing storage is a HashTable with numbers as keys.
class JSArray : public JSObject {
 public:
  // [length]: The length property.
  DECL_ACCESSORS(length, Object)

  // Overload the length setter to skip write barrier when the length
  // is set to a smi. This matches the set function on FixedArray.
  inline void set_length(Smi* length);

  static bool HasReadOnlyLength(Handle<JSArray> array);
  static bool WouldChangeReadOnlyLength(Handle<JSArray> array, uint32_t index);

  // Initialize the array with the given capacity. The function may
  // fail due to out-of-memory situations, but only if the requested
  // capacity is non-zero.
  static void Initialize(Handle<JSArray> array, int capacity, int length = 0);

  // If the JSArray has fast elements, and new_length would result in
  // normalization, returns true.
  bool SetLengthWouldNormalize(uint32_t new_length);
  static inline bool SetLengthWouldNormalize(Heap* heap, uint32_t new_length);

  // Initializes the array to a certain length.
  inline bool AllowsSetLength();

  static void SetLength(Handle<JSArray> array, uint32_t length);

  // Set the content of the array to the content of storage.
  static inline void SetContent(Handle<JSArray> array,
                                Handle<FixedArrayBase> storage);

  // ES6 9.4.2.1
  MUST_USE_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSArray> o, Handle<Object> name,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  static bool AnythingToArrayLength(Isolate* isolate,
                                    Handle<Object> length_object,
                                    uint32_t* output);
  MUST_USE_RESULT static Maybe<bool> ArraySetLength(Isolate* isolate,
                                                    Handle<JSArray> a,
                                                    PropertyDescriptor* desc,
                                                    ShouldThrow should_throw);

  // Checks whether the Array has the current realm's Array.prototype as its
  // prototype. This function is best-effort and only gives a conservative
  // approximation, erring on the side of false, in particular with respect
  // to Proxies and objects with a hidden prototype.
  inline bool HasArrayPrototype(Isolate* isolate);

  DECL_CAST(JSArray)

  // Dispatched behavior.
  DECL_PRINTER(JSArray)
  DECL_VERIFIER(JSArray)

  // Number of element slots to pre-allocate for an empty array.
  static const int kPreallocatedArrayElements = 4;

  // Layout description.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;

  // Max. number of elements being copied in Array builtins.
  static const int kMaxCopyElements = 100;

  // This constant is somewhat arbitrary. Any large enough value would work.
  static const uint32_t kMaxFastArrayLength = 32 * 1024 * 1024;

  static const int kInitialMaxFastElementArray =
      (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - kSize -
       AllocationMemento::kSize) >>
      kDoubleSizeLog2;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArray);
};

Handle<Object> CacheInitialJSArrayMaps(Handle<Context> native_context,
                                       Handle<Map> initial_map);

class JSArrayIterator : public JSObject {
 public:
  DECL_PRINTER(JSArrayIterator)
  DECL_VERIFIER(JSArrayIterator)

  DECL_CAST(JSArrayIterator)

  // [object]: the [[IteratedObject]] inobject property.
  DECL_ACCESSORS(object, Object)

  // [index]: The [[ArrayIteratorNextIndex]] inobject property.
  DECL_ACCESSORS(index, Object)

  // [map]: The Map of the [[IteratedObject]] field at the time the iterator is
  // allocated.
  DECL_ACCESSORS(object_map, Object)

  // Return the ElementsKind that a JSArrayIterator's [[IteratedObject]] is
  // expected to have, based on its instance type.
  static ElementsKind ElementsKindForInstanceType(InstanceType instance_type);

  static const int kIteratedObjectOffset = JSObject::kHeaderSize;
  static const int kNextIndexOffset = kIteratedObjectOffset + kPointerSize;
  static const int kIteratedObjectMapOffset = kNextIndexOffset + kPointerSize;
  static const int kSize = kIteratedObjectMapOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayIterator);
};

// Whether a JSArrayBuffer is a SharedArrayBuffer or not.
enum class SharedFlag { kNotShared, kShared };

class JSArrayBuffer : public JSObject {
 public:
  // [byte_length]: length in bytes
  DECL_ACCESSORS(byte_length, Object)

  // [backing_store]: backing memory for this array
  DECL_ACCESSORS(backing_store, void)

  // [allocation_base]: the start of the memory allocation for this array,
  // normally equal to backing_store
  DECL_ACCESSORS(allocation_base, void)

  // [allocation_length]: the size of the memory allocation for this array,
  // normally equal to byte_length
  inline size_t allocation_length() const;
  inline void set_allocation_length(size_t value);

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

  inline bool has_guard_region() const;
  inline void set_has_guard_region(bool value);

  inline bool is_growable();
  inline void set_is_growable(bool value);

  DECL_CAST(JSArrayBuffer)

  void Neuter();

  inline ArrayBuffer::Allocator::AllocationMode allocation_mode() const;

  struct Allocation {
    using AllocationMode = ArrayBuffer::Allocator::AllocationMode;

    Allocation(void* allocation_base, size_t length, AllocationMode mode)
        : allocation_base(allocation_base), length(length), mode(mode) {}

    void* allocation_base;
    size_t length;
    AllocationMode mode;
  };

  void FreeBackingStore();
  static void FreeBackingStore(Isolate* isolate, Allocation allocation);

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* data, size_t allocated_length,
      SharedFlag shared = SharedFlag::kNotShared);

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* allocation_base, size_t allocation_length, void* data,
      size_t byte_length, SharedFlag shared = SharedFlag::kNotShared);

  // Returns false if array buffer contents could not be allocated.
  // In this case, |array_buffer| will not be set up.
  static bool SetupAllocatingData(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
      size_t allocated_length, bool initialize = true,
      SharedFlag shared = SharedFlag::kNotShared) WARN_UNUSED_RESULT;

  // Dispatched behavior.
  DECL_PRINTER(JSArrayBuffer)
  DECL_VERIFIER(JSArrayBuffer)

  static const int kByteLengthOffset = JSObject::kHeaderSize;
  // The rest of the fields are not JSObjects, so they are not iterated over in
  // objects-body-descriptors-inl.h.
  static const int kBackingStoreOffset = kByteLengthOffset + kPointerSize;
  static const int kAllocationBaseOffset = kBackingStoreOffset + kPointerSize;
  static const int kAllocationLengthOffset =
      kAllocationBaseOffset + kPointerSize;
  static const int kBitFieldSlot = kAllocationLengthOffset + kSizetSize;
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
  class HasGuardRegion : public BitField<bool, 5, 1> {};
  class IsGrowable : public BitField<bool, 6, 1> {};

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
  inline uint32_t length_value() const;

  // ES6 9.4.5.3
  MUST_USE_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSTypedArray> o, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  DECL_CAST(JSTypedArray)

  ExternalArrayType type();
  V8_EXPORT_PRIVATE size_t element_size();

  Handle<JSArrayBuffer> GetBuffer();

  static inline MaybeHandle<JSTypedArray> Validate(Isolate* isolate,
                                                   Handle<Object> receiver,
                                                   const char* method_name);
  // ES7 section 22.2.4.6 Create ( constructor, argumentList )
  static MaybeHandle<JSTypedArray> Create(Isolate* isolate,
                                          Handle<Object> default_ctor, int argc,
                                          Handle<Object>* argv,
                                          const char* method_name);
  // ES7 section 22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )
  static MaybeHandle<JSTypedArray> SpeciesCreate(Isolate* isolate,
                                                 Handle<JSTypedArray> exemplar,
                                                 int argc, Handle<Object>* argv,
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

#endif  // V8_OBJECTS_JS_ARRAY_H_
