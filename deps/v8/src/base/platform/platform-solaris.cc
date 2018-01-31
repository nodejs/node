// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for Solaris 10 goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

#ifdef __sparc
# error "V8 does not support the SPARC CPU architecture."
#endif

#include <dlfcn.h>  // dladdr
#include <errno.h>
#include <ieeefp.h>  // finite()
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>  // sigemptyset(), etc
#include <sys/mman.h>  // mmap()
#include <sys/regset.h>
#include <sys/stack.h>  // for stack alignment
#include <sys/time.h>  // gettimeofday(), timeradd()
#include <time.h>
#include <ucontext.h>  // walkstack(), getcontext()
#include <unistd.h>  // getpagesize(), usleep()

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

class SolarisTimezoneCache : public PosixTimezoneCache {
  const char* LocalTimezone(double time) override;

  double LocalTimeOffset() override;

  ~SolarisTimezoneCache() override {}
};

const char* SolarisTimezoneCache::LocalTimezone(double time) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(std::floor(time/msPerSecond));
  struct tm tm;
  struct tm* t = localtime_r(&tv, &tm);
  if (nullptr == t) return "";
  return tzname[0];  // The location of the timezone string on Solaris.
}

double SolarisTimezoneCache::LocalTimeOffset() {
  tzset();
  return -static_cast<double>(timezone * msPerSecond);
}

TimezoneCache* OS::CreateTimezoneCache() { return new SolarisTimezoneCache(); }

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  return std::vector<SharedLibraryAddress>();
}

void OS::SignalCodeMovingGC() {}

}  // namespace base
}  // namespace v8
