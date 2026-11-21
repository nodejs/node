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
#include "LIEF/runtime/Host.hpp"
#include "LIEF/runtime/Process.hpp"
#include <array>
#include <spdlog/fmt/fmt.h>

#include <unistd.h>
#include <sys/param.h>

namespace LIEF::runtime {

#if defined(_CS_DARWIN_USER_CACHE_DIR)
static constexpr auto CS_DARWIN_USER_CACHE_DIR = _CS_DARWIN_USER_CACHE_DIR;
#else
static constexpr auto CS_DARWIN_USER_CACHE_DIR = 0;
#endif

std::string Host::name() {
  std::array<char, MAXHOSTNAMELEN + 1> buffer = {0};
  if (gethostname(buffer.data(), buffer.size()) == 0) {
    return buffer.data();
  }
  return "";
}

std::string Host::home_dir() {
  if (auto path = Process::get_env("HOME")) {
    return *path;
  }
  return "";
}

std::string Host::tmp_dir() {
  for (const auto& key : {"TMPDIR", "TMP", "TEMP", "TEMPDIR"}) {
    if (auto path = Process::get_env(key)) {
      return *path;
    }
  }
  return "/tmp";
}

std::string Host::config_dir() {
  return home_dir() + "/Library/Preferences";
}

std::string Host::cache_dir() {
  if constexpr (CS_DARWIN_USER_CACHE_DIR != 0) {
    size_t len = confstr(CS_DARWIN_USER_CACHE_DIR, nullptr, 0);
    if (len > 0) {
      std::string result;
      do {
        result.resize(len);
        len = confstr(_CS_DARWIN_USER_CACHE_DIR, result.data(), result.size());
      } while (len > 0 && len != result.size());

      return result.c_str();
    }
  }

  return home_dir() + "/.cache";
}

}
