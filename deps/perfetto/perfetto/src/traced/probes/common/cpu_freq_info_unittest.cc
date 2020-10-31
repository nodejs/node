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

#include "src/traced/probes/common/cpu_freq_info_for_testing.h"

#include "test/gtest_and_gmock.h"

using ::testing::ElementsAre;

namespace perfetto {
namespace {

class CpuFreqInfoTest : public ::testing::Test {
 protected:
  std::unique_ptr<CpuFreqInfo> GetCpuFreqInfo() {
    return std::unique_ptr<CpuFreqInfo>(
        cpu_freq_info_for_testing.GetInstance());
  }

  CpuFreqInfoForTesting cpu_freq_info_for_testing;
};

std::vector<uint32_t> FreqsToVector(CpuFreqInfo::Range range) {
  std::vector<uint32_t> freqs;
  for (auto it = range.first; it != range.second; it++)
    freqs.push_back(*it);
  return freqs;
}

TEST_F(CpuFreqInfoTest, CpuFreqInfo) {
  auto cpu_freq_info = GetCpuFreqInfo();

  EXPECT_THAT(FreqsToVector(cpu_freq_info->GetFreqs(0u)),
              ElementsAre(300000, 576000, 748800, 998400, 1209600, 1324800,
                          1516800, 1612800, 1708800));
  EXPECT_THAT(FreqsToVector(cpu_freq_info->GetFreqs(1u)),
              ElementsAre(300000, 652800, 825600, 979200, 1132800, 1363200,
                          1536000, 1747200, 1843200, 1996800));
  EXPECT_THAT(FreqsToVector(cpu_freq_info->GetFreqs(2u)), ElementsAre());
  EXPECT_THAT(FreqsToVector(cpu_freq_info->GetFreqs(100u)), ElementsAre());

  EXPECT_EQ(cpu_freq_info->GetCpuFreqIndex(0u, 300000u), 1u);
  EXPECT_EQ(cpu_freq_info->GetCpuFreqIndex(0u, 748800u), 3u);
  EXPECT_EQ(cpu_freq_info->GetCpuFreqIndex(1u, 300000u), 10u);
  EXPECT_EQ(cpu_freq_info->GetCpuFreqIndex(1u, 1996800u), 19u);
  EXPECT_EQ(cpu_freq_info->GetCpuFreqIndex(1u, 5u), 0u);
}

}  // namespace
}  // namespace perfetto
