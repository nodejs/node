/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/proto_trace_parser.h"

#include <inttypes.h>
#include <string.h>

#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/metatrace_events.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/string_writer.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/base/uuid.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/ftrace/ftrace_module.h"
#include "src/trace_processor/importers/proto/heap_profile_tracker.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/perf_sample_tracker.h"
#include "src/trace_processor/importers/proto/profile_packet_utils.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/types/variadic.h"

#include "protos/perfetto/common/trace_stats.pbzero.h"
#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/chrome/chrome_benchmark_metadata.pbzero.h"
#include "protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/perfetto/perfetto_metatrace.pbzero.h"
#include "protos/perfetto/trace/perfetto/tracing_service_event.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "protos/perfetto/trace/profiling/smaps.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/trigger.pbzero.h"

namespace perfetto {
namespace trace_processor {

ProtoTraceParser::ProtoTraceParser(TraceProcessorContext* context)
    : context_(context),
      metatrace_id_(context->storage->InternString("metatrace")),
      data_name_id_(context->storage->InternString("data")),
      raw_chrome_metadata_event_id_(
          context->storage->InternString("chrome_event.metadata")),
      raw_chrome_legacy_system_trace_event_id_(
          context->storage->InternString("chrome_event.legacy_system_trace")),
      raw_chrome_legacy_user_trace_event_id_(
          context->storage->InternString("chrome_event.legacy_user_trace")) {
  // TODO(140860736): Once we support null values for
  // stack_profile_frame.symbol_set_id remove this hack
  context_->storage->mutable_symbol_table()->Insert(
      {0, kNullStringId, kNullStringId, 0});
}

ProtoTraceParser::~ProtoTraceParser() = default;

void ProtoTraceParser::ParseTracePacket(int64_t ts, TimestampedTracePiece ttp) {
  const TracePacketData* data = nullptr;
  if (ttp.type == TimestampedTracePiece::Type::kTracePacket) {
    data = &ttp.packet_data;
  } else {
    PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTrackEvent);
    data = ttp.track_event_data.get();
  }

  const TraceBlobView& blob = data->packet;
  protos::pbzero::TracePacket::Decoder packet(blob.data(), blob.length());

  ParseTracePacketImpl(ts, std::move(ttp), data, packet);

  // TODO(lalitm): maybe move this to the flush method in the trace processor
  // once we have it. This may reduce performance in the ArgsTracker though so
  // needs to be handled carefully.
  context_->args_tracker->Flush();
  PERFETTO_DCHECK(!packet.bytes_left());
}

void ProtoTraceParser::ParseTracePacketImpl(
    int64_t ts,
    TimestampedTracePiece ttp,
    const TracePacketData* data,
    const protos::pbzero::TracePacket::Decoder& packet) {
  // TODO(eseckler): Propagate statuses from modules.
  auto& modules = context_->modules_by_field;
  for (uint32_t field_id = 1; field_id < modules.size(); ++field_id) {
    if (modules[field_id] && packet.Get(field_id).valid()) {
      modules[field_id]->ParsePacket(packet, ttp, field_id);
      return;
    }
  }

  if (packet.has_trace_stats())
    ParseTraceStats(packet.trace_stats());

  if (packet.has_profile_packet()) {
    ParseProfilePacket(ts, data->sequence_state,
                       packet.trusted_packet_sequence_id(),
                       packet.profile_packet());
  }

  if (packet.has_perf_sample()) {
    ParsePerfSample(ts, data->sequence_state, packet.perf_sample());
  }

  if (packet.has_chrome_benchmark_metadata()) {
    ParseChromeBenchmarkMetadata(packet.chrome_benchmark_metadata());
  }

  if (packet.has_chrome_events()) {
    ParseChromeEvents(ts, packet.chrome_events());
  }

  if (packet.has_perfetto_metatrace()) {
    ParseMetatraceEvent(ts, packet.perfetto_metatrace());
  }

  if (packet.has_trace_config()) {
    ParseTraceConfig(packet.trace_config());
  }

  if (packet.has_module_symbols()) {
    ParseModuleSymbols(packet.module_symbols());
  }

  if (packet.has_trigger()) {
    ParseTrigger(ts, packet.trigger());
  }

  if (packet.has_service_event()) {
    ParseServiceEvent(ts, packet.service_event());
  }

  if (packet.has_smaps_packet()) {
    ParseSmapsPacket(ts, packet.smaps_packet());
  }
}

void ProtoTraceParser::ParseFtracePacket(uint32_t cpu,
                                         int64_t /*ts*/,
                                         TimestampedTracePiece ttp) {
  PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kFtraceEvent ||
                  ttp.type == TimestampedTracePiece::Type::kInlineSchedSwitch ||
                  ttp.type == TimestampedTracePiece::Type::kInlineSchedWaking);
  PERFETTO_DCHECK(context_->ftrace_module);
  context_->ftrace_module->ParseFtracePacket(cpu, ttp);

  // TODO(lalitm): maybe move this to the flush method in the trace processor
  // once we have it. This may reduce performance in the ArgsTracker though so
  // needs to be handled carefully.
  context_->args_tracker->Flush();
}

void ProtoTraceParser::ParseTraceStats(ConstBytes blob) {
  protos::pbzero::TraceStats::Decoder evt(blob.data, blob.size);
  auto* storage = context_->storage.get();
  storage->SetStats(stats::traced_producers_connected,
                    static_cast<int64_t>(evt.producers_connected()));
  storage->SetStats(stats::traced_data_sources_registered,
                    static_cast<int64_t>(evt.data_sources_registered()));
  storage->SetStats(stats::traced_data_sources_seen,
                    static_cast<int64_t>(evt.data_sources_seen()));
  storage->SetStats(stats::traced_tracing_sessions,
                    static_cast<int64_t>(evt.tracing_sessions()));
  storage->SetStats(stats::traced_total_buffers,
                    static_cast<int64_t>(evt.total_buffers()));
  storage->SetStats(stats::traced_chunks_discarded,
                    static_cast<int64_t>(evt.chunks_discarded()));
  storage->SetStats(stats::traced_patches_discarded,
                    static_cast<int64_t>(evt.patches_discarded()));

  int buf_num = 0;
  for (auto it = evt.buffer_stats(); it; ++it, ++buf_num) {
    protos::pbzero::TraceStats::BufferStats::Decoder buf(*it);
    storage->SetIndexedStats(stats::traced_buf_buffer_size, buf_num,
                             static_cast<int64_t>(buf.buffer_size()));
    storage->SetIndexedStats(stats::traced_buf_bytes_written, buf_num,
                             static_cast<int64_t>(buf.bytes_written()));
    storage->SetIndexedStats(stats::traced_buf_bytes_overwritten, buf_num,
                             static_cast<int64_t>(buf.bytes_overwritten()));
    storage->SetIndexedStats(stats::traced_buf_bytes_read, buf_num,
                             static_cast<int64_t>(buf.bytes_read()));
    storage->SetIndexedStats(stats::traced_buf_padding_bytes_written, buf_num,
                             static_cast<int64_t>(buf.padding_bytes_written()));
    storage->SetIndexedStats(stats::traced_buf_padding_bytes_cleared, buf_num,
                             static_cast<int64_t>(buf.padding_bytes_cleared()));
    storage->SetIndexedStats(stats::traced_buf_chunks_written, buf_num,
                             static_cast<int64_t>(buf.chunks_written()));
    storage->SetIndexedStats(stats::traced_buf_chunks_rewritten, buf_num,
                             static_cast<int64_t>(buf.chunks_rewritten()));
    storage->SetIndexedStats(stats::traced_buf_chunks_overwritten, buf_num,
                             static_cast<int64_t>(buf.chunks_overwritten()));
    storage->SetIndexedStats(stats::traced_buf_chunks_discarded, buf_num,
                             static_cast<int64_t>(buf.chunks_discarded()));
    storage->SetIndexedStats(stats::traced_buf_chunks_read, buf_num,
                             static_cast<int64_t>(buf.chunks_read()));
    storage->SetIndexedStats(
        stats::traced_buf_chunks_committed_out_of_order, buf_num,
        static_cast<int64_t>(buf.chunks_committed_out_of_order()));
    storage->SetIndexedStats(stats::traced_buf_write_wrap_count, buf_num,
                             static_cast<int64_t>(buf.write_wrap_count()));
    storage->SetIndexedStats(stats::traced_buf_patches_succeeded, buf_num,
                             static_cast<int64_t>(buf.patches_succeeded()));
    storage->SetIndexedStats(stats::traced_buf_patches_failed, buf_num,
                             static_cast<int64_t>(buf.patches_failed()));
    storage->SetIndexedStats(stats::traced_buf_readaheads_succeeded, buf_num,
                             static_cast<int64_t>(buf.readaheads_succeeded()));
    storage->SetIndexedStats(stats::traced_buf_readaheads_failed, buf_num,
                             static_cast<int64_t>(buf.readaheads_failed()));
    storage->SetIndexedStats(
        stats::traced_buf_trace_writer_packet_loss, buf_num,
        static_cast<int64_t>(buf.trace_writer_packet_loss()));
  }
}

void ProtoTraceParser::ParseProfilePacket(
    int64_t,
    PacketSequenceStateGeneration* sequence_state,
    uint32_t seq_id,
    ConstBytes blob) {
  protos::pbzero::ProfilePacket::Decoder packet(blob.data, blob.size);
  context_->heap_profile_tracker->SetProfilePacketIndex(seq_id, packet.index());

  for (auto it = packet.strings(); it; ++it) {
    protos::pbzero::InternedString::Decoder entry(*it);

    const char* str = reinterpret_cast<const char*>(entry.str().data);
    auto str_view = base::StringView(str, entry.str().size);
    sequence_state->state()->stack_profile_tracker().AddString(entry.iid(),
                                                               str_view);
  }

  for (auto it = packet.mappings(); it; ++it) {
    protos::pbzero::Mapping::Decoder entry(*it);
    StackProfileTracker::SourceMapping src_mapping =
        ProfilePacketUtils::MakeSourceMapping(entry);
    sequence_state->state()->stack_profile_tracker().AddMapping(entry.iid(),
                                                                src_mapping);
  }

  for (auto it = packet.frames(); it; ++it) {
    protos::pbzero::Frame::Decoder entry(*it);
    StackProfileTracker::SourceFrame src_frame =
        ProfilePacketUtils::MakeSourceFrame(entry);
    sequence_state->state()->stack_profile_tracker().AddFrame(entry.iid(),
                                                              src_frame);
  }

  for (auto it = packet.callstacks(); it; ++it) {
    protos::pbzero::Callstack::Decoder entry(*it);
    StackProfileTracker::SourceCallstack src_callstack =
        ProfilePacketUtils::MakeSourceCallstack(entry);
    sequence_state->state()->stack_profile_tracker().AddCallstack(
        entry.iid(), src_callstack);
  }

  for (auto it = packet.process_dumps(); it; ++it) {
    protos::pbzero::ProfilePacket::ProcessHeapSamples::Decoder entry(*it);

    auto maybe_timestamp = context_->clock_tracker->ToTraceTime(
        protos::pbzero::ClockSnapshot::Clock::MONOTONIC_COARSE,
        static_cast<int64_t>(entry.timestamp()));

    if (!maybe_timestamp) {
      context_->storage->IncrementStats(stats::clock_sync_failure);
      continue;
    }

    int64_t timestamp = *maybe_timestamp;

    int pid = static_cast<int>(entry.pid());

    if (entry.disconnected())
      context_->storage->IncrementIndexedStats(
          stats::heapprofd_client_disconnected, pid);
    if (entry.buffer_corrupted())
      context_->storage->IncrementIndexedStats(
          stats::heapprofd_buffer_corrupted, pid);
    if (entry.buffer_overran())
      context_->storage->IncrementIndexedStats(stats::heapprofd_buffer_overran,
                                               pid);
    if (entry.rejected_concurrent())
      context_->storage->IncrementIndexedStats(
          stats::heapprofd_rejected_concurrent, pid);
    if (entry.hit_guardrail())
      context_->storage->IncrementIndexedStats(stats::heapprofd_hit_guardrail,
                                               pid);

    for (auto sample_it = entry.samples(); sample_it; ++sample_it) {
      protos::pbzero::ProfilePacket::HeapSample::Decoder sample(*sample_it);

      HeapProfileTracker::SourceAllocation src_allocation;
      src_allocation.pid = entry.pid();
      src_allocation.timestamp = timestamp;
      src_allocation.callstack_id = sample.callstack_id();
      if (sample.self_max()) {
        src_allocation.self_allocated = sample.self_max();
      } else {
        src_allocation.self_allocated = sample.self_allocated();
        src_allocation.self_freed = sample.self_freed();
      }
      src_allocation.alloc_count = sample.alloc_count();
      src_allocation.free_count = sample.free_count();

      context_->heap_profile_tracker->StoreAllocation(seq_id, src_allocation);
    }
  }
  if (!packet.continued()) {
    PERFETTO_CHECK(sequence_state);
    ProfilePacketInternLookup intern_lookup(sequence_state);
    context_->heap_profile_tracker->FinalizeProfile(
        seq_id, &sequence_state->state()->stack_profile_tracker(),
        &intern_lookup);
  }
}

void ProtoTraceParser::ParsePerfSample(
    int64_t ts,
    PacketSequenceStateGeneration* sequence_state,
    ConstBytes blob) {
  using PerfSample = protos::pbzero::PerfSample;
  PerfSample::Decoder sample(blob.data, blob.size);

  // Not a sample, but an indication of data loss in the ring buffer shared with
  // the kernel.
  if (sample.kernel_records_lost() > 0) {
    PERFETTO_DCHECK(sample.pid() == 0);

    context_->storage->IncrementIndexedStats(
        stats::perf_cpu_lost_records, static_cast<int>(sample.cpu()),
        static_cast<int64_t>(sample.kernel_records_lost()));
    return;
  }

  // Sample that looked relevant for the tracing session, but had to be skipped.
  // Either we failed to look up the procfs file descriptors necessary for
  // remote stack unwinding (not unexpected in most cases), or the unwind queue
  // was out of capacity (producer lost data on its own).
  if (sample.has_sample_skipped_reason()) {
    context_->storage->IncrementStats(stats::perf_samples_skipped);

    if (sample.sample_skipped_reason() ==
        PerfSample::PROFILER_SKIP_UNWIND_ENQUEUE)
      context_->storage->IncrementStats(stats::perf_samples_skipped_dataloss);

    return;
  }

  uint64_t callstack_iid = sample.callstack_iid();
  StackProfileTracker& stack_tracker =
      sequence_state->state()->stack_profile_tracker();
  ProfilePacketInternLookup intern_lookup(sequence_state);

  base::Optional<CallsiteId> cs_id =
      stack_tracker.FindOrInsertCallstack(callstack_iid, &intern_lookup);
  if (!cs_id) {
    context_->storage->IncrementStats(stats::stackprofile_parser_error);
    PERFETTO_ELOG("PerfSample referencing invalid callstack iid [%" PRIu64
                  "] at timestamp [%" PRIi64 "]",
                  callstack_iid, ts);
    return;
  }

  context_->perf_sample_tracker->AddStackToSliceTrack(
      ts, *cs_id, sample.pid(), sample.tid(), sample.cpu());
}

void ProtoTraceParser::ParseChromeBenchmarkMetadata(ConstBytes blob) {
  TraceStorage* storage = context_->storage.get();
  MetadataTracker* metadata = context_->metadata_tracker.get();

  protos::pbzero::ChromeBenchmarkMetadata::Decoder packet(blob.data, blob.size);
  if (packet.has_benchmark_name()) {
    auto benchmark_name_id = storage->InternString(packet.benchmark_name());
    metadata->SetMetadata(metadata::benchmark_name,
                          Variadic::String(benchmark_name_id));
  }
  if (packet.has_benchmark_description()) {
    auto benchmark_description_id =
        storage->InternString(packet.benchmark_description());
    metadata->SetMetadata(metadata::benchmark_description,
                          Variadic::String(benchmark_description_id));
  }
  if (packet.has_label()) {
    auto label_id = storage->InternString(packet.label());
    metadata->SetMetadata(metadata::benchmark_label,
                          Variadic::String(label_id));
  }
  if (packet.has_story_name()) {
    auto story_name_id = storage->InternString(packet.story_name());
    metadata->SetMetadata(metadata::benchmark_story_name,
                          Variadic::String(story_name_id));
  }
  for (auto it = packet.story_tags(); it; ++it) {
    auto story_tag_id = storage->InternString(*it);
    metadata->AppendMetadata(metadata::benchmark_story_tags,
                             Variadic::String(story_tag_id));
  }
  if (packet.has_benchmark_start_time_us()) {
    metadata->SetMetadata(metadata::benchmark_start_time_us,
                          Variadic::Integer(packet.benchmark_start_time_us()));
  }
  if (packet.has_story_run_time_us()) {
    metadata->SetMetadata(metadata::benchmark_story_run_time_us,
                          Variadic::Integer(packet.story_run_time_us()));
  }
  if (packet.has_story_run_index()) {
    metadata->SetMetadata(metadata::benchmark_story_run_index,
                          Variadic::Integer(packet.story_run_index()));
  }
  if (packet.has_had_failures()) {
    metadata->SetMetadata(metadata::benchmark_had_failures,
                          Variadic::Integer(packet.had_failures()));
  }
}

void ProtoTraceParser::ParseChromeEvents(int64_t ts, ConstBytes blob) {
  TraceStorage* storage = context_->storage.get();
  protos::pbzero::ChromeEventBundle::Decoder bundle(blob.data, blob.size);
  ArgsTracker args(context_);
  if (bundle.has_metadata()) {
    RawId id = storage->mutable_raw_table()
                   ->Insert({ts, raw_chrome_metadata_event_id_, 0, 0})
                   .id;
    auto inserter = args.AddArgsTo(id);

    // Metadata is proxied via a special event in the raw table to JSON export.
    for (auto it = bundle.metadata(); it; ++it) {
      protos::pbzero::ChromeMetadata::Decoder metadata(*it);
      StringId name_id = storage->InternString(metadata.name());
      Variadic value;
      if (metadata.has_string_value()) {
        value =
            Variadic::String(storage->InternString(metadata.string_value()));
      } else if (metadata.has_int_value()) {
        value = Variadic::Integer(metadata.int_value());
      } else if (metadata.has_bool_value()) {
        value = Variadic::Integer(metadata.bool_value());
      } else if (metadata.has_json_value()) {
        value = Variadic::Json(storage->InternString(metadata.json_value()));
      } else {
        context_->storage->IncrementStats(stats::empty_chrome_metadata);
        continue;
      }
      args.AddArgsTo(id).AddArg(name_id, value);
    }
  }

  if (bundle.has_legacy_ftrace_output()) {
    RawId id =
        storage->mutable_raw_table()
            ->Insert({ts, raw_chrome_legacy_system_trace_event_id_, 0, 0})
            .id;

    std::string data;
    for (auto it = bundle.legacy_ftrace_output(); it; ++it) {
      data += (*it).ToStdString();
    }
    Variadic value =
        Variadic::String(storage->InternString(base::StringView(data)));
    args.AddArgsTo(id).AddArg(data_name_id_, value);
  }

  if (bundle.has_legacy_json_trace()) {
    for (auto it = bundle.legacy_json_trace(); it; ++it) {
      protos::pbzero::ChromeLegacyJsonTrace::Decoder legacy_trace(*it);
      if (legacy_trace.type() !=
          protos::pbzero::ChromeLegacyJsonTrace::USER_TRACE) {
        continue;
      }
      RawId id =
          storage->mutable_raw_table()
              ->Insert({ts, raw_chrome_legacy_user_trace_event_id_, 0, 0})
              .id;
      Variadic value =
          Variadic::String(storage->InternString(legacy_trace.data()));
      args.AddArgsTo(id).AddArg(data_name_id_, value);
    }
  }
}

void ProtoTraceParser::ParseMetatraceEvent(int64_t ts, ConstBytes blob) {
  protos::pbzero::PerfettoMetatrace::Decoder event(blob.data, blob.size);
  auto utid = context_->process_tracker->GetOrCreateThread(event.thread_id());

  StringId cat_id = metatrace_id_;
  StringId name_id = kNullStringId;
  char fallback[64];

  if (event.has_event_id()) {
    auto eid = event.event_id();
    if (eid < metatrace::EVENTS_MAX) {
      name_id = context_->storage->InternString(metatrace::kEventNames[eid]);
    } else {
      sprintf(fallback, "Event %d", eid);
      name_id = context_->storage->InternString(fallback);
    }
    TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
    context_->slice_tracker->Scoped(ts, track_id, cat_id, name_id,
                                    event.event_duration_ns());
  } else if (event.has_counter_id()) {
    auto cid = event.counter_id();
    if (cid < metatrace::COUNTERS_MAX) {
      name_id = context_->storage->InternString(metatrace::kCounterNames[cid]);
    } else {
      sprintf(fallback, "Counter %d", cid);
      name_id = context_->storage->InternString(fallback);
    }
    TrackId track =
        context_->track_tracker->InternThreadCounterTrack(name_id, utid);
    context_->event_tracker->PushCounter(ts, event.counter_value(), track);
  }

  if (event.has_overruns())
    context_->storage->IncrementStats(stats::metatrace_overruns);
}

void ProtoTraceParser::ParseTraceConfig(ConstBytes blob) {
  protos::pbzero::TraceConfig::Decoder trace_config(blob.data, blob.size);

  // TODO(eseckler): Propagate statuses from modules.
  for (auto& module : context_->modules) {
    module->ParseTraceConfig(trace_config);
  }

  int64_t uuid_msb = trace_config.trace_uuid_msb();
  int64_t uuid_lsb = trace_config.trace_uuid_lsb();
  if (uuid_msb != 0 || uuid_lsb != 0) {
    base::Uuid uuid(uuid_lsb, uuid_msb);
    std::string str = uuid.ToPrettyString();
    StringId id = context_->storage->InternString(base::StringView(str));
    context_->metadata_tracker->SetMetadata(metadata::trace_uuid,
                                            Variadic::String(id));
  }
}

void ProtoTraceParser::ParseModuleSymbols(ConstBytes blob) {
  protos::pbzero::ModuleSymbols::Decoder module_symbols(blob.data, blob.size);
  StringId build_id;
  // TODO(b/148109467): Remove workaround once all active Chrome versions
  // write raw bytes instead of a string as build_id.
  if (module_symbols.build_id().size == 33) {
    build_id = context_->storage->InternString(module_symbols.build_id());
  } else {
    build_id = context_->storage->InternString(base::StringView(base::ToHex(
        module_symbols.build_id().data, module_symbols.build_id().size)));
  }

  auto mapping_ids = context_->storage->FindMappingRow(
      context_->storage->InternString(module_symbols.path()), build_id);
  if (mapping_ids.empty()) {
    context_->storage->IncrementStats(stats::stackprofile_invalid_mapping_id);
    return;
  }
  for (auto addr_it = module_symbols.address_symbols(); addr_it; ++addr_it) {
    protos::pbzero::AddressSymbols::Decoder address_symbols(*addr_it);

    uint32_t symbol_set_id = context_->storage->symbol_table().row_count();
    bool frame_found = false;
    for (MappingId mapping_id : mapping_ids) {
      std::vector<FrameId> frame_ids = context_->storage->FindFrameIds(
          mapping_id, address_symbols.address());

      for (const FrameId frame_id : frame_ids) {
        auto* frames = context_->storage->mutable_stack_profile_frame_table();
        uint32_t frame_row = *frames->id().IndexOf(frame_id);
        frames->mutable_symbol_set_id()->Set(frame_row, symbol_set_id);
        frame_found = true;
      }
    }

    if (!frame_found) {
      context_->storage->IncrementStats(stats::stackprofile_invalid_frame_id);
      continue;
    }

    for (auto line_it = address_symbols.lines(); line_it; ++line_it) {
      protos::pbzero::Line::Decoder line(*line_it);
      context_->storage->mutable_symbol_table()->Insert(
          {symbol_set_id, context_->storage->InternString(line.function_name()),
           context_->storage->InternString(line.source_file_name()),
           line.line_number()});
    }
  }
}

void ProtoTraceParser::ParseTrigger(int64_t ts, ConstBytes blob) {
  protos::pbzero::Trigger::Decoder trigger(blob.data, blob.size);
  StringId cat_id = kNullStringId;
  TrackId track_id = context_->track_tracker->GetOrCreateTriggerTrack();
  StringId name_id = context_->storage->InternString(trigger.trigger_name());
  context_->slice_tracker->Scoped(
      ts, track_id, cat_id, name_id,
      /* duration = */ 0,
      [&trigger, this](ArgsTracker::BoundInserter* args_table) {
        StringId producer_name_key =
            context_->storage->InternString("producer_name");
        args_table->AddArg(producer_name_key,
                           Variadic::String(context_->storage->InternString(
                               trigger.producer_name())));
        StringId trusted_producer_uid_key =
            context_->storage->InternString("trusted_producer_uid");
        args_table->AddArg(trusted_producer_uid_key,
                           Variadic::Integer(trigger.trusted_producer_uid()));
      });
}

void ProtoTraceParser::ParseServiceEvent(int64_t ts, ConstBytes blob) {
  protos::pbzero::TracingServiceEvent::Decoder tse(blob.data, blob.size);
  if (tse.all_data_sources_started()) {
    context_->metadata_tracker->SetMetadata(
        metadata::all_data_source_started_ns, Variadic::Integer(ts));
  }
}

void ProtoTraceParser::ParseSmapsPacket(int64_t ts, ConstBytes blob) {
  protos::pbzero::SmapsPacket::Decoder sp(blob.data, blob.size);
  auto upid = context_->process_tracker->GetOrCreateProcess(sp.pid());

  for (auto it = sp.entries(); it; ++it) {
    protos::pbzero::SmapsEntry::Decoder e(*it);
    context_->storage->mutable_profiler_smaps_table()->Insert(
        {upid, ts, context_->storage->InternString(e.path()),
         static_cast<int64_t>(e.size_kb()),
         static_cast<int64_t>(e.private_dirty_kb()),
         static_cast<int64_t>(e.swap_kb())});
  }
}

}  // namespace trace_processor
}  // namespace perfetto
