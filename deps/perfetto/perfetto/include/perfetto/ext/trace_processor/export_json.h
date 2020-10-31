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

#ifndef INCLUDE_PERFETTO_EXT_TRACE_PROCESSOR_EXPORT_JSON_H_
#define INCLUDE_PERFETTO_EXT_TRACE_PROCESSOR_EXPORT_JSON_H_

#include <stdio.h>

#include <functional>

#include "perfetto/base/export.h"
#include "perfetto/trace_processor/status.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorStorage;

namespace json {

using ArgumentNameFilterPredicate = std::function<bool(const char* arg_name)>;
using ArgumentFilterPredicate =
    std::function<bool(const char* category_group_name,
                       const char* event_name,
                       ArgumentNameFilterPredicate*)>;
using MetadataFilterPredicate = std::function<bool(const char* metadata_name)>;
using LabelFilterPredicate = std::function<bool(const char* label_name)>;

class PERFETTO_EXPORT OutputWriter {
 public:
  OutputWriter();
  virtual ~OutputWriter();

  virtual util::Status AppendString(const std::string&) = 0;
};

// Public for Chrome. Exports the trace loaded in TraceProcessorStorage to json,
// applying argument, metadata and label filtering using the callbacks.
util::Status PERFETTO_EXPORT ExportJson(TraceProcessorStorage*,
                                        OutputWriter*,
                                        ArgumentFilterPredicate = nullptr,
                                        MetadataFilterPredicate = nullptr,
                                        LabelFilterPredicate = nullptr);

}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACE_PROCESSOR_EXPORT_JSON_H_
