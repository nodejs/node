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

#include "src/trace_processor/importers/json/json_tracker.h"

#include "test/gtest_and_gmock.h"

#include <json/value.h>

namespace perfetto {
namespace trace_processor {
namespace {

TEST(JsonTrackerTest, Ns) {
  JsonTracker tracker(nullptr);
  tracker.SetTimeUnit(json::TimeUnit::kNs);
  ASSERT_EQ(tracker.CoerceToTs(Json::Value(42)).value_or(-1), 42);
}

TEST(JsonTraceUtilsTest, Us) {
  JsonTracker tracker(nullptr);
  ASSERT_EQ(tracker.CoerceToTs(Json::Value(42)).value_or(-1), 42000);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
