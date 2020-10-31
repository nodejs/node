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

#ifndef SRC_TRACE_PROCESSOR_TABLES_SLICE_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_SLICE_TABLES_H_

#include "src/trace_processor/tables/macros.h"
#include "src/trace_processor/tables/track_tables.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_SLICE_TABLE_DEF(NAME, PARENT, C) \
  NAME(SliceTable, "internal_slice")                 \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                  \
  C(int64_t, ts, Column::Flag::kSorted)              \
  C(int64_t, dur)                                    \
  C(TrackTable::Id, track_id)                        \
  C(StringPool::Id, category)                        \
  C(StringPool::Id, name)                            \
  C(uint32_t, depth)                                 \
  C(int64_t, stack_id)                               \
  C(int64_t, parent_stack_id)                        \
  C(base::Optional<SliceTable::Id>, parent_id)       \
  C(uint32_t, arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_SLICE_TABLE_DEF);

#define PERFETTO_TP_INSTANT_TABLE_DEF(NAME, PARENT, C) \
  NAME(InstantTable, "instant")                        \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                    \
  C(int64_t, ts, Column::Flag::kSorted)                \
  C(StringPool::Id, name)                              \
  C(int64_t, ref)                                      \
  C(StringPool::Id, ref_type)                          \
  C(uint32_t, arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_INSTANT_TABLE_DEF);

#define PERFETTO_TP_SCHED_SLICE_TABLE_DEF(NAME, PARENT, C) \
  NAME(SchedSliceTable, "sched_slice")                     \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                        \
  C(int64_t, ts, Column::Flag::kSorted)                    \
  C(int64_t, dur)                                          \
  C(uint32_t, cpu)                                         \
  C(uint32_t, utid)                                        \
  C(StringPool::Id, end_state)                             \
  C(int32_t, priority)

PERFETTO_TP_TABLE(PERFETTO_TP_SCHED_SLICE_TABLE_DEF);

#define PERFETTO_TP_GPU_SLICES_DEF(NAME, PARENT, C) \
  NAME(GpuSliceTable, "gpu_slice")                  \
  PARENT(PERFETTO_TP_SLICE_TABLE_DEF, C)            \
  C(base::Optional<int64_t>, context_id)            \
  C(base::Optional<int64_t>, render_target)         \
  C(StringPool::Id, render_target_name)             \
  C(base::Optional<int64_t>, render_pass)           \
  C(StringPool::Id, render_pass_name)               \
  C(base::Optional<int64_t>, command_buffer)        \
  C(StringPool::Id, command_buffer_name)            \
  C(base::Optional<uint32_t>, frame_id)             \
  C(base::Optional<uint32_t>, submission_id)        \
  C(base::Optional<uint32_t>, hw_queue_id)

PERFETTO_TP_TABLE(PERFETTO_TP_GPU_SLICES_DEF);

#define PERFETTO_TP_GRAPHICS_FRAME_SLICES_DEF(NAME, PARENT, C) \
  NAME(GraphicsFrameSliceTable, "frame_slice")                 \
  PARENT(PERFETTO_TP_SLICE_TABLE_DEF, C)                       \
  C(StringPool::Id, frame_numbers)                             \
  C(StringPool::Id, layer_names)

PERFETTO_TP_TABLE(PERFETTO_TP_GRAPHICS_FRAME_SLICES_DEF);

// frame_slice -> frame_stats : 1 -> Many,
// with frame_slice.id = frame_stats.slice_id
#define PERFETTO_TP_GRAPHICS_FRAME_STATS_DEF(NAME, PARENT, C) \
  NAME(GraphicsFrameStatsTable, "frame_stats")                \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                           \
  C(uint32_t, slice_id)                                       \
  C(int64_t, queue_to_acquire_time)                           \
  C(int64_t, acquire_to_latch_time)                           \
  C(int64_t, latch_to_present_time)

PERFETTO_TP_TABLE(PERFETTO_TP_GRAPHICS_FRAME_STATS_DEF);

#define PERFETTO_TP_DESCRIBE_SLICE_TABLE(NAME, PARENT, C) \
  NAME(DescribeSliceTable, "describe_slice")              \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                       \
  C(uint32_t, slice_id, Column::Flag::kHidden)            \
  C(StringPool::Id, description)                          \
  C(StringPool::Id, doc_link)

PERFETTO_TP_TABLE(PERFETTO_TP_DESCRIBE_SLICE_TABLE);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_SLICE_TABLES_H_
