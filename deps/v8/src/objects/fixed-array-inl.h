// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_INL_H_
#define V8_OBJECTS_FIXED_ARRAY_INL_H_

#include "src/objects/fixed-array.h"
// Include the non-inl header before the rest of the headers.

#include <optional>

#include "src/base/numerics/checked_math.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/bigint.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/heap-object.h"
#include "src/objects/hole.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/slots-inl.h"
#include "src/objects/slots.h"
#include "src/roots/roots-inl.h"
#include "src/sandbox/sandboxed-pointer-inl.h"
#include "src/torque/runtime-macro-shims.h"
#include "src/torque/runtime-support.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {
template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, false>::capacity() const {
  return SafeHeapObjectSize(capacity_);
}

template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, false>::capacity(
    AcquireLoadTag tag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Acquire_Load(&capacity_));
}

template <class S>
void detail::ArrayHeaderBase<S, false>::set_capacity(uint32_t value) {
  capacity_ = value;
}

template <class S>
void detail::ArrayHeaderBase<S, false>::set_capacity(uint32_t value,
                                                     ReleaseStoreTag tag) {
  base::AsAtomic32::Release_Store(&capacity_, value);
}

template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, true>::length() const {
  return SafeHeapObjectSize(length_);
}

template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, true>::ulength() const {
  return length();
}

template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, true>::length(
    AcquireLoadTag tag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Acquire_Load(&length_));
}

template <class S>
void detail::ArrayHeaderBase<S, true>::set_length(uint32_t value) {
  length_ = value;
}

template <class S>
void detail::ArrayHeaderBase<S, true>::set_length(uint32_t value,
                                                  ReleaseStoreTag tag) {
  base::AsAtomic32::Release_Store(&length_, value);
}

template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, true>::capacity() const {
  return ulength();
}

template <class S>
SafeHeapObjectSize detail::ArrayHeaderBase<S, true>::capacity(
    AcquireLoadTag tag) const {
  return length(tag);
}

template <class S>
void detail::ArrayHeaderBase<S, true>::set_capacity(uint32_t value) {
  set_length(value);
}

template <class S>
void detail::ArrayHeaderBase<S, true>::set_capacity(uint32_t value,
                                                    ReleaseStoreTag tag) {
  set_length(value, tag);
}

#if TAGGED_SIZE_8_BYTES
template <class S>
void detail::ArrayHeaderBase<S, true>::clear_optional_padding() {
  optional_padding_ = 0;
}
#endif

template <class D, class S, class P>
bool TaggedArrayBase<D, S, P>::IsInBounds(int index) const {
  return static_cast<uint32_t>(index) < this->capacity().value();
}

template <class D, class S, class P>
bool TaggedArrayBase<D, S, P>::IsCowArray() const {
  return this->map() ==
         this->EarlyGetReadOnlyRoots().unchecked_fixed_cow_array_map();
}

template <class D, class S, class P>
Tagged<typename TaggedArrayBase<D, S, P>::ElementT>
TaggedArrayBase<D, S, P>::get(uint32_t index) const {
  DCHECK(IsInBounds(index));
  // TODO(jgruber): This tag-less overload shouldn't be relaxed.
  return objects()[index].Relaxed_Load();
}

template <class D, class S, class P>
Tagged<typename TaggedArrayBase<D, S, P>::ElementT>
TaggedArrayBase<D, S, P>::get(uint32_t index, RelaxedLoadTag) const {
  DCHECK(IsInBounds(index));
  return objects()[index].Relaxed_Load();
}

template <class D, class S, class P>
Tagged<typename TaggedArrayBase<D, S, P>::ElementT>
TaggedArrayBase<D, S, P>::get(uint32_t index, AcquireLoadTag) const {
  DCHECK(IsInBounds(index));
  return objects()[index].Acquire_Load();
}

template <class D, class S, class P>
Tagged<typename TaggedArrayBase<D, S, P>::ElementT>
TaggedArrayBase<D, S, P>::get(uint32_t index, SeqCstAccessTag) const {
  DCHECK(IsInBounds(index));
  return objects()[index].SeqCst_Load();
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<ElementT> value,
                                   WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  // TODO(jgruber): This tag-less overload shouldn't be relaxed.
  objects()[index].Relaxed_Store(this, value, mode);
}

template <class D, class S, class P>
template <typename, typename>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<Smi> value) {
  set(index, value, SKIP_WRITE_BARRIER);
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<ElementT> value,
                                   RelaxedStoreTag tag, WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  objects()[index].Relaxed_Store(this, value, mode);
}

template <class D, class S, class P>
template <typename, typename>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<Smi> value,
                                   RelaxedStoreTag tag) {
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<ElementT> value,
                                   ReleaseStoreTag tag, WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  objects()[index].Release_Store(this, value, mode);
}

template <class D, class S, class P>
template <typename, typename>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<Smi> value,
                                   ReleaseStoreTag tag) {
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<ElementT> value,
                                   SeqCstAccessTag tag, WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  objects()[index].SeqCst_Store(this, value, mode);
}

template <class D, class S, class P>
template <typename, typename>
void TaggedArrayBase<D, S, P>::set(uint32_t index, Tagged<Smi> value,
                                   SeqCstAccessTag tag) {
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

template <class D, class S, class P>
Tagged<typename TaggedArrayBase<D, S, P>::ElementT>
TaggedArrayBase<D, S, P>::swap(uint32_t index, Tagged<ElementT> value,
                               SeqCstAccessTag, WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  return objects()[index].SeqCst_Swap(this, value, mode);
}

template <class D, class S, class P>
Tagged<typename TaggedArrayBase<D, S, P>::ElementT>
TaggedArrayBase<D, S, P>::compare_and_swap(uint32_t index,
                                           Tagged<ElementT> expected,
                                           Tagged<ElementT> value,
                                           SeqCstAccessTag,
                                           WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  return objects()[index].SeqCst_CompareAndSwap(this, expected, value, mode);
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::MoveElements(Isolate* isolate, Tagged<D> dst,
                                            uint32_t dst_index, Tagged<D> src,
                                            uint32_t src_index, uint32_t len,
                                            WriteBarrierMode mode) {
  if (len == 0) return;

  DCHECK_GE(len, 0);
  DCHECK(dst->IsInBounds(dst_index));
  DCHECK_LE(dst_index + len, dst->ulength().value());
  DCHECK(src->IsInBounds(src_index));
  DCHECK_LE(src_index + len, src->ulength().value());

  DisallowGarbageCollection no_gc;
  SlotType dst_slot(&dst->objects()[dst_index]);
  SlotType src_slot(&src->objects()[src_index]);
  isolate->heap()->MoveRange(dst, dst_slot, src_slot, len, mode);
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::CopyElements(Isolate* isolate, Tagged<D> dst,
                                            uint32_t dst_index, Tagged<D> src,
                                            uint32_t src_index, uint32_t len,
                                            WriteBarrierMode mode) {
  if (len == 0) return;

  DCHECK_GE(len, 0);
  DCHECK(dst->IsInBounds(dst_index));
  DCHECK_LE(dst_index + len, dst->capacity().value());
  DCHECK(src->IsInBounds(src_index));
  DCHECK_LE(src_index + len, src->capacity().value());

  DisallowGarbageCollection no_gc;
  SlotType dst_slot(&dst->objects()[dst_index]);
  SlotType src_slot(&src->objects()[src_index]);
  isolate->heap()->CopyRange(dst, dst_slot, src_slot, len, mode);
}

template <class D, class S, class P>
void TaggedArrayBase<D, S, P>::RightTrim(Isolate* isolate,
                                         uint32_t new_capacity) {
  const uint32_t old_capacity = this->capacity().value();
  CHECK_GT(new_capacity, 0);  // Due to possible canonicalization.
  CHECK_LE(new_capacity, old_capacity);
  if (new_capacity == old_capacity) return;
  isolate->heap()->RightTrimArray(Cast<D>(this), new_capacity, old_capacity);
}

// Due to right-trimming (which creates a filler object before publishing the
// length through a release-store, see Heap::RightTrimArray), concurrent
// visitors need to read the length with acquire semantics.
template <class D, class S, class P>
int TaggedArrayBase<D, S, P>::AllocatedSize() const {
  return SizeFor(static_cast<int>(this->capacity(kAcquireLoad).value()));
}

template <class D, class S, class P>
typename TaggedArrayBase<D, S, P>::SlotType
TaggedArrayBase<D, S, P>::RawFieldOfFirstElement() const {
  return RawFieldOfElementAt(0);
}

template <class D, class S, class P>
typename TaggedArrayBase<D, S, P>::SlotType
TaggedArrayBase<D, S, P>::RawFieldOfElementAt(uint32_t index) const {
  return SlotType(&objects()[index]);
}

// static
template <class IsolateT>
Handle<FixedArray> FixedArray::New(IsolateT* isolate, uint32_t length,
                                   AllocationType allocation,
                                   AllocationHint hint) {
  if (V8_UNLIKELY(length > FixedArrayBase::kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        length);
  } else if (V8_UNLIKELY(length == 0)) {
    return isolate->factory()->empty_fixed_array();
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<FixedArray> result =
      Cast<FixedArray>(Allocate(isolate, length, &no_gc, allocation, hint));
  ReadOnlyRoots roots{isolate};
  MemsetTagged((*result)->RawFieldOfFirstElement(), roots.undefined_value(),
               length);
  return result;
}

// static
template <class IsolateT, typename ElementsCallback>
Handle<FixedArray> FixedArray::New(IsolateT* isolate, uint32_t length,
                                   ElementsCallback elements_callback,
                                   AllocationType allocation,
                                   AllocationHint hint) {
  if (V8_UNLIKELY(length > FixedArrayBase::kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        length);
  } else if (V8_UNLIKELY(length == 0)) {
    return isolate->factory()->empty_fixed_array();
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<FixedArray> result =
      Cast<FixedArray>(Allocate(isolate, length, &no_gc, allocation, hint));
  const WriteBarrierMode write_barrier =
      allocation == AllocationType::kYoung
          ? WriteBarrierMode::SKIP_WRITE_BARRIER
          : WriteBarrierMode::UPDATE_WRITE_BARRIER;
  for (uint32_t i = 0; i < length; ++i) {
    result->set(i, elements_callback(i), write_barrier);
  }
  return result;
}

// static
template <class IsolateT>
Handle<TrustedFixedArray> TrustedFixedArray::New(IsolateT* isolate,
                                                 uint32_t capacity,
                                                 AllocationType allocation) {
  DCHECK(allocation == AllocationType::kTrusted ||
         allocation == AllocationType::kSharedTrusted);

  if (V8_UNLIKELY(capacity > TrustedFixedArray::kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        capacity);
  }
  // TODO(saelo): once we have trusted read-only roots, we can return the
  // empty_trusted_fixed_array here. Currently this isn't possible because the
  // (mutable) empty_trusted_fixed_array will be created via this function.
  // The same is true for the other trusted-space arrays below.

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<TrustedFixedArray> result = TrustedCast<TrustedFixedArray>(
      Allocate(isolate, capacity, &no_gc, allocation));
  MemsetTagged((*result)->RawFieldOfFirstElement(), Smi::zero(), capacity);
  return result;
}

// static
template <class IsolateT>
Handle<ProtectedFixedArray> ProtectedFixedArray::New(IsolateT* isolate,
                                                     uint32_t capacity,
                                                     SharedFlag shared) {
  if (V8_UNLIKELY(capacity > ProtectedFixedArray::kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        capacity);
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<ProtectedFixedArray> result = TrustedCast<ProtectedFixedArray>(
      Allocate(isolate, capacity, &no_gc,
               shared == SharedFlag::kYes ? AllocationType::kSharedTrusted
                                          : AllocationType::kTrusted));
  MemsetTagged((*result)->RawFieldOfFirstElement(), Smi::zero(), capacity);
  return result;
}

// static
template <class D, class S, class P>
template <class IsolateT>
Handle<D> TaggedArrayBase<D, S, P>::Allocate(
    IsolateT* isolate, uint32_t capacity,
    std::optional<DisallowGarbageCollection>* no_gc_out,
    AllocationType allocation, AllocationHint hint) {
  // Note 0-capacity is explicitly allowed since not all subtypes can be
  // assumed to have canonical 0-capacity instances.
  DCHECK_LE(capacity, kMaxCapacity);
  DCHECK(!no_gc_out->has_value());

  Tagged<D> xs = UncheckedCast<D>(isolate->factory()->AllocateRawArray(
      SizeFor(capacity), allocation, hint));

  ReadOnlyRoots roots{isolate};
  if (DEBUG_BOOL) no_gc_out->emplace();
  Tagged<Map> map = Cast<Map>(roots.object_at(S::kMapRootIndex));
  DCHECK(ReadOnlyHeap::Contains(map));

  xs->set_map_after_allocation(isolate, map, SKIP_WRITE_BARRIER);
  xs->set_capacity(capacity);

  return handle(xs, isolate);
}

// static
template <class D, class S, class P>
constexpr uint32_t TaggedArrayBase<D, S, P>::NewCapacityForIndex(
    uint32_t index, uint32_t old_capacity) {
  DCHECK_GE(index, old_capacity);
  // Note this is currently based on JSObject::NewElementsCapacity.
  uint32_t capacity = old_capacity;
  do {
    capacity = capacity + (capacity >> 1) + 16;
  } while (capacity <= index);
  return capacity;
}

inline SafeHeapObjectSize WeakArrayList::capacity() const {
  return SafeHeapObjectSize(capacity_);
}

inline SafeHeapObjectSize WeakArrayList::capacity(RelaxedLoadTag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Relaxed_Load(&capacity_));
}

inline SafeHeapObjectSize WeakArrayList::length() const {
  return SafeHeapObjectSize(length_);
}

inline SafeHeapObjectSize WeakArrayList::ulength() const { return length(); }

inline void WeakArrayList::set_length(uint32_t value) { length_ = value; }

bool FixedArray::is_the_hole(Isolate* isolate, uint32_t index) {
  return IsTheHole(get(index), isolate);
}

void FixedArray::set_the_hole(Isolate* isolate, uint32_t index) {
  set_the_hole(ReadOnlyRoots(isolate), index);
}

void FixedArray::set_the_hole(ReadOnlyRoots ro_roots, uint32_t index) {
  set(index, ro_roots.the_hole_value(), SKIP_WRITE_BARRIER);
}

void FixedArray::FillWithHoles(uint32_t from, uint32_t to) {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  for (uint32_t i = from; i < to; i++) {
    set(i, roots.the_hole_value(), SKIP_WRITE_BARRIER);
  }
}

void FixedArray::MoveElements(Isolate* isolate, uint32_t dst_index,
                              uint32_t src_index, uint32_t len,
                              WriteBarrierMode mode) {
  MoveElements(isolate, this, dst_index, this, src_index, len, mode);
}

void FixedArray::CopyElements(Isolate* isolate, uint32_t dst_index,
                              Tagged<FixedArray> src, uint32_t src_index,
                              uint32_t len, WriteBarrierMode mode) {
  CopyElements(isolate, this, dst_index, src, src_index, len, mode);
}

// static
Handle<FixedArray> FixedArray::Resize(Isolate* isolate,
                                      DirectHandle<FixedArray> xs,
                                      uint32_t new_capacity,
                                      AllocationType allocation,
                                      WriteBarrierMode mode) {
  Handle<FixedArray> ys = New(isolate, new_capacity, allocation);
  const uint32_t elements_to_copy =
      std::min(new_capacity, xs->capacity().value());
  FixedArray::CopyElements(isolate, *ys, 0, *xs, 0, elements_to_copy, mode);
  return ys;
}

inline int WeakArrayList::AllocatedSize() const {
  // TODO(375937549): Convert to uint32_t.
  return SizeFor(static_cast<int>(capacity(kRelaxedLoad).value()));
}

template <class D, class S, class P>
bool PrimitiveArrayBase<D, S, P>::IsInBounds(int index) const {
  return static_cast<unsigned>(index) < this->ulength().value();
}

template <class D, class S, class P>
auto PrimitiveArrayBase<D, S, P>::get(int index) const -> ElementMemberT {
  DCHECK(IsInBounds(index));
  return values()[index];
}

template <class D, class S, class P>
void PrimitiveArrayBase<D, S, P>::set(int index, ElementMemberT value) {
  DCHECK(IsInBounds(index));
  values()[index] = value;
}

// Due to right-trimming (which creates a filler object before publishing the
// length through a release-store, see Heap::RightTrimArray), concurrent
// visitors need to read the length with acquire semantics.
template <class D, class S, class P>
int PrimitiveArrayBase<D, S, P>::AllocatedSize() const {
  return SizeFor(this->length(kAcquireLoad).value());
}

template <class D, class S, class P>
auto PrimitiveArrayBase<D, S, P>::begin() -> ElementMemberT* {
  return &values()[0];
}

template <class D, class S, class P>
auto PrimitiveArrayBase<D, S, P>::begin() const -> const ElementMemberT* {
  return &values()[0];
}

template <class D, class S, class P>
auto PrimitiveArrayBase<D, S, P>::end() -> ElementMemberT* {
  return &values()[this->ulength().value()];
}

template <class D, class S, class P>
auto PrimitiveArrayBase<D, S, P>::end() const -> const ElementMemberT* {
  return &values()[this->ulength().value()];
}

template <class D, class S, class P>
int PrimitiveArrayBase<D, S, P>::DataSize() const {
  int data_size = SizeFor(this->ulength().value()) - sizeof(Header);
  DCHECK_EQ(data_size,
            OBJECT_POINTER_ALIGN(this->ulength().value() * kElementSize));
  return data_size;
}

// static
template <class D, class S, class P>
inline Tagged<D> PrimitiveArrayBase<D, S, P>::FromAddressOfFirstElement(
    Address address) {
  DCHECK_TAG_ALIGNED(address);
  return Cast<D>(Tagged<Object>(address - S::kHeaderSize + kHeapObjectTag));
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

// static
template <class D, class S, class P>
template <class IsolateT>
Handle<D> PrimitiveArrayBase<D, S, P>::Allocate(
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
  Tagged<Map> map = Cast<Map>(roots.object_at(S::kMapRootIndex));
  DCHECK(ReadOnlyHeap::Contains(map));

  xs->set_map_after_allocation(isolate, map, SKIP_WRITE_BARRIER);
  xs->set_length(length);

  return handle(xs, isolate);
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
Handle<WeakFixedArray> WeakFixedArray::New(
    IsolateT* isolate, uint32_t capacity, AllocationType allocation,
    MaybeDirectHandle<Object> initial_value) {
  CHECK_LE(capacity, kMaxCapacity);

  if (V8_UNLIKELY(capacity == 0)) {
    return isolate->factory()->empty_weak_fixed_array();
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<WeakFixedArray> result =
      Cast<WeakFixedArray>(Allocate(isolate, capacity, &no_gc, allocation));
  ReadOnlyRoots roots{isolate};
  MemsetTagged((*result)->RawFieldOfFirstElement(),
               initial_value.is_null() ? roots.undefined_value()
                                       : *initial_value.ToHandleChecked(),
               capacity);
  return result;
}

template <class IsolateT>
Handle<WeakHomomorphicFixedArray> WeakHomomorphicFixedArray::New(
    IsolateT* isolate, uint32_t capacity, AllocationType allocation,
    MaybeDirectHandle<Object> initial_value) {
  CHECK_LE(capacity, kMaxCapacity);
  DCHECK_NE(capacity, 0);

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<WeakHomomorphicFixedArray> result = Cast<WeakHomomorphicFixedArray>(
      Allocate(isolate, capacity, &no_gc, allocation));
  ReadOnlyRoots roots{isolate};
  MemsetTagged((*result)->RawFieldOfFirstElement(),
               initial_value.is_null() ? roots.undefined_value()
                                       : *initial_value.ToHandleChecked(),
               capacity);
  return result;
}

template <class IsolateT>
Handle<TrustedWeakFixedArray> TrustedWeakFixedArray::New(IsolateT* isolate,
                                                         uint32_t capacity) {
  if (V8_UNLIKELY(capacity > TrustedFixedArray::kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        capacity);
  }

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<TrustedWeakFixedArray> result = TrustedCast<TrustedWeakFixedArray>(
      Allocate(isolate, capacity, &no_gc, AllocationType::kTrusted));
  MemsetTagged((*result)->RawFieldOfFirstElement(), Smi::zero(), capacity);
  return result;
}

template <class IsolateT>
Handle<ProtectedWeakFixedArray> ProtectedWeakFixedArray::New(
    IsolateT* isolate, uint32_t capacity) {
  if (V8_UNLIKELY(capacity > TrustedFixedArray::kMaxLength)) {
    base::FatalNoSecurityImpact(
        "Fatal JavaScript invalid size error %d (see crbug.com/1201626)",
        capacity);
  }
  std::optional<DisallowGarbageCollection> no_gc;
  Handle<ProtectedWeakFixedArray> result = TrustedCast<ProtectedWeakFixedArray>(
      Allocate(isolate, capacity, &no_gc, AllocationType::kTrusted));
  MemsetTagged((*result)->RawFieldOfFirstElement(), Smi::zero(), capacity);
  return result;
}

Tagged<MaybeObject> WeakArrayList::Get(uint32_t index) const {
  return get(index);
}

void WeakArrayList::Set(uint32_t index, Tagged<MaybeObject> value,
                        WriteBarrierMode mode) {
  set(index, value, kRelaxedStore, mode);
}

void WeakArrayList::Set(uint32_t index, Tagged<Smi> value) {
  Set(index, value, SKIP_WRITE_BARRIER);
}

void WeakArrayList::CopyElements(Isolate* isolate, uint32_t dst_index,
                                 Tagged<WeakArrayList> src, uint32_t src_index,
                                 uint32_t len, WriteBarrierMode mode) {
  Super::CopyElements(isolate, this, dst_index, src, src_index, len, mode);
}

Tagged<HeapObject> WeakArrayList::Iterator::Next() {
  if (!array_.is_null()) {
    const uint32_t array_len = array_->length().value();
    while (index_ < array_len) {
      Tagged<MaybeObject> item = array_->Get(index_++);
      DCHECK(item.IsWeakOrCleared());
      if (!item.IsCleared()) return item.GetHeapObjectAssumeWeak();
    }
    array_ = Tagged<WeakArrayList>();
  }
  return Tagged<HeapObject>();
}

int ArrayList::length() const {
  DCHECK_LE(length_, kMaxInt);
  return static_cast<int>(length_);
}

SafeHeapObjectSize ArrayList::ulength() const {
  return SafeHeapObjectSize(static_cast<uint32_t>(length()));
}

void ArrayList::set_length(uint32_t value) { length_ = value; }

// static
template <class IsolateT>
DirectHandle<ArrayList> ArrayList::New(IsolateT* isolate, uint32_t capacity,
                                       AllocationType allocation) {
  if (capacity == 0) return isolate->factory()->empty_array_list();

  DCHECK_LE(capacity, kMaxCapacity);

  std::optional<DisallowGarbageCollection> no_gc;
  DirectHandle<ArrayList> result =
      Cast<ArrayList>(Allocate(isolate, capacity, &no_gc, allocation));
  result->set_length(0);
  ReadOnlyRoots roots{isolate};
  MemsetTagged(result->RawFieldOfFirstElement(), roots.undefined_value(),
               capacity);
  return result;
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

template <class T, class Super>
SafeHeapObjectSize PodArrayBase<T, Super>::length() const {
  return SafeHeapObjectSize(Super::length().value() / sizeof(T));
}

// static
template <class T>
Handle<PodArray<T>> PodArray<T>::New(Isolate* isolate, uint32_t length,
                                     AllocationType allocation) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return Cast<PodArray<T>>(
      isolate->factory()->NewByteArray(byte_length, allocation));
}

// static
template <class T>
Handle<PodArray<T>> PodArray<T>::New(LocalIsolate* isolate, uint32_t length,
                                     AllocationType allocation) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return Cast<PodArray<T>>(
      isolate->factory()->NewByteArray(byte_length, allocation));
}

// static
template <class T>
DirectHandle<TrustedPodArray<T>> TrustedPodArray<T>::New(
    Isolate* isolate, uint32_t length, AllocationType allocation_type) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return TrustedCast<TrustedPodArray<T>>(
      isolate->factory()->NewTrustedByteArray(byte_length, allocation_type));
}

// static
template <class T>
DirectHandle<TrustedPodArray<T>> TrustedPodArray<T>::New(
    LocalIsolate* isolate, uint32_t length, AllocationType allocation_type) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return TrustedCast<TrustedPodArray<T>>(
      isolate->factory()->NewTrustedByteArray(byte_length, allocation_type));
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_INL_H_
