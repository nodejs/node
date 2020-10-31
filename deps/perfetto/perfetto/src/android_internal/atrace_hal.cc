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

#include "src/android_internal/atrace_hal.h"

#include <android/hardware/atrace/1.0/IAtraceDevice.h>
#include <iostream>

namespace perfetto {
namespace android_internal {

using android::hardware::hidl_string;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::atrace::V1_0::IAtraceDevice;
using android::hardware::atrace::V1_0::TracingCategory;

namespace {

android::sp<IAtraceDevice> g_atraceHal;

bool GetService() {
  if (!g_atraceHal)
    g_atraceHal = IAtraceDevice::getService();

  return g_atraceHal != nullptr;
}

}  // namespace

bool ListCategories(TracingVendorCategory* categories, size_t* size_of_arr) {
  const size_t in_array_size = *size_of_arr;
  *size_of_arr = 0;
  if (!GetService())
    return false;

  auto category_cb = [categories, size_of_arr,
                      &in_array_size](hidl_vec<TracingCategory> r) {
    *size_of_arr = std::min(in_array_size, r.size());
    for (int i = 0; i < *size_of_arr; ++i) {
      const TracingCategory& cat = r[i];
      TracingVendorCategory& result = categories[i];
      strncpy(result.name, cat.name.c_str(), sizeof(result.name));
      strncpy(result.description, cat.description.c_str(),
              sizeof(result.description));
      result.name[sizeof(result.name) - 1] = '\0';
      result.description[sizeof(result.description) - 1] = '\0';
    }
  };

  g_atraceHal->listCategories(category_cb);
  return true;
}

bool EnableCategories(const char** categories, size_t categories_count) {
  if (!GetService())
    return false;
  std::vector<hidl_string> args;
  args.resize(categories_count);
  for (size_t i = 0; i < categories_count; ++i) {
    args[i] = categories[i];
  }
  g_atraceHal->enableCategories(args);
  // TODO(hjd): Check status.
  return true;
}

bool DisableAllCategories() {
  if (!GetService())
    return false;
  g_atraceHal->disableAllCategories();
  // TODO(hjd): Check status.
  return true;
}

void ForgetService() {
  g_atraceHal = nullptr;
}

}  // namespace android_internal
}  // namespace perfetto
