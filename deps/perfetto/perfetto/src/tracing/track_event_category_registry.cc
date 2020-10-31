/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/tracing/track_event_category_registry.h"

namespace perfetto {

// static
Category Category::FromDynamicCategory(const char* name) {
  if (GetNthNameSize(1, name, name)) {
    Category group(Group(name));
    PERFETTO_DCHECK(group.name);
    return group;
  }
  Category category(name);
  PERFETTO_DCHECK(category.name);
  return category;
}

Category Category::FromDynamicCategory(
    const DynamicCategory& dynamic_category) {
  return FromDynamicCategory(dynamic_category.name.c_str());
}

namespace internal {

perfetto::DynamicCategory NullCategory(const perfetto::DynamicCategory&) {
  return perfetto::DynamicCategory{};
}

const Category* TrackEventCategoryRegistry::GetCategory(size_t index) const {
  PERFETTO_DCHECK(index < category_count_);
  return &categories_[index];
}

void TrackEventCategoryRegistry::EnableCategoryForInstance(
    size_t category_index,
    uint32_t instance_index) const {
  PERFETTO_DCHECK(instance_index < kMaxDataSourceInstances);
  PERFETTO_DCHECK(category_index < category_count_);
  // Matches the acquire_load in DataSource::Trace().
  state_storage_[category_index].fetch_or(
      static_cast<uint8_t>(1u << instance_index), std::memory_order_release);
}

void TrackEventCategoryRegistry::DisableCategoryForInstance(
    size_t category_index,
    uint32_t instance_index) const {
  PERFETTO_DCHECK(instance_index < kMaxDataSourceInstances);
  PERFETTO_DCHECK(category_index < category_count_);
  // Matches the acquire_load in DataSource::Trace().
  state_storage_[category_index].fetch_and(
      static_cast<uint8_t>(~(1u << instance_index)), std::memory_order_release);
}

}  // namespace internal
}  // namespace perfetto
