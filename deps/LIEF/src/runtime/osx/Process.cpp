
/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "LIEF/runtime/Process.hpp"
#include "LIEF/runtime/osx/Process.hpp"
#include "LIEF/optional.hpp"
#include "logging.hpp"

#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>

#include <mach-o/dyld_images.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach-o/dyld_images.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern const char** environ;

namespace LIEF::runtime {
int32_t Process::pid() {
  return getpid();
}

uint32_t Process::tid() {
  return pthread_mach_thread_np(pthread_self());
}

uint32_t Process::page_size() {
  static uint32_t PAGESZ = ::getpagesize();
  return PAGESZ;
}

optional<std::string> Process::get_env(const std::string& key) {
  if (const char* value = ::getenv(key.c_str())) {
    return std::string(value);
  }
  return nullopt();
}

Process::EnvVars Process::get_envs() {
  EnvVars out;
  for (const char** env = environ; *env != nullptr; ++env) {
    std::string env_str = *env;
    size_t pos = env_str.rfind('=');
    if (pos == std::string::npos) {
      LIEF_DEBUG("Error: can't find '=' delimiter in {}", env_str);
      continue;
    }
    std::string key = env_str.substr(0, pos);
    std::string value = env_str.substr(pos + 1);
    out.vars.insert({key, value});
  }
  return out;
}

namespace osx {

static optional<task_dyld_info_data_t> get_dyld_info_data_t() {
  task_dyld_info_data_t dyld_info{};
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;

  kern_return_t kr = task_info(mach_task_self(), TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info), &count);

  if (kr != KERN_SUCCESS) {
    LIEF_ERR("task_info failed with error code: {}", kr);
    return nullopt();
  }
  return dyld_info;
}

static std::string dyld_version_impl() {
  auto dyld_info = get_dyld_info_data_t();
  if (!dyld_info) {
    return "";
  }
  const auto* infos = reinterpret_cast<const dyld_all_image_infos*>(
      dyld_info->all_image_info_addr
  );
  if (infos == nullptr || infos->version < 5 || infos->dyldVersion == nullptr) {
    return "";
  }
  return infos->dyldVersion;
}

std::string Process::dyld_version() {
  static std::string version = dyld_version_impl();
  return version;
}
}

}
