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

// Tool that takes in a stream of allocations from stdin and outputs samples
//
// Input format is code_location size tuples, output format is iteration number
// code_location sample_size tuples. The sum of all allocations in the input is
// echoed back in the special iteration 'g'
//
// Example input:
// foo 1
// bar 10
// foo 1000
// baz 1
//
// Example output;
// g foo 1001
// g bar 10
// g baz 1
// 1 foo 1000
// 1 bar 100

#include <iostream>
#include <map>
#include <string>
#include <thread>

#include <unistd.h>

#include "src/profiling/memory/sampler.h"

#include "perfetto/base/logging.h"

namespace perfetto {
namespace profiling {
namespace {

constexpr uint64_t kDefaultSamplingInterval = 128000;

int ProfilingSampleDistributionMain(int argc, char** argv) {
  int opt;
  uint64_t sampling_interval = kDefaultSamplingInterval;
  uint64_t times = 1;
  uint64_t init_seed = 1;

  while ((opt = getopt(argc, argv, "t:i:s:")) != -1) {
    switch (opt) {
      case 'i': {
        char* end;
        long long sampling_interval_arg = strtoll(optarg, &end, 10);
        if (*end != '\0' || *optarg == '\0')
          PERFETTO_FATAL("Invalid sampling interval: %s", optarg);
        PERFETTO_CHECK(sampling_interval_arg > 0);
        sampling_interval = static_cast<uint64_t>(sampling_interval_arg);
        break;
      }
      case 't': {
        char* end;
        long long times_arg = strtoll(optarg, &end, 10);
        if (*end != '\0' || *optarg == '\0')
          PERFETTO_FATAL("Invalid times: %s", optarg);
        PERFETTO_CHECK(times_arg > 0);
        times = static_cast<uint64_t>(times_arg);
        break;
      }
      case 's': {
        char* end;
        init_seed = static_cast<uint64_t>(strtoll(optarg, &end, 10));
        if (*end != '\0' || *optarg == '\0')
          PERFETTO_FATAL("Invalid seed: %s", optarg);
        break;
      }

      default:
        PERFETTO_FATAL("%s [-t times] [-i interval] [-s seed]", argv[0]);
    }
  }

  std::vector<std::pair<std::string, uint64_t>> allocations;

  while (std::cin) {
    std::string callsite;
    uint64_t size;
    std::cin >> callsite;
    if (std::cin.fail()) {
      // Skip trailing newline.
      if (std::cin.eof())
        break;
      PERFETTO_FATAL("Could not read callsite");
    }
    std::cin >> size;
    if (std::cin.fail())
      PERFETTO_FATAL("Could not read size");
    allocations.emplace_back(std::move(callsite), size);
  }
  std::map<std::string, uint64_t> total_ground_truth;
  for (const auto& pair : allocations)
    total_ground_truth[pair.first] += pair.second;

  for (const auto& pair : total_ground_truth)
    std::cout << "g " << pair.first << " " << pair.second << std::endl;

  while (times-- > 0) {
    Sampler sampler(sampling_interval);
    std::map<std::string, uint64_t> totals;
    for (const auto& pair : allocations) {
      size_t sample_size = sampler.SampleSize(pair.second);
      // We also want to add 0 to make downstream processing easier, making
      // sure every iteration has an entry for every key, even if it is
      // zero.
      totals[pair.first] += sample_size;
    }

    for (const auto& pair : totals)
      std::cout << times << " " << pair.first << " " << pair.second
                << std::endl;
  }

  return 0;
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

int main(int argc, char** argv) {
  return perfetto::profiling::ProfilingSampleDistributionMain(argc, argv);
}
