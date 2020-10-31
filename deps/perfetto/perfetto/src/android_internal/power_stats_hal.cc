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

#include "src/android_internal/power_stats_hal.h"

#include <string.h>

#include <algorithm>

#include <android/hardware/power/stats/1.0/IPowerStats.h>

namespace perfetto {
namespace android_internal {

using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::power::stats::V1_0::EnergyData;
using android::hardware::power::stats::V1_0::IPowerStats;
using android::hardware::power::stats::V1_0::RailInfo;
using android::hardware::power::stats::V1_0::Status;

namespace {

android::sp<IPowerStats> g_svc;

bool GetService() {
  if (!g_svc)
    g_svc = IPowerStats::getService();

  return g_svc != nullptr;
}

}  // namespace

bool GetAvailableRails(RailDescriptor* rail_descriptors, size_t* size_of_arr) {
  const size_t in_array_size = *size_of_arr;
  *size_of_arr = 0;
  if (!GetService())
    return false;

  Status status;
  auto rails_cb = [rail_descriptors, size_of_arr, &in_array_size, &status](
                      hidl_vec<RailInfo> r, Status s) {
    status = s;
    if (status == Status::SUCCESS) {
      *size_of_arr = std::min(in_array_size, r.size());
      for (int i = 0; i < *size_of_arr; ++i) {
        const RailInfo& rail_info = r[i];
        RailDescriptor& descriptor = rail_descriptors[i];

        descriptor.index = rail_info.index;
        descriptor.sampling_rate = rail_info.samplingRate;

        strncpy(descriptor.rail_name, rail_info.railName.c_str(),
                sizeof(descriptor.rail_name));
        strncpy(descriptor.subsys_name, rail_info.subsysName.c_str(),
                sizeof(descriptor.subsys_name));
        descriptor.rail_name[sizeof(descriptor.rail_name) - 1] = '\0';
        descriptor.subsys_name[sizeof(descriptor.subsys_name) - 1] = '\0';
      }
    }
  };

  Return<void> ret = g_svc->getRailInfo(rails_cb);
  return status == Status::SUCCESS;
}

bool GetRailEnergyData(RailEnergyData* rail_energy_array, size_t* size_of_arr) {
  const size_t in_array_size = *size_of_arr;
  *size_of_arr = 0;

  if (!GetService())
    return false;

  Status status;
  auto energy_cb = [rail_energy_array, size_of_arr, &in_array_size, &status](
                       hidl_vec<EnergyData> m, Status s) {
    status = s;
    if (status == Status::SUCCESS) {
      *size_of_arr = std::min(in_array_size, m.size());
      for (int i = 0; i < *size_of_arr; ++i) {
        const EnergyData& measurement = m[i];
        RailEnergyData& element = rail_energy_array[i];

        element.index = measurement.index;
        element.timestamp = measurement.timestamp;
        element.energy = measurement.energy;
      }
    }
  };

  Return<void> ret = g_svc->getEnergyData(hidl_vec<uint32_t>(), energy_cb);
  return status == Status::SUCCESS;
}

}  // namespace android_internal
}  // namespace perfetto
