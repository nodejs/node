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

#include "src/trace_processor/importers/proto/graphics_event_parser.h"

#include <inttypes.h>

#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/field.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/common/gpu_counter_descriptor.pbzero.h"
#include "protos/perfetto/trace/android/graphics_frame_event.pbzero.h"
#include "protos/perfetto/trace/gpu/gpu_counter_event.pbzero.h"
#include "protos/perfetto/trace/gpu/gpu_log.pbzero.h"
#include "protos/perfetto/trace/gpu/gpu_render_stage_event.pbzero.h"
#include "protos/perfetto/trace/gpu/vulkan_api_event.pbzero.h"
#include "protos/perfetto/trace/gpu/vulkan_memory_event.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {

// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkObjectType.html
typedef enum VkObjectType {
  VK_OBJECT_TYPE_UNKNOWN = 0,
  VK_OBJECT_TYPE_INSTANCE = 1,
  VK_OBJECT_TYPE_PHYSICAL_DEVICE = 2,
  VK_OBJECT_TYPE_DEVICE = 3,
  VK_OBJECT_TYPE_QUEUE = 4,
  VK_OBJECT_TYPE_SEMAPHORE = 5,
  VK_OBJECT_TYPE_COMMAND_BUFFER = 6,
  VK_OBJECT_TYPE_FENCE = 7,
  VK_OBJECT_TYPE_DEVICE_MEMORY = 8,
  VK_OBJECT_TYPE_BUFFER = 9,
  VK_OBJECT_TYPE_IMAGE = 10,
  VK_OBJECT_TYPE_EVENT = 11,
  VK_OBJECT_TYPE_QUERY_POOL = 12,
  VK_OBJECT_TYPE_BUFFER_VIEW = 13,
  VK_OBJECT_TYPE_IMAGE_VIEW = 14,
  VK_OBJECT_TYPE_SHADER_MODULE = 15,
  VK_OBJECT_TYPE_PIPELINE_CACHE = 16,
  VK_OBJECT_TYPE_PIPELINE_LAYOUT = 17,
  VK_OBJECT_TYPE_RENDER_PASS = 18,
  VK_OBJECT_TYPE_PIPELINE = 19,
  VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT = 20,
  VK_OBJECT_TYPE_SAMPLER = 21,
  VK_OBJECT_TYPE_DESCRIPTOR_POOL = 22,
  VK_OBJECT_TYPE_DESCRIPTOR_SET = 23,
  VK_OBJECT_TYPE_FRAMEBUFFER = 24,
  VK_OBJECT_TYPE_COMMAND_POOL = 25,
  VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION = 1000156000,
  VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE = 1000085000,
  VK_OBJECT_TYPE_SURFACE_KHR = 1000000000,
  VK_OBJECT_TYPE_SWAPCHAIN_KHR = 1000001000,
  VK_OBJECT_TYPE_DISPLAY_KHR = 1000002000,
  VK_OBJECT_TYPE_DISPLAY_MODE_KHR = 1000002001,
  VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT = 1000011000,
  VK_OBJECT_TYPE_OBJECT_TABLE_NVX = 1000086000,
  VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX = 1000086001,
  VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT = 1000128000,
  VK_OBJECT_TYPE_VALIDATION_CACHE_EXT = 1000160000,
  VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV = 1000165000,
  VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL = 1000210000,
  VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR =
      VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE,
  VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_KHR =
      VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION,
  VK_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkObjectType;

}  // anonymous namespace

GraphicsEventParser::GraphicsEventParser(TraceProcessorContext* context)
    : context_(context),
      vulkan_memory_tracker_(context),
      description_id_(context->storage->InternString("description")),
      gpu_render_stage_scope_id_(
          context->storage->InternString("gpu_render_stage")),
      graphics_event_scope_id_(
          context->storage->InternString("graphics_frame_event")),
      unknown_event_name_id_(context->storage->InternString("unknown_event")),
      no_layer_name_name_id_(context->storage->InternString("no_layer_name")),
      layer_name_key_id_(context->storage->InternString("layer_name")),
      event_type_name_ids_{
          {context->storage->InternString(
               "unspecified_event") /* UNSPECIFIED */,
           context->storage->InternString("Dequeue") /* DEQUEUE */,
           context->storage->InternString("Queue") /* QUEUE */,
           context->storage->InternString("Post") /* POST */,
           context->storage->InternString(
               "AcquireFenceSignaled") /* ACQUIRE_FENCE */,
           context->storage->InternString("Latch") /* LATCH */,
           context->storage->InternString(
               "HWCCompositionQueued") /* HWC_COMPOSITION_QUEUED */,
           context->storage->InternString(
               "FallbackComposition") /* FALLBACK_COMPOSITION */,
           context->storage->InternString(
               "PresentFenceSignaled") /* PRESENT_FENCE */,
           context->storage->InternString(
               "ReleaseFenceSignaled") /* RELEASE_FENCE */,
           context->storage->InternString("Modify") /* MODIFY */,
           context->storage->InternString("Detach") /* DETACH */,
           context->storage->InternString("Attach") /* ATTACH */,
           context->storage->InternString("Cancel") /* CANCEL */}},
      present_frame_name_(present_frame_buffer_,
                          base::ArraySize(present_frame_buffer_)),
      present_frame_layer_name_(present_frame_layer_buffer_,
                                base::ArraySize(present_frame_layer_buffer_)),
      present_frame_numbers_(present_frame_numbers_buffer_,
                             base::ArraySize(present_frame_numbers_buffer_)),
      gpu_log_track_name_id_(context_->storage->InternString("GPU Log")),
      gpu_log_scope_id_(context_->storage->InternString("gpu_log")),
      tag_id_(context_->storage->InternString("tag")),
      log_message_id_(context->storage->InternString("message")),
      log_severity_ids_{{context_->storage->InternString("UNSPECIFIED"),
                         context_->storage->InternString("VERBOSE"),
                         context_->storage->InternString("DEBUG"),
                         context_->storage->InternString("INFO"),
                         context_->storage->InternString("WARNING"),
                         context_->storage->InternString("ERROR"),
                         context_->storage->InternString(
                             "UNKNOWN_SEVERITY") /* must be last */}},
      vk_event_track_id_(context->storage->InternString("Vulkan Events")),
      vk_event_scope_id_(context->storage->InternString("vulkan_events")),
      vk_queue_submit_id_(context->storage->InternString("vkQueueSubmit")) {}

void GraphicsEventParser::ParseGpuCounterEvent(int64_t ts, ConstBytes blob) {
  protos::pbzero::GpuCounterEvent::Decoder event(blob.data, blob.size);

  protos::pbzero::GpuCounterDescriptor::Decoder descriptor(
      event.counter_descriptor());
  // Add counter spec to ID map.
  for (auto it = descriptor.specs(); it; ++it) {
    protos::pbzero::GpuCounterDescriptor_GpuCounterSpec::Decoder spec(*it);
    if (!spec.has_counter_id()) {
      PERFETTO_ELOG("Counter spec missing counter id");
      context_->storage->IncrementStats(stats::gpu_counters_invalid_spec);
      continue;
    }
    if (!spec.has_name()) {
      context_->storage->IncrementStats(stats::gpu_counters_invalid_spec);
      continue;
    }

    auto counter_id = spec.counter_id();
    auto name = spec.name();
    if (gpu_counter_track_ids_.find(counter_id) ==
        gpu_counter_track_ids_.end()) {
      auto desc = spec.description();

      StringId unit_id = kNullStringId;
      if (spec.has_numerator_units() || spec.has_denominator_units()) {
        char buffer[1024];
        base::StringWriter unit(buffer, sizeof(buffer));
        for (auto numer = spec.numerator_units(); numer; ++numer) {
          if (unit.pos()) {
            unit.AppendChar(':');
          }
          unit.AppendInt(*numer);
        }
        char sep = '/';
        for (auto denom = spec.denominator_units(); denom; ++denom) {
          unit.AppendChar(sep);
          unit.AppendInt(*denom);
          sep = ':';
        }
        unit_id = context_->storage->InternString(unit.GetStringView());
      }

      auto name_id = context_->storage->InternString(name);
      auto desc_id = context_->storage->InternString(desc);
      auto track_id = context_->track_tracker->CreateGpuCounterTrack(
          name_id, 0 /* gpu_id */, desc_id, unit_id);
      gpu_counter_track_ids_.emplace(counter_id, track_id);
    } else {
      // Either counter spec was repeated or it came after counter data.
      PERFETTO_ELOG("Duplicated counter spec found. (counter_id=%d, name=%s)",
                    counter_id, name.ToStdString().c_str());
      context_->storage->IncrementStats(stats::gpu_counters_invalid_spec);
    }
  }

  for (auto it = event.counters(); it; ++it) {
    protos::pbzero::GpuCounterEvent_GpuCounter::Decoder counter(*it);
    if (counter.has_counter_id() &&
        (counter.has_int_value() || counter.has_double_value())) {
      auto counter_id = counter.counter_id();
      // Check missing counter_id
      if (gpu_counter_track_ids_.find(counter_id) ==
          gpu_counter_track_ids_.end()) {
        char buffer[64];
        base::StringWriter writer(buffer, sizeof(buffer));
        writer.AppendString("gpu_counter(");
        writer.AppendUnsignedInt(counter_id);
        writer.AppendString(")");
        auto name_id = context_->storage->InternString(writer.GetStringView());

        TrackId track = context_->track_tracker->CreateGpuCounterTrack(
            name_id, 0 /* gpu_id */);
        gpu_counter_track_ids_.emplace(counter_id, track);
        context_->storage->IncrementStats(stats::gpu_counters_missing_spec);
      }
      if (counter.has_int_value()) {
        context_->event_tracker->PushCounter(
            ts, counter.int_value(), gpu_counter_track_ids_[counter_id]);
      } else {
        context_->event_tracker->PushCounter(
            ts, counter.double_value(), gpu_counter_track_ids_[counter_id]);
      }
    }
  }
}

const StringId GraphicsEventParser::GetFullStageName(
    const protos::pbzero::GpuRenderStageEvent_Decoder& event) const {
  size_t stage_id = static_cast<size_t>(event.stage_id());
  StringId stage_name;

  if (stage_id < gpu_render_stage_ids_.size()) {
    stage_name = gpu_render_stage_ids_[stage_id].first;
  } else {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "render stage(%zu)", stage_id);
    stage_name = context_->storage->InternString(buffer);
  }
  return stage_name;
}

/**
 * Create a GPU render stage track based
 * GpuRenderStageEvent.Specifications.Description.
 */
void GraphicsEventParser::InsertGpuTrack(
    const protos::pbzero::
        GpuRenderStageEvent_Specifications_Description_Decoder& hw_queue) {
  StringId track_name = context_->storage->InternString(hw_queue.name());
  if (gpu_hw_queue_counter_ >= gpu_hw_queue_ids_.size() ||
      !gpu_hw_queue_ids_[gpu_hw_queue_counter_].has_value()) {
    tables::GpuTrackTable::Row track(track_name);
    track.scope = gpu_render_stage_scope_id_;
    track.description = context_->storage->InternString(hw_queue.description());
    if (gpu_hw_queue_counter_ >= gpu_hw_queue_ids_.size()) {
      gpu_hw_queue_ids_.emplace_back(
          context_->track_tracker->InternGpuTrack(track));
    } else {
      // If a gpu_render_stage_event is received before the specification, it is
      // possible that the slot has already been allocated.
      gpu_hw_queue_ids_[gpu_hw_queue_counter_] =
          context_->track_tracker->InternGpuTrack(track);
    }
  } else {
    // If a gpu_render_stage_event is received before the specification, a track
    // will be automatically generated.  In that case, update the name and
    // description.
    auto track_id = gpu_hw_queue_ids_[gpu_hw_queue_counter_];
    if (track_id.has_value()) {
      auto row = context_->storage->mutable_gpu_track_table()
                     ->id()
                     .IndexOf(track_id.value())
                     .value();
      context_->storage->mutable_gpu_track_table()->mutable_name()->Set(
          row, track_name);
      context_->storage->mutable_gpu_track_table()->mutable_description()->Set(
          row, context_->storage->InternString(hw_queue.description()));
    } else {
      tables::GpuTrackTable::Row track(track_name);
      track.scope = gpu_render_stage_scope_id_;
      track.description =
          context_->storage->InternString(hw_queue.description());
    }
  }
  ++gpu_hw_queue_counter_;
}
base::Optional<std::string> GraphicsEventParser::FindDebugName(
    int32_t vk_object_type,
    uint64_t vk_handle) const {
  auto map = debug_marker_names_.find(vk_object_type);
  if (map == debug_marker_names_.end()) {
    return base::nullopt;
  }

  auto name = map->second.find(vk_handle);
  if (name == map->second.end()) {
    return base::nullopt;
  } else {
    return name->second;
  }
}

void GraphicsEventParser::ParseGpuRenderStageEvent(int64_t ts,
                                                   ConstBytes blob) {
  protos::pbzero::GpuRenderStageEvent::Decoder event(blob.data, blob.size);

  if (event.has_specifications()) {
    protos::pbzero::GpuRenderStageEvent_Specifications::Decoder spec(
        event.specifications().data, event.specifications().size);
    for (auto it = spec.hw_queue(); it; ++it) {
      protos::pbzero::GpuRenderStageEvent_Specifications_Description::Decoder
          hw_queue(*it);
      if (hw_queue.has_name()) {
        InsertGpuTrack(hw_queue);
      }
    }
    for (auto it = spec.stage(); it; ++it) {
      protos::pbzero::GpuRenderStageEvent_Specifications_Description::Decoder
          stage(*it);
      if (stage.has_name()) {
        gpu_render_stage_ids_.emplace_back(std::make_pair(
            context_->storage->InternString(stage.name()),
            context_->storage->InternString(stage.description())));
      }
    }
  }

  auto args_callback = [this, &event](ArgsTracker::BoundInserter* inserter) {
    size_t stage_id = static_cast<size_t>(event.stage_id());
    if (stage_id < gpu_render_stage_ids_.size()) {
      auto description = gpu_render_stage_ids_[stage_id].second;
      if (description != kNullStringId) {
        inserter->AddArg(description_id_, Variadic::String(description));
      }
    }
    for (auto it = event.extra_data(); it; ++it) {
      protos::pbzero::GpuRenderStageEvent_ExtraData_Decoder datum(*it);
      StringId name_id = context_->storage->InternString(datum.name());
      StringId value = context_->storage->InternString(
          datum.has_value() ? datum.value() : base::StringView());
      inserter->AddArg(name_id, Variadic::String(value));
    }
  };

  if (event.has_event_id()) {
    TrackId track_id;
    uint32_t hw_queue_id = static_cast<uint32_t>(event.hw_queue_id());
    if (hw_queue_id < gpu_hw_queue_ids_.size() &&
        gpu_hw_queue_ids_[hw_queue_id].has_value()) {
      track_id = gpu_hw_queue_ids_[hw_queue_id].value();
    } else {
      // If the event has a hw_queue_id that does not have a Specification,
      // create a new track for it.
      char buf[128];
      base::StringWriter writer(buf, sizeof(buf));
      writer.AppendLiteral("Unknown GPU Queue ");
      if (hw_queue_id > 1024) {
        // We don't expect this to happen, but just in case there is a corrupt
        // packet, make sure we don't allocate a ridiculous amount of memory.
        hw_queue_id = 1024;
        context_->storage->IncrementStats(
            stats::gpu_render_stage_parser_errors);
        PERFETTO_ELOG("Invalid hw_queue_id.");
      } else {
        writer.AppendInt(event.hw_queue_id());
      }
      StringId track_name =
          context_->storage->InternString(writer.GetStringView());
      tables::GpuTrackTable::Row track(track_name);
      track.scope = gpu_render_stage_scope_id_;
      track_id = context_->track_tracker->InternGpuTrack(track);
      gpu_hw_queue_ids_.resize(hw_queue_id + 1);
      gpu_hw_queue_ids_[hw_queue_id] = track_id;
    }

    auto render_target_name = FindDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, event.render_target_handle());
    auto render_target_name_id = render_target_name.has_value()
                                  ? context_->storage->InternString(
                                      render_target_name.value().c_str())
                                  : kNullStringId;
    auto render_pass_name = FindDebugName(VK_OBJECT_TYPE_RENDER_PASS, event.render_pass_handle());
    auto render_pass_name_id = render_pass_name.has_value()
                                  ? context_->storage->InternString(
                                      render_pass_name.value().c_str())
                                  : kNullStringId;
    auto command_buffer_name = FindDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, event.command_buffer_handle());
    auto command_buffer_name_id = command_buffer_name.has_value()
                                  ? context_->storage->InternString(
                                      command_buffer_name.value().c_str())
                                  : kNullStringId;

    tables::GpuSliceTable::Row row;
    row.ts = ts;
    row.track_id = track_id;
    row.name = GetFullStageName(event);
    row.dur = static_cast<int64_t>(event.duration());
    row.context_id = static_cast<int64_t>(event.context());
    row.render_target = static_cast<int64_t>(event.render_target_handle());
    row.render_target_name = render_target_name_id;
    row.render_pass = static_cast<int64_t>(event.render_pass_handle());
    row.render_pass_name = render_pass_name_id;
    row.command_buffer = static_cast<int64_t>(event.command_buffer_handle());
    row.command_buffer_name = command_buffer_name_id;
    row.submission_id = event.submission_id();
    row.hw_queue_id = hw_queue_id;

    context_->slice_tracker->ScopedGpu(row, args_callback);
  }
}

void GraphicsEventParser::ParseGraphicsFrameEvent(int64_t timestamp,
                                                  ConstBytes blob) {
  using GraphicsFrameEvent = protos::pbzero::GraphicsFrameEvent;
  protos::pbzero::GraphicsFrameEvent_Decoder frame_event(blob.data, blob.size);
  if (!frame_event.has_buffer_event()) {
    return;
  }

  ConstBytes bufferBlob = frame_event.buffer_event();
  protos::pbzero::GraphicsFrameEvent_BufferEvent_Decoder event(bufferBlob.data,
                                                               bufferBlob.size);

  if (!event.has_buffer_id()) {
    context_->storage->IncrementStats(
        stats::graphics_frame_event_parser_errors);
    PERFETTO_ELOG("GraphicsFrameEvent with missing buffer id field.");
    return;
  }

  StringId event_name_id = unknown_event_name_id_;
  if (event.has_type()) {
    const auto type = static_cast<size_t>(event.type());
    if (type < event_type_name_ids_.size()) {
      event_name_id = event_type_name_ids_[type];
      graphics_frame_stats_map_[event.buffer_id()][type] = timestamp;
    } else {
      context_->storage->IncrementStats(
          stats::graphics_frame_event_parser_errors);
      PERFETTO_ELOG("GraphicsFrameEvent with unknown type %zu.", type);
    }
  } else {
    context_->storage->IncrementStats(
        stats::graphics_frame_event_parser_errors);
    PERFETTO_ELOG("GraphicsFrameEvent with missing type field.");
  }

  const uint32_t buffer_id = event.buffer_id();
  StringId layer_name_id;

  char buffer[4096];
  const size_t layerNameMaxLength = 4000;
  base::StringWriter track_name(buffer, sizeof(buffer));
  if (event.has_layer_name()) {
    const base::StringView layer_name(event.layer_name());
    layer_name_id = context_->storage->InternString(layer_name);
    track_name.AppendString(layer_name.substr(0, layerNameMaxLength));
  } else {
    layer_name_id = no_layer_name_name_id_;
    track_name.AppendLiteral("unknown_layer");
  }
  track_name.AppendLiteral("[buffer:");
  track_name.AppendUnsignedInt(buffer_id);
  track_name.AppendChar(']');

  const StringId track_name_id =
      context_->storage->InternString(track_name.GetStringView());
  const int64_t duration =
      event.has_duration_ns() ? static_cast<int64_t>(event.duration_ns()) : 0;
  const uint32_t frame_number =
      event.has_frame_number() ? event.frame_number() : 0;

  tables::GpuTrackTable::Row track(track_name_id);
  track.scope = graphics_event_scope_id_;
  TrackId track_id = context_->track_tracker->InternGpuTrack(track);

  {
    char frame_number_buffer[256];
    base::StringWriter frame_numbers(frame_number_buffer,
                                     base::ArraySize(frame_number_buffer));
    frame_numbers.AppendUnsignedInt(frame_number);

    tables::GraphicsFrameSliceTable::Row row;
    row.ts = timestamp;
    row.track_id = track_id;
    row.name = event_name_id;
    row.dur = duration;
    row.frame_numbers =
        context_->storage->InternString(frame_numbers.GetStringView());
    row.layer_names = layer_name_id;
    context_->slice_tracker->ScopedFrameEvent(row);
  }

  /* Displayed Frame track */
  if (event.type() == GraphicsFrameEvent::PRESENT_FENCE) {
    // Insert the frame stats for the buffer that was presented
    auto acquire_ts =
        graphics_frame_stats_map_[event.buffer_id()]
                                 [GraphicsFrameEvent::ACQUIRE_FENCE];
    auto queue_ts =
        graphics_frame_stats_map_[event.buffer_id()][GraphicsFrameEvent::QUEUE];
    auto latch_ts =
        graphics_frame_stats_map_[event.buffer_id()][GraphicsFrameEvent::LATCH];
    tables::GraphicsFrameStatsTable::Row stats_row;
    // AcquireFence can signal before Queue sometimes, so have 0 as a bound.
    stats_row.queue_to_acquire_time =
        std::max(acquire_ts - queue_ts, static_cast<int64_t>(0));
    stats_row.acquire_to_latch_time = latch_ts - acquire_ts;
    stats_row.latch_to_present_time = timestamp - latch_ts;
    auto stats_row_id =
        context_->storage->mutable_graphics_frame_stats_table()->Insert(
            stats_row);

    if (previous_timestamp_ == 0) {
      const StringId present_track_name_id =
          context_->storage->InternString("Displayed Frame");
      tables::GpuTrackTable::Row present_track(present_track_name_id);
      present_track.scope = graphics_event_scope_id_;
      present_track_id_ =
          context_->track_tracker->InternGpuTrack(present_track);
    }

    // The displayed frame is a slice from one present fence to another present
    // fence. If multiple buffers have present fence at the same time, they all
    // are shown on screen at the same time. So that particular displayed frame
    // slice should include info from all those buffers in it.
    // Since the events are parsed one by one, the following bookkeeping is
    // needed to create the slice properly.
    if (previous_timestamp_ == timestamp && previous_timestamp_ != 0) {
      // Same timestamp as previous present fence. This buffer should also
      // contribute to this slice.
      present_frame_name_.AppendLiteral(", ");
      present_frame_name_.AppendUnsignedInt(buffer_id);

      // Append Layer names
      present_frame_layer_name_.AppendLiteral(", ");
      present_frame_layer_name_.AppendString(event.layer_name());

      // Append Frame numbers
      present_frame_numbers_.AppendLiteral(", ");
      present_frame_numbers_.AppendUnsignedInt(frame_number);

      // Add the current stats row to the list of stats that go with this frame
      graphics_frame_stats_idx_.push_back(stats_row_id.row);
    } else {

      if (previous_timestamp_ != 0) {
        StringId present_frame_layer_name_id = context_->storage->InternString(
            present_frame_layer_name_.GetStringView());
        // End the current slice that's being tracked.
        const auto opt_slice_id = context_->slice_tracker->EndFrameEvent(
            timestamp, present_track_id_);

        if (opt_slice_id) {
          // The slice could have had additional buffers in it, so we need to
          // update the table.
          auto* graphics_frame_slice_table =
              context_->storage->mutable_graphics_frame_slice_table();

          uint32_t row_idx =
              *graphics_frame_slice_table->id().IndexOf(*opt_slice_id);
          StringId frame_name_id = context_->storage->InternString(
              present_frame_name_.GetStringView());
          graphics_frame_slice_table->mutable_name()->Set(row_idx,
                                                          frame_name_id);

          StringId present_frame_numbers_id = context_->storage->InternString(
              present_frame_numbers_.GetStringView());
          graphics_frame_slice_table->mutable_frame_numbers()->Set(
              row_idx, present_frame_numbers_id);
          graphics_frame_slice_table->mutable_layer_names()->Set(
              row_idx, present_frame_layer_name_id);

          // Set the slice_id for the frame_stats rows under this displayed
          // frame
          auto* slice_table = context_->storage->mutable_slice_table();
          uint32_t slice_idx = *slice_table->id().IndexOf(*opt_slice_id);
          for (uint32_t i = 0; i < graphics_frame_stats_idx_.size(); i++) {
            context_->storage->mutable_graphics_frame_stats_table()
                ->mutable_slice_id()
                ->Set(graphics_frame_stats_idx_[i], slice_idx);
          }
        }
        present_frame_layer_name_.reset();
        present_frame_name_.reset();
        present_frame_numbers_.reset();
        graphics_frame_stats_idx_.clear();
      }

      // Start a new slice
      present_frame_name_.AppendUnsignedInt(buffer_id);
      previous_timestamp_ = timestamp;
      present_frame_layer_name_.AppendString(event.layer_name());
      present_frame_numbers_.AppendUnsignedInt(frame_number);
      present_event_name_id_ =
          context_->storage->InternString(present_frame_name_.GetStringView());
      graphics_frame_stats_idx_.push_back(stats_row_id.row);

      tables::GraphicsFrameSliceTable::Row row;
      row.ts = timestamp;
      row.track_id = present_track_id_;
      row.name = present_event_name_id_;
      context_->slice_tracker->BeginFrameEvent(row);
    }
  }
}

void GraphicsEventParser::UpdateVulkanMemoryAllocationCounters(
    UniquePid upid,
    const VulkanMemoryEvent::Decoder& event) {
  StringId track_str_id = kNullStringId;
  TrackId track = kInvalidTrackId;
  auto allocation_scope = VulkanMemoryEvent::SCOPE_UNSPECIFIED;
  uint32_t memory_type = std::numeric_limits<uint32_t>::max();
  switch (event.source()) {
    case VulkanMemoryEvent::SOURCE_DRIVER:
      allocation_scope = static_cast<VulkanMemoryEvent::AllocationScope>(
          event.allocation_scope());
      if (allocation_scope == VulkanMemoryEvent::SCOPE_UNSPECIFIED)
        return;
      switch (event.operation()) {
        case VulkanMemoryEvent::OP_CREATE:
          vulkan_driver_memory_counters_[allocation_scope] +=
              event.memory_size();
          break;
        case VulkanMemoryEvent::OP_DESTROY:
          vulkan_driver_memory_counters_[allocation_scope] -=
              event.memory_size();
          break;
        case VulkanMemoryEvent::OP_UNSPECIFIED:
        case VulkanMemoryEvent::OP_BIND:
        case VulkanMemoryEvent::OP_DESTROY_BOUND:
        case VulkanMemoryEvent::OP_ANNOTATIONS:
          return;
      }
      track_str_id = vulkan_memory_tracker_.FindAllocationScopeCounterString(
          allocation_scope);
      track = context_->track_tracker->InternProcessCounterTrack(track_str_id,
                                                                 upid);
      context_->event_tracker->PushCounter(
          event.timestamp(), vulkan_driver_memory_counters_[allocation_scope],
          track);
      break;

    case VulkanMemoryEvent::SOURCE_DEVICE_MEMORY:
      memory_type = static_cast<uint32_t>(event.memory_type());
      switch (event.operation()) {
        case VulkanMemoryEvent::OP_CREATE:
          vulkan_device_memory_counters_allocate_[memory_type] +=
              event.memory_size();
          break;
        case VulkanMemoryEvent::OP_DESTROY:
          vulkan_device_memory_counters_allocate_[memory_type] -=
              event.memory_size();
          break;
        case VulkanMemoryEvent::OP_UNSPECIFIED:
        case VulkanMemoryEvent::OP_BIND:
        case VulkanMemoryEvent::OP_DESTROY_BOUND:
        case VulkanMemoryEvent::OP_ANNOTATIONS:
          return;
      }
      track_str_id = vulkan_memory_tracker_.FindMemoryTypeCounterString(
          memory_type,
          VulkanMemoryTracker::DeviceCounterType::kAllocationCounter);
      track = context_->track_tracker->InternProcessCounterTrack(track_str_id,
                                                                 upid);
      context_->event_tracker->PushCounter(
          event.timestamp(),
          vulkan_device_memory_counters_allocate_[memory_type], track);
      break;

    case VulkanMemoryEvent::SOURCE_BUFFER:
    case VulkanMemoryEvent::SOURCE_IMAGE:
      memory_type = static_cast<uint32_t>(event.memory_type());
      switch (event.operation()) {
        case VulkanMemoryEvent::OP_BIND:
          vulkan_device_memory_counters_bind_[memory_type] +=
              event.memory_size();
          break;
        case VulkanMemoryEvent::OP_DESTROY_BOUND:
          vulkan_device_memory_counters_bind_[memory_type] -=
              event.memory_size();
          break;
        case VulkanMemoryEvent::OP_UNSPECIFIED:
        case VulkanMemoryEvent::OP_CREATE:
        case VulkanMemoryEvent::OP_DESTROY:
        case VulkanMemoryEvent::OP_ANNOTATIONS:
          return;
      }
      track_str_id = vulkan_memory_tracker_.FindMemoryTypeCounterString(
          memory_type, VulkanMemoryTracker::DeviceCounterType::kBindCounter);
      track = context_->track_tracker->InternProcessCounterTrack(track_str_id,
                                                                 upid);
      context_->event_tracker->PushCounter(
          event.timestamp(), vulkan_device_memory_counters_bind_[memory_type],
          track);
      break;
    case VulkanMemoryEvent::SOURCE_UNSPECIFIED:
    case VulkanMemoryEvent::SOURCE_DEVICE:
      return;
  }
}

void GraphicsEventParser::ParseVulkanMemoryEvent(
    PacketSequenceStateGeneration* sequence_state,
    ConstBytes blob) {
  using protos::pbzero::InternedData;
  VulkanMemoryEvent::Decoder vulkan_memory_event(blob.data, blob.size);
  tables::VulkanMemoryAllocationsTable::Row vulkan_memory_event_row;
  vulkan_memory_event_row.source = vulkan_memory_tracker_.FindSourceString(
      static_cast<VulkanMemoryEvent::Source>(vulkan_memory_event.source()));
  vulkan_memory_event_row.operation =
      vulkan_memory_tracker_.FindOperationString(
          static_cast<VulkanMemoryEvent::Operation>(
              vulkan_memory_event.operation()));
  vulkan_memory_event_row.timestamp = vulkan_memory_event.timestamp();
  vulkan_memory_event_row.upid =
      context_->process_tracker->GetOrCreateProcess(vulkan_memory_event.pid());
  if (vulkan_memory_event.has_device())
    vulkan_memory_event_row.device =
        static_cast<int64_t>(vulkan_memory_event.device());
  if (vulkan_memory_event.has_device_memory())
    vulkan_memory_event_row.device_memory =
        static_cast<int64_t>(vulkan_memory_event.device_memory());
  if (vulkan_memory_event.has_heap())
    vulkan_memory_event_row.heap = vulkan_memory_event.heap();
  if (vulkan_memory_event.has_memory_type())
    vulkan_memory_event_row.memory_type = vulkan_memory_event.memory_type();
  if (vulkan_memory_event.has_caller_iid()) {
    vulkan_memory_event_row.function_name =
        vulkan_memory_tracker_
            .GetInternedString<InternedData::kFunctionNamesFieldNumber>(
                sequence_state,
                static_cast<uint64_t>(vulkan_memory_event.caller_iid()));
  }
  if (vulkan_memory_event.has_object_handle())
    vulkan_memory_event_row.object_handle =
        static_cast<int64_t>(vulkan_memory_event.object_handle());
  if (vulkan_memory_event.has_memory_address())
    vulkan_memory_event_row.memory_address =
        static_cast<int64_t>(vulkan_memory_event.memory_address());
  if (vulkan_memory_event.has_memory_size())
    vulkan_memory_event_row.memory_size =
        static_cast<int64_t>(vulkan_memory_event.memory_size());
  if (vulkan_memory_event.has_allocation_scope())
    vulkan_memory_event_row.scope =
        vulkan_memory_tracker_.FindAllocationScopeString(
            static_cast<VulkanMemoryEvent::AllocationScope>(
                vulkan_memory_event.allocation_scope()));

  UpdateVulkanMemoryAllocationCounters(vulkan_memory_event_row.upid.value(),
                                       vulkan_memory_event);

  auto* allocs = context_->storage->mutable_vulkan_memory_allocations_table();
  VulkanAllocId id = allocs->Insert(vulkan_memory_event_row).id;

  if (vulkan_memory_event.has_annotations()) {
    auto inserter = context_->args_tracker->AddArgsTo(id);

    for (auto it = vulkan_memory_event.annotations(); it; ++it) {
      protos::pbzero::VulkanMemoryEventAnnotation::Decoder annotation(*it);

      auto key_id =
          vulkan_memory_tracker_
              .GetInternedString<InternedData::kVulkanMemoryKeysFieldNumber>(
                  sequence_state, static_cast<uint64_t>(annotation.key_iid()));

      if (annotation.has_int_value()) {
        inserter.AddArg(key_id, Variadic::Integer(annotation.int_value()));
      } else if (annotation.has_double_value()) {
        inserter.AddArg(key_id, Variadic::Real(annotation.double_value()));
      } else if (annotation.has_string_iid()) {
        auto string_id =
            vulkan_memory_tracker_
                .GetInternedString<InternedData::kVulkanMemoryKeysFieldNumber>(
                    sequence_state,
                    static_cast<uint64_t>(annotation.string_iid()));

        inserter.AddArg(key_id, Variadic::String(string_id));
      }
    }
  }
}

void GraphicsEventParser::ParseGpuLog(int64_t ts, ConstBytes blob) {
  protos::pbzero::GpuLog::Decoder event(blob.data, blob.size);

  tables::GpuTrackTable::Row track(gpu_log_track_name_id_);
  track.scope = gpu_log_scope_id_;
  TrackId track_id = context_->track_tracker->InternGpuTrack(track);

  auto args_callback = [this, &event](ArgsTracker::BoundInserter* inserter) {
    if (event.has_tag()) {
      inserter->AddArg(
          tag_id_,
          Variadic::String(context_->storage->InternString(event.tag())));
    }
    if (event.has_log_message()) {
      inserter->AddArg(log_message_id_,
                       Variadic::String(context_->storage->InternString(
                           event.log_message())));
    }
  };

  auto severity = static_cast<size_t>(event.severity());
  StringId severity_id =
      severity < log_severity_ids_.size()
          ? log_severity_ids_[static_cast<size_t>(event.severity())]
          : log_severity_ids_[log_severity_ids_.size() - 1];

  tables::GpuSliceTable::Row row;
  row.ts = ts;
  row.track_id = track_id;
  row.name = severity_id;
  row.dur = 0;
  context_->slice_tracker->ScopedGpu(row, args_callback);
}

void GraphicsEventParser::ParseVulkanApiEvent(int64_t ts, ConstBytes blob) {
  protos::pbzero::VulkanApiEvent::Decoder vk_event(blob.data, blob.size);
  if (vk_event.has_vk_debug_utils_object_name()) {
    protos::pbzero::VulkanApiEvent_VkDebugUtilsObjectName::Decoder event(
        vk_event.vk_debug_utils_object_name());
    debug_marker_names_[event.object_type()][event.object()] =
        event.object_name().ToStdString();
  }
  if (vk_event.has_vk_queue_submit()) {
    protos::pbzero::VulkanApiEvent_VkQueueSubmit::Decoder event(
        vk_event.vk_queue_submit());
    // Once flow table is implemented, we can create a nice UI that link the
    // vkQueueSubmit to GpuRenderStageEvent.  For now, just add it as in a GPU
    // track so that they can appear close to the render stage slices.
    tables::GpuTrackTable::Row track(vk_event_track_id_);
    track.scope = vk_event_scope_id_;
    TrackId track_id = context_->track_tracker->InternGpuTrack(track);
    tables::GpuSliceTable::Row row;
    row.ts = ts;
    row.dur = static_cast<int64_t>(event.duration_ns());
    row.track_id = track_id;
    row.name = vk_queue_submit_id_;
    if (event.has_vk_command_buffers()) {
      row.command_buffer = static_cast<int64_t>(*event.vk_command_buffers());
    }
    row.submission_id = event.submission_id();
    auto args_callback = [this, &event](ArgsTracker::BoundInserter* inserter) {
      inserter->AddArg(context_->storage->InternString("pid"),
                       Variadic::Integer(event.pid()));
      inserter->AddArg(context_->storage->InternString("tid"),
                       Variadic::Integer(event.tid()));
    };
    context_->slice_tracker->ScopedGpu(row, args_callback);
  }
}

}  // namespace trace_processor
}  // namespace perfetto
