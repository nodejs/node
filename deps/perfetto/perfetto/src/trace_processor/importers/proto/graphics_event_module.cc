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

#include "src/trace_processor/importers/proto/graphics_event_module.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

GraphicsEventModule::GraphicsEventModule(TraceProcessorContext* context)
    : parser_(context) {
  RegisterForField(TracePacket::kGpuCounterEventFieldNumber, context);
  RegisterForField(TracePacket::kGpuRenderStageEventFieldNumber, context);
  RegisterForField(TracePacket::kGpuLogFieldNumber, context);
  RegisterForField(TracePacket::kGraphicsFrameEventFieldNumber, context);
  RegisterForField(TracePacket::kVulkanMemoryEventFieldNumber, context);
  RegisterForField(TracePacket::kVulkanApiEventFieldNumber, context);
}

GraphicsEventModule::~GraphicsEventModule() = default;

void GraphicsEventModule::ParsePacket(const TracePacket::Decoder& decoder,
                                      const TimestampedTracePiece& ttp,
                                      uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kGpuCounterEventFieldNumber:
      parser_.ParseGpuCounterEvent(ttp.timestamp, decoder.gpu_counter_event());
      return;
    case TracePacket::kGpuRenderStageEventFieldNumber:
      parser_.ParseGpuRenderStageEvent(ttp.timestamp,
                                       decoder.gpu_render_stage_event());
      return;
    case TracePacket::kGpuLogFieldNumber:
      parser_.ParseGpuLog(ttp.timestamp, decoder.gpu_log());
      return;
    case TracePacket::kGraphicsFrameEventFieldNumber:
      parser_.ParseGraphicsFrameEvent(ttp.timestamp,
                                      decoder.graphics_frame_event());
      return;
    case TracePacket::kVulkanMemoryEventFieldNumber:
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      parser_.ParseVulkanMemoryEvent(ttp.packet_data.sequence_state,
                                     decoder.vulkan_memory_event());
      return;
    case TracePacket::kVulkanApiEventFieldNumber:
      parser_.ParseVulkanApiEvent(ttp.timestamp, decoder.vulkan_api_event());
      return;
  }
}

}  // namespace trace_processor
}  // namespace perfetto
