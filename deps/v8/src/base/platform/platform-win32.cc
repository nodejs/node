// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for Win32.

// Secure API functions are not available using MinGW with msvcrt.dll
// on Windows XP. Make sure MINGW_HAS_SECURE_API is not defined to
// disable definition of secure API functions in standard headers that
// would conflict with our own implementation.
#ifdef __MINGW32__
#include <_mingw.h>
#ifdef MINGW_HAS_SECURE_API
#undef MINGW_HAS_SECURE_API
#endif  // MINGW_HAS_SECURE_API
#endif  // __MINGW32__

#ifdef _MSC_VER
#include <limits>
#endif

#include "src/base/win32-headers.h"

#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/utils/random-number-generator.h"

#ifdef _MSC_VER

// Case-insensitive bounded string comparisons. Use stricmp() on Win32. Usually
// defined in strings.h.
int strncasecmp(const char* s1, const char* s2, int n) {
  return _strnicmp(s1, s2, n);
}

#endif  // _MSC_VER


// Extra functions for MinGW. Most of these are the _s functions which are in
// the Microsoft Visual Studio C++ CRT.
#ifdef __MINGW32__


#ifndef __MINGW64_VERSION_MAJOR

#define _TRUNCATE 0
#define STRUNCATE 80

inline void MemoryBarrier() {
  int barrier = 0;
  __asm__ __volatile__("xchgl %%eax,%0 ":"=r" (barrier));
}

#endif  // __MINGW64_VERSION_MAJOR


int localtime_s(tm* out_tm, const time_t* time) {
  tm* posix_local_time_struct = localtime(time);
  if (posix_local_time_struct == NULL) return 1;
  *out_tm = *posix_local_time_struct;
  return 0;
}


int fopen_s(FILE** pFile, const char* filename, const char* mode) {
  *pFile = fopen(filename, mode);
  return *pFile != NULL ? 0 : 1;
}

int _vsnprintf_s(char* buffer, size_t sizeOfBuffer, size_t count,
                 const char* format, va_list argptr) {
  DCHECK(count == _TRUNCATE);
  return _vsnprintf(buffer, sizeOfBuffer, format, argptr);
}


int strncpy_s(char* dest, size_t dest_size, const char* source, size_t count) {
  CHECK(source != NULL);
  CHECK(dest != NULL);
  CHECK_GT(dest_size, 0);

  if (count == _TRUNCATE) {
    while (dest_size > 0 && *source != 0) {
      *(dest++) = *(source++);
      --dest_size;
    }
    if (dest_size == 0) {
      *(dest - 1) = 0;
      return STRUNCATE;
    }
  } else {
    while (dest_size > 0 && count > 0 && *source != 0) {
      *(dest++) = *(source++);
      --dest_size;
      --count;
    }
  }
  CHECK_GT(dest_size, 0);
  *dest = 0;
  return 0;
}

#endif  // __MINGW32__

namespace v8 {
namespace base {

namespace {

bool g_hard_abort = false;

}  // namespace

intptr_t OS::MaxVirtualMemory() {
  return 0;
}


class TimezoneCache {
 public:
  TimezoneCache() : initialized_(false) { }

  void Clear() {
    initialized_ = false;
  }

  // Initialize timezone information. The timezone information is obtained from
  // windows. If we cannot get the timezone information we fall back to CET.
  void InitializeIfNeeded() {
    // Just return if timezone information has already been initialized.
    if (initialized_) return;

    // Initialize POSIX time zone data.
    _tzset();
    // Obtain timezone information from operating system.
    memset(&tzinfo_, 0, sizeof(tzinfo_));
    if (GetTimeZoneInformation(&tzinfo_) == TIME_ZONE_ID_INVALID) {
      // If we cannot get timezone information we fall back to CET.
      tzinfo_.Bias = -60;
      tzinfo_.StandardDate.wMonth = 10;
      tzinfo_.StandardDate.wDay = 5;
      tzinfo_.StandardDate.wHour = 3;
      tzinfo_.StandardBias = 0;
      tzinfo_.DaylightDate.wMonth = 3;
      tzinfo_.DaylightDate.wDay = 5;
      tzinfo_.DaylightDate.wHour = 2;
      tzinfo_.DaylightBias = -60;
    }

    // Make standard and DST timezone names.
    WideCharToMultiByte(CP_UTF8, 0, tzinfo_.StandardName, -1,
                        std_tz_name_, kTzNameSize, NULL, NULL);
    std_tz_name_[kTzNameSize - 1] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, tzinfo_.DaylightName, -1,
                        dst_tz_name_, kTzNameSize, NULL, NULL);
    dst_tz_name_[kTzNameSize - 1] = '\0';

    // If OS returned empty string or resource id (like "@tzres.dll,-211")
    // simply guess the name from the UTC bias of the timezone.
    // To properly resolve the resource identifier requires a library load,
    // which is not possible in a sandbox.
    if (std_tz_name_[0] == '\0' || std_tz_name_[0] == '@') {
      OS::SNPrintF(std_tz_name_, kTzNameSize - 1,
                   "%s Standard Time",
                   GuessTimezoneNameFromBias(tzinfo_.Bias));
    }
    if (dst_tz_name_[0] == '\0' || dst_tz_name_[0] == '@') {
      OS::SNPrintF(dst_tz_name_, kTzNameSize - 1,
                   "%s Daylight Time",
                   GuessTimezoneNameFromBias(tzinfo_.Bias));
    }
    // Timezone information initialized.
    initialized_ = true;
  }

  // Guess the name of the timezone from the bias.
  // The guess is very biased towards the northern hemisphere.
  const char* GuessTimezoneNameFromBias(int bias) {
    static const int kHour = 60;
    switch (-bias) {
      case -9*kHour: return "Alaska";
      case -8*kHour: return "Pacific";
      case -7*kHour: return "Mountain";
      case -6*kHour: return "Central";
      case -5*kHour: return "Eastern";
      case -4*kHour: return "Atlantic";
      case  0*kHour: return "GMT";
      case +1*kHour: return "Central Europe";
      case +2*kHour: return "Eastern Europe";
      case +3*kHour: return "Russia";
      case +5*kHour + 30: return "India";
      case +8*kHour: return "China";
      case +9*kHour: return "Japan";
      case +12*kHour: return "New Zealand";
      default: return "Local";
    }
  }


 private:
  static const int kTzNameSize = 128;
  bool initialized_;
  char std_tz_name_[kTzNameSize];
  char dst_tz_name_[kTzNameSize];
  TIME_ZONE_INFORMATION tzinfo_;
  friend class Win32Time;
};


// ----------------------------------------------------------------------------
// The Time class represents time on win32. A timestamp is represented as
// a 64-bit integer in 100 nanoseconds since January 1, 1601 (UTC). JavaScript
// timestamps are represented as a doubles in milliseconds since 00:00:00 UTC,
// January 1, 1970.

class Win32Time {
 public:
  // Constructors.
  Win32Time();
  explicit Win32Time(double jstime);
  Win32Time(int year, int mon, int day, int hour, int min, int sec);

  // Convert timestamp to JavaScript representation.
  double ToJSTime();

  // Set timestamp to current time.
  void SetToCurrentTime();

  // Returns the local timezone offset in milliseconds east of UTC. This is
  // the number of milliseconds you must add to UTC to get local time, i.e.
  // LocalOffset(CET) = 3600000 and LocalOffset(PST) = -28800000. This
  // routine also takes into account whether daylight saving is effect
  // at the time.
  int64_t LocalOffset(TimezoneCache* cache);

  // Returns the daylight savings time offset for the time in milliseconds.
  int64_t DaylightSavingsOffset(TimezoneCache* cache);

  // Returns a string identifying the current timezone for the
  // timestamp taking into account daylight saving.
  char* LocalTimezone(TimezoneCache* cache);

 private:
  // Constants for time conversion.
  static const int64_t kTimeEpoc = 116444736000000000LL;
  static const int64_t kTimeScaler = 10000;
  static const int64_t kMsPerMinute = 60000;

  // Constants for timezone information.
  static const bool kShortTzNames = false;

  // Return whether or not daylight savings time is in effect at this time.
  bool InDST(TimezoneCache* cache);

  // Accessor for FILETIME representation.
  FILETIME& ft() { return time_.ft_; }

  // Accessor for integer representation.
  int64_t& t() { return time_.t_; }

  // Although win32 uses 64-bit integers for representing timestamps,
  // these are packed into a FILETIME structure. The FILETIME structure
  // is just a struct representing a 64-bit integer. The TimeStamp union
  // allows access to both a FILETIME and an integer representation of
  // the timestamp.
  union TimeStamp {
    FILETIME ft_;
    int64_t t_;
  };

  TimeStamp time_;
};


// Initialize timestamp to start of epoc.
Win32Time::Win32Time() {
  t() = 0;
}


// Initialize timestamp from a JavaScript timestamp.
Win32Time::Win32Time(double jstime) {
  t() = static_cast<int64_t>(jstime) * kTimeScaler + kTimeEpoc;
}


// Initialize timestamp from date/time components.
Win32Time::Win32Time(int year, int mon, int day, int hour, int min, int sec) {
  SYSTEMTIME st;
  st.wYear = year;
  st.wMonth = mon;
  st.wDay = day;
  st.wHour = hour;
  st.wMinute = min;
  st.wSecond = sec;
  st.wMilliseconds = 0;
  SystemTimeToFileTime(&st, &ft());
}


// Convert timestamp to JavaScript timestamp.
double Win32Time::ToJSTime() {
  return static_cast<double>((t() - kTimeEpoc) / kTimeScaler);
}


// Set timestamp to current time.
void Win32Time::SetToCurrentTime() {
  // The default GetSystemTimeAsFileTime has a ~15.5ms resolution.
  // Because we're fast, we like fast timers which have at least a
  // 1ms resolution.
  //
  // timeGetTime() provides 1ms granularity when combined with
  // timeBeginPeriod().  If the host application for v8 wants fast
  // timers, it can use timeBeginPeriod to increase the resolution.
  //
  // Using timeGetTime() has a drawback because it is a 32bit value
  // and hence rolls-over every ~49days.
  //
  // To use the clock, we use GetSystemTimeAsFileTime as our base;
  // and then use timeGetTime to extrapolate current time from the
  // start time.  To deal with rollovers, we resync the clock
  // any time when more than kMaxClockElapsedTime has passed or
  // whenever timeGetTime creates a rollover.

  static bool initialized = false;
  static TimeStamp init_time;
  static DWORD init_ticks;
  static const int64_t kHundredNanosecondsPerSecond = 10000000;
  static const int64_t kMaxClockElapsedTime =
      60*kHundredNanosecondsPerSecond;  // 1 minute

  // If we are uninitialized, we need to resync the clock.
  bool needs_resync = !initialized;

  // Get the current time.
  TimeStamp time_now;
  GetSystemTimeAsFileTime(&time_now.ft_);
  DWORD ticks_now = timeGetTime();

  // Check if we need to resync due to clock rollover.
  needs_resync |= ticks_now < init_ticks;

  // Check if we need to resync due to elapsed time.
  needs_resync |= (time_now.t_ - init_time.t_) > kMaxClockElapsedTime;

  // Check if we need to resync due to backwards time change.
  needs_resync |= time_now.t_ < init_time.t_;

  // Resync the clock if necessary.
  if (needs_resync) {
    GetSystemTimeAsFileTime(&init_time.ft_);
    init_ticks = ticks_now = timeGetTime();
    initialized = true;
  }

  // Finally, compute the actual time.  Why is this so hard.
  DWORD elapsed = ticks_now - init_ticks;
  this->time_.t_ = init_time.t_ + (static_cast<int64_t>(elapsed) * 10000);
}


// Return the local timezone offset in milliseconds east of UTC. This
// takes into account whether daylight saving is in effect at the time.
// Only times in the 32-bit Unix range may be passed to this function.
// Also, adding the time-zone offset to the input must not overflow.
// The function EquivalentTime() in date.js guarantees this.
int64_t Win32Time::LocalOffset(TimezoneCache* cache) {
  cache->InitializeIfNeeded();

  Win32Time rounded_to_second(*this);
  rounded_to_second.t() = rounded_to_second.t() / 1000 / kTimeScaler *
      1000 * kTimeScaler;
  // Convert to local time using POSIX localtime function.
  // Windows XP Service Pack 3 made SystemTimeToTzSpecificLocalTime()
  // very slow.  Other browsers use localtime().

  // Convert from JavaScript milliseconds past 1/1/1970 0:00:00 to
  // POSIX seconds past 1/1/1970 0:00:00.
  double unchecked_posix_time = rounded_to_second.ToJSTime() / 1000;
  if (unchecked_posix_time > INT_MAX || unchecked_posix_time < 0) {
    return 0;
  }
  // Because _USE_32BIT_TIME_T is defined, time_t is a 32-bit int.
  time_t posix_time = static_cast<time_t>(unchecked_posix_time);

  // Convert to local time, as struct with fields for day, hour, year, etc.
  tm posix_local_time_struct;
  if (localtime_s(&posix_local_time_struct, &posix_time)) return 0;

  if (posix_local_time_struct.tm_isdst > 0) {
    return (cache->tzinfo_.Bias + cache->tzinfo_.DaylightBias) * -kMsPerMinute;
  } else if (posix_local_time_struct.tm_isdst == 0) {
    return (cache->tzinfo_.Bias + cache->tzinfo_.StandardBias) * -kMsPerMinute;
  } else {
    return cache->tzinfo_.Bias * -kMsPerMinute;
  }
}


// Return whether or not daylight savings time is in effect at this time.
bool Win32Time::InDST(TimezoneCache* cache) {
  cache->InitializeIfNeeded();

  // Determine if DST is in effect at the specified time.
  bool in_dst = false;
  if (cache->tzinfo_.StandardDate.wMonth != 0 ||
      cache->tzinfo_.DaylightDate.wMonth != 0) {
    // Get the local timezone offset for the timestamp in milliseconds.
    int64_t offset = LocalOffset(cache);

    // Compute the offset for DST. The bias parameters in the timezone info
    // are specified in minutes. These must be converted to milliseconds.
    int64_t dstofs =
        -(cache->tzinfo_.Bias + cache->tzinfo_.DaylightBias) * kMsPerMinute;

    // If the local time offset equals the timezone bias plus the daylight
    // bias then DST is in effect.
    in_dst = offset == dstofs;
  }

  return in_dst;
}


// Return the daylight savings time offset for this time.
int64_t Win32Time::DaylightSavingsOffset(TimezoneCache* cache) {
  return InDST(cache) ? 60 * kMsPerMinute : 0;
}


// Returns a string identifying the current timezone for the
// timestamp taking into account daylight saving.
char* Win32Time::LocalTimezone(TimezoneCache* cache) {
  // Return the standard or DST time zone name based on whether daylight
  // saving is in effect at the given time.
  return InDST(cache) ? cache->dst_tz_name_ : cache->std_tz_name_;
}


// Returns the accumulated user time for thread.
int OS::GetUserTime(uint32_t* secs,  uint32_t* usecs) {
  FILETIME dummy;
  uint64_t usertime;

  // Get the amount of time that the thread has executed in user mode.
  if (!GetThreadTimes(GetCurrentThread(), &dummy, &dummy, &dummy,
                      reinterpret_cast<FILETIME*>(&usertime))) return -1;

  // Adjust the resolution to micro-seconds.
  usertime /= 10;

  // Convert to seconds and microseconds
  *secs = static_cast<uint32_t>(usertime / 1000000);
  *usecs = static_cast<uint32_t>(usertime % 1000000);
  return 0;
}


// Returns current time as the number of milliseconds since
// 00:00:00 UTC, January 1, 1970.
double OS::TimeCurrentMillis() {
  return Time::Now().ToJsTime();
}


TimezoneCache* OS::CreateTimezoneCache() {
  return new TimezoneCache();
}


void OS::DisposeTimezoneCache(TimezoneCache* cache) {
  delete cache;
}


void OS::ClearTimezoneCache(TimezoneCache* cache) {
  cache->Clear();
}


// Returns a string identifying the current timezone taking into
// account daylight saving.
const char* OS::LocalTimezone(double time, TimezoneCache* cache) {
  return Win32Time(time).LocalTimezone(cache);
}


// Returns the local time offset in milliseconds east of UTC without
// taking daylight savings time into account.
double OS::LocalTimeOffset(TimezoneCache* cache) {
  // Use current time, rounded to the millisecond.
  Win32Time t(TimeCurrentMillis());
  // Time::LocalOffset inlcudes any daylight savings offset, so subtract it.
  return static_cast<double>(t.LocalOffset(cache) -
                             t.DaylightSavingsOffset(cache));
}


// Returns the daylight savings offset in milliseconds for the given
// time.
double OS::DaylightSavingsOffset(double time, TimezoneCache* cache) {
  int64_t offset = Win32Time(time).DaylightSavingsOffset(cache);
  return static_cast<double>(offset);
}


int OS::GetLastError() {
  return ::GetLastError();
}


int OS::GetCurrentProcessId() {
  return static_cast<int>(::GetCurrentProcessId());
}


int OS::GetCurrentThreadId() {
  return static_cast<int>(::GetCurrentThreadId());
}


// ----------------------------------------------------------------------------
// Win32 console output.
//
// If a Win32 application is linked as a console application it has a normal
// standard output and standard error. In this case normal printf works fine
// for output. However, if the application is linked as a GUI application,
// the process doesn't have a console, and therefore (debugging) output is lost.
// This is the case if we are embedded in a windows program (like a browser).
// In order to be able to get debug output in this case the the debugging
// facility using OutputDebugString. This output goes to the active debugger
// for the process (if any). Else the output can be monitored using DBMON.EXE.

enum OutputMode {
  UNKNOWN,  // Output method has not yet been determined.
  CONSOLE,  // Output is written to stdout.
  ODS       // Output is written to debug facility.
};

static OutputMode output_mode = UNKNOWN;  // Current output mode.


// Determine if the process has a console for output.
static bool HasConsole() {
  // Only check the first time. Eventual race conditions are not a problem,
  // because all threads will eventually determine the same mode.
  if (output_mode == UNKNOWN) {
    // We cannot just check that the standard output is attached to a console
    // because this would fail if output is redirected to a file. Therefore we
    // say that a process does not have an output console if either the
    // standard output handle is invalid or its file type is unknown.
    if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE &&
        GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_UNKNOWN)
      output_mode = CONSOLE;
    else
      output_mode = ODS;
  }
  return output_mode == CONSOLE;
}


static void VPrintHelper(FILE* stream, const char* format, va_list args) {
  if ((stream == stdout || stream == stderr) && !HasConsole()) {
    // It is important to use safe print here in order to avoid
    // overflowing the buffer. We might truncate the output, but this
    // does not crash.
    char buffer[4096];
    OS::VSNPrintF(buffer, sizeof(buffer), format, args);
    OutputDebugStringA(buffer);
  } else {
    vfprintf(stream, format, args);
  }
}


FILE* OS::FOpen(const char* path, const char* mode) {
  FILE* result;
  if (fopen_s(&result, path, mode) == 0) {
    return result;
  } else {
    return NULL;
  }
}


bool OS::Remove(const char* path) {
  return (DeleteFileA(path) != 0);
}


FILE* OS::OpenTemporaryFile() {
  // tmpfile_s tries to use the root dir, don't use it.
  char tempPathBuffer[MAX_PATH];
  DWORD path_result = 0;
  path_result = GetTempPathA(MAX_PATH, tempPathBuffer);
  if (path_result > MAX_PATH || path_result == 0) return NULL;
  UINT name_result = 0;
  char tempNameBuffer[MAX_PATH];
  name_result = GetTempFileNameA(tempPathBuffer, "", 0, tempNameBuffer);
  if (name_result == 0) return NULL;
  FILE* result = FOpen(tempNameBuffer, "w+");  // Same mode as tmpfile uses.
  if (result != NULL) {
    Remove(tempNameBuffer);  // Delete on close.
  }
  return result;
}


// Open log file in binary mode to avoid /n -> /r/n conversion.
const char* const OS::LogFileOpenMode = "wb";


// Print (debug) message to console.
void OS::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrint(format, args);
  va_end(args);
}


void OS::VPrint(const char* format, va_list args) {
  VPrintHelper(stdout, format, args);
}


void OS::FPrint(FILE* out, const char* format, ...) {
  va_list args;
  va_start(args, format);
  VFPrint(out, format, args);
  va_end(args);
}


void OS::VFPrint(FILE* out, const char* format, va_list args) {
  VPrintHelper(out, format, args);
}


// Print error message to console.
void OS::PrintError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrintError(format, args);
  va_end(args);
}


void OS::VPrintError(const char* format, va_list args) {
  VPrintHelper(stderr, format, args);
}


int OS::SNPrintF(char* str, int length, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, length, format, args);
  va_end(args);
  return result;
}


int OS::VSNPrintF(char* str, int length, const char* format, va_list args) {
  int n = _vsnprintf_s(str, length, _TRUNCATE, format, args);
  // Make sure to zero-terminate the string if the output was
  // truncated or if there was an error.
  if (n < 0 || n >= length) {
    if (length > 0)
      str[length - 1] = '\0';
    return -1;
  } else {
    return n;
  }
}


char* OS::StrChr(char* str, int c) {
  return const_cast<char*>(strchr(str, c));
}


void OS::StrNCpy(char* dest, int length, const char* src, size_t n) {
  // Use _TRUNCATE or strncpy_s crashes (by design) if buffer is too small.
  size_t buffer_size = static_cast<size_t>(length);
  if (n + 1 > buffer_size)  // count for trailing '\0'
    n = _TRUNCATE;
  int result = strncpy_s(dest, length, src, n);
  USE(result);
  DCHECK(result == 0 || (n == _TRUNCATE && result == STRUNCATE));
}


#undef _TRUNCATE
#undef STRUNCATE


// Get the system's page size used by VirtualAlloc() or the next power
// of two. The reason for always returning a power of two is that the
// rounding up in OS::Allocate expects that.
static size_t GetPageSize() {
  static size_t page_size = 0;
  if (page_size == 0) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    page_size = RoundUpToPowerOf2(info.dwPageSize);
  }
  return page_size;
}


// The allocation alignment is the guaranteed alignment for
// VirtualAlloc'ed blocks of memory.
size_t OS::AllocateAlignment() {
  static size_t allocate_alignment = 0;
  if (allocate_alignment == 0) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    allocate_alignment = info.dwAllocationGranularity;
  }
  return allocate_alignment;
}


static LazyInstance<RandomNumberGenerator>::type
    platform_random_number_generator = LAZY_INSTANCE_INITIALIZER;


void OS::Initialize(int64_t random_seed, bool hard_abort,
                    const char* const gc_fake_mmap) {
  if (random_seed) {
    platform_random_number_generator.Pointer()->SetSeed(random_seed);
  }
  g_hard_abort = hard_abort;
}


void* OS::GetRandomMmapAddr() {
  // The address range used to randomize RWX allocations in OS::Allocate
  // Try not to map pages into the default range that windows loads DLLs
  // Use a multiple of 64k to prevent committing unused memory.
  // Note: This does not guarantee RWX regions will be within the
  // range kAllocationRandomAddressMin to kAllocationRandomAddressMax
#ifdef V8_HOST_ARCH_64_BIT
  static const intptr_t kAllocationRandomAddressMin = 0x0000000080000000;
  static const intptr_t kAllocationRandomAddressMax = 0x000003FFFFFF0000;
#else
  static const intptr_t kAllocationRandomAddressMin = 0x04000000;
  static const intptr_t kAllocationRandomAddressMax = 0x3FFF0000;
#endif
  uintptr_t address =
      (platform_random_number_generator.Pointer()->NextInt() << kPageSizeBits) |
      kAllocationRandomAddressMin;
  address &= kAllocationRandomAddressMax;
  return reinterpret_cast<void *>(address);
}


static void* RandomizedVirtualAlloc(size_t size, int action, int protection) {
  LPVOID base = NULL;

  if (protection == PAGE_EXECUTE_READWRITE || protection == PAGE_NOACCESS) {
    // For exectutable pages try and randomize the allocation address
    for (size_t attempts = 0; base == NULL && attempts < 3; ++attempts) {
      base = VirtualAlloc(OS::GetRandomMmapAddr(), size, action, protection);
    }
  }

  // After three attempts give up and let the OS find an address to use.
  if (base == NULL) base = VirtualAlloc(NULL, size, action, protection);

  return base;
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  // VirtualAlloc rounds allocated size to page size automatically.
  size_t msize = RoundUp(requested, static_cast<int>(GetPageSize()));

  // Windows XP SP2 allows Data Excution Prevention (DEP).
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

  LPVOID mbase = RandomizedVirtualAlloc(msize,
                                        MEM_COMMIT | MEM_RESERVE,
                                        prot);

  if (mbase == NULL) return NULL;

  DCHECK(IsAligned(reinterpret_cast<size_t>(mbase), OS::AllocateAlignment()));

  *allocated = msize;
  return mbase;
}


void OS::Free(void* address, const size_t size) {
  // TODO(1240712): VirtualFree has a return value which is ignored here.
  VirtualFree(address, 0, MEM_RELEASE);
  USE(size);
}


intptr_t OS::CommitPageSize() {
  return 4096;
}


void OS::ProtectCode(void* address, const size_t size) {
  DWORD old_protect;
  VirtualProtect(address, size, PAGE_EXECUTE_READ, &old_protect);
}


void OS::Guard(void* address, const size_t size) {
  DWORD oldprotect;
  VirtualProtect(address, size, PAGE_NOACCESS, &oldprotect);
}


void OS::Sleep(int milliseconds) {
  ::Sleep(milliseconds);
}


void OS::Abort() {
  if (g_hard_abort) {
    V8_IMMEDIATE_CRASH();
  }
  // Make the MSVCRT do a silent abort.
  raise(SIGABRT);
}


void OS::DebugBreak() {
#ifdef _MSC_VER
  // To avoid Visual Studio runtime support the following code can be used
  // instead
  // __asm { int 3 }
  __debugbreak();
#else
  ::DebugBreak();
#endif
}


class Win32MemoryMappedFile : public OS::MemoryMappedFile {
 public:
  Win32MemoryMappedFile(HANDLE file,
                        HANDLE file_mapping,
                        void* memory,
                        int size)
      : file_(file),
        file_mapping_(file_mapping),
        memory_(memory),
        size_(size) { }
  virtual ~Win32MemoryMappedFile();
  virtual void* memory() { return memory_; }
  virtual int size() { return size_; }
 private:
  HANDLE file_;
  HANDLE file_mapping_;
  void* memory_;
  int size_;
};


OS::MemoryMappedFile* OS::MemoryMappedFile::open(const char* name) {
  // Open a physical file
  HANDLE file = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (file == INVALID_HANDLE_VALUE) return NULL;

  int size = static_cast<int>(GetFileSize(file, NULL));

  // Create a file mapping for the physical file
  HANDLE file_mapping = CreateFileMapping(file, NULL,
      PAGE_READWRITE, 0, static_cast<DWORD>(size), NULL);
  if (file_mapping == NULL) return NULL;

  // Map a view of the file into memory
  void* memory = MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
  return new Win32MemoryMappedFile(file, file_mapping, memory, size);
}


OS::MemoryMappedFile* OS::MemoryMappedFile::create(const char* name, int size,
    void* initial) {
  // Open a physical file
  HANDLE file = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
  if (file == NULL) return NULL;
  // Create a file mapping for the physical file
  HANDLE file_mapping = CreateFileMapping(file, NULL,
      PAGE_READWRITE, 0, static_cast<DWORD>(size), NULL);
  if (file_mapping == NULL) return NULL;
  // Map a view of the file into memory
  void* memory = MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
  if (memory) memmove(memory, initial, size);
  return new Win32MemoryMappedFile(file, file_mapping, memory, size);
}


Win32MemoryMappedFile::~Win32MemoryMappedFile() {
  if (memory_ != NULL)
    UnmapViewOfFile(memory_);
  CloseHandle(file_mapping_);
  CloseHandle(file_);
}


// The following code loads functions defined in DbhHelp.h and TlHelp32.h
// dynamically. This is to avoid being depending on dbghelp.dll and
// tlhelp32.dll when running (the functions in tlhelp32.dll have been moved to
// kernel32.dll at some point so loading functions defines in TlHelp32.h
// dynamically might not be necessary any more - for some versions of Windows?).

// Function pointers to functions dynamically loaded from dbghelp.dll.
#define DBGHELP_FUNCTION_LIST(V)  \
  V(SymInitialize)                \
  V(SymGetOptions)                \
  V(SymSetOptions)                \
  V(SymGetSearchPath)             \
  V(SymLoadModule64)              \
  V(StackWalk64)                  \
  V(SymGetSymFromAddr64)          \
  V(SymGetLineFromAddr64)         \
  V(SymFunctionTableAccess64)     \
  V(SymGetModuleBase64)

// Function pointers to functions dynamically loaded from dbghelp.dll.
#define TLHELP32_FUNCTION_LIST(V)  \
  V(CreateToolhelp32Snapshot)      \
  V(Module32FirstW)                \
  V(Module32NextW)

// Define the decoration to use for the type and variable name used for
// dynamically loaded DLL function..
#define DLL_FUNC_TYPE(name) _##name##_
#define DLL_FUNC_VAR(name) _##name

// Define the type for each dynamically loaded DLL function. The function
// definitions are copied from DbgHelp.h and TlHelp32.h. The IN and VOID macros
// from the Windows include files are redefined here to have the function
// definitions to be as close to the ones in the original .h files as possible.
#ifndef IN
#define IN
#endif
#ifndef VOID
#define VOID void
#endif

// DbgHelp isn't supported on MinGW yet
#ifndef __MINGW32__
// DbgHelp.h functions.
typedef BOOL (__stdcall *DLL_FUNC_TYPE(SymInitialize))(IN HANDLE hProcess,
                                                       IN PSTR UserSearchPath,
                                                       IN BOOL fInvadeProcess);
typedef DWORD (__stdcall *DLL_FUNC_TYPE(SymGetOptions))(VOID);
typedef DWORD (__stdcall *DLL_FUNC_TYPE(SymSetOptions))(IN DWORD SymOptions);
typedef BOOL (__stdcall *DLL_FUNC_TYPE(SymGetSearchPath))(
    IN HANDLE hProcess,
    OUT PSTR SearchPath,
    IN DWORD SearchPathLength);
typedef DWORD64 (__stdcall *DLL_FUNC_TYPE(SymLoadModule64))(
    IN HANDLE hProcess,
    IN HANDLE hFile,
    IN PSTR ImageName,
    IN PSTR ModuleName,
    IN DWORD64 BaseOfDll,
    IN DWORD SizeOfDll);
typedef BOOL (__stdcall *DLL_FUNC_TYPE(StackWalk64))(
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
typedef BOOL (__stdcall *DLL_FUNC_TYPE(SymGetSymFromAddr64))(
    IN HANDLE hProcess,
    IN DWORD64 qwAddr,
    OUT PDWORD64 pdwDisplacement,
    OUT PIMAGEHLP_SYMBOL64 Symbol);
typedef BOOL (__stdcall *DLL_FUNC_TYPE(SymGetLineFromAddr64))(
    IN HANDLE hProcess,
    IN DWORD64 qwAddr,
    OUT PDWORD pdwDisplacement,
    OUT PIMAGEHLP_LINE64 Line64);
// DbgHelp.h typedefs. Implementation found in dbghelp.dll.
typedef PVOID (__stdcall *DLL_FUNC_TYPE(SymFunctionTableAccess64))(
    HANDLE hProcess,
    DWORD64 AddrBase);  // DbgHelp.h typedef PFUNCTION_TABLE_ACCESS_ROUTINE64
typedef DWORD64 (__stdcall *DLL_FUNC_TYPE(SymGetModuleBase64))(
    HANDLE hProcess,
    DWORD64 AddrBase);  // DbgHelp.h typedef PGET_MODULE_BASE_ROUTINE64

// TlHelp32.h functions.
typedef HANDLE (__stdcall *DLL_FUNC_TYPE(CreateToolhelp32Snapshot))(
    DWORD dwFlags,
    DWORD th32ProcessID);
typedef BOOL (__stdcall *DLL_FUNC_TYPE(Module32FirstW))(HANDLE hSnapshot,
                                                        LPMODULEENTRY32W lpme);
typedef BOOL (__stdcall *DLL_FUNC_TYPE(Module32NextW))(HANDLE hSnapshot,
                                                       LPMODULEENTRY32W lpme);

#undef IN
#undef VOID

// Declare a variable for each dynamically loaded DLL function.
#define DEF_DLL_FUNCTION(name) DLL_FUNC_TYPE(name) DLL_FUNC_VAR(name) = NULL;
DBGHELP_FUNCTION_LIST(DEF_DLL_FUNCTION)
TLHELP32_FUNCTION_LIST(DEF_DLL_FUNCTION)
#undef DEF_DLL_FUNCTION

// Load the functions. This function has a lot of "ugly" macros in order to
// keep down code duplication.

static bool LoadDbgHelpAndTlHelp32() {
  static bool dbghelp_loaded = false;

  if (dbghelp_loaded) return true;

  HMODULE module;

  // Load functions from the dbghelp.dll module.
  module = LoadLibrary(TEXT("dbghelp.dll"));
  if (module == NULL) {
    return false;
  }

#define LOAD_DLL_FUNC(name)                                                 \
  DLL_FUNC_VAR(name) =                                                      \
      reinterpret_cast<DLL_FUNC_TYPE(name)>(GetProcAddress(module, #name));

DBGHELP_FUNCTION_LIST(LOAD_DLL_FUNC)

#undef LOAD_DLL_FUNC

  // Load functions from the kernel32.dll module (the TlHelp32.h function used
  // to be in tlhelp32.dll but are now moved to kernel32.dll).
  module = LoadLibrary(TEXT("kernel32.dll"));
  if (module == NULL) {
    return false;
  }

#define LOAD_DLL_FUNC(name)                                                 \
  DLL_FUNC_VAR(name) =                                                      \
      reinterpret_cast<DLL_FUNC_TYPE(name)>(GetProcAddress(module, #name));

TLHELP32_FUNCTION_LIST(LOAD_DLL_FUNC)

#undef LOAD_DLL_FUNC

  // Check that all functions where loaded.
  bool result =
#define DLL_FUNC_LOADED(name) (DLL_FUNC_VAR(name) != NULL) &&

DBGHELP_FUNCTION_LIST(DLL_FUNC_LOADED)
TLHELP32_FUNCTION_LIST(DLL_FUNC_LOADED)

#undef DLL_FUNC_LOADED
  true;

  dbghelp_loaded = result;
  return result;
  // NOTE: The modules are never unloaded and will stay around until the
  // application is closed.
}

#undef DBGHELP_FUNCTION_LIST
#undef TLHELP32_FUNCTION_LIST
#undef DLL_FUNC_VAR
#undef DLL_FUNC_TYPE


// Load the symbols for generating stack traces.
static std::vector<OS::SharedLibraryAddress> LoadSymbols(
    HANDLE process_handle) {
  static std::vector<OS::SharedLibraryAddress> result;

  static bool symbols_loaded = false;

  if (symbols_loaded) return result;

  BOOL ok;

  // Initialize the symbol engine.
  ok = _SymInitialize(process_handle,  // hProcess
                      NULL,            // UserSearchPath
                      false);          // fInvadeProcess
  if (!ok) return result;

  DWORD options = _SymGetOptions();
  options |= SYMOPT_LOAD_LINES;
  options |= SYMOPT_FAIL_CRITICAL_ERRORS;
  options = _SymSetOptions(options);

  char buf[OS::kStackWalkMaxNameLen] = {0};
  ok = _SymGetSearchPath(process_handle, buf, OS::kStackWalkMaxNameLen);
  if (!ok) {
    int err = GetLastError();
    OS::Print("%d\n", err);
    return result;
  }

  HANDLE snapshot = _CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE,       // dwFlags
      GetCurrentProcessId());  // th32ProcessId
  if (snapshot == INVALID_HANDLE_VALUE) return result;
  MODULEENTRY32W module_entry;
  module_entry.dwSize = sizeof(module_entry);  // Set the size of the structure.
  BOOL cont = _Module32FirstW(snapshot, &module_entry);
  while (cont) {
    DWORD64 base;
    // NOTE the SymLoadModule64 function has the peculiarity of accepting a
    // both unicode and ASCII strings even though the parameter is PSTR.
    base = _SymLoadModule64(
        process_handle,                                       // hProcess
        0,                                                    // hFile
        reinterpret_cast<PSTR>(module_entry.szExePath),       // ImageName
        reinterpret_cast<PSTR>(module_entry.szModule),        // ModuleName
        reinterpret_cast<DWORD64>(module_entry.modBaseAddr),  // BaseOfDll
        module_entry.modBaseSize);                            // SizeOfDll
    if (base == 0) {
      int err = GetLastError();
      if (err != ERROR_MOD_NOT_FOUND &&
          err != ERROR_INVALID_HANDLE) {
        result.clear();
        return result;
      }
    }
    int lib_name_length = WideCharToMultiByte(
        CP_UTF8, 0, module_entry.szExePath, -1, NULL, 0, NULL, NULL);
    std::string lib_name(lib_name_length, 0);
    WideCharToMultiByte(CP_UTF8, 0, module_entry.szExePath, -1, &lib_name[0],
                        lib_name_length, NULL, NULL);
    result.push_back(OS::SharedLibraryAddress(
        lib_name, reinterpret_cast<unsigned int>(module_entry.modBaseAddr),
        reinterpret_cast<unsigned int>(module_entry.modBaseAddr +
                                       module_entry.modBaseSize)));
    cont = _Module32NextW(snapshot, &module_entry);
  }
  CloseHandle(snapshot);

  symbols_loaded = true;
  return result;
}


std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  // SharedLibraryEvents are logged when loading symbol information.
  // Only the shared libraries loaded at the time of the call to
  // GetSharedLibraryAddresses are logged.  DLLs loaded after
  // initialization are not accounted for.
  if (!LoadDbgHelpAndTlHelp32()) return std::vector<OS::SharedLibraryAddress>();
  HANDLE process_handle = GetCurrentProcess();
  return LoadSymbols(process_handle);
}


void OS::SignalCodeMovingGC() {
}


uint64_t OS::TotalPhysicalMemory() {
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(memory_info);
  if (!GlobalMemoryStatusEx(&memory_info)) {
    UNREACHABLE();
    return 0;
  }

  return static_cast<uint64_t>(memory_info.ullTotalPhys);
}


#else  // __MINGW32__
std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  return std::vector<OS::SharedLibraryAddress>();
}


void OS::SignalCodeMovingGC() { }
#endif  // __MINGW32__


int OS::NumberOfProcessorsOnline() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
}


double OS::nan_value() {
#ifdef _MSC_VER
  return std::numeric_limits<double>::quiet_NaN();
#else  // _MSC_VER
  return NAN;
#endif  // _MSC_VER
}


int OS::ActivationFrameAlignment() {
#ifdef _WIN64
  return 16;  // Windows 64-bit ABI requires the stack to be 16-byte aligned.
#elif defined(__MINGW32__)
  // With gcc 4.4 the tree vectorization optimizer can generate code
  // that requires 16 byte alignment such as movdqa on x86.
  return 16;
#else
  return 8;  // Floating-point math runs faster with 8-byte alignment.
#endif
}


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  DCHECK(IsAligned(alignment, static_cast<intptr_t>(OS::AllocateAlignment())));
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* address = ReserveRegion(request_size);
  if (address == NULL) return;
  uint8_t* base = RoundUp(static_cast<uint8_t*>(address), alignment);
  // Try reducing the size by freeing and then reallocating a specific area.
  bool result = ReleaseRegion(address, request_size);
  USE(result);
  DCHECK(result);
  address = VirtualAlloc(base, size, MEM_RESERVE, PAGE_NOACCESS);
  if (address != NULL) {
    request_size = size;
    DCHECK(base == static_cast<uint8_t*>(address));
  } else {
    // Resizing failed, just go with a bigger area.
    address = ReserveRegion(request_size);
    if (address == NULL) return;
  }
  address_ = address;
  size_ = request_size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = ReleaseRegion(address(), size());
    DCHECK(result);
    USE(result);
  }
}


bool VirtualMemory::IsReserved() {
  return address_ != NULL;
}


void VirtualMemory::Reset() {
  address_ = NULL;
  size_ = 0;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  return CommitRegion(address, size, is_executable);
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  DCHECK(IsReserved());
  return UncommitRegion(address, size);
}


bool VirtualMemory::Guard(void* address) {
  if (NULL == VirtualAlloc(address,
                           OS::CommitPageSize(),
                           MEM_COMMIT,
                           PAGE_NOACCESS)) {
    return false;
  }
  return true;
}


void* VirtualMemory::ReserveRegion(size_t size) {
  return RandomizedVirtualAlloc(size, MEM_RESERVE, PAGE_NOACCESS);
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (NULL == VirtualAlloc(base, size, MEM_COMMIT, prot)) {
    return false;
  }
  return true;
}


bool VirtualMemory::UncommitRegion(void* base, size_t size) {
  return VirtualFree(base, size, MEM_DECOMMIT) != 0;
}


bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
  return VirtualFree(base, 0, MEM_RELEASE) != 0;
}


bool VirtualMemory::HasLazyCommits() {
  // TODO(alph): implement for the platform.
  return false;
}


// ----------------------------------------------------------------------------
// Win32 thread support.

// Definition of invalid thread handle and id.
static const HANDLE kNoThread = INVALID_HANDLE_VALUE;

// Entry point for threads. The supplied argument is a pointer to the thread
// object. The entry function dispatches to the run method in the thread
// object. It is important that this function has __stdcall calling
// convention.
static unsigned int __stdcall ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  thread->NotifyStartedAndRun();
  return 0;
}


class Thread::PlatformData {
 public:
  explicit PlatformData(HANDLE thread) : thread_(thread) {}
  HANDLE thread_;
  unsigned thread_id_;
};


// Initialize a Win32 thread object. The thread has an invalid thread
// handle until it is started.

Thread::Thread(const Options& options)
    : stack_size_(options.stack_size()),
      start_semaphore_(NULL) {
  data_ = new PlatformData(kNoThread);
  set_name(options.name());
}


void Thread::set_name(const char* name) {
  OS::StrNCpy(name_, sizeof(name_), name, strlen(name));
  name_[sizeof(name_) - 1] = '\0';
}


// Close our own handle for the thread.
Thread::~Thread() {
  if (data_->thread_ != kNoThread) CloseHandle(data_->thread_);
  delete data_;
}


// Create a new thread. It is important to use _beginthreadex() instead of
// the Win32 function CreateThread(), because the CreateThread() does not
// initialize thread specific structures in the C runtime library.
void Thread::Start() {
  data_->thread_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     static_cast<unsigned>(stack_size_),
                     ThreadEntry,
                     this,
                     0,
                     &data_->thread_id_));
}


// Wait for thread to terminate.
void Thread::Join() {
  if (data_->thread_id_ != GetCurrentThreadId()) {
    WaitForSingleObject(data_->thread_, INFINITE);
  }
}


Thread::LocalStorageKey Thread::CreateThreadLocalKey() {
  DWORD result = TlsAlloc();
  DCHECK(result != TLS_OUT_OF_INDEXES);
  return static_cast<LocalStorageKey>(result);
}


void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  BOOL result = TlsFree(static_cast<DWORD>(key));
  USE(result);
  DCHECK(result);
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  return TlsGetValue(static_cast<DWORD>(key));
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  BOOL result = TlsSetValue(static_cast<DWORD>(key), value);
  USE(result);
  DCHECK(result);
}



void Thread::YieldCPU() {
  Sleep(0);
}

} }  // namespace v8::base
