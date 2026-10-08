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
#include <memory>
#include "LIEF/runtime/Process.hpp"
#include "LIEF/runtime/windows/Process.hpp"
#include "LIEF/runtime/windows/PEB.hpp"
#include "LIEF/utils.hpp"
#include "logging.hpp"

#include <Windows.h>
#include <sysinfoapi.h>
#include <processenv.h>

namespace LIEF::runtime {
int32_t Process::pid() {
  return GetCurrentProcessId();
}

uint32_t Process::tid() {
  return GetCurrentThreadId();
}

uint32_t Process::page_size() {
  static uint32_t PAGESZ = [] {
    SYSTEM_INFO info = {};
    GetNativeSystemInfo(&info);
    return (uint32_t)info.dwPageSize;
  }();
  return PAGESZ;
}

optional<std::string> Process::get_env(const std::string& key) {
  auto u16key = u8tou16(key);
  if (!u16key) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return nullopt();
  }
  std::array<wchar_t, MAX_PATH> value{};

  size_t size =
      GetEnvironmentVariableW(reinterpret_cast<const wchar_t*>(u16key->c_str()),
                              value.data(), value.size());

  if (size == 0) {
    LIEF_DEBUG("Error: {}:{}. GetEnvironmentVariableW('{}') failed", __FUNCTION__,
               __LINE__, key);
    return nullopt();
  }

  if (size <= value.size()) {
    return u16tou8(reinterpret_cast<const char16_t*>(value.data()), size);
  }

  std::vector<wchar_t> out(size, 0);
  size = GetEnvironmentVariableW(reinterpret_cast<const wchar_t*>(u16key->c_str()),
                                 out.data(), out.size());

  if (size <= out.size()) {
    return u16tou8(reinterpret_cast<const char16_t*>(out.data()), size);
  }

  return nullopt();
}

Process::EnvVars Process::get_envs() {
  EnvVars out;
  auto raw_vars = std::unique_ptr<wchar_t[], decltype(&FreeEnvironmentStringsW)>(
      GetEnvironmentStringsW(), FreeEnvironmentStringsW
  );

  if (raw_vars == nullptr) {
    return out;
  }

  size_t i = 0;
  std::vector<std::wstring> vars;
  while (raw_vars[i] != '\0') {
    vars.emplace_back(&raw_vars[i]);
    i += vars.back().size() + 1;
  }

  out.vars.reserve(vars.size());

  for (const std::wstring& var : vars) {
    std::string u8var = u16tou8(reinterpret_cast<const char16_t*>(var.c_str()),
                                var.size(), /*remove_null_char=*/true);
    if (u8var.empty()) {
      continue;
    }
    size_t pos = u8var.rfind('=');

    if (pos == std::string::npos) {
      LIEF_DEBUG("Error: can't find '=' delimiter in {}", u8var);
      continue;
    }
    std::string key = u8var.substr(0, pos);
    std::string value = u8var.substr(pos + 1);
    out.vars.insert({key, value});
  }
  return out;
}

namespace windows {
std::unique_ptr<PEB> Process::peb() {
  return PEB::create();
}
}

}
