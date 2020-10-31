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

#ifndef SRC_TRACE_PROCESSOR_ANALYSIS_DESCRIBE_SLICE_H_
#define SRC_TRACE_PROCESSOR_ANALYSIS_DESCRIBE_SLICE_H_

#include <string>

#include "perfetto/trace_processor/status.h"

#include "src/trace_processor/tables/slice_tables.h"

namespace perfetto {
namespace trace_processor {

struct SliceDescription {
  std::string description;
  std::string doc_link;
};

// Provides a description and doc link (if available) for the given slice id in
// the slice table. For a high level overview of this function's use, see
// /docs/analysis.md.
util::Status DescribeSlice(const tables::SliceTable&,
                           tables::SliceTable::Id,
                           base::Optional<SliceDescription>* description);

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_ANALYSIS_DESCRIBE_SLICE_H_
