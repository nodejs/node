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
#include "LIEF/platforms.hpp"
#include "LIEF/runtime/Process.hpp"
#include "LIEF/runtime/linux/Process.hpp"

#include "linux_android_shared/Process.hpp"
#include "logging.hpp"

#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#if defined(LIEF_PLATFORM_GLIBC)
  #include <gnu/libc-version.h>
#endif

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern char** environ;

namespace LIEF::runtime {
int32_t Process::pid() {
  return getpid();
}

uint32_t Process::tid() {
  return static_cast<uint32_t>(::syscall(SYS_gettid)); // NOLINT
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
  for (char** env = environ; *env != nullptr; ++env) {
    std::string env_str = *env;
    size_t pos = env_str.find('=');
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

namespace Linux {
std::string Process::glibc_version() {
#if defined(LIEF_PLATFORM_GLIBC)
  return gnu_get_libc_version();
#else
  return "";
#endif
}


std::string Process::cmdline() {
  return LIEF::runtime::linux_android::cmdline().value_or("");
}

}

}
