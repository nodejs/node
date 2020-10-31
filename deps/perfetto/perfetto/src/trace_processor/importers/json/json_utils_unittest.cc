/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/importers/json/json_utils.h"

#include <json/value.h>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace json {
namespace {

TEST(JsonTraceUtilsTest, CoerceToUint32) {
  ASSERT_EQ(CoerceToUint32(Json::Value(42)).value_or(0), 42u);
  ASSERT_EQ(CoerceToUint32(Json::Value("42")).value_or(0), 42u);
  ASSERT_EQ(CoerceToInt64(Json::Value(42.1)).value_or(-1), 42);
}

TEST(JsonTraceUtilsTest, CoerceToInt64) {
  ASSERT_EQ(CoerceToInt64(Json::Value(42)).value_or(-1), 42);
  ASSERT_EQ(CoerceToInt64(Json::Value("42")).value_or(-1), 42);
  ASSERT_EQ(CoerceToInt64(Json::Value(42.1)).value_or(-1), 42);
  ASSERT_FALSE(CoerceToInt64(Json::Value("foo")).has_value());
  ASSERT_FALSE(CoerceToInt64(Json::Value("1234!")).has_value());

  Json::UInt64 n = 18446744073709551615UL;
  ASSERT_EQ(CoerceToInt64(Json::Value{n}).value_or(0), -1);
}

TEST(JsonTraceUtilsTest, CoerceToTs) {
  ASSERT_EQ(CoerceToTs(TimeUnit::kUs, Json::Value(42)).value_or(-1), 42000);
  ASSERT_EQ(CoerceToTs(TimeUnit::kUs, Json::Value("42")).value_or(-1), 42000);
  ASSERT_EQ(CoerceToTs(TimeUnit::kUs, Json::Value(42.1)).value_or(-1), 42100);
  ASSERT_EQ(CoerceToTs(TimeUnit::kNs, Json::Value(42)).value_or(-1), 42);
  ASSERT_EQ(CoerceToTs(TimeUnit::kMs, Json::Value(42)).value_or(-1), 42000000);
  ASSERT_FALSE(CoerceToTs(TimeUnit::kNs, Json::Value("foo")).has_value());
  ASSERT_FALSE(CoerceToTs(TimeUnit::kNs, Json::Value("1234!")).has_value());
  ASSERT_FALSE(CoerceToTs(TimeUnit::kUs, Json::Value("1234!")).has_value());
  ASSERT_FALSE(CoerceToTs(TimeUnit::kMs, Json::Value("1234!")).has_value());
}

}  // namespace
}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto
