// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_INL_H_
#define V8_OBJECTS_FIXED_ARRAY_INL_H_

#include "src/objects/fixed-array.h"
// Include the non-inl header before the rest of the headers.

#include <optional>

#include "src/base/strong-alias.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/read-only-heap-inl.h"
#include "src/objects/fixed-array-base-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/heap-object-set-map-inl.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/oddball-predicates-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/slots.h"
#include "src/objects/tagged-field-inl.h"
#include "src/roots/roots-inl.h"
#include "src/sandbox/sandboxed-pointer-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

template <class D, class ElementT_, class P>
bool TaggedArrayBase<D, ElementT_, P>::IsInBounds(int index) const {
  return static_cast<uint32_t>(index) < this->capacity().value();
}

template <class D, class ElementT_, class P>
bool TaggedArrayBase<D, ElementT_, P>::IsCowArray() const {
  return this->map() == ReadOnlyHeap::EarlyGetReadOnlyRoots(this)
                            .unchecked_fixed_cow_array_map();
}

template <class D, class ElementT_, class P>
Tagged<ElementT_> TaggedArrayBase<D, ElementT_, P>::get(uint32_t index) const {
  DCHECK(IsInBounds(index));
  // TODO(jgruber): This tag-less overload shouldn't be relaxed.
  return derived()->objects()[index].Relaxed_Load();
}

template <class D, class ElementT_, class P>
Tagged<ElementT_> TaggedArrayBase<D, ElementT_, P>::get(uint32_t index,
                                                        RelaxedLoadTag) const {
  DCHECK(IsInBounds(index));
  return derived()->objects()[index].Relaxed_Load();
}

template <class D, class ElementT_, class P>
Tagged<ElementT_> TaggedArrayBase<D, ElementT_, P>::get(uint32_t index,
                                                        AcquireLoadTag) const {
  DCHECK(IsInBounds(index));
  return derived()->objects()[index].Acquire_Load();
}

template <class D, class ElementT_, class P>
Tagged<ElementT_> TaggedArrayBase<D, ElementT_, P>::get(uint32_t index,
                                                        SeqCstAccessTag) const {
  DCHECK(IsInBounds(index));
  return derived()->objects()[index].SeqCst_Load();
}

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index,
                                           Tagged<ElementT_> value,
                                           WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  // TODO(jgruber): This tag-less overload shouldn't be relaxed.
  derived()->objects()[index].Relaxed_Store(derived(), value, mode);
}

template <class D, class ElementT_, class P>
template <typename, typename>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index, Tagged<Smi> value) {
  set(index, value, SKIP_WRITE_BARRIER);
}

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index,
                                           Tagged<ElementT_> value,
                                           RelaxedStoreTag tag,
                                           WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  derived()->objects()[index].Relaxed_Store(derived(), value, mode);
}

template <class D, class ElementT_, class P>
template <typename, typename>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index, Tagged<Smi> value,
                                           RelaxedStoreTag tag) {
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index,
                                           Tagged<ElementT_> value,
                                           ReleaseStoreTag tag,
                                           WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  derived()->objects()[index].Release_Store(derived(), value, mode);
}

template <class D, class ElementT_, class P>
template <typename, typename>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index, Tagged<Smi> value,
                                           ReleaseStoreTag tag) {
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index,
                                           Tagged<ElementT_> value,
                                           SeqCstAccessTag tag,
                                           WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  derived()->objects()[index].SeqCst_Store(derived(), value, mode);
}

template <class D, class ElementT_, class P>
template <typename, typename>
void TaggedArrayBase<D, ElementT_, P>::set(uint32_t index, Tagged<Smi> value,
                                           SeqCstAccessTag tag) {
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

template <class D, class ElementT_, class P>
Tagged<ElementT_> TaggedArrayBase<D, ElementT_, P>::swap(
    uint32_t index, Tagged<ElementT_> value, SeqCstAccessTag,
    WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  return derived()->objects()[index].SeqCst_Swap(derived(), value, mode);
}

template <class D, class ElementT_, class P>
Tagged<ElementT_> TaggedArrayBase<D, ElementT_, P>::compare_and_swap(
    uint32_t index, Tagged<ElementT_> expected, Tagged<ElementT_> value,
    SeqCstAccessTag, WriteBarrierMode mode) {
  DCHECK(!IsCowArray());
  DCHECK(IsInBounds(index));
  return derived()->objects()[index].SeqCst_CompareAndSwap(derived(), expected,
                                                           value, mode);
}

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::MoveElements(
    Isolate* isolate, Tagged<D> dst, uint32_t dst_index, Tagged<D> src,
    uint32_t src_index, uint32_t len, WriteBarrierMode mode) {
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

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::CopyElements(
    Isolate* isolate, Tagged<D> dst, uint32_t dst_index, Tagged<D> src,
    uint32_t src_index, uint32_t len, WriteBarrierMode mode) {
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

template <class D, class ElementT_, class P>
void TaggedArrayBase<D, ElementT_, P>::RightTrim(Isolate* isolate,
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
template <class D, class ElementT_, class P>
int TaggedArrayBase<D, ElementT_, P>::AllocatedSize() const {
  return SizeFor(static_cast<int>(this->capacity(kAcquireLoad).value()));
}

template <class D, class ElementT_, class P>
typename TaggedArrayBase<D, ElementT_, P>::SlotType
TaggedArrayBase<D, ElementT_, P>::RawFieldOfFirstElement() const {
  return RawFieldOfElementAt(0);
}

template <class D, class ElementT_, class P>
typename TaggedArrayBase<D, ElementT_, P>::SlotType
TaggedArrayBase<D, ElementT_, P>::RawFieldOfElementAt(uint32_t index) const {
  return SlotType(&derived()->objects()[index]);
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
    return isolate->roots_table().empty_fixed_array();
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
    return isolate->roots_table().empty_fixed_array();
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
  Handle<ProtectedFixedArray> result =
      TrustedCast<ProtectedFixedArray>(Allocate(
          isolate, capacity, &no_gc,
          shared ? AllocationType::kSharedTrusted : AllocationType::kTrusted));
  MemsetTagged((*result)->RawFieldOfFirstElement(), Smi::zero(), capacity);
  return result;
}

// static
template <class D, class ElementT_, class P>
template <class IsolateT>
Handle<D> TaggedArrayBase<D, ElementT_, P>::Allocate(
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
  Tagged<Map> map = Cast<Map>(roots.object_at(D::kMapRootIndex));
  DCHECK(ReadOnlyHeap::Contains(map));

  xs->set_map_after_allocation(isolate, map, SKIP_WRITE_BARRIER);
  xs->set_capacity(capacity);
#if TAGGED_SIZE_8_BYTES
  if constexpr (requires(D d) { d.clear_optional_padding(); }) {
    xs->clear_optional_padding();
  }
#endif  // TAGGED_SIZE_8_BYTES

  return handle(xs, isolate);
}

// static
template <class D, class ElementT_, class P>
constexpr uint32_t TaggedArrayBase<D, ElementT_, P>::NewCapacityForIndex(
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
  return IsTheHole(get(index));
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

// static
template <class IsolateT>
Handle<WeakFixedArray> WeakFixedArray::New(
    IsolateT* isolate, uint32_t capacity, AllocationType allocation,
    MaybeDirectHandle<Object> initial_value) {
  CHECK_LE(capacity, kMaxCapacity);

  if (V8_UNLIKELY(capacity == 0)) {
    return isolate->roots_table().empty_weak_fixed_array();
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
  if (capacity == 0) return isolate->roots_table().empty_array_list();

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

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_INL_H_
