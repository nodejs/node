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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_GRAPHICS_EVENT_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_GRAPHICS_EVENT_PARSER_H_

#include <vector>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_writer.h"
#include "perfetto/protozero/field.h"
#include "protos/perfetto/trace/gpu/gpu_render_stage_event.pbzero.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/proto/proto_incremental_state.h"
#include "src/trace_processor/importers/proto/vulkan_memory_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"

#include "protos/perfetto/trace/gpu/vulkan_memory_event.pbzero.h"

namespace perfetto {

namespace protos {
namespace pbzero {

class GpuRenderStageEvent_Decoder;

}  // namespace pbzero
}  // namespace protos

namespace trace_processor {

class TraceProcessorContext;

struct ProtoEnumHasher {
  template <typename T>
  std::size_t operator()(T t) const {
    return static_cast<std::size_t>(t);
  }
};

// Class for parsing graphics related events.
class GraphicsEventParser {
 public:
  using ConstBytes = protozero::ConstBytes;
  using VulkanMemoryEventSource = VulkanMemoryEvent::Source;
  using VulkanMemoryEventOperation = VulkanMemoryEvent::Operation;
  explicit GraphicsEventParser(TraceProcessorContext*);

  void ParseGpuCounterEvent(int64_t ts, ConstBytes);
  void ParseGpuRenderStageEvent(int64_t ts, ConstBytes);
  void ParseGraphicsFrameEvent(int64_t timestamp, ConstBytes);
  void ParseGpuLog(int64_t ts, ConstBytes);

  void ParseVulkanMemoryEvent(PacketSequenceStateGeneration*, ConstBytes);
  void UpdateVulkanMemoryAllocationCounters(UniquePid,
                                            const VulkanMemoryEvent::Decoder&);

  void ParseVulkanApiEvent(int64_t, ConstBytes);

 private:
  const StringId GetFullStageName(
      const protos::pbzero::GpuRenderStageEvent_Decoder& event) const;
  void InsertGpuTrack(
      const protos::pbzero::
          GpuRenderStageEvent_Specifications_Description_Decoder& hw_queue);
  base::Optional<std::string> FindDebugName(int32_t vk_object_type,
                                            uint64_t vk_handle) const;

  TraceProcessorContext* const context_;
  VulkanMemoryTracker vulkan_memory_tracker_;
  // For GpuCounterEvent
  std::unordered_map<uint32_t, TrackId> gpu_counter_track_ids_;
  // For GpuRenderStageEvent
  const StringId description_id_;
  const StringId gpu_render_stage_scope_id_;
  std::vector<perfetto::base::Optional<TrackId>> gpu_hw_queue_ids_;
  size_t gpu_hw_queue_counter_ = 0;
  // Map of stage ID -> pair(stage name, stage description)
  std::vector<std::pair<StringId, StringId>> gpu_render_stage_ids_;
  // For GraphicsFrameEvent
  const StringId graphics_event_scope_id_;
  const StringId unknown_event_name_id_;
  const StringId no_layer_name_name_id_;
  const StringId layer_name_key_id_;
  std::array<StringId, 14> event_type_name_ids_;
  int64_t previous_timestamp_ = 0;
  char present_frame_buffer_[4096];
  char present_frame_layer_buffer_[4096];
  char present_frame_numbers_buffer_[4096];
  StringId present_event_name_id_;
  base::StringWriter present_frame_name_;
  base::StringWriter present_frame_layer_name_;
  base::StringWriter present_frame_numbers_;
  TrackId present_track_id_;
  // Row indices of frame stats table. Used to populate the slice_id after
  // inserting the rows.
  std::vector<uint32_t> graphics_frame_stats_idx_;
  // Map of buffer ID -> (Map of GraphicsFrameEvent -> ts of that event)
  std::unordered_map<uint32_t, std::unordered_map<uint64_t, int64_t>>
      graphics_frame_stats_map_;
  // For VulkanMemoryEvent
  std::unordered_map<VulkanMemoryEvent::AllocationScope,
                     int64_t /*counter_value*/,
                     ProtoEnumHasher>
      vulkan_driver_memory_counters_;
  std::unordered_map<uint32_t /*memory_type*/, int64_t /*counter_value*/>
      vulkan_device_memory_counters_allocate_;
  std::unordered_map<uint32_t /*memory_type*/, int64_t /*counter_value*/>
      vulkan_device_memory_counters_bind_;
  // For GpuLog
  const StringId gpu_log_track_name_id_;
  const StringId gpu_log_scope_id_;
  const StringId tag_id_;
  const StringId log_message_id_;
  std::array<StringId, 7> log_severity_ids_;
  // For Vulkan events.
  // For VulkanApiEvent.VkDebugUtilsObjectName.
  // Map of vk handle -> vk object name.
  using DebugMarkerMap = std::unordered_map<uint64_t, std::string>;
  // Map of VkObjectType -> DebugMarkerMap.
  std::unordered_map<int32_t, DebugMarkerMap> debug_marker_names_;
  // For VulkanApiEvent.VkQueueSubmit.
  StringId vk_event_track_id_;
  StringId vk_event_scope_id_;
  StringId vk_queue_submit_id_;
};
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_GRAPHICS_EVENT_PARSER_H_
