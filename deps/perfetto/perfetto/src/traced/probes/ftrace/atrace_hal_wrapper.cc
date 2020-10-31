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

#include "src/traced/probes/ftrace/atrace_hal_wrapper.h"

#include "perfetto/base/build_config.h"
#include "src/android_internal/atrace_hal.h"
#include "src/android_internal/lazy_library_loader.h"

namespace perfetto {

namespace {
constexpr size_t kMaxNumCategories = 64;
}

struct AtraceHalWrapper::DynamicLibLoader {
  PERFETTO_LAZY_LOAD(android_internal::ForgetService, forget_service_);
  PERFETTO_LAZY_LOAD(android_internal::ListCategories, list_categories_);
  PERFETTO_LAZY_LOAD(android_internal::EnableCategories, enable_categories_);
  PERFETTO_LAZY_LOAD(android_internal::DisableAllCategories,
                     disable_all_categories_);

  std::vector<std::string> ListCategories() {
    std::vector<std::string> results;
    if (!list_categories_)
      return results;

    std::vector<android_internal::TracingVendorCategory> categories(
        kMaxNumCategories);
    size_t num_cat = categories.size();
    bool success = list_categories_(&categories[0], &num_cat);
    if (!success)
      return results;
    categories.resize(num_cat);

    for (const auto& category : categories) {
      results.push_back(category.name);
    }

    return results;
  }

  bool EnableCategories(const std::vector<std::string>& categories) {
    if (!enable_categories_)
      return false;
    std::vector<const char*> args;
    for (const std::string& category : categories) {
      args.push_back(category.c_str());
    }
    return enable_categories_(&args[0], args.size());
  }

  bool DisableAllCategories() {
    if (!disable_all_categories_)
      return false;
    return disable_all_categories_();
  }

  void ForgetService() {
    if (!forget_service_)
      return;
    forget_service_();
  }
};

AtraceHalWrapper::AtraceHalWrapper() {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  lib_.reset(new DynamicLibLoader());
#endif
}

AtraceHalWrapper::~AtraceHalWrapper() {
  if (lib_)
    lib_->ForgetService();
}

std::vector<std::string> AtraceHalWrapper::ListCategories() {
  if (!lib_)
    return {};
  return lib_->ListCategories();
}

bool AtraceHalWrapper::EnableCategories(
    const std::vector<std::string>& categories) {
  if (!lib_)
    return true;
  return lib_->EnableCategories(categories);
}

bool AtraceHalWrapper::DisableAllCategories() {
  if (!lib_)
    return true;
  return lib_->DisableAllCategories();
}

}  // namespace perfetto
