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
#include "LIEF/runtime/windows/Host.hpp"
#include "LIEF/runtime/windows/Module.hpp"
#include "LIEF/optional.hpp"
#include "LIEF/utils.hpp"

#include <spdlog/fmt/fmt.h>

#include <winsock.h>
#include <shlobj.h>
#include <shtypes.h>

namespace LIEF::runtime {
inline optional<std::string> get_folder_id(KNOWNFOLDERID id) {
  wchar_t* path = nullptr;
  HRESULT ret = ::SHGetKnownFolderPath(id, /*dwFlags=*/KF_FLAG_DEFAULT,
                                       /*hToken=*/nullptr, &path);
  if (ret != S_OK) {
    return nullopt();
  }
  size_t len = wcsnlen_s(path, 255);
  if (len == 0) {
    return nullopt();
  }
  std::string u8 = u16tou8(reinterpret_cast<const char16_t*>(path), len,
                           /*remove_null_char=*/true);
  return u8;
}

std::string Host::name() {
  std::array<char, MAX_COMPUTERNAME_LENGTH + 1> hostname{};

  DWORD size = sizeof(hostname);

  if (GetComputerNameA(hostname.data(), &size)) {
    return {hostname.data(), size};
  }

  return "";
}

std::string Host::home_dir() {
  return get_folder_id(FOLDERID_Profile).value_or("");
}

std::string Host::tmp_dir() {
  for (const auto& key : {"TMP", "TEMP", "USERPROFILE"}) {
    if (auto path = Process::get_env(key)) {
      return *path;
    }
  }
  return "C:\\Temp";
}

std::string Host::config_dir() {
  return get_folder_id(FOLDERID_LocalAppData).value_or("");
}

std::string Host::cache_dir() {
  return get_folder_id(FOLDERID_LocalAppData).value_or("");
}

namespace windows {
static optional<RTL_OSVERSIONINFOW> get_os_info() {
  // using LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
  using RtlGetVersionPtr = LONG (*)(PRTL_OSVERSIONINFOW);
  RTL_OSVERSIONINFOW os_info{};
  auto ntdll = find_module("ntdll.dll");
  if (ntdll == nullptr) {
    return nullopt();
  }
  void* ptr = ntdll->dlsym("RtlGetVersion");
  if (ptr == nullptr) {
    return nullopt();
  }

  auto _RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(ptr);
  os_info.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
  if (_RtlGetVersion(&os_info) != 0) {
    return nullopt();
  }

  return os_info;
}

Host::version_t Host::version() {
  static const auto OS_VERSION_INFO = get_os_info();
  if (!OS_VERSION_INFO) {
    return {};
  }
  return {
      OS_VERSION_INFO->dwMajorVersion,
      OS_VERSION_INFO->dwMinorVersion,
      OS_VERSION_INFO->dwBuildNumber,
  };
}

}
}
