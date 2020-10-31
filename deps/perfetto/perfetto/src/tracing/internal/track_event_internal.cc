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

#include "perfetto/tracing/internal/track_event_internal.h"

#include "perfetto/base/proc_utils.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/track_event.h"
#include "perfetto/tracing/track_event_category_registry.h"
#include "perfetto/tracing/track_event_interned_data_index.h"
#include "protos/perfetto/common/data_source_descriptor.gen.h"
#include "protos/perfetto/common/track_event_descriptor.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

namespace perfetto {
namespace internal {

BaseTrackEventInternedDataIndex::~BaseTrackEventInternedDataIndex() = default;

namespace {

std::atomic<perfetto::base::PlatformThreadId> g_main_thread;
static constexpr const char kLegacySlowPrefix[] = "disabled-by-default-";
static constexpr const char kSlowTag[] = "slow";
static constexpr const char kDebugTag[] = "debug";

struct InternedEventCategory
    : public TrackEventInternedDataIndex<
          InternedEventCategory,
          perfetto::protos::pbzero::InternedData::kEventCategoriesFieldNumber,
          const char*,
          SmallInternedDataTraits> {
  static void Add(protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const char* value,
                  size_t length) {
    auto category = interned_data->add_event_categories();
    category->set_iid(iid);
    category->set_name(value, length);
  }
};

struct InternedEventName
    : public TrackEventInternedDataIndex<
          InternedEventName,
          perfetto::protos::pbzero::InternedData::kEventNamesFieldNumber,
          const char*,
          SmallInternedDataTraits> {
  static void Add(protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const char* value) {
    auto name = interned_data->add_event_names();
    name->set_iid(iid);
    name->set_name(value);
  }
};

struct InternedDebugAnnotationName
    : public TrackEventInternedDataIndex<
          InternedDebugAnnotationName,
          perfetto::protos::pbzero::InternedData::
              kDebugAnnotationNamesFieldNumber,
          const char*,
          SmallInternedDataTraits> {
  static void Add(protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const char* value) {
    auto name = interned_data->add_debug_annotation_names();
    name->set_iid(iid);
    name->set_name(value);
  }
};

enum class MatchType { kExact, kPattern };

bool NameMatchesPattern(const std::string& pattern,
                        const std::string& name,
                        MatchType match_type) {
  // To avoid pulling in all of std::regex, for now we only support a single "*"
  // wildcard at the end of the pattern.
  size_t i = pattern.find('*');
  if (i != std::string::npos) {
    PERFETTO_DCHECK(i == pattern.size() - 1);
    if (match_type != MatchType::kPattern)
      return false;
    return name.substr(0, i) == pattern.substr(0, i);
  }
  return name == pattern;
}

bool NameMatchesPatternList(const std::vector<std::string>& patterns,
                            const std::string& name,
                            MatchType match_type) {
  for (const auto& pattern : patterns) {
    if (NameMatchesPattern(pattern, name, match_type))
      return true;
  }
  return false;
}

}  // namespace

// static
bool TrackEventInternal::Initialize(
    const TrackEventCategoryRegistry& registry,
    bool (*register_data_source)(const DataSourceDescriptor&)) {
  if (!g_main_thread)
    g_main_thread = perfetto::base::GetThreadId();

  DataSourceDescriptor dsd;
  dsd.set_name("track_event");

  protozero::HeapBuffered<protos::pbzero::TrackEventDescriptor> ted;
  for (size_t i = 0; i < registry.category_count(); i++) {
    auto category = registry.GetCategory(i);
    // Don't register group categories.
    if (category->IsGroup())
      continue;
    auto cat = ted->add_available_categories();
    cat->set_name(category->name);
    if (category->description)
      cat->set_description(category->description);
    for (const auto& tag : category->tags) {
      if (tag)
        cat->add_tags(tag);
    }
    // Disabled-by-default categories get a "slow" tag.
    if (!strncmp(category->name, kLegacySlowPrefix, strlen(kLegacySlowPrefix)))
      cat->add_tags(kSlowTag);
  }
  dsd.set_track_event_descriptor_raw(ted.SerializeAsString());

  return register_data_source(dsd);
}

// static
void TrackEventInternal::EnableTracing(
    const TrackEventCategoryRegistry& registry,
    const protos::gen::TrackEventConfig& config,
    uint32_t instance_index) {
  for (size_t i = 0; i < registry.category_count(); i++) {
    if (IsCategoryEnabled(registry, config, *registry.GetCategory(i)))
      registry.EnableCategoryForInstance(i, instance_index);
  }
}

// static
void TrackEventInternal::DisableTracing(
    const TrackEventCategoryRegistry& registry,
    uint32_t instance_index) {
  for (size_t i = 0; i < registry.category_count(); i++)
    registry.DisableCategoryForInstance(i, instance_index);
}

// static
bool TrackEventInternal::IsCategoryEnabled(
    const TrackEventCategoryRegistry& registry,
    const protos::gen::TrackEventConfig& config,
    const Category& category) {
  // If this is a group category, check if any of its constituent categories are
  // enabled. If so, then this one is enabled too.
  if (category.IsGroup()) {
    bool result = false;
    category.ForEachGroupMember([&](const char* member_name, size_t name_size) {
      for (size_t i = 0; i < registry.category_count(); i++) {
        const auto ref_category = registry.GetCategory(i);
        // Groups can't refer to other groups.
        if (ref_category->IsGroup())
          continue;
        // Require an exact match.
        if (ref_category->name_size() != name_size ||
            strncmp(ref_category->name, member_name, name_size)) {
          continue;
        }
        if (IsCategoryEnabled(registry, config, *ref_category)) {
          result = true;
          // Break ForEachGroupMember() loop.
          return false;
        }
        break;
      }
      // No match found => keep iterating.
      return true;
    });
    return result;
  }

  auto has_matching_tag = [&](std::function<bool(const char*)> matcher) {
    for (const auto& tag : category.tags) {
      if (!tag)
        break;
      if (matcher(tag))
        return true;
    }
    // Legacy "disabled-by-default" categories automatically get the "slow" tag.
    if (!strncmp(category.name, kLegacySlowPrefix, strlen(kLegacySlowPrefix)) &&
        matcher(kSlowTag)) {
      return true;
    }
    return false;
  };

  // First try exact matches, then pattern matches.
  const std::array<MatchType, 2> match_types = {
      {MatchType::kExact, MatchType::kPattern}};
  for (auto match_type : match_types) {
    // 1. Enabled categories.
    if (NameMatchesPatternList(config.enabled_categories(), category.name,
                               match_type)) {
      return true;
    }

    // 2. Enabled tags.
    if (has_matching_tag([&](const char* tag) {
          return NameMatchesPatternList(config.enabled_tags(), tag, match_type);
        })) {
      return true;
    }

    // 3. Disabled categories.
    if (NameMatchesPatternList(config.disabled_categories(), category.name,
                               match_type)) {
      return false;
    }

    // 4. Disabled tags.
    if (has_matching_tag([&](const char* tag) {
          if (config.disabled_tags_size()) {
            return NameMatchesPatternList(config.disabled_tags(), tag,
                                          match_type);
          } else {
            // The "slow" and "debug" tags are disabled by default.
            return NameMatchesPattern(kSlowTag, tag, match_type) ||
                   NameMatchesPattern(kDebugTag, tag, match_type);
          }
        })) {
      return false;
    }
  }

  // If nothing matched, enable the category by default.
  return true;
}

// static
uint64_t TrackEventInternal::GetTimeNs() {
  if (GetClockId() == protos::pbzero::ClockSnapshot::Clock::BOOTTIME)
    return static_cast<uint64_t>(perfetto::base::GetBootTimeNs().count());
  PERFETTO_DCHECK(GetClockId() ==
                  protos::pbzero::ClockSnapshot::Clock::MONOTONIC);
  return static_cast<uint64_t>(perfetto::base::GetWallTimeNs().count());
}

// static
void TrackEventInternal::ResetIncrementalState(TraceWriterBase* trace_writer,
                                               uint64_t timestamp) {
  auto default_track = ThreadTrack::Current();
  {
    // Mark any incremental state before this point invalid. Also set up
    // defaults so that we don't need to repeat constant data for each packet.
    auto packet = NewTracePacket(
        trace_writer, timestamp,
        protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);
    auto defaults = packet->set_trace_packet_defaults();
    defaults->set_timestamp_clock_id(GetClockId());

    // Establish the default track for this event sequence.
    auto track_defaults = defaults->set_track_event_defaults();
    track_defaults->set_track_uuid(default_track.uuid);
  }

  // Every thread should write a descriptor for its default track, because most
  // trace points won't explicitly reference it.
  WriteTrackDescriptor(default_track, trace_writer);

  // Additionally the main thread should dump the process descriptor.
  if (perfetto::base::GetThreadId() == g_main_thread)
    WriteTrackDescriptor(ProcessTrack::Current(), trace_writer);
}

// static
protozero::MessageHandle<protos::pbzero::TracePacket>
TrackEventInternal::NewTracePacket(TraceWriterBase* trace_writer,
                                   uint64_t timestamp,
                                   uint32_t seq_flags) {
  auto packet = trace_writer->NewTracePacket();
  packet->set_timestamp(timestamp);
  // TODO(skyostil): Stop emitting this for every event once the trace
  // processor understands trace packet defaults.
  if (GetClockId() != protos::pbzero::ClockSnapshot::Clock::BOOTTIME)
    packet->set_timestamp_clock_id(GetClockId());
  packet->set_sequence_flags(seq_flags);
  return packet;
}

// static
EventContext TrackEventInternal::WriteEvent(
    TraceWriterBase* trace_writer,
    TrackEventIncrementalState* incr_state,
    const Category* category,
    const char* name,
    perfetto::protos::pbzero::TrackEvent::Type type,
    uint64_t timestamp) {
  PERFETTO_DCHECK(g_main_thread);

  if (incr_state->was_cleared) {
    incr_state->was_cleared = false;
    ResetIncrementalState(trace_writer, timestamp);
  }
  auto packet = NewTracePacket(trace_writer, timestamp);
  EventContext ctx(std::move(packet), incr_state);

  auto track_event = ctx.event();
  if (type != protos::pbzero::TrackEvent::TYPE_UNSPECIFIED)
    track_event->set_type(type);

  // We assume that |category| and |name| point to strings with static lifetime.
  // This means we can use their addresses as interning keys.
  if (category && type != protos::pbzero::TrackEvent::TYPE_SLICE_END) {
    category->ForEachGroupMember(
        [&](const char* member_name, size_t name_size) {
          size_t category_iid =
              InternedEventCategory::Get(&ctx, member_name, name_size);
          track_event->add_category_iids(category_iid);
          return true;
        });
  }
  if (name) {
    size_t name_iid = InternedEventName::Get(&ctx, name);
    track_event->set_name_iid(name_iid);
  }
  return ctx;
}

// static
protos::pbzero::DebugAnnotation* TrackEventInternal::AddDebugAnnotation(
    perfetto::EventContext* event_ctx,
    const char* name) {
  auto annotation = event_ctx->event()->add_debug_annotations();
  annotation->set_name_iid(InternedDebugAnnotationName::Get(event_ctx, name));
  return annotation;
}

}  // namespace internal
}  // namespace perfetto
