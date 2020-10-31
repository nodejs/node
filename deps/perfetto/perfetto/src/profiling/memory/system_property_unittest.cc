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

#include "src/profiling/memory/system_property.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

using ::testing::InSequence;
using ::testing::Return;

class MockSystemProperties : public SystemProperties {
 public:
  MOCK_METHOD2(SetAndroidProperty,
               bool(const std::string&, const std::string&));
};

TEST(SystemPropertyTest, All) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "all"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  auto handle = prop.SetAll();
}

TEST(SystemPropertyTest, RefcountAll) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "all"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  {
    auto handle = prop.SetAll();
    { auto handle2 = prop.SetAll(); }
  }
}

TEST(SystemPropertyTest, CleanupAll) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "all"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  { auto handle = prop.SetAll(); }
}

TEST(SystemPropertyTest, Specific) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", ""))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  auto handle2 = prop.SetProperty("system_server");
}

TEST(SystemPropertyTest, RefcountSpecific) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", ""))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  {
    auto handle = prop.SetProperty("system_server");
    { auto handle2 = prop.SetProperty("system_server"); }
  }
}

TEST(SystemPropertyTest, CleanupSpecific) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", ""))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  { auto handle2 = prop.SetProperty("system_server"); }
}

TEST(SystemPropertyTest, AllAndSpecific) {
  MockSystemProperties prop;
  InSequence s;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "all"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", ""))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", ""))
      .WillOnce(Return(true));
  auto handle = prop.SetAll();
  auto handle2 = prop.SetProperty("system_server");
  { SystemProperties::Handle destroy = std::move(handle); }
}

TEST(SystemPropertyTest, AllFailed) {
  MockSystemProperties prop;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "all"))
      .WillOnce(Return(false));
  auto handle = prop.SetAll();
  EXPECT_FALSE(handle);
}

TEST(SystemPropertyTest, SpecificFailed) {
  MockSystemProperties prop;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", "1"))
      .WillOnce(Return(false));
  auto handle = prop.SetProperty("system_server");
  EXPECT_FALSE(handle);
}

TEST(SystemPropertyTest, SpecificFailedMainProperty) {
  MockSystemProperties prop;
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable.system_server", "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(prop, SetAndroidProperty("heapprofd.enable", "1"))
      .WillOnce(Return(false));
  auto handle = prop.SetProperty("system_server");
  EXPECT_FALSE(handle);
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
