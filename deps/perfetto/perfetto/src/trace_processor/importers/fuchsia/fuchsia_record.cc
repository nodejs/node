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

#include "src/trace_processor/importers/fuchsia/fuchsia_record.h"

namespace perfetto {
namespace trace_processor {

void FuchsiaRecord::InsertString(uint32_t index, StringId string_id) {
  StringTableEntry entry;
  entry.index = index;
  entry.string_id = string_id;

  string_entries_.push_back(entry);
}

StringId FuchsiaRecord::GetString(uint32_t index) {
  for (const auto& entry : string_entries_) {
    if (entry.index == index)
      return entry.string_id;
  }
  return StringId();
}

void FuchsiaRecord::InsertThread(uint32_t index,
                                 fuchsia_trace_utils::ThreadInfo info) {
  ThreadTableEntry entry;
  entry.index = index;
  entry.info = info;

  thread_entries_.push_back(entry);
}

fuchsia_trace_utils::ThreadInfo FuchsiaRecord::GetThread(uint32_t index) {
  for (const auto& entry : thread_entries_) {
    if (entry.index == index)
      return entry.info;
  }
  return fuchsia_trace_utils::ThreadInfo();
}

}  // namespace trace_processor
}  // namespace perfetto
