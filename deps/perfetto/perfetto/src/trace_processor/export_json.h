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

#ifndef SRC_TRACE_PROCESSOR_EXPORT_JSON_H_
#define SRC_TRACE_PROCESSOR_EXPORT_JSON_H_

#include <stdio.h>

#include "perfetto/ext/trace_processor/export_json.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {
namespace json {

// Export trace to a file stream in json format.
util::Status ExportJson(const TraceStorage*, FILE* output);

// For testing.
util::Status ExportJson(const TraceStorage* storage,
                        OutputWriter*,
                        ArgumentFilterPredicate = nullptr,
                        MetadataFilterPredicate = nullptr,
                        LabelFilterPredicate = nullptr);

}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_EXPORT_JSON_H_
