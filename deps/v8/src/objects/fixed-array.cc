// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/fixed-array.h"

#include "src/objects/map-inl.h"

namespace v8 {
namespace internal {

int FixedArrayBase::GetMaxLengthForNewSpaceAllocation(ElementsKind kind) {
  return ((kMaxRegularHeapObjectSize - FixedArrayBase::kHeaderSize) >>
          ElementsKindToShiftSize(kind));
}

bool FixedArrayBase::IsCowArray() const {
  return map() == GetReadOnlyRoots().fixed_cow_array_map();
}

template <template <typename> typename HandleType>
  requires(
      std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
HandleType<FixedArray> FixedArray::Grow(Isolate* isolate,
                                        HandleType<FixedArray> array,
                                        uint32_t index) {
  const uint32_t len = array->ulength().value();
  if (index >= len) {
    const uint32_t new_capacity = FixedArray::NewCapacityForIndex(index, len);
    array = Cast<FixedArray>(FixedArray::Resize(isolate, array, new_capacity));
    // TODO(jgruber): This is somewhat subtle - other FixedArray methods
    // use `undefined` as a filler. Make this more explicit.
    array->FillWithHoles(len, new_capacity);
  }
  return array;
}

template <template <typename> typename HandleType>
  requires(
      std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
HandleType<FixedArray> FixedArray::SetAndGrow(Isolate* isolate,
                                              HandleType<FixedArray> array,
                                              uint32_t index,
                                              DirectHandle<Object> value) {
  array = Grow(isolate, array, index);
  array->set(index, *value);
  return array;
}

template DirectHandle<FixedArray> FixedArray::SetAndGrow(
    Isolate* isolate, DirectHandle<FixedArray> array, uint32_t index,
    DirectHandle<Object> value);
template IndirectHandle<FixedArray> FixedArray::SetAndGrow(
    Isolate* isolate, IndirectHandle<FixedArray> array, uint32_t index,
    DirectHandle<Object> value);

template <template <typename> typename HandleType>
  requires(
      std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
HandleType<FixedArray> FixedArray::SetAndGrow(Isolate* isolate,
                                              HandleType<FixedArray> array,
                                              uint32_t index,
                                              Tagged<Smi> value) {
  array = Grow(isolate, array, index);
  array->set(index, value);
  return array;
}

template DirectHandle<FixedArray> FixedArray::SetAndGrow(
    Isolate* isolate, DirectHandle<FixedArray> array, uint32_t index,
    Tagged<Smi> value);
template IndirectHandle<FixedArray> FixedArray::SetAndGrow(
    Isolate* isolate, IndirectHandle<FixedArray> array, uint32_t index,
    Tagged<Smi> value);

void FixedArray::RightTrim(Isolate* isolate, uint32_t new_capacity) {
  DCHECK_NE(map(), ReadOnlyRoots{isolate}.fixed_cow_array_map());
  Super::RightTrim(isolate, new_capacity);
}

template <template <typename> typename HandleType>
  requires(
      std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
HandleType<FixedArray> FixedArray::RightTrimOrEmpty(
    Isolate* isolate, HandleType<FixedArray> array, uint32_t new_length) {
  if (new_length == 0) {
    return isolate->factory()->empty_fixed_array();
  }
  array->RightTrim(isolate, new_length);
  return array;
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    DirectHandle<FixedArray> FixedArray::RightTrimOrEmpty(
        Isolate* isolate, DirectHandle<FixedArray> array, uint32_t new_length);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    IndirectHandle<FixedArray> FixedArray::RightTrimOrEmpty(
        Isolate* isolate, IndirectHandle<FixedArray> array,
        uint32_t new_length);

// static
DirectHandle<ArrayList> ArrayList::Add(Isolate* isolate,
                                       DirectHandle<ArrayList> array,
                                       Tagged<Smi> obj,
                                       AllocationType allocation) {
  const uint32_t length = array->ulength().value();
  const uint32_t new_length = length + 1;
  array = EnsureSpace(isolate, array, new_length, allocation);
  DCHECK_EQ(array->ulength().value(), length);

  DisallowGarbageCollection no_gc;
  array->set(length, obj, SKIP_WRITE_BARRIER);
  array->set_length(new_length);
  return array;
}

// static
DirectHandle<ArrayList> ArrayList::Add(Isolate* isolate,
                                       DirectHandle<ArrayList> array,
                                       DirectHandle<Object> obj,
                                       AllocationType allocation) {
  const uint32_t length = array->ulength().value();
  const uint32_t new_length = length + 1;
  array = EnsureSpace(isolate, array, new_length, allocation);
  DCHECK_EQ(array->ulength().value(), length);

  DisallowGarbageCollection no_gc;
  array->set(length, *obj);
  array->set_length(new_length);
  return array;
}

// static
DirectHandle<ArrayList> ArrayList::Add(Isolate* isolate,
                                       DirectHandle<ArrayList> array,
                                       DirectHandle<Object> obj0,
                                       DirectHandle<Object> obj1,
                                       AllocationType allocation) {
  const uint32_t length = array->ulength().value();
  const uint32_t new_length = length + 2;
  array = EnsureSpace(isolate, array, new_length, allocation);
  DCHECK_EQ(array->ulength().value(), length);

  DisallowGarbageCollection no_gc;
  array->set(length + 0, *obj0);
  array->set(length + 1, *obj1);
  array->set_length(new_length);
  return array;
}

// static
DirectHandle<FixedArray> ArrayList::ToFixedArray(Isolate* isolate,
                                                 DirectHandle<ArrayList> array,
                                                 AllocationType allocation) {
  const uint32_t length = array->ulength().value();
  if (length == 0) return isolate->factory()->empty_fixed_array();

  DirectHandle<FixedArray> result =
      FixedArray::New(isolate, length, allocation);
  DisallowGarbageCollection no_gc;
  WriteBarrierModeScope mode = result->GetWriteBarrierMode(no_gc);
  ObjectSlot dst_slot(result->RawFieldOfElementAt(0));
  ObjectSlot src_slot(array->RawFieldOfElementAt(0));
  isolate->heap()->CopyRange(*result, dst_slot, src_slot, length, *mode);
  return result;
}

void ArrayList::RightTrim(Isolate* isolate, uint32_t new_capacity) {
  Super::RightTrim(isolate, new_capacity);
  if (new_capacity < ulength().value()) set_length(new_capacity);
}

// static
DirectHandle<ArrayList> ArrayList::EnsureSpace(Isolate* isolate,
                                               DirectHandle<ArrayList> array,
                                               uint32_t length,
                                               AllocationType allocation) {
  DCHECK_LT(0, length);
  const uint32_t old_capacity = array->capacity().value();
  if (old_capacity >= length) return array;

  const uint32_t old_length = array->ulength().value();
  // Ensure calculation matches CodeStubAssembler::ArrayListEnsureSpace.
  uint32_t new_capacity = length + std::max(length / 2, 2u);
  DirectHandle<ArrayList> new_array =
      ArrayList::New(isolate, new_capacity, allocation);
  DisallowGarbageCollection no_gc;
  new_array->set_length(old_length);
  WriteBarrierModeScope mode = new_array->GetWriteBarrierMode(no_gc);
  CopyElements(isolate, *new_array, 0, *array, 0, old_length, *mode);
  return new_array;
}

// static
Handle<WeakArrayList> WeakArrayList::AddToEnd(Isolate* isolate,
                                              Handle<WeakArrayList> array,
                                              MaybeObjectDirectHandle value) {
  uint32_t length = array->length().value();
  array = EnsureSpace(isolate, array, length + 1);
  {
    DisallowGarbageCollection no_gc;
    Tagged<WeakArrayList> raw = *array;
    // Reload length; GC might have removed elements from the array.
    length = raw->length().value();
    raw->Set(length, *value);
    raw->set_length(length + 1);
  }
  return array;
}

Handle<WeakArrayList> WeakArrayList::AddToEnd(Isolate* isolate,
                                              Handle<WeakArrayList> array,
                                              MaybeObjectDirectHandle value1,
                                              Tagged<Smi> value2) {
  uint32_t length = array->length().value();
  array = EnsureSpace(isolate, array, length + 2);
  {
    DisallowGarbageCollection no_gc;
    Tagged<WeakArrayList> raw = *array;
    // Reload length; GC might have removed elements from the array.
    length = array->length().value();
    raw->Set(length, *value1);
    raw->Set(length + 1, value2);
    raw->set_length(length + 2);
  }
  return array;
}

// static
DirectHandle<WeakArrayList> WeakArrayList::Append(
    Isolate* isolate, DirectHandle<WeakArrayList> array,
    MaybeObjectDirectHandle value, AllocationType allocation) {
  uint32_t length = 0;
  uint32_t new_length = 0;
  {
    DisallowGarbageCollection no_gc;
    Tagged<WeakArrayList> raw = *array;
    length = raw->length().value();

    if (length < raw->capacity().value()) {
      raw->Set(length, *value);
      raw->set_length(length + 1);
      return array;
    }

    // Not enough space in the array left, either grow, shrink or
    // compact the array.
    new_length = raw->CountLiveElements() + 1;
  }

  bool shrink = new_length < length / 4;
  bool grow = 3 * (length / 4) < new_length;

  if (shrink || grow) {
    // Grow or shrink array and compact out-of-place.
    uint32_t new_capacity = CapacityForLength(new_length);
    array = isolate->factory()->CompactWeakArrayList(array, new_capacity,
                                                     allocation);

  } else {
    // Perform compaction in the current array.
    array->Compact(isolate);
  }

  // Now append value to the array, there should always be enough space now.
  DCHECK_LT(array->length(), array->capacity());

  {
    DisallowGarbageCollection no_gc;
    Tagged<WeakArrayList> raw = *array;
    // Reload length, allocation might have killed some weak refs.
    uint32_t index = raw->length().value();
    raw->Set(index, *value);
    raw->set_length(index + 1);
  }
  return array;
}

void WeakArrayList::Compact(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  const uint32_t length = this->length().value();
  uint32_t new_length = 0;

  for (uint32_t i = 0; i < length; i++) {
    Tagged<MaybeObject> value = Get(i);

    if (!value.IsCleared()) {
      if (new_length != i) {
        Set(new_length, value);
      }
      ++new_length;
    }
  }

  set_length(new_length);
}

bool WeakArrayList::IsFull() const { return length() == capacity(); }

// static
Handle<WeakArrayList> WeakArrayList::EnsureSpace(Isolate* isolate,
                                                 Handle<WeakArrayList> array,
                                                 uint32_t length,
                                                 AllocationType allocation) {
  uint32_t capacity = array->capacity().value();
  if (capacity < length) {
    uint32_t grow_by = CapacityForLength(length) - capacity;
    array = isolate->factory()->CopyWeakArrayListAndGrow(array, grow_by,
                                                         allocation);
  }
  return array;
}

uint32_t WeakArrayList::CountLiveWeakReferences() const {
  uint32_t live_weak_references = 0;
  const uint32_t len = length().value();
  for (uint32_t i = 0; i < len; i++) {
    if (Get(i).IsWeak()) {
      ++live_weak_references;
    }
  }
  return live_weak_references;
}

uint32_t WeakArrayList::CountLiveElements() const {
  uint32_t non_cleared_objects = 0;
  const uint32_t len = length().value();
  for (uint32_t i = 0; i < len; i++) {
    if (!Get(i).IsCleared()) {
      ++non_cleared_objects;
    }
  }
  return non_cleared_objects;
}

bool WeakArrayList::RemoveOne(MaybeObjectDirectHandle value) {
  const uint32_t len = length().value();
  DCHECK_LE(len, kMaxInt);
  int last_index = static_cast<int>(len) - 1;
  // Optimize for the most recently added element to be removed again.
  Tagged<ClearedWeakValue> cleared_value = ClearedValue();
  for (int i = last_index; i >= 0; --i) {
    if (Get(i) != *value) continue;
    // Move the last element into this slot (or no-op, if this is the last
    // slot).
    Set(i, Get(last_index));
    Set(last_index, cleared_value);
    set_length(last_index);
    return true;
  }
  return false;
}

bool WeakArrayList::Contains(Tagged<MaybeObject> value) {
  const uint32_t len = length().value();
  for (uint32_t i = 0; i < len; ++i) {
    if (Get(i) == value) return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8
