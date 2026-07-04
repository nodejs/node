// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_PRIMITIVE_ARRAY_H_
#define V8_OBJECTS_FIXED_PRIMITIVE_ARRAY_H_

#include <optional>

#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/fixed-array-base.h"
#include "src/objects/heap-object.h"
#include "src/objects/tagged.h"
#include "src/objects/trusted-object.h"
#include "src/roots/roots.h"
#include "src/utils/memcopy.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

// The Super template parameter names the concrete base class (e.g.
// (Trusted)FixedArrayBase) from which subclasses inherit the length_ field.
V8_OBJECT
template <class Derived, typename ElementT_, class Super = FixedArrayBase>
class PrimitiveArrayBase : public Super {
 private:
  V8_INLINE Derived* derived() { return static_cast<Derived*>(this); }
  V8_INLINE const Derived* derived() const {
    return static_cast<const Derived*>(this);
  }

  uint32_t& length_field() { return derived()->length_; }
  const uint32_t& length_field() const { return derived()->length_; }

  static_assert(std::is_base_of_v<HeapObject, Super>);
  static_assert(!is_subtype_v<ElementT_, Object>);

 public:
  using ElementT = ElementT_;
  // Bug(v8:8875): Doubles may be unaligned.
  using ElementMemberT = std::conditional_t<std::is_same_v<ElementT_, double>,
                                            UnalignedDoubleMember, ElementT_>;
  static_assert(alignof(ElementMemberT) <= alignof(Tagged_t));
  static constexpr bool kElementsAreMaybeObject = false;
  static constexpr int kElementSize = sizeof(ElementMemberT);

  // length() / set_length() are inherited from (Trusted)FixedArrayBase. For
  // primitive arrays capacity() is an alias for length().
  inline SafeHeapObjectSize capacity() const {
    return SafeHeapObjectSize(length_field());
  }
  inline SafeHeapObjectSize capacity(AcquireLoadTag tag) const {
    return this->length(tag);
  }
  inline SafeHeapObjectSize capacity(RelaxedLoadTag tag) const {
    return this->length(tag);
  }
  inline void set_capacity(uint32_t value) { this->set_length(value); }
  inline void set_capacity(uint32_t value, ReleaseStoreTag tag) {
    this->set_length(value, tag);
  }

  inline void clear_optional_padding() {
    if constexpr (requires { derived()->optional_padding_; }) {
      derived()->optional_padding_ = 0;
    }
  }

  inline ElementMemberT get(int index) const;
  inline void set(int index, ElementMemberT value);

  inline int AllocatedSize() const;
  // TODO(375937549): Convert to use uint32_t.
  static constexpr int SizeFor(int length);
  static constexpr int OffsetOfElementAt(int index);

  // Gives access to raw memory which stores the array's data.
  // Note that on 32-bit archs and on 64-bit platforms with pointer compression
  // the pointers to 8-byte size elements are not guaranteed to be aligned.
  inline ElementMemberT* begin();
  inline const ElementMemberT* begin() const;
  inline ElementMemberT* end();
  inline const ElementMemberT* end() const;
  inline int DataSize() const;

  static inline Tagged<Derived> FromAddressOfFirstElement(Address address);

  // Maximal allowed length, in number of elements. Chosen s.t. the byte size
  // fits into a Smi which is necessary for being able to create a free space
  // filler.
  static constexpr uint32_t kMaxLength = kMaxFixedArrayCapacity;

  // Maximally allowed length for regular (non large object space) object.
  static constexpr int MaxRegularLength();

 protected:
  template <class IsolateT>
  static Handle<Derived> Allocate(
      IsolateT* isolate, uint32_t length,
      std::optional<DisallowGarbageCollection>* no_gc_out,
      AllocationType allocation = AllocationType::kYoung,
      AllocationAlignment alignment = kTaggedAligned);

  inline bool IsInBounds(int index) const;
} V8_OBJECT_END;

// FixedDoubleArray describes fixed-sized arrays with element type double.
V8_OBJECT class FixedDoubleArray
    : public PrimitiveArrayBase<FixedDoubleArray, double> {
  using Super = PrimitiveArrayBase<FixedDoubleArray, double>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kFixedDoubleArrayMap;
  using ElementMemberT = UnalignedDoubleMember;

 public:
  // Note this returns FixedArrayBase due to canonicalization to
  // empty_fixed_array.
  template <class IsolateT>
  static inline Handle<FixedArrayBase> New(
      IsolateT* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kYoung);

  template <class IsolateT, typename ElementsCallback>
  static inline Handle<FixedArrayBase> New(
      IsolateT* isolate, uint32_t length, ElementsCallback elements_callback,
      AllocationType allocation = AllocationType::kYoung);

  // Setter and getter for elements.
  inline double get_scalar(uint32_t index);
  inline uint64_t get_representation(uint32_t index);
  static inline Handle<Object> get(Tagged<FixedDoubleArray> array,
                                   uint32_t index, Isolate* isolate);
  inline void set(uint32_t index, double value);
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
  inline void set_undefined(uint32_t index);
  inline bool is_undefined(uint32_t index);
#endif  // V8_ENABLE_UNDEFINED_DOUBLE

  inline void set_the_hole(Isolate* isolate, uint32_t index);
  inline void set_the_hole(uint32_t index);
  inline bool is_the_hole(Isolate* isolate, uint32_t index);
  inline bool is_the_hole(uint32_t index);

  inline void MoveElements(Isolate* isolate, uint32_t dst_index,
                           uint32_t src_index, uint32_t len,
                           WriteBarrierMode /* unused */);

  inline void FillWithHoles(uint32_t from, uint32_t to);

  DECL_PRINTER(FixedDoubleArray)
  DECL_VERIFIER(FixedDoubleArray)

  class BodyDescriptor;

 public:
  // length_ / optional_padding_ live in FixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(ElementMemberT, values);
} V8_OBJECT_END;

// ByteArray represents fixed sized arrays containing raw bytes that will not
// be scanned by the garbage collector.
V8_OBJECT class ByteArray : public PrimitiveArrayBase<ByteArray, uint8_t> {
  using Super = PrimitiveArrayBase<ByteArray, uint8_t>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kByteArrayMap;

  template <class IsolateT>
  static inline Handle<ByteArray> New(
      IsolateT* isolate, uint32_t capacity,
      AllocationType allocation = AllocationType::kYoung,
      AllocationAlignment alignment = kTaggedAligned);

  inline uint32_t get_int(int offset) const;
  inline void set_int(int offset, uint32_t value);

  // Given the full object size in bytes, return the length that should be
  // passed to New s.t. an object of the same size is created.
  static constexpr uint32_t LengthFor(int size_in_bytes) {
    DCHECK(IsAligned(size_in_bytes, kTaggedSize));
    DCHECK_GE(size_in_bytes, OFFSET_OF_DATA_START(ByteArray));
    return size_in_bytes - OFFSET_OF_DATA_START(ByteArray);
  }

  DECL_PRINTER(ByteArray)
  DECL_VERIFIER(ByteArray)

  class BodyDescriptor;

 public:
  // length_ / optional_padding_ live in FixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(uint8_t, values);
} V8_OBJECT_END;

// A ByteArray in trusted space.
V8_OBJECT
class TrustedByteArray : public PrimitiveArrayBase<TrustedByteArray, uint8_t,
                                                   TrustedFixedArrayBase> {
  using Super =
      PrimitiveArrayBase<TrustedByteArray, uint8_t, TrustedFixedArrayBase>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kTrustedByteArrayMap;

  template <class IsolateT>
  static inline Handle<TrustedByteArray> New(
      IsolateT* isolate, uint32_t capacity,
      AllocationType allocation_type = AllocationType::kTrusted);

  inline uint32_t get_int(int offset) const;
  inline void set_int(int offset, uint32_t value);

  // Given the full object size in bytes, return the length that should be
  // passed to New s.t. an object of the same size is created.
  static constexpr int LengthFor(int size_in_bytes) {
    DCHECK(IsAligned(size_in_bytes, kTaggedSize));
    DCHECK_GE(size_in_bytes, OFFSET_OF_DATA_START(TrustedByteArray));
    return size_in_bytes - OFFSET_OF_DATA_START(TrustedByteArray);
  }

  DECL_PRINTER(TrustedByteArray)
  DECL_VERIFIER(TrustedByteArray)

  class BodyDescriptor;

 public:
  // length_ / optional_padding_ live in TrustedFixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(uint8_t, values);
} V8_OBJECT_END;

// Convenience class for treating a ByteArray / TrustedByteArray as array of
// fixed-size integers.
V8_OBJECT
template <typename T, typename Base>
class FixedIntegerArrayBase : public Base {
  static_assert(std::is_integral_v<T>);

 public:
  // {MoreArgs...} allows passing the `AllocationType` if `Base` is `ByteArray`.
  template <typename... MoreArgs>
  static Handle<FixedIntegerArrayBase<T, Base>> New(Isolate* isolate,
                                                    uint32_t length,
                                                    MoreArgs&&... more_args);

  // Get/set the contents of this array.
  T get(uint32_t index) const;
  void set(uint32_t index, T value);

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index) {
    return OFFSET_OF_DATA_START(Base) + index * sizeof(T);
  }

  inline SafeHeapObjectSize length() const;

 protected:
  Address get_element_address(uint32_t index) const;
} V8_OBJECT_END;

using FixedInt8Array = FixedIntegerArrayBase<int8_t, ByteArray>;
using FixedUInt8Array = FixedIntegerArrayBase<uint8_t, ByteArray>;
using FixedInt16Array = FixedIntegerArrayBase<int16_t, ByteArray>;
using FixedUInt16Array = FixedIntegerArrayBase<uint16_t, ByteArray>;
using FixedInt32Array = FixedIntegerArrayBase<int32_t, ByteArray>;
using FixedUInt32Array = FixedIntegerArrayBase<uint32_t, ByteArray>;
using FixedInt64Array = FixedIntegerArrayBase<int64_t, ByteArray>;
using FixedUInt64Array = FixedIntegerArrayBase<uint64_t, ByteArray>;

V8_OBJECT
class TrustedFixedAddressArray
    : public FixedIntegerArrayBase<Address, TrustedByteArray> {
  using Underlying = FixedIntegerArrayBase<Address, TrustedByteArray>;

 public:
  // {MoreArgs...} allows passing the `AllocationType` if `Base` is `ByteArray`.
  template <typename... MoreArgs>
  static inline DirectHandle<TrustedFixedAddressArray> New(
      Isolate* isolate, uint32_t length, MoreArgs&&... more_args);
} V8_OBJECT_END;

template <class Derived, typename ElementT, class Super>
constexpr int PrimitiveArrayBase<Derived, ElementT, Super>::SizeFor(
    int length) {
  return OBJECT_POINTER_ALIGN(OffsetOfElementAt(length));
}

template <class Derived, typename ElementT, class Super>
constexpr int PrimitiveArrayBase<Derived, ElementT, Super>::OffsetOfElementAt(
    int index) {
  return OFFSET_OF_DATA_START(Derived) + index * kElementSize;
}

template <class Derived, typename ElementT, class Super>
constexpr int PrimitiveArrayBase<Derived, ElementT, Super>::MaxRegularLength() {
  return (kMaxRegularHeapObjectSize - OFFSET_OF_DATA_START(Derived)) /
         kElementSize;
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_PRIMITIVE_ARRAY_H_
