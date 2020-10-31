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

#ifndef SRC_TRACE_PROCESSOR_STORAGE_STATS_H_
#define SRC_TRACE_PROCESSOR_STORAGE_STATS_H_

#include <stddef.h>

namespace perfetto {
namespace trace_processor {
namespace stats {

// Compile time list of parsing and processing stats.
// clang-format off
#define PERFETTO_TP_STATS(F)                                                     \
  F(android_log_num_failed,                   kSingle,  kError,    kTrace),    \
  F(android_log_num_skipped,                  kSingle,  kError,    kTrace),    \
  F(android_log_num_total,                    kSingle,  kInfo,     kTrace),    \
  F(counter_events_out_of_order,              kSingle,  kError,    kAnalysis), \
  F(ftrace_bundle_tokenizer_errors,           kSingle,  kError,    kAnalysis), \
  F(ftrace_cpu_bytes_read_begin,              kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_bytes_read_end,                kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_commit_overrun_begin,          kIndexed, kError,    kTrace),    \
  F(ftrace_cpu_commit_overrun_end,            kIndexed, kError,    kTrace),    \
  F(ftrace_cpu_dropped_events_begin,          kIndexed, kError,    kTrace),    \
  F(ftrace_cpu_dropped_events_end,            kIndexed, kError,    kTrace),    \
  F(ftrace_cpu_entries_begin,                 kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_entries_end,                   kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_now_ts_begin,                  kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_now_ts_end,                    kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_oldest_event_ts_begin,         kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_oldest_event_ts_end,           kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_overrun_begin,                 kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_overrun_end,                   kIndexed, kDataLoss, kTrace),    \
  F(ftrace_cpu_read_events_begin,             kIndexed, kInfo,     kTrace),    \
  F(ftrace_cpu_read_events_end,               kIndexed, kInfo,     kTrace),    \
  F(fuchsia_non_numeric_counters,             kSingle,  kError,    kAnalysis), \
  F(fuchsia_timestamp_overflow,               kSingle,  kError,    kAnalysis), \
  F(fuchsia_invalid_event,                    kSingle,  kError,    kAnalysis), \
  F(gpu_counters_invalid_spec,                kSingle,  kError,    kAnalysis), \
  F(gpu_counters_missing_spec,                kSingle,  kError,    kAnalysis), \
  F(gpu_render_stage_parser_errors,           kSingle,  kError,    kAnalysis), \
  F(graphics_frame_event_parser_errors,       kSingle,  kInfo,     kAnalysis), \
  F(guess_trace_type_duration_ns,             kSingle,  kInfo,     kAnalysis), \
  F(interned_data_tokenizer_errors,           kSingle,  kInfo,     kAnalysis), \
  F(invalid_clock_snapshots,                  kSingle,  kError,    kAnalysis), \
  F(invalid_cpu_times,                        kSingle,  kError,    kAnalysis), \
  F(meminfo_unknown_keys,                     kSingle,  kError,    kAnalysis), \
  F(mismatched_sched_switch_tids,             kSingle,  kError,    kAnalysis), \
  F(mm_unknown_type,                          kSingle,  kError,    kAnalysis), \
  F(parse_trace_duration_ns,                  kSingle,  kInfo,     kAnalysis), \
  F(power_rail_unknown_index,                 kSingle,  kError,    kTrace),    \
  F(proc_stat_unknown_counters,               kSingle,  kError,    kAnalysis), \
  F(rss_stat_unknown_keys,                    kSingle,  kError,    kAnalysis), \
  F(rss_stat_negative_size,                   kSingle,  kInfo,     kAnalysis), \
  F(rss_stat_unknown_thread_for_mm_id,        kSingle,  kInfo,     kAnalysis), \
  F(sched_switch_out_of_order,                kSingle,  kError,    kAnalysis), \
  F(slice_out_of_order,                       kSingle,  kError,    kAnalysis), \
  F(stackprofile_invalid_string_id,           kSingle,  kError,    kTrace),    \
  F(stackprofile_invalid_mapping_id,          kSingle,  kError,    kTrace),    \
  F(stackprofile_invalid_frame_id,            kSingle,  kError,    kTrace),    \
  F(stackprofile_invalid_callstack_id,        kSingle,  kError,    kTrace),    \
  F(stackprofile_parser_error,                kSingle,  kError,    kTrace),    \
  F(systrace_parse_failure,                   kSingle,  kError,    kAnalysis), \
  F(task_state_invalid,                       kSingle,  kError,    kAnalysis), \
  F(traced_buf_buffer_size,                   kIndexed, kInfo,     kTrace),    \
  F(traced_buf_bytes_overwritten,             kIndexed, kDataLoss, kTrace),    \
  F(traced_buf_bytes_read,                    kIndexed, kInfo,     kTrace),    \
  F(traced_buf_bytes_written,                 kIndexed, kInfo,     kTrace),    \
  F(traced_buf_chunks_discarded,              kIndexed, kInfo,     kTrace),    \
  F(traced_buf_chunks_overwritten,            kIndexed, kDataLoss, kTrace),    \
  F(traced_buf_chunks_read,                   kIndexed, kInfo,     kTrace),    \
  F(traced_buf_chunks_rewritten,              kIndexed, kInfo,     kTrace),    \
  F(traced_buf_chunks_written,                kIndexed, kInfo,     kTrace),    \
  F(traced_buf_chunks_committed_out_of_order, kIndexed, kInfo,     kTrace),    \
  F(traced_buf_padding_bytes_cleared,         kIndexed, kInfo,     kTrace),    \
  F(traced_buf_padding_bytes_written,         kIndexed, kInfo,     kTrace),    \
  F(traced_buf_patches_failed,                kIndexed, kInfo,     kTrace),    \
  F(traced_buf_patches_succeeded,             kIndexed, kInfo,     kTrace),    \
  F(traced_buf_readaheads_failed,             kIndexed, kInfo,     kTrace),    \
  F(traced_buf_readaheads_succeeded,          kIndexed, kInfo,     kTrace),    \
  F(traced_buf_trace_writer_packet_loss,      kIndexed, kInfo,     kTrace),    \
  F(traced_buf_write_wrap_count,              kIndexed, kInfo,     kTrace),    \
  F(traced_chunks_discarded,                  kSingle,  kInfo,     kTrace),    \
  F(traced_data_sources_registered,           kSingle,  kInfo,     kTrace),    \
  F(traced_data_sources_seen,                 kSingle,  kInfo,     kTrace),    \
  F(traced_patches_discarded,                 kSingle,  kInfo,     kTrace),    \
  F(traced_producers_connected,               kSingle,  kInfo,     kTrace),    \
  F(traced_producers_seen,                    kSingle,  kInfo,     kTrace),    \
  F(traced_total_buffers,                     kSingle,  kInfo,     kTrace),    \
  F(traced_tracing_sessions,                  kSingle,  kInfo,     kTrace),    \
  F(track_event_parser_errors,                kSingle,  kInfo,     kAnalysis), \
  F(track_event_tokenizer_errors,             kSingle,  kInfo,     kAnalysis), \
  F(tokenizer_skipped_packets,                kSingle,  kInfo,     kAnalysis), \
  F(vmstat_unknown_keys,                      kSingle,  kError,    kAnalysis), \
  F(vulkan_allocations_invalid_string_id,     kSingle,  kError,    kTrace),    \
  F(clock_sync_failure,                       kSingle,  kError,    kAnalysis), \
  F(clock_sync_cache_miss,                    kSingle,  kInfo,     kAnalysis), \
  F(process_tracker_errors,                   kSingle,  kError,    kAnalysis), \
  F(json_tokenizer_failure,                   kSingle,  kError,    kTrace),    \
  F(heap_graph_invalid_string_id,             kIndexed, kError,    kTrace),    \
  F(heap_graph_non_finalized_graph,           kSingle,  kError,    kTrace),    \
  F(heap_graph_malformed_packet,              kIndexed, kError,    kTrace),    \
  F(heap_graph_missing_packet,                kIndexed, kError,    kTrace),    \
  F(heap_graph_location_parse_error,          kSingle,  kError,    kTrace),    \
  F(heapprofd_buffer_corrupted,               kIndexed, kError,    kTrace),    \
  F(heapprofd_hit_guardrail,                  kIndexed, kError,    kTrace),    \
  F(heapprofd_buffer_overran,                 kIndexed, kDataLoss, kTrace),    \
  F(heapprofd_client_disconnected,            kIndexed, kInfo,     kTrace),    \
  F(heapprofd_malformed_packet,               kIndexed, kError,    kTrace),    \
  F(heapprofd_missing_packet,                 kSingle,  kError,    kTrace),    \
  F(heapprofd_rejected_concurrent,            kIndexed, kError,    kTrace),    \
  F(heapprofd_non_finalized_profile,          kSingle,  kError,    kTrace),    \
  F(metatrace_overruns,                       kSingle,  kError,    kTrace),    \
  F(packages_list_has_parse_errors,           kSingle,  kError,    kTrace),    \
  F(packages_list_has_read_errors,            kSingle,  kError,    kTrace),    \
  F(compact_sched_has_parse_errors,           kSingle,  kError,    kTrace),    \
  F(misplaced_end_event,                      kSingle,  kDataLoss, kAnalysis), \
  F(sched_waking_out_of_order,                kSingle,  kError,    kAnalysis), \
  F(compact_sched_switch_skipped,             kSingle,  kInfo,     kAnalysis), \
  F(compact_sched_waking_skipped,             kSingle,  kInfo,     kAnalysis), \
  F(empty_chrome_metadata,                    kSingle,  kError,    kTrace),    \
  F(perf_cpu_lost_records,                    kIndexed, kDataLoss, kTrace),    \
  F(ninja_parse_errors,                       kSingle,  kError,    kTrace),    \
  F(perf_samples_skipped,                     kSingle,  kInfo,     kTrace),    \
  F(perf_samples_skipped_dataloss,            kSingle,  kDataLoss, kTrace),    \
  F(thread_time_in_state_out_of_order,        kSingle,  kError,    kAnalysis), \
  F(thread_time_in_state_unknown_cpu_freq,    kSingle,  kError,    kAnalysis)
// clang-format on

enum Type {
  kSingle,  // Single-value property, one value per key.
  kIndexed  // Indexed property, multiple value per key (e.g. cpu_stats[1]).
};

enum Severity {
  kInfo,      // Diagnostic counters
  kDataLoss,  // Correct operation that still resulted in data loss
  kError      // If any kError counter is > 0 trace_processor_shell will
              // raise an error. This is *not* surfaced in the web UI.
              // TODO(b/148587181): Surface these errors in the UI.
};

enum Source {
  // The counter is collected when recording the trace on-device and is just
  // being reflected in the stats table.
  kTrace,

  // The counter is genrated when importing / processing the trace in the trace
  // processor.
  kAnalysis
};

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header

// Declares an enum of literals (one for each stat). The enum values of each
// literal corresponds to the string index in the arrays below.
#define PERFETTO_TP_STATS_ENUM(name, ...) name
enum KeyIDs : size_t { PERFETTO_TP_STATS(PERFETTO_TP_STATS_ENUM), kNumKeys };

// The code below declares an array for each property (name, type, ...).

#define PERFETTO_TP_STATS_NAME(name, ...) #name
constexpr char const* kNames[] = {PERFETTO_TP_STATS(PERFETTO_TP_STATS_NAME)};

#define PERFETTO_TP_STATS_TYPE(_, type, ...) type
constexpr Type kTypes[] = {PERFETTO_TP_STATS(PERFETTO_TP_STATS_TYPE)};

#define PERFETTO_TP_STATS_SEVERITY(_, __, severity, ...) severity
constexpr Severity kSeverities[] = {
    PERFETTO_TP_STATS(PERFETTO_TP_STATS_SEVERITY)};

#define PERFETTO_TP_STATS_SOURCE(_, __, ___, source, ...) source
constexpr Source kSources[] = {PERFETTO_TP_STATS(PERFETTO_TP_STATS_SOURCE)};

}  // namespace stats
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_STORAGE_STATS_H_
