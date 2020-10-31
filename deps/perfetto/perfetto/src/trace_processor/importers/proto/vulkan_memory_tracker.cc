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

#include "src/trace_processor/importers/proto/vulkan_memory_tracker.h"

#include <string>
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

VulkanMemoryTracker::VulkanMemoryTracker(TraceProcessorContext* context)
    : context_(context),
      vulkan_driver_memory_counter_str_("vulkan.mem.driver.scope."),
      vulkan_device_memory_counter_str_("vulkan.mem.device.memory.type.") {
  SetupSourceAndTypeInternedStrings();
}

void VulkanMemoryTracker::SetupSourceAndTypeInternedStrings() {
  const std::vector<std::string> event_sources = {
      "UNSPECIFIED",       "DRIVER",     "DEVICE",
      "GPU_DEVICE_MEMORY", "GPU_BUFFER", "GPU_IMAGE"};
  for (size_t i = 0; i < event_sources.size(); i++) {
    source_strs_id_.emplace_back(context_->storage->InternString(
        base::StringView(event_sources[i].c_str(), event_sources[i].length())));
  }

  const std::vector<std::string> event_operations = {
      "UNSPECIFIED", "CREATE",        "DESTROY",
      "BIND",        "DESTROY_BOUND", "ANNOTATIONS"};
  for (size_t i = 0; i < event_operations.size(); i++) {
    operation_strs_id_.emplace_back(
        context_->storage->InternString(base::StringView(
            event_operations[i].c_str(), event_operations[i].length())));
  }

  const std::vector<std::string> event_scopes = {
      "UNSPECIFIED", "COMMAND", "OBJECT", "CACHE", "DEVICE", "INSTANCE"};
  for (size_t i = 0; i < event_scopes.size(); i++) {
    scope_strs_id_.emplace_back(context_->storage->InternString(
        base::StringView(event_scopes[i].c_str(), event_scopes[i].length())));
    auto scope_counter_str =
        vulkan_driver_memory_counter_str_ + event_scopes[i];
    scope_counter_strs_id_.emplace_back(
        context_->storage->InternString(base::StringView(
            scope_counter_str.c_str(), scope_counter_str.length())));
  }
}

StringId VulkanMemoryTracker::FindSourceString(
    VulkanMemoryEvent::Source source) {
  return source_strs_id_[static_cast<size_t>(source)];
}

StringId VulkanMemoryTracker::FindOperationString(
    VulkanMemoryEvent::Operation operation) {
  return operation_strs_id_[static_cast<size_t>(operation)];
}

StringId VulkanMemoryTracker::FindAllocationScopeString(
    VulkanMemoryEvent::AllocationScope scope) {
  return scope_strs_id_[static_cast<size_t>(scope)];
}

StringId VulkanMemoryTracker::FindAllocationScopeCounterString(
    VulkanMemoryEvent::AllocationScope scope) {
  return scope_counter_strs_id_[static_cast<size_t>(scope)];
}

StringId VulkanMemoryTracker::FindMemoryTypeCounterString(
    uint32_t memory_type,
    DeviceCounterType counter_type) {
  StringId res = kNullStringId;
  std::unordered_map<uint32_t, StringId>::iterator it;
  std::string type_counter_str;
  switch (counter_type) {
    case DeviceCounterType::kAllocationCounter:
      it = memory_type_allocation_counter_string_map_.find(memory_type);
      if (it == memory_type_allocation_counter_string_map_.end()) {
        type_counter_str = vulkan_device_memory_counter_str_ +
                           std::to_string(memory_type) + ".allocation";
        res = context_->storage->InternString(base::StringView(
            type_counter_str.c_str(), type_counter_str.length()));
        memory_type_allocation_counter_string_map_.emplace(memory_type, res);
      } else {
        res = it->second;
      }
      break;
    case DeviceCounterType::kBindCounter:
      it = memory_type_bind_counter_string_map_.find(memory_type);
      if (it == memory_type_bind_counter_string_map_.end()) {
        type_counter_str = vulkan_device_memory_counter_str_ +
                           std::to_string(memory_type) + ".bind";
        res = context_->storage->InternString(base::StringView(
            type_counter_str.c_str(), type_counter_str.length()));
        memory_type_bind_counter_string_map_.emplace(memory_type, res);
      } else {
        res = it->second;
      }
      break;
  }
  return res;
}

}  // namespace trace_processor
}  // namespace perfetto
