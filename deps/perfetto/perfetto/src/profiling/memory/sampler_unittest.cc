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

#include "src/profiling/memory/sampler.h"

#include <thread>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

TEST(SamplerTest, TestLarge) {
  Sampler sampler(512);
  EXPECT_EQ(sampler.SampleSize(1024), 1024u);
}

TEST(SamplerTest, TestSmall) {
  Sampler sampler(512);
  EXPECT_EQ(sampler.SampleSize(511), 512u);
}

TEST(SamplerTest, TestSequence) {
  Sampler sampler(1);
  EXPECT_EQ(sampler.SampleSize(3), 3u);
  EXPECT_EQ(sampler.SampleSize(7), 7u);
  EXPECT_EQ(sampler.SampleSize(5), 5u);
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
