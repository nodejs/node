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

#ifndef SRC_TRACE_PROCESSOR_TABLES_COUNTER_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_COUNTER_TABLES_H_

#include "src/trace_processor/tables/macros.h"
#include "src/trace_processor/tables/track_tables.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_COUNTER_TABLE_DEF(NAME, PARENT, C) \
  NAME(CounterTable, "counter")                        \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                    \
  C(int64_t, ts, Column::Flag::kSorted)                \
  C(CounterTrackTable::Id, track_id)                   \
  C(double, value)                                     \
  C(base::Optional<uint32_t>, arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_COUNTER_TABLE_DEF);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_COUNTER_TABLES_H_
