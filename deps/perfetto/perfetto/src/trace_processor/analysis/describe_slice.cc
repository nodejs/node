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

#include "src/trace_processor/analysis/describe_slice.h"

namespace perfetto {
namespace trace_processor {

util::Status DescribeSlice(const tables::SliceTable& table,
                           tables::SliceTable::Id id,
                           base::Optional<SliceDescription>* description) {
  auto opt_row = table.id().IndexOf(id);
  if (!opt_row)
    return util::ErrStatus("Unable to find slice id");

  uint32_t row = *opt_row;

  base::StringView name = table.name().GetString(row);
  if (name == "inflate") {
    *description = SliceDescription{
        "Constructing a View hierarchy from pre-processed XML via "
        "LayoutInflater#layout. This includes constructing all of the View "
        "objects in the hierarchy, and applying styled attributes.",
        ""};
    return util::OkStatus();
  }

  if (name == "measure") {
    *description = SliceDescription{
        "First of two phases in view hierarchy layout. Views are asked to size "
        "themselves according to constraints supplied by their parent. Some "
        "ViewGroups may measure a child more than once to help satisfy their "
        "own constraints. Nesting ViewGroups that measure children more than "
        "once can lead to excessive and repeated work.",
        "https://developer.android.com/reference/android/view/"
        "View.html#Layout"};
    return util::OkStatus();
  }

  *description = base::nullopt;
  return util::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto
