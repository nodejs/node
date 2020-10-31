/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_TABLES_METADATA_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_METADATA_TABLES_H_

#include "src/trace_processor/tables/macros.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_RAW_TABLE_DEF(NAME, PARENT, C) \
  NAME(RawTable, "raw")                            \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                \
  C(int64_t, ts, Column::Flag::kSorted)            \
  C(StringPool::Id, name)                          \
  C(uint32_t, cpu)                                 \
  C(uint32_t, utid)                                \
  C(uint32_t, arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_RAW_TABLE_DEF);

#define PERFETTO_TP_ARG_TABLE_DEF(NAME, PARENT, C) \
  NAME(ArgTable, "args")                           \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                \
  C(uint32_t, arg_set_id, Column::Flag::kSorted)   \
  C(StringPool::Id, flat_key)                      \
  C(StringPool::Id, key)                           \
  C(base::Optional<int64_t>, int_value)            \
  C(base::Optional<StringPool::Id>, string_value)  \
  C(base::Optional<double>, real_value)            \
  C(StringPool::Id, value_type)

PERFETTO_TP_TABLE(PERFETTO_TP_ARG_TABLE_DEF);

#define PERFETTO_TP_METADATA_TABLE_DEF(NAME, PARENT, C) \
  NAME(MetadataTable, "metadata")                       \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                     \
  C(StringPool::Id, name)                               \
  C(StringPool::Id, key_type)                           \
  C(base::Optional<int64_t>, int_value)                 \
  C(base::Optional<StringPool::Id>, str_value)

PERFETTO_TP_TABLE(PERFETTO_TP_METADATA_TABLE_DEF);

#define PERFETTO_TP_THREAD_TABLE_DEF(NAME, PARENT, C) \
  NAME(ThreadTable, "internal_thread")                \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                   \
  C(uint32_t, tid)                                    \
  C(StringPool::Id, name)                             \
  C(base::Optional<int64_t>, start_ts)                \
  C(base::Optional<int64_t>, end_ts)                  \
  C(base::Optional<uint32_t>, upid)

PERFETTO_TP_TABLE(PERFETTO_TP_THREAD_TABLE_DEF);

#define PERFETTO_TP_PROCESS_TABLE_DEF(NAME, PARENT, C) \
  NAME(ProcessTable, "internal_process")               \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                    \
  C(uint32_t, pid)                                     \
  C(StringPool::Id, name)                              \
  C(base::Optional<int64_t>, start_ts)                 \
  C(base::Optional<int64_t>, end_ts)                   \
  C(base::Optional<uint32_t>, parent_upid)             \
  C(base::Optional<uint32_t>, uid)                     \
  C(base::Optional<uint32_t>, android_appid)

PERFETTO_TP_TABLE(PERFETTO_TP_PROCESS_TABLE_DEF);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_METADATA_TABLES_H_
