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

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <sys/system_properties.h>
#endif

namespace perfetto {
namespace profiling {

SystemProperties::Handle::Handle(Handle&& other) {
  system_properties_ = other.system_properties_;
  property_ = std::move(other.property_);
  all_ = other.all_;
  other.system_properties_ = nullptr;
}

SystemProperties::Handle& SystemProperties::Handle::operator=(Handle&& other) {
  // Construct this temporary because the RHS could be an lvalue cast to an
  // rvalue reference whose lifetime we do not know.
  Handle tmp(std::move(other));
  using std::swap;
  swap(*this, tmp);
  return *this;
}

SystemProperties::Handle::Handle(SystemProperties* system_properties)
    : system_properties_(system_properties), all_(true) {}

SystemProperties::Handle::Handle(SystemProperties* system_properties,
                                 std::string property)
    : system_properties_(system_properties), property_(std::move(property)) {}

SystemProperties::Handle::~Handle() {
  if (system_properties_) {
    if (all_)
      system_properties_->UnsetAll();
    else
      system_properties_->UnsetProperty(property_);
  }
}

SystemProperties::Handle::operator bool() {
  return system_properties_ != nullptr;
}

SystemProperties::Handle SystemProperties::SetProperty(std::string name) {
  auto it = properties_.find(name);
  if (it == properties_.end()) {
    if (!SetAndroidProperty("heapprofd.enable." + name, "1"))
      return Handle(nullptr);
    if (properties_.size() == 1 || alls_ == 0) {
      if (!SetAndroidProperty("heapprofd.enable", "1"))
        return Handle(nullptr);
    }
    properties_.emplace(name, 1);
  } else {
    it->second++;
  }
  return Handle(this, std::move(name));
}

SystemProperties::Handle SystemProperties::SetAll() {
  if (alls_ == 0) {
    if (!SetAndroidProperty("heapprofd.enable", "all"))
      return Handle(nullptr);
  }
  alls_++;
  return Handle(this);
}

// This is conditionally noreturn, so disable the warning.
#pragma GCC diagnostic push
#if PERFETTO_DCHECK_IS_ON()
#pragma GCC diagnostic ignored "-Wmissing-noreturn"
#endif

// static
void SystemProperties::ResetHeapprofdProperties() {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  int r = __system_property_foreach(
      [](const prop_info* pi, void*) {
        __system_property_read_callback(
            pi,
            [](void*, const char* name, const char*, uint32_t) {
              constexpr char kDebugModePropName[] = "heapprofd.userdebug.mode";

              // Unset everything starting with "heapprofd.", except for the
              // property stating which mode to use on debug builds.
              const char* found = strstr(name, "heapprofd.");
              if (found == name && strncmp(name, kDebugModePropName,
                                           strlen(kDebugModePropName))) {
                int ret = __system_property_set(name, "");
                PERFETTO_DCHECK(ret == 0);
              }
            },
            nullptr);
      },
      nullptr);
  PERFETTO_DCHECK(r == 0);
#else
  PERFETTO_DFATAL_OR_ELOG(
      "Cannot ResetHeapprofdProperties on out-of-tree builds.");
#endif
}

#pragma GCC diagnostic pop

SystemProperties::~SystemProperties() {
  PERFETTO_DCHECK(alls_ == 0 && properties_.empty());
}

bool SystemProperties::SetAndroidProperty(const std::string& name,
                                          const std::string& value) {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  return __system_property_set(name.c_str(), value.c_str()) == 0;
#else
  // Allow this to be mocked out for tests on other platforms.
  base::ignore_result(name);
  base::ignore_result(value);
  PERFETTO_FATAL("Properties can only be set on Android.");
#endif
}

void SystemProperties::UnsetProperty(const std::string& name) {
  auto it = properties_.find(name);
  if (it == properties_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Unsetting unknown property.");
    return;
  }
  if (--(it->second) == 0) {
    properties_.erase(it);
    SetAndroidProperty("heapprofd.enable." + name, "");
    if (properties_.empty() && alls_ == 0)
      SetAndroidProperty("heapprofd.enable", "");
  }
}

void SystemProperties::UnsetAll() {
  if (--alls_ == 0) {
    if (properties_.empty())
      SetAndroidProperty("heapprofd.enable", "");
    else
      SetAndroidProperty("heapprofd.enable", "1");
  }
}

void swap(SystemProperties::Handle& a, SystemProperties::Handle& b) {
  using std::swap;
  swap(a.system_properties_, b.system_properties_);
  swap(a.property_, b.property_);
  swap(a.all_, b.all_);
}

}  // namespace profiling
}  // namespace perfetto
