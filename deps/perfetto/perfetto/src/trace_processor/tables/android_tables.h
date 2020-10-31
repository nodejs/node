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

#ifndef SRC_TRACE_PROCESSOR_TABLES_ANDROID_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_ANDROID_TABLES_H_

#include "src/trace_processor/tables/macros.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

// Note: this table is not sorted by timestamp. This is why we omit the
// sorted flag on the ts column.
#define PERFETTO_TP_ANDROID_LOG_TABLE_DEF(NAME, PARENT, C) \
  NAME(AndroidLogTable, "android_logs")                    \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                        \
  C(int64_t, ts)                                           \
  C(uint32_t, utid)                                        \
  C(uint32_t, prio)                                        \
  C(StringPool::Id, tag)                                   \
  C(StringPool::Id, msg)

PERFETTO_TP_TABLE(PERFETTO_TP_ANDROID_LOG_TABLE_DEF);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_ANDROID_TABLES_H_
