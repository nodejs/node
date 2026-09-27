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
#include "LIEF/runtime/linux/Host.hpp"

#include <array>
#include <unistd.h>
#include <climits>
#include <sys/utsname.h>

namespace LIEF::runtime {
std::string Host::name() {
  std::array<char, HOST_NAME_MAX + 1> buffer = {0};
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
  if (auto path = Process::get_env("XDG_CONFIG_HOME")) {
    return *path;
  }

  return home_dir() + "/.config";
}

std::string Host::cache_dir() {
  if (auto path = Process::get_env("XDG_CACHE_HOME")) {
    return *path;
  }

  return home_dir() + "/.cache";
}

namespace Linux {

static const utsname& sys_info() {
  static utsname SYS_INFO = [] {
    struct utsname tmp{};
    uname(&tmp);
    return tmp;
  }();
  return SYS_INFO;
}

std::string Host::sys_name() {
  return sys_info().sysname;
}

std::string Host::sys_release() {
  return sys_info().release;
}

std::string Host::sys_version() {
  return sys_info().version;
}

std::string Host::hardware() {
  return sys_info().machine;
}

}

}
