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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_RSS_STAT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_RSS_STAT_TRACKER_H_

#include <unordered_map>

#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class RssStatTracker {
 public:
  using ConstBytes = protozero::ConstBytes;

  explicit RssStatTracker(TraceProcessorContext*);

  void ParseRssStat(int64_t ts, uint32_t pid, ConstBytes blob);

 private:
  base::Optional<UniqueTid> FindUtidForMmId(int64_t mm_id,
                                            bool is_curr,
                                            uint32_t pid);

  std::unordered_map<int64_t, UniqueTid> mm_id_to_utid_;
  std::vector<StringId> rss_members_;
  TraceProcessorContext* const context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_RSS_STAT_TRACKER_H_
