// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/materialized-object-store.h"

#include "src/base/numerics/safe_conversions.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/oddball.h"

namespace v8 {
namespace internal {

DirectHandle<FixedArray> MaterializedObjectStore::Get(Address fp) {
  std::optional<uint32_t> index = StackIdToIndex(fp);
  if (!index.has_value()) {
    return Handle<FixedArray>::null();
  }
  DirectHandle<FixedArray> array = GetStackEntries();
  CHECK_GT(array->ulength().value(), *index);
  return Cast<FixedArray>(Handle<Object>(array->get(*index), isolate()));
}

void MaterializedObjectStore::Set(
    Address fp, DirectHandle<FixedArray> materialized_objects) {
  std::optional<uint32_t> index = StackIdToIndex(fp);
  if (!index.has_value()) {
    index = base::checked_cast<uint32_t>(frame_fps_.size());
    frame_fps_.push_back(fp);
  }

  DirectHandle<FixedArray> array = EnsureStackEntries(*index + 1);
  array->set(*index, *materialized_objects);
}

bool MaterializedObjectStore::Remove(Address fp) {
  auto it = std::find(frame_fps_.begin(), frame_fps_.end(), fp);
  if (it == frame_fps_.end()) return false;
  uint32_t index = static_cast<uint32_t>(std::distance(frame_fps_.begin(), it));

  frame_fps_.erase(it);
  Tagged<FixedArray> array = isolate()->heap()->materialized_objects();

  CHECK_LT(index, array->ulength().value());
  uint32_t fps_size = base::checked_cast<uint32_t>(frame_fps_.size());
  for (uint32_t i = index; i < fps_size; i++) {
    array->set(i, array->get(i + 1));
  }
  array->set(fps_size, ReadOnlyRoots(isolate()).undefined_value());
  return true;
}

std::optional<uint32_t> MaterializedObjectStore::StackIdToIndex(Address fp) {
  auto it = std::find(frame_fps_.begin(), frame_fps_.end(), fp);
  return it == frame_fps_.end() ? std::nullopt
                                : std::make_optional(static_cast<uint32_t>(
                                      std::distance(frame_fps_.begin(), it)));
}

DirectHandle<FixedArray> MaterializedObjectStore::GetStackEntries() {
  return DirectHandle<FixedArray>(isolate()->heap()->materialized_objects(),
                                  isolate());
}

DirectHandle<FixedArray> MaterializedObjectStore::EnsureStackEntries(
    uint32_t length) {
  DirectHandle<FixedArray> array = GetStackEntries();
  uint32_t array_len = array->ulength().value();
  if (array_len >= length) {
    return array;
  }

  uint32_t new_length = length > 10 ? length : 10;
  if (new_length < 2 * array_len) {
    new_length = 2 * array_len;
  }

  DirectHandle<FixedArray> new_array =
      isolate()->factory()->NewFixedArray(new_length, AllocationType::kOld);
  for (uint32_t i = 0; i < array_len; i++) {
    new_array->set(i, array->get(i));
  }
  Tagged<HeapObject> undefined_value =
      ReadOnlyRoots(isolate()).undefined_value();
  for (uint32_t i = array_len; i < length; i++) {
    new_array->set(i, undefined_value);
  }
  isolate()->heap()->SetRootMaterializedObjects(*new_array);
  return new_array;
}

}  // namespace internal
}  // namespace v8
