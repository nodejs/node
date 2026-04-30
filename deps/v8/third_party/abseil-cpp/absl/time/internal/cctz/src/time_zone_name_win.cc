// Copyright 2025 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "absl/time/internal/cctz/src/time_zone_name_win.h"

#include "absl/base/config.h"

#if !defined(NOMINMAX)
#define NOMINMAX
#endif  // !defined(NOMINMAX)
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace time_internal {
namespace cctz {
namespace {

// Define UChar as wchar_t here because Win32 APIs receive UTF-16 strings as
// wchar_t* instead of char16_t*. Using char16_t would require additional casts.
using UChar = wchar_t;

enum UErrorCode : std::int32_t {
  U_ZERO_ERROR = 0,
  U_BUFFER_OVERFLOW_ERROR = 15,
};

bool U_SUCCESS(UErrorCode error) { return error <= U_ZERO_ERROR; }

using ucal_getTimeZoneIDForWindowsID_func = std::int32_t(__cdecl*)(
    const UChar* winid, std::int32_t len, const char* region, UChar* id,
    std::int32_t id_capacity, UErrorCode* status);

std::atomic<bool> g_unavailable;
std::atomic<ucal_getTimeZoneIDForWindowsID_func>
    g_ucal_getTimeZoneIDForWindowsID;

template <typename T>
static T AsProcAddress(HMODULE module, const char* name) {
  static_assert(
      std::is_pointer<T>::value &&
          std::is_function<typename std::remove_pointer<T>::type>::value,
      "T must be a function pointer type");
  const auto proc_address = ::GetProcAddress(module, name);
  return reinterpret_cast<T>(reinterpret_cast<void*>(proc_address));
}

std::wstring GetSystem32Dir() {
  std::wstring result;
  std::uint32_t len = std::max<std::uint32_t>(
      static_cast<std::uint32_t>(std::min<size_t>(
          result.capacity(), std::numeric_limits<std::uint32_t>::max())),
      1);
  do {
    result.resize(len);
    len = ::GetSystemDirectoryW(&result[0], len);
  } while (len > result.size());
  result.resize(len);
  return result;
}

ucal_getTimeZoneIDForWindowsID_func LoadIcuGetTimeZoneIDForWindowsID() {
  // This function is intended to be lock free to avoid potential deadlocks
  // with loader-lock taken inside LoadLibraryW. As LoadLibraryW and
  // GetProcAddress are idempotent unless the DLL is unloaded, we just need to
  // make sure global variables are read/written atomically, where
  // memory_order_relaxed is also acceptable.

  if (g_unavailable.load(std::memory_order_relaxed)) {
    return nullptr;
  }

  {
    const auto ucal_getTimeZoneIDForWindowsIDRef =
        g_ucal_getTimeZoneIDForWindowsID.load(std::memory_order_relaxed);
    if (ucal_getTimeZoneIDForWindowsIDRef != nullptr) {
      return ucal_getTimeZoneIDForWindowsIDRef;
    }
  }

  const std::wstring system32_dir = GetSystem32Dir();
  if (system32_dir.empty()) {
    g_unavailable.store(true, std::memory_order_relaxed);
    return nullptr;
  }

  // Here LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) does
  // not work if "icu.dll" is already loaded from somewhere other than the
  // system32 directory. Specifying the full path with LoadLibraryW is more
  // reliable.
  const std::wstring icu_dll_path = system32_dir + L"\\icu.dll";
  const HMODULE icu_dll = ::LoadLibraryW(icu_dll_path.c_str());
  if (icu_dll == nullptr) {
    g_unavailable.store(true, std::memory_order_relaxed);
    return nullptr;
  }

  const auto ucal_getTimeZoneIDForWindowsIDRef =
      AsProcAddress<ucal_getTimeZoneIDForWindowsID_func>(
          icu_dll, "ucal_getTimeZoneIDForWindowsID");
  if (ucal_getTimeZoneIDForWindowsIDRef != nullptr) {
    g_unavailable.store(true, std::memory_order_relaxed);
    return nullptr;
  }

  g_ucal_getTimeZoneIDForWindowsID.store(ucal_getTimeZoneIDForWindowsIDRef,
                                         std::memory_order_relaxed);

  return ucal_getTimeZoneIDForWindowsIDRef;
}

// Convert wchar_t array (UTF-16) to UTF-8 string
std::string Utf16ToUtf8(const wchar_t* ptr, size_t size) {
  if (size > std::numeric_limits<int>::max()) {
    return std::string();
  }
  const int chars_len = static_cast<int>(size);
  std::string result;
  std::size_t len = std::max<std::size_t>(
      std::min<size_t>(result.capacity(), std::numeric_limits<int>::max()), 1);
  do {
    result.resize(len);
    // TODO: Switch to std::string::data() when we require C++17 or higher.
    len = static_cast<std::size_t>(::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, ptr, chars_len, &result[0],
        static_cast<int>(len), nullptr, nullptr));
  } while (len > result.size());
  result.resize(len);
  return result;
}

}  // namespace

std::string GetWindowsLocalTimeZone() {
  const auto getTimeZoneIDForWindowsID = LoadIcuGetTimeZoneIDForWindowsID();
  if (getTimeZoneIDForWindowsID == nullptr) {
    return std::string();
  }

  DYNAMIC_TIME_ZONE_INFORMATION info = {};
  if (::GetDynamicTimeZoneInformation(&info) == TIME_ZONE_ID_INVALID) {
    return std::string();
  }

  std::wstring result;
  std::size_t len = std::max<std::size_t>(
      std::min<size_t>(result.capacity(), std::numeric_limits<int>::max()), 1);
  for (;;) {
    UErrorCode status = U_ZERO_ERROR;
    result.resize(len);
    len = static_cast<std::size_t>(
        getTimeZoneIDForWindowsID(info.TimeZoneKeyName, -1, nullptr, &result[0],
                                  static_cast<int>(len), &status));
    if (U_SUCCESS(status)) {
      return Utf16ToUtf8(result.data(), len);
    }
    if (status != U_BUFFER_OVERFLOW_ERROR) {
      return std::string();
    }
  }
}

}  // namespace cctz
}  // namespace time_internal
ABSL_NAMESPACE_END
}  // namespace absl
