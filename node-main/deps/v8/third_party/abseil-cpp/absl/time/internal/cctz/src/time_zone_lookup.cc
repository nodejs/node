// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "absl/base/config.h"
#include "absl/time/internal/cctz/include/cctz/time_zone.h"

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CFTimeZone.h>

#include <vector>
#endif

#if defined(__Fuchsia__)
#include <fuchsia/intl/cpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/fdio/directory.h>
#include <zircon/types.h>
#endif

#if defined(_WIN32)
// Include only when <icu.h> is available.
// https://learn.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu-
// https://devblogs.microsoft.com/oldnewthing/20210527-00/?p=105255
#if defined(__has_include)
#if __has_include(<icu.h>)
#define USE_WIN32_LOCAL_TIME_ZONE
#include <windows.h>
#pragma push_macro("_WIN32_WINNT")
#pragma push_macro("NTDDI_VERSION")
// Minimum _WIN32_WINNT and NTDDI_VERSION to use ucal_getTimeZoneIDForWindowsID
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // == _WIN32_WINNT_WIN10
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000004  // == NTDDI_WIN10_RS3
#include <icu.h>
#pragma pop_macro("NTDDI_VERSION")
#pragma pop_macro("_WIN32_WINNT")
#include <timezoneapi.h>

#include <atomic>
#endif  // __has_include(<icu.h>)
#endif  // __has_include
#endif  // _WIN32

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "absl/time/internal/cctz/src/time_zone_fixed.h"
#include "absl/time/internal/cctz/src/time_zone_impl.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace time_internal {
namespace cctz {

namespace {
#if defined(USE_WIN32_LOCAL_TIME_ZONE)
// True if we have already failed to load the API.
static std::atomic_bool g_ucal_getTimeZoneIDForWindowsIDUnavailable;
static std::atomic<decltype(ucal_getTimeZoneIDForWindowsID)*>
    g_ucal_getTimeZoneIDForWindowsIDRef;

std::string win32_local_time_zone() {
  // If we have already failed to load the API, then just give up.
  if (g_ucal_getTimeZoneIDForWindowsIDUnavailable.load()) {
    return "";
  }

  auto ucal_getTimeZoneIDForWindowsIDFunc =
      g_ucal_getTimeZoneIDForWindowsIDRef.load();
  if (ucal_getTimeZoneIDForWindowsIDFunc == nullptr) {
    // If we have already failed to load the API, then just give up.
    if (g_ucal_getTimeZoneIDForWindowsIDUnavailable.load()) {
      return "";
    }

    const HMODULE icudll =
        ::LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (icudll == nullptr) {
      g_ucal_getTimeZoneIDForWindowsIDUnavailable.store(true);
      return "";
    }

    ucal_getTimeZoneIDForWindowsIDFunc =
        reinterpret_cast<decltype(ucal_getTimeZoneIDForWindowsID)*>(
            ::GetProcAddress(icudll, "ucal_getTimeZoneIDForWindowsID"));

    if (ucal_getTimeZoneIDForWindowsIDFunc == nullptr) {
      g_ucal_getTimeZoneIDForWindowsIDUnavailable.store(true);
      return "";
    }
    // store-race is not a problem here, because ::GetProcAddress() returns the
    // same address for the same function in the same DLL.
    g_ucal_getTimeZoneIDForWindowsIDRef.store(
        ucal_getTimeZoneIDForWindowsIDFunc);

    // We intentionally do not call ::FreeLibrary() here to avoid frequent DLL
    // loadings and unloading. As "icu.dll" is a system library, keeping it on
    // memory is supposed to have no major drawback.
  }

  DYNAMIC_TIME_ZONE_INFORMATION info = {};
  if (::GetDynamicTimeZoneInformation(&info) == TIME_ZONE_ID_INVALID) {
    return "";
  }

  std::array<UChar, 128> buffer;
  UErrorCode status = U_ZERO_ERROR;
  const auto num_chars_in_buffer = ucal_getTimeZoneIDForWindowsIDFunc(
      reinterpret_cast<const UChar*>(info.TimeZoneKeyName), -1, nullptr,
      buffer.data(), static_cast<int32_t>(buffer.size()), &status);
  if (status != U_ZERO_ERROR || num_chars_in_buffer <= 0 ||
      num_chars_in_buffer > static_cast<int32_t>(buffer.size())) {
    return "";
  }

  const int num_bytes_in_utf8 = ::WideCharToMultiByte(
      CP_UTF8, 0, reinterpret_cast<const wchar_t*>(buffer.data()),
      static_cast<int>(num_chars_in_buffer), nullptr, 0, nullptr, nullptr);
  std::string local_time_str;
  local_time_str.resize(static_cast<size_t>(num_bytes_in_utf8));
  ::WideCharToMultiByte(
      CP_UTF8, 0, reinterpret_cast<const wchar_t*>(buffer.data()),
      static_cast<int>(num_chars_in_buffer), &local_time_str[0],
      num_bytes_in_utf8, nullptr, nullptr);
  return local_time_str;
}
#endif  // USE_WIN32_LOCAL_TIME_ZONE
}  // namespace

std::string time_zone::name() const { return effective_impl().Name(); }

time_zone::absolute_lookup time_zone::lookup(
    const time_point<seconds>& tp) const {
  return effective_impl().BreakTime(tp);
}

time_zone::civil_lookup time_zone::lookup(const civil_second& cs) const {
  return effective_impl().MakeTime(cs);
}

bool time_zone::next_transition(const time_point<seconds>& tp,
                                civil_transition* trans) const {
  return effective_impl().NextTransition(tp, trans);
}

bool time_zone::prev_transition(const time_point<seconds>& tp,
                                civil_transition* trans) const {
  return effective_impl().PrevTransition(tp, trans);
}

std::string time_zone::version() const { return effective_impl().Version(); }

std::string time_zone::description() const {
  return effective_impl().Description();
}

const time_zone::Impl& time_zone::effective_impl() const {
  if (impl_ == nullptr) {
    // Dereferencing an implicit-UTC time_zone is expected to be
    // rare, so we don't mind paying a small synchronization cost.
    return *time_zone::Impl::UTC().impl_;
  }
  return *impl_;
}

bool load_time_zone(const std::string& name, time_zone* tz) {
  return time_zone::Impl::LoadTimeZone(name, tz);
}

time_zone utc_time_zone() {
  return time_zone::Impl::UTC();  // avoid name lookup
}

time_zone fixed_time_zone(const seconds& offset) {
  time_zone tz;
  load_time_zone(FixedOffsetToName(offset), &tz);
  return tz;
}

time_zone local_time_zone() {
  const char* zone = ":localtime";
#if defined(__ANDROID__)
  char sysprop[PROP_VALUE_MAX];
  if (__system_property_get("persist.sys.timezone", sysprop) > 0) {
    zone = sysprop;
  }
#endif
#if defined(__APPLE__)
  std::vector<char> buffer;
  CFTimeZoneRef tz_default = CFTimeZoneCopyDefault();
  if (CFStringRef tz_name = CFTimeZoneGetName(tz_default)) {
    CFStringEncoding encoding = kCFStringEncodingUTF8;
    CFIndex length = CFStringGetLength(tz_name);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, encoding) + 1;
    buffer.resize(static_cast<size_t>(max_size));
    if (CFStringGetCString(tz_name, &buffer[0], max_size, encoding)) {
      zone = &buffer[0];
    }
  }
  CFRelease(tz_default);
#endif
#if defined(__Fuchsia__)
  std::string primary_tz;
  [&]() {
    // Note: We can't use the synchronous FIDL API here because it doesn't
    // allow timeouts; if the FIDL call failed, local_time_zone() would never
    // return.

    const zx::duration kTimeout = zx::msec(500);

    // Don't attach to the thread because otherwise the thread's dispatcher
    // would be set to null when the loop is destroyed, causing any other FIDL
    // code running on the same thread to crash.
    async::Loop loop(&kAsyncLoopConfigNeverAttachToThread);

    fuchsia::intl::PropertyProviderHandle handle;
    zx_status_t status = fdio_service_connect_by_name(
        fuchsia::intl::PropertyProvider::Name_,
        handle.NewRequest().TakeChannel().release());
    if (status != ZX_OK) {
      return;
    }

    fuchsia::intl::PropertyProviderPtr intl_provider;
    status = intl_provider.Bind(std::move(handle), loop.dispatcher());
    if (status != ZX_OK) {
      return;
    }

    intl_provider->GetProfile(
        [&loop, &primary_tz](fuchsia::intl::Profile profile) {
          if (!profile.time_zones().empty()) {
            primary_tz = profile.time_zones()[0].id;
          }
          loop.Quit();
        });
    loop.Run(zx::deadline_after(kTimeout));
  }();

  if (!primary_tz.empty()) {
    zone = primary_tz.c_str();
  }
#endif
#if defined(USE_WIN32_LOCAL_TIME_ZONE)
  std::string win32_tz = win32_local_time_zone();
  if (!win32_tz.empty()) {
    zone = win32_tz.c_str();
  }
#endif

  // Allow ${TZ} to override to default zone.
  char* tz_env = nullptr;
#if defined(_MSC_VER)
  _dupenv_s(&tz_env, nullptr, "TZ");
#else
  tz_env = std::getenv("TZ");
#endif
  if (tz_env) zone = tz_env;

  // We only support the "[:]<zone-name>" form.
  if (*zone == ':') ++zone;

  // Map "localtime" to a system-specific name, but
  // allow ${LOCALTIME} to override the default name.
  char* localtime_env = nullptr;
  if (strcmp(zone, "localtime") == 0) {
#if defined(_MSC_VER)
    // System-specific default is just "localtime".
    _dupenv_s(&localtime_env, nullptr, "LOCALTIME");
#else
    zone = "/etc/localtime";  // System-specific default.
    localtime_env = std::getenv("LOCALTIME");
#endif
    if (localtime_env) zone = localtime_env;
  }

  const std::string name = zone;
#if defined(_MSC_VER)
  free(localtime_env);
  free(tz_env);
#endif

  time_zone tz;
  load_time_zone(name, &tz);  // Falls back to UTC.
  // TODO: Follow the RFC3339 "Unknown Local Offset Convention" and
  // arrange for %z to generate "-0000" when we don't know the local
  // offset because the load_time_zone() failed and we're using UTC.
  return tz;
}

}  // namespace cctz
}  // namespace time_internal
ABSL_NAMESPACE_END
}  // namespace absl
