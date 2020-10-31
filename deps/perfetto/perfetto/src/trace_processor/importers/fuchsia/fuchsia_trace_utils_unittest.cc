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

#include "src/trace_processor/importers/fuchsia/fuchsia_trace_utils.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace fuchsia_trace_utils {
namespace {

TEST(FuchsiaTraceUtilsTest, TicksToNs) {
  ASSERT_EQ(TicksToNs(40, 10), 4000000000);

  // Case where naive computation would overflow
  ASSERT_EQ(TicksToNs(40000000000, 1000000000), 40000000000);

  // Test detecting overflow
  ASSERT_EQ(TicksToNs(40000000000, 1), -1);
}

}  // namespace
}  // namespace fuchsia_trace_utils
}  // namespace trace_processor
}  // namespace perfetto
