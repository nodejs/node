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

#ifndef SRC_TRACE_PROCESSOR_TABLES_TRACK_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_TRACK_TABLES_H_

#include "src/trace_processor/tables/macros.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_TRACK_TABLE_DEF(NAME, PARENT, C) \
  NAME(TrackTable, "track")                          \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                  \
  C(StringPool::Id, name)                            \
  C(base::Optional<uint32_t>, source_arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_TRACK_TABLE_DEF);

#define PERFETTO_TP_PROCESS_TRACK_TABLE_DEF(NAME, PARENT, C) \
  NAME(ProcessTrackTable, "process_track")                   \
  PARENT(PERFETTO_TP_TRACK_TABLE_DEF, C)                     \
  C(uint32_t, upid)

PERFETTO_TP_TABLE(PERFETTO_TP_PROCESS_TRACK_TABLE_DEF);

#define PERFETTO_TP_THREAD_TRACK_TABLE_DEF(NAME, PARENT, C) \
  NAME(ThreadTrackTable, "thread_track")                    \
  PARENT(PERFETTO_TP_TRACK_TABLE_DEF, C)                    \
  C(uint32_t, utid)

PERFETTO_TP_TABLE(PERFETTO_TP_THREAD_TRACK_TABLE_DEF);

#define PERFETTO_TP_GPU_TRACK_DEF(NAME, PARENT, C) \
  NAME(GpuTrackTable, "gpu_track")                 \
  PARENT(PERFETTO_TP_TRACK_TABLE_DEF, C)           \
  C(StringPool::Id, scope)                         \
  C(StringPool::Id, description)                   \
  C(base::Optional<int64_t>, context_id)

PERFETTO_TP_TABLE(PERFETTO_TP_GPU_TRACK_DEF);

#define PERFETTO_TP_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(CounterTrackTable, "counter_track")             \
  PARENT(PERFETTO_TP_TRACK_TABLE_DEF, C)               \
  C(StringPool::Id, unit)                              \
  C(StringPool::Id, description)

PERFETTO_TP_TABLE(PERFETTO_TP_COUNTER_TRACK_DEF);

#define PERFETTO_TP_THREAD_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(ThreadCounterTrackTable, "thread_counter_track")       \
  PARENT(PERFETTO_TP_COUNTER_TRACK_DEF, C)                    \
  C(uint32_t, utid)

PERFETTO_TP_TABLE(PERFETTO_TP_THREAD_COUNTER_TRACK_DEF);

#define PERFETTO_TP_PROCESS_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(ProcessCounterTrackTable, "process_counter_track")      \
  PARENT(PERFETTO_TP_COUNTER_TRACK_DEF, C)                     \
  C(uint32_t, upid)

PERFETTO_TP_TABLE(PERFETTO_TP_PROCESS_COUNTER_TRACK_DEF);

#define PERFETTO_TP_CPU_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(CpuCounterTrackTable, "cpu_counter_track")          \
  PARENT(PERFETTO_TP_COUNTER_TRACK_DEF, C)                 \
  C(uint32_t, cpu)

PERFETTO_TP_TABLE(PERFETTO_TP_CPU_COUNTER_TRACK_DEF);

#define PERFETTO_TP_IRQ_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(IrqCounterTrackTable, "irq_counter_track")          \
  PARENT(PERFETTO_TP_COUNTER_TRACK_DEF, C)                 \
  C(int32_t, irq)

PERFETTO_TP_TABLE(PERFETTO_TP_IRQ_COUNTER_TRACK_DEF);

#define PERFETTO_TP_SOFTIRQ_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(SoftirqCounterTrackTable, "softirq_counter_track")      \
  PARENT(PERFETTO_TP_COUNTER_TRACK_DEF, C)                     \
  C(int32_t, softirq)

PERFETTO_TP_TABLE(PERFETTO_TP_SOFTIRQ_COUNTER_TRACK_DEF);

#define PERFETTO_TP_GPU_COUNTER_TRACK_DEF(NAME, PARENT, C) \
  NAME(GpuCounterTrackTable, "gpu_counter_track")          \
  PARENT(PERFETTO_TP_COUNTER_TRACK_DEF, C)                 \
  C(uint32_t, gpu_id)

PERFETTO_TP_TABLE(PERFETTO_TP_GPU_COUNTER_TRACK_DEF);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_TRACK_TABLES_H_
