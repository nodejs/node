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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_VULKAN_MEMORY_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_VULKAN_MEMORY_TRACKER_H_

#include "src/trace_processor/importers/proto/proto_incremental_state.h"
#include "src/trace_processor/storage/trace_storage.h"

#include "protos/perfetto/trace/gpu/vulkan_memory_event.pbzero.h"

namespace perfetto {
namespace trace_processor {

using protos::pbzero::VulkanMemoryEvent;

class TraceProcessorContext;

class VulkanMemoryTracker {
 public:
  enum class DeviceCounterType {
    kAllocationCounter = 0,
    kBindCounter = 1,
  };

  explicit VulkanMemoryTracker(TraceProcessorContext* context);
  ~VulkanMemoryTracker() = default;

  template <int32_t FieldId>
  StringId GetInternedString(PacketSequenceStateGeneration* state,
                             uint64_t iid) {
    auto* decoder =
        state->LookupInternedMessage<FieldId, protos::pbzero::InternedString>(
            iid);
    if (!decoder)
      return kNullStringId;
    return context_->storage->InternString(
        base::StringView(reinterpret_cast<const char*>(decoder->str().data),
                         decoder->str().size));
  }

  StringId FindSourceString(VulkanMemoryEvent::Source);
  StringId FindOperationString(VulkanMemoryEvent::Operation);
  StringId FindAllocationScopeString(VulkanMemoryEvent::AllocationScope);
  StringId FindAllocationScopeCounterString(VulkanMemoryEvent::AllocationScope);
  StringId FindMemoryTypeCounterString(uint32_t /*memory_type*/,
                                       DeviceCounterType);

 private:
  TraceProcessorContext* const context_;

  const std::string vulkan_driver_memory_counter_str_;
  const std::string vulkan_device_memory_counter_str_;
  std::vector<StringId> source_strs_id_;
  std::vector<StringId> operation_strs_id_;
  std::vector<StringId> scope_strs_id_;
  std::vector<StringId> scope_counter_strs_id_;
  std::unordered_map<uint32_t /*memory_type*/, StringId>
      memory_type_allocation_counter_string_map_;
  std::unordered_map<uint32_t /*memory_type*/, StringId>
      memory_type_bind_counter_string_map_;

  void SetupSourceAndTypeInternedStrings();
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_VULKAN_MEMORY_TRACKER_H_
