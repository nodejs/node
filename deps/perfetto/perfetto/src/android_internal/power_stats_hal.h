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

#ifndef SRC_ANDROID_INTERNAL_POWER_STATS_HAL_H_
#define SRC_ANDROID_INTERNAL_POWER_STATS_HAL_H_

#include <stddef.h>
#include <stdint.h>

// This header declares proxy functions defined in
// libperfetto_android_internal.so that allow traced_probes to access internal
// android functions (e.g., hwbinder).
// Do not add any include to either perfetto headers or android headers. See
// README.md for more.

namespace perfetto {
namespace android_internal {

struct RailDescriptor {
  // Index corresponding to the rail
  uint32_t index;
  // Name of the rail
  char rail_name[64];
  // Name of the subsystem to which this rail belongs
  char subsys_name[64];
  // Hardware sampling rate
  uint32_t sampling_rate;
};

struct RailEnergyData {
  // Index corresponding to RailDescriptor.index
  uint32_t index;
  // Time since device boot(CLOCK_BOOTTIME) in milli-seconds
  uint64_t timestamp;
  // Accumulated energy since device boot in microwatt-seconds (uWs)
  uint64_t energy;
};

extern "C" {

// These functions are not thread safe unless specified otherwise.

bool __attribute__((visibility("default")))
GetAvailableRails(RailDescriptor*, size_t* size_of_arr);

bool __attribute__((visibility("default")))
GetRailEnergyData(RailEnergyData*, size_t* size_of_arr);

}  // extern "C"

}  // namespace android_internal
}  // namespace perfetto

#endif  // SRC_ANDROID_INTERNAL_POWER_STATS_HAL_H_
