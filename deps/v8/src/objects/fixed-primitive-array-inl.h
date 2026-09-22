// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_PRIMITIVE_ARRAY_INL_H_
#define V8_OBJECTS_FIXED_PRIMITIVE_ARRAY_INL_H_

#include "src/objects/fixed-primitive-array.h"
// Include the non-inl header before the rest of the headers.

#include <optional>

#include "src/base/numerics/checked_math.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/read-only-heap-inl.h"
#include "src/objects/heap-object-set-map-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/hole.h"
#include "src/objects/map-inl.h"
#include "src/objects/oddball-predicates-inl.h"
#include "src/objects/slots-inl.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

template <class D, class ElementT_, class P>
bool PrimitiveArrayBase<D, ElementT_, P>::IsInBounds(int index) const {
  return static_cast<unsigned>(index) < this->ulength().value();
}

template <class D, class ElementT_, class P>
auto PrimitiveArrayBase<D, ElementT_, P>::get(int index) const
    -> ElementMemberT {
  DCHECK(IsInBounds(index));
  return derived()->values()[index];
}

template <class D, class ElementT_, class P>
void PrimitiveArrayBase<D, ElementT_, P>::set(int index, ElementMemberT value) {
  DCHECK(IsInBounds(index));
  derived()->values()[index] = value;
}

// Due to right-trimming (which creates a filler object before publishing the
// length through a release-store, see Heap::RightTrimArray), concurrent
// visitors need to read the length with acquire semantics.
template <class D, class ElementT_, class P>
int PrimitiveArrayBase<D, ElementT_, P>::AllocatedSize() const {
  return SizeFor(this->length(kAcquireLoad).value());
}

template <class D, class ElementT_, class P>
auto PrimitiveArrayBase<D, ElementT_, P>::begin() -> ElementMemberT* {
  return &derived()->values()[0];
}

template <class D, class ElementT_, class P>
auto PrimitiveArrayBase<D, ElementT_, P>::begin() const
    -> const ElementMemberT* {
  return &derived()->values()[0];
}

template <class D, class ElementT_, class P>
auto PrimitiveArrayBase<D, ElementT_, P>::end() -> ElementMemberT* {
  return &derived()->values()[this->ulength().value()];
}

template <class D, class ElementT_, class P>
auto PrimitiveArrayBase<D, ElementT_, P>::end() const -> const ElementMemberT* {
  return &derived()->values()[this->ulength().value()];
}

template <class D, class ElementT_, class P>
int PrimitiveArrayBase<D, ElementT_, P>::DataSize() const {
  int data_size = SizeFor(this->ulength().value()) - OFFSET_OF_DATA_START(D);
  DCHECK_EQ(data_size,
            OBJECT_POINTER_ALIGN(this->ulength().value() * kElementSize));
  return data_size;
}

// static
template <class D, class ElementT_, class P>
inline Tagged<D> PrimitiveArrayBase<D, ElementT_, P>::FromAddressOfFirstElement(
    Address address) {
  DCHECK_TAG_ALIGNED(address);
  return Cast<D>(
      Tagged<Object>(address - OFFSET_OF_DATA_START(D) + kHeapObjectTag));
}

// static
template <class D, class ElementT_, class P>
template <class IsolateT>
Handle<D> PrimitiveArrayBase<D, ElementT_, P>::Allocate(
    IsolateT* isolate, uint32_t length,
    std::optional<DisallowGarbageCollection>* no_gc_out,
    AllocationType allocation, AllocationAlignment alignment) {
  // Note 0-length is explicitly allowed since not all subtypes can be
  // assumed to have canonical 0-length instances.
  DCHECK_LE(length, kMaxLength);
  DCHECK(!no_gc_out->has_value());

  Tagged<D> xs = UncheckedCast<D>(isolate->factory()->AllocateRawArray(
      SizeFor(length), allocation, AllocationHint(), alignment));

  ReadOnlyRoots roots{isolate};
  if (DEBUG_BOOL) no_gc_out->emplace();
  Tagged<Map> map = Cast<Map>(roots.object_at(D::kMapRootIndex));
  DCHECK(ReadOnlyHeap::Contains(map));

  xs->set_map_after_allocation(isolate, map, SKIP_WRITE_BARRIER);
  xs->set_length(length);
#if TAGGED_SIZE_8_BYTES
  xs->clear_optional_padding();
#endif  // TAGGED_SIZE_8_BYTES

  return handle(xs, isolate);
}

// static
template <class IsolateT>
Handle<FixedArrayBase> FixedDoubleArray::New(IsolateT* isolate, uint32_t length,
                                             AllocationType allocation) {
  if (V8_UNLIKELY(length > kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        length);
  } else if (V8_UNLIKELY(length == 0)) {
    return isolate->factory()->empty_fixed_array();
  }

  std::optional<DisallowGarbageCollection> no_gc;
  return Cast<FixedDoubleArray>(Allocate(isolate, length, &no_gc, allocation));
}

// static
template <class IsolateT, typename ElementsCallback>
Handle<FixedArrayBase> FixedDoubleArray::New(IsolateT* isolate, uint32_t length,
                                             ElementsCallback elements_callback,
                                             AllocationType allocation) {
  if (V8_UNLIKELY(length > kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        length);
  } else if (V8_UNLIKELY(length == 0)) {
    return isolate->factory()->empty_fixed_array();
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<FixedDoubleArray> array =
      Cast<FixedDoubleArray>(Allocate(isolate, length, &no_gc, allocation));
  for (uint32_t i = 0; i < length; ++i) {
    array->set(i, elements_callback(i));
  }
  return array;
}

double FixedDoubleArray::get_scalar(uint32_t index) {
  DCHECK(!is_the_hole(index));
  return values()[index].value();
}

uint64_t FixedDoubleArray::get_representation(uint32_t index) {
  DCHECK(IsInBounds(index));
  return values()[index].value_as_bits();
}

Handle<Object> FixedDoubleArray::get(Tagged<FixedDoubleArray> array,
                                     uint32_t index, Isolate* isolate) {
  if (array->is_the_hole(index)) {
    return isolate->factory()->the_hole_value();
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
  } else if (array->is_undefined(index)) {
    return isolate->factory()->undefined_value();
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
  } else {
    return isolate->factory()->NewNumber(array->get_scalar(index));
  }
}

void FixedDoubleArray::set(uint32_t index, double value) {
  if (std::isnan(value)) {
    value = std::numeric_limits<double>::quiet_NaN();
  }
  values()[index].set_value(value);
  DCHECK(!is_the_hole(index));
}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE
void FixedDoubleArray::set_undefined(uint32_t index) {
  DCHECK(IsInBounds(index));
  values()[index].set_value_as_bits(kUndefinedNanInt64);
  DCHECK(!is_the_hole(index));
  DCHECK(is_undefined(index));
}

bool FixedDoubleArray::is_undefined(uint32_t index) {
  return get_representation(index) == kUndefinedNanInt64;
}
#endif  // V8_ENABLE_UNDEFINED_DOUBLE

void FixedDoubleArray::set_the_hole(Isolate* isolate, uint32_t index) {
  set_the_hole(index);
}

void FixedDoubleArray::set_the_hole(uint32_t index) {
  DCHECK(IsInBounds(index));
  values()[index].set_value_as_bits(kHoleNanInt64);
}

bool FixedDoubleArray::is_the_hole(Isolate* isolate, uint32_t index) {
  return is_the_hole(index);
}

bool FixedDoubleArray::is_the_hole(uint32_t index) {
  return get_representation(index) == kHoleNanInt64;
}

void FixedDoubleArray::MoveElements(Isolate* isolate, uint32_t dst_index,
                                    uint32_t src_index, uint32_t len,
                                    WriteBarrierMode mode) {
  DCHECK_EQ(SKIP_WRITE_BARRIER, mode);
  MemMove(&values()[dst_index], &values()[src_index], len * kElementSize);
}

void FixedDoubleArray::FillWithHoles(uint32_t from, uint32_t to) {
  for (uint32_t i = from; i < to; i++) {
    set_the_hole(i);
  }
}

// static
template <class IsolateT>
Handle<ByteArray> ByteArray::New(IsolateT* isolate, uint32_t length,
                                 AllocationType allocation,
                                 AllocationAlignment alignment) {
  if (V8_UNLIKELY(length > kMaxLength)) {
    base::FatalNoSecurityImpact("Fatal JavaScript invalid size error %u",
                                length);
  } else if (V8_UNLIKELY(length == 0)) {
    return isolate->factory()->empty_byte_array();
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<ByteArray> result =
      Cast<ByteArray>(Allocate(isolate, length, &no_gc, allocation, alignment));

  int padding_size = SizeFor(length) - OffsetOfElementAt(length);
  memset(&result->values()[length], 0, padding_size);

  return result;
}

uint32_t ByteArray::get_int(int offset) const {
  DCHECK(IsInBounds(offset));
  DCHECK_LE(static_cast<uint32_t>(offset) + sizeof(uint32_t),
            ulength().value());
  return base::ReadUnalignedValue<uint32_t>(
      reinterpret_cast<Address>(&values()[offset]));
}

void ByteArray::set_int(int offset, uint32_t value) {
  DCHECK(IsInBounds(offset));
  DCHECK_LE(static_cast<uint32_t>(offset) + sizeof(uint32_t),
            ulength().value());
  base::WriteUnalignedValue<uint32_t>(
      reinterpret_cast<Address>(&values()[offset]), value);
}

// static
template <class IsolateT>
Handle<TrustedByteArray> TrustedByteArray::New(IsolateT* isolate,
                                               uint32_t length,
                                               AllocationType allocation_type) {
  DCHECK(allocation_type == AllocationType::kTrusted ||
         allocation_type == AllocationType::kSharedTrusted);
  if (V8_UNLIKELY(length > kMaxLength)) {
    base::FatalNoSecurityImpact("Fatal JavaScript invalid size error %u",
                                length);
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<TrustedByteArray> result = TrustedCast<TrustedByteArray>(
      Allocate(isolate, length, &no_gc, allocation_type));

  int padding_size = SizeFor(length) - OffsetOfElementAt(length);
  memset(&result->values()[length], 0, padding_size);

  return result;
}

uint32_t TrustedByteArray::get_int(int offset) const {
  DCHECK(IsInBounds(offset));
  DCHECK_LE(static_cast<uint32_t>(offset) + sizeof(uint32_t),
            ulength().value());
  return base::ReadUnalignedValue<uint32_t>(
      reinterpret_cast<Address>(&values()[offset]));
}

void TrustedByteArray::set_int(int offset, uint32_t value) {
  DCHECK(IsInBounds(offset));
  DCHECK_LE(static_cast<uint32_t>(offset) + sizeof(uint32_t),
            ulength().value());
  base::WriteUnalignedValue<uint32_t>(
      reinterpret_cast<Address>(&values()[offset]), value);
}

template <typename... MoreArgs>
// static
DirectHandle<TrustedFixedAddressArray> TrustedFixedAddressArray::New(
    Isolate* isolate, uint32_t length, MoreArgs&&... more_args) {
  return TrustedCast<TrustedFixedAddressArray>(
      Underlying::New(isolate, length, std::forward<MoreArgs>(more_args)...));
}

template <typename T, typename Base>
template <typename... MoreArgs>
// static
Handle<FixedIntegerArrayBase<T, Base>> FixedIntegerArrayBase<T, Base>::New(
    Isolate* isolate, uint32_t length, MoreArgs&&... more_args) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return TrustedCast<FixedIntegerArrayBase<T, Base>>(
      Base::New(isolate, byte_length, std::forward<MoreArgs>(more_args)...));
}

template <typename T, typename Base>
Address FixedIntegerArrayBase<T, Base>::get_element_address(
    uint32_t index) const {
  DCHECK_LT(index, length().value());
  return reinterpret_cast<Address>(&this->values()[index * sizeof(T)]);
}

template <typename T, typename Base>
T FixedIntegerArrayBase<T, Base>::get(uint32_t index) const {
  static_assert(std::is_integral_v<T>);
  return base::ReadUnalignedValue<T>(get_element_address(index));
}

template <typename T, typename Base>
void FixedIntegerArrayBase<T, Base>::set(uint32_t index, T value) {
  static_assert(std::is_integral_v<T>);
  base::WriteUnalignedValue<T>(get_element_address(index), value);
}

template <typename T, typename Base>
SafeHeapObjectSize FixedIntegerArrayBase<T, Base>::length() const {
  uint32_t len = Base::length().value();
  DCHECK_EQ(len % sizeof(T), 0);
  return SafeHeapObjectSize(len / sizeof(T));
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_PRIMITIVE_ARRAY_INL_H_
