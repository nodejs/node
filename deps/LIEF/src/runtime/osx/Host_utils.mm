#import <Foundation/Foundation.h>
#include <string>

#include <dlfcn.h>

#include "LIEF/runtime/osx/Host.hpp"
#include "runtime/osx/utils.h"
#include "logging.hpp"

namespace LIEF::runtime::osx {

bool Host::is_sip_enabled() {
  using fn_t = int (*)(uint32_t*);
  static fn_t csr_get_active_config = [] {
    auto F = reinterpret_cast<fn_t>(
        dlsym(RTLD_DEFAULT, "csr_get_active_config"));
    if (F == nullptr) {
      LIEF_ERR("dlsym err: {} ({})", dlerror(), Host::os_version_name());
    }
    return F;
  }();

  if (csr_get_active_config == nullptr) {
    return true; // unable to resolve: assume SIP is enabled
  }

  uint32_t config = 0;
  if (csr_get_active_config(&config) != 0) {
    return true; // unable to determine: assume SIP is enabled
  }
  return config == 0;
}

std::string Host::os_version_name() {
  return NSStringToUTF8(NSProcessInfo.processInfo.operatingSystemVersionString);
}

Host::version_t Host::os_version() {
  NSOperatingSystemVersion ver = NSProcessInfo.processInfo.operatingSystemVersion;
  return {
    static_cast<uint32_t>(ver.majorVersion),
    static_cast<uint32_t>(ver.minorVersion),
    static_cast<uint32_t>(ver.patchVersion)
  };
}
}
