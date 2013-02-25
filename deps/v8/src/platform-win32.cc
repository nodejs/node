// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Platform specific code for Win32.

#define V8_WIN32_HEADERS_FULL
#include "win32-headers.h"

#include "v8.h"

#include "codegen.h"
#include "platform.h"
#include "vm-state-inl.h"

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


#ifndef MINGW_HAS_SECURE_API

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
  ASSERT(count == _TRUNCATE);
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

#endif  // MINGW_HAS_SECURE_API

#endif  // __MINGW32__

// Generate a pseudo-random number in the range 0-2^31-1. Usually
// defined in stdlib.h. Missing in both Microsoft Visual Studio C++ and MinGW.
int random() {
  return rand();
}


namespace v8 {
namespace internal {

intptr_t OS::MaxVirtualMemory() {
  return 0;
}


double ceiling(double x) {
  return ceil(x);
}


static Mutex* limit_mutex = NULL;

#if defined(V8_TARGET_ARCH_IA32)
static OS::MemCopyFunction memcopy_function = NULL;
// Defined in codegen-ia32.cc.
OS::MemCopyFunction CreateMemCopyFunction();

// Copy memory area to disjoint memory area.
void OS::MemCopy(void* dest, const void* src, size_t size) {
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  (*memcopy_function)(dest, src, size);
#ifdef DEBUG
  CHECK_EQ(0, memcmp(dest, src, size));
#endif
}
#endif  // V8_TARGET_ARCH_IA32

#ifdef _WIN64
typedef double (*ModuloFunction)(double, double);
static ModuloFunction modulo_function = NULL;
// Defined in codegen-x64.cc.
ModuloFunction CreateModuloFunction();

void init_modulo_function() {
  modulo_function = CreateModuloFunction();
}

double modulo(double x, double y) {
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  return (*modulo_function)(x, y);
}
#else  // Win32

double modulo(double x, double y) {
  // Workaround MS fmod bugs. ECMA-262 says:
  // dividend is finite and divisor is an infinity => result equals dividend
  // dividend is a zero and divisor is nonzero finite => result equals dividend
  if (!(isfinite(x) && (!isfinite(y) && !isnan(y))) &&
      !(x == 0 && (y != 0 && isfinite(y)))) {
    x = fmod(x, y);
  }
  return x;
}

#endif  // _WIN64


#define UNARY_MATH_FUNCTION(name, generator)             \
static UnaryMathFunction fast_##name##_function = NULL;  \
void init_fast_##name##_function() {                     \
  fast_##name##_function = generator;                    \
}                                                        \
double fast_##name(double x) {                           \
  return (*fast_##name##_function)(x);                   \
}

UNARY_MATH_FUNCTION(sin, CreateTranscendentalFunction(TranscendentalCache::SIN))
UNARY_MATH_FUNCTION(cos, CreateTranscendentalFunction(TranscendentalCache::COS))
UNARY_MATH_FUNCTION(tan, CreateTranscendentalFunction(TranscendentalCache::TAN))
UNARY_MATH_FUNCTION(log, CreateTranscendentalFunction(TranscendentalCache::LOG))
UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction())

#undef MATH_FUNCTION


void MathSetup() {
#ifdef _WIN64
  init_modulo_function();
#endif
  init_fast_sin_function();
  init_fast_cos_function();
  init_fast_tan_function();
  init_fast_log_function();
  init_fast_sqrt_function();
}


// ----------------------------------------------------------------------------
// The Time class represents time on win32. A timestamp is represented as
// a 64-bit integer in 100 nanoseconds since January 1, 1601 (UTC). JavaScript
// timestamps are represented as a doubles in milliseconds since 00:00:00 UTC,
// January 1, 1970.

class Time {
 public:
  // Constructors.
  Time();
  explicit Time(double jstime);
  Time(int year, int mon, int day, int hour, int min, int sec);

  // Convert timestamp to JavaScript representation.
  double ToJSTime();

  // Set timestamp to current time.
  void SetToCurrentTime();

  // Returns the local timezone offset in milliseconds east of UTC. This is
  // the number of milliseconds you must add to UTC to get local time, i.e.
  // LocalOffset(CET) = 3600000 and LocalOffset(PST) = -28800000. This
  // routine also takes into account whether daylight saving is effect
  // at the time.
  int64_t LocalOffset();

  // Returns the daylight savings time offset for the time in milliseconds.
  int64_t DaylightSavingsOffset();

  // Returns a string identifying the current timezone for the
  // timestamp taking into account daylight saving.
  char* LocalTimezone();

 private:
  // Constants for time conversion.
  static const int64_t kTimeEpoc = 116444736000000000LL;
  static const int64_t kTimeScaler = 10000;
  static const int64_t kMsPerMinute = 60000;

  // Constants for timezone information.
  static const int kTzNameSize = 128;
  static const bool kShortTzNames = false;

  // Timezone information. We need to have static buffers for the
  // timezone names because we return pointers to these in
  // LocalTimezone().
  static bool tz_initialized_;
  static TIME_ZONE_INFORMATION tzinfo_;
  static char std_tz_name_[kTzNameSize];
  static char dst_tz_name_[kTzNameSize];

  // Initialize the timezone information (if not already done).
  static void TzSet();

  // Guess the name of the timezone from the bias.
  static const char* GuessTimezoneNameFromBias(int bias);

  // Return whether or not daylight savings time is in effect at this time.
  bool InDST();

  // Return the difference (in milliseconds) between this timestamp and
  // another timestamp.
  int64_t Diff(Time* other);

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

// Static variables.
bool Time::tz_initialized_ = false;
TIME_ZONE_INFORMATION Time::tzinfo_;
char Time::std_tz_name_[kTzNameSize];
char Time::dst_tz_name_[kTzNameSize];


// Initialize timestamp to start of epoc.
Time::Time() {
  t() = 0;
}


// Initialize timestamp from a JavaScript timestamp.
Time::Time(double jstime) {
  t() = static_cast<int64_t>(jstime) * kTimeScaler + kTimeEpoc;
}


// Initialize timestamp from date/time components.
Time::Time(int year, int mon, int day, int hour, int min, int sec) {
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
double Time::ToJSTime() {
  return static_cast<double>((t() - kTimeEpoc) / kTimeScaler);
}


// Guess the name of the timezone from the bias.
// The guess is very biased towards the northern hemisphere.
const char* Time::GuessTimezoneNameFromBias(int bias) {
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


// Initialize timezone information. The timezone information is obtained from
// windows. If we cannot get the timezone information we fall back to CET.
// Please notice that this code is not thread-safe.
void Time::TzSet() {
  // Just return if timezone information has already been initialized.
  if (tz_initialized_) return;

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
    OS::SNPrintF(Vector<char>(std_tz_name_, kTzNameSize - 1),
                 "%s Standard Time",
                 GuessTimezoneNameFromBias(tzinfo_.Bias));
  }
  if (dst_tz_name_[0] == '\0' || dst_tz_name_[0] == '@') {
    OS::SNPrintF(Vector<char>(dst_tz_name_, kTzNameSize - 1),
                 "%s Daylight Time",
                 GuessTimezoneNameFromBias(tzinfo_.Bias));
  }

  // Timezone information initialized.
  tz_initialized_ = true;
}


// Return the difference in milliseconds between this and another timestamp.
int64_t Time::Diff(Time* other) {
  return (t() - other->t()) / kTimeScaler;
}


// Set timestamp to current time.
void Time::SetToCurrentTime() {
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
int64_t Time::LocalOffset() {
  // Initialize timezone information, if needed.
  TzSet();

  Time rounded_to_second(*this);
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
    return (tzinfo_.Bias + tzinfo_.DaylightBias) * -kMsPerMinute;
  } else if (posix_local_time_struct.tm_isdst == 0) {
    return (tzinfo_.Bias + tzinfo_.StandardBias) * -kMsPerMinute;
  } else {
    return tzinfo_.Bias * -kMsPerMinute;
  }
}


// Return whether or not daylight savings time is in effect at this time.
bool Time::InDST() {
  // Initialize timezone information, if needed.
  TzSet();

  // Determine if DST is in effect at the specified time.
  bool in_dst = false;
  if (tzinfo_.StandardDate.wMonth != 0 || tzinfo_.DaylightDate.wMonth != 0) {
    // Get the local timezone offset for the timestamp in milliseconds.
    int64_t offset = LocalOffset();

    // Compute the offset for DST. The bias parameters in the timezone info
    // are specified in minutes. These must be converted to milliseconds.
    int64_t dstofs = -(tzinfo_.Bias + tzinfo_.DaylightBias) * kMsPerMinute;

    // If the local time offset equals the timezone bias plus the daylight
    // bias then DST is in effect.
    in_dst = offset == dstofs;
  }

  return in_dst;
}


// Return the daylight savings time offset for this time.
int64_t Time::DaylightSavingsOffset() {
  return InDST() ? 60 * kMsPerMinute : 0;
}


// Returns a string identifying the current timezone for the
// timestamp taking into account daylight saving.
char* Time::LocalTimezone() {
  // Return the standard or DST time zone name based on whether daylight
  // saving is in effect at the given time.
  return InDST() ? dst_tz_name_ : std_tz_name_;
}


void OS::PostSetUp() {
  // Math functions depend on CPU features therefore they are initialized after
  // CPU.
  MathSetup();
#if defined(V8_TARGET_ARCH_IA32)
  memcopy_function = CreateMemCopyFunction();
#endif
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
  Time t;
  t.SetToCurrentTime();
  return t.ToJSTime();
}

// Returns the tickcounter based on timeGetTime.
int64_t OS::Ticks() {
  return timeGetTime() * 1000;  // Convert to microseconds.
}


// Returns a string identifying the current timezone taking into
// account daylight saving.
const char* OS::LocalTimezone(double time) {
  return Time(time).LocalTimezone();
}


// Returns the local time offset in milliseconds east of UTC without
// taking daylight savings time into account.
double OS::LocalTimeOffset() {
  // Use current time, rounded to the millisecond.
  Time t(TimeCurrentMillis());
  // Time::LocalOffset inlcudes any daylight savings offset, so subtract it.
  return static_cast<double>(t.LocalOffset() - t.DaylightSavingsOffset());
}


// Returns the daylight savings offset in milliseconds for the given
// time.
double OS::DaylightSavingsOffset(double time) {
  int64_t offset = Time(time).DaylightSavingsOffset();
  return static_cast<double>(offset);
}


int OS::GetLastError() {
  return ::GetLastError();
}


int OS::GetCurrentProcessId() {
  return static_cast<int>(::GetCurrentProcessId());
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
  if (HasConsole()) {
    vfprintf(stream, format, args);
  } else {
    // It is important to use safe print here in order to avoid
    // overflowing the buffer. We might truncate the output, but this
    // does not crash.
    EmbeddedVector<char, 4096> buffer;
    OS::VSNPrintF(buffer, format, args);
    OutputDebugStringA(buffer.start());
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


int OS::SNPrintF(Vector<char> str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, format, args);
  va_end(args);
  return result;
}


int OS::VSNPrintF(Vector<char> str, const char* format, va_list args) {
  int n = _vsnprintf_s(str.start(), str.length(), _TRUNCATE, format, args);
  // Make sure to zero-terminate the string if the output was
  // truncated or if there was an error.
  if (n < 0 || n >= str.length()) {
    if (str.length() > 0)
      str[str.length() - 1] = '\0';
    return -1;
  } else {
    return n;
  }
}


char* OS::StrChr(char* str, int c) {
  return const_cast<char*>(strchr(str, c));
}


void OS::StrNCpy(Vector<char> dest, const char* src, size_t n) {
  // Use _TRUNCATE or strncpy_s crashes (by design) if buffer is too small.
  size_t buffer_size = static_cast<size_t>(dest.length());
  if (n + 1 > buffer_size)  // count for trailing '\0'
    n = _TRUNCATE;
  int result = strncpy_s(dest.start(), dest.length(), src, n);
  USE(result);
  ASSERT(result == 0 || (n == _TRUNCATE && result == STRUNCATE));
}


// We keep the lowest and highest addresses mapped as a quick way of
// determining that pointers are outside the heap (used mostly in assertions
// and verification).  The estimate is conservative, i.e., not all addresses in
// 'allocated' space are actually allocated to our heap.  The range is
// [lowest, highest), inclusive on the low and and exclusive on the high end.
static void* lowest_ever_allocated = reinterpret_cast<void*>(-1);
static void* highest_ever_allocated = reinterpret_cast<void*>(0);


static void UpdateAllocatedSpaceLimits(void* address, int size) {
  ASSERT(limit_mutex != NULL);
  ScopedLock lock(limit_mutex);

  lowest_ever_allocated = Min(lowest_ever_allocated, address);
  highest_ever_allocated =
      Max(highest_ever_allocated,
          reinterpret_cast<void*>(reinterpret_cast<char*>(address) + size));
}


bool OS::IsOutsideAllocatedSpace(void* pointer) {
  if (pointer < lowest_ever_allocated || pointer >= highest_ever_allocated)
    return true;
  // Ask the Windows API
  if (IsBadWritePtr(pointer, 1))
    return true;
  return false;
}


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


static void* GetRandomAddr() {
  Isolate* isolate = Isolate::UncheckedCurrent();
  // Note that the current isolate isn't set up in a call path via
  // CpuFeatures::Probe. We don't care about randomization in this case because
  // the code page is immediately freed.
  if (isolate != NULL) {
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
    uintptr_t address = (V8::RandomPrivate(isolate) << kPageSizeBits)
        | kAllocationRandomAddressMin;
    address &= kAllocationRandomAddressMax;
    return reinterpret_cast<void *>(address);
  }
  return NULL;
}


static void* RandomizedVirtualAlloc(size_t size, int action, int protection) {
  LPVOID base = NULL;

  if (protection == PAGE_EXECUTE_READWRITE || protection == PAGE_NOACCESS) {
    // For exectutable pages try and randomize the allocation address
    for (size_t attempts = 0; base == NULL && attempts < 3; ++attempts) {
      base = VirtualAlloc(GetRandomAddr(), size, action, protection);
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

  if (mbase == NULL) {
    LOG(ISOLATE, StringEvent("OS::Allocate", "VirtualAlloc failed"));
    return NULL;
  }

  ASSERT(IsAligned(reinterpret_cast<size_t>(mbase), OS::AllocateAlignment()));

  *allocated = msize;
  UpdateAllocatedSpaceLimits(mbase, static_cast<int>(msize));
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
  VirtualProtect(address, size, PAGE_READONLY | PAGE_GUARD, &oldprotect);
}


void OS::Sleep(int milliseconds) {
  ::Sleep(milliseconds);
}


void OS::Abort() {
  if (IsDebuggerPresent() || FLAG_break_on_abort) {
    DebugBreak();
  } else {
    // Make the MSVCRT do a silent abort.
    raise(SIGABRT);
  }
}


void OS::DebugBreak() {
#ifdef _MSC_VER
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


// Load the symbols for generating stack traces.
static bool LoadSymbols(HANDLE process_handle) {
  static bool symbols_loaded = false;

  if (symbols_loaded) return true;

  BOOL ok;

  // Initialize the symbol engine.
  ok = _SymInitialize(process_handle,  // hProcess
                      NULL,            // UserSearchPath
                      false);          // fInvadeProcess
  if (!ok) return false;

  DWORD options = _SymGetOptions();
  options |= SYMOPT_LOAD_LINES;
  options |= SYMOPT_FAIL_CRITICAL_ERRORS;
  options = _SymSetOptions(options);

  char buf[OS::kStackWalkMaxNameLen] = {0};
  ok = _SymGetSearchPath(process_handle, buf, OS::kStackWalkMaxNameLen);
  if (!ok) {
    int err = GetLastError();
    PrintF("%d\n", err);
    return false;
  }

  HANDLE snapshot = _CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE,       // dwFlags
      GetCurrentProcessId());  // th32ProcessId
  if (snapshot == INVALID_HANDLE_VALUE) return false;
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
          err != ERROR_INVALID_HANDLE) return false;
    }
    LOG(i::Isolate::Current(),
        SharedLibraryEvent(
            module_entry.szExePath,
            reinterpret_cast<unsigned int>(module_entry.modBaseAddr),
            reinterpret_cast<unsigned int>(module_entry.modBaseAddr +
                                           module_entry.modBaseSize)));
    cont = _Module32NextW(snapshot, &module_entry);
  }
  CloseHandle(snapshot);

  symbols_loaded = true;
  return true;
}


void OS::LogSharedLibraryAddresses() {
  // SharedLibraryEvents are logged when loading symbol information.
  // Only the shared libraries loaded at the time of the call to
  // LogSharedLibraryAddresses are logged.  DLLs loaded after
  // initialization are not accounted for.
  if (!LoadDbgHelpAndTlHelp32()) return;
  HANDLE process_handle = GetCurrentProcess();
  LoadSymbols(process_handle);
}


void OS::SignalCodeMovingGC() {
}


// Walk the stack using the facilities in dbghelp.dll and tlhelp32.dll

// Switch off warning 4748 (/GS can not protect parameters and local variables
// from local buffer overrun because optimizations are disabled in function) as
// it is triggered by the use of inline assembler.
#pragma warning(push)
#pragma warning(disable : 4748)
int OS::StackWalk(Vector<OS::StackFrame> frames) {
  BOOL ok;

  // Load the required functions from DLL's.
  if (!LoadDbgHelpAndTlHelp32()) return kStackWalkError;

  // Get the process and thread handles.
  HANDLE process_handle = GetCurrentProcess();
  HANDLE thread_handle = GetCurrentThread();

  // Read the symbols.
  if (!LoadSymbols(process_handle)) return kStackWalkError;

  // Capture current context.
  CONTEXT context;
  RtlCaptureContext(&context);

  // Initialize the stack walking
  STACKFRAME64 stack_frame;
  memset(&stack_frame, 0, sizeof(stack_frame));
#ifdef  _WIN64
  stack_frame.AddrPC.Offset = context.Rip;
  stack_frame.AddrFrame.Offset = context.Rbp;
  stack_frame.AddrStack.Offset = context.Rsp;
#else
  stack_frame.AddrPC.Offset = context.Eip;
  stack_frame.AddrFrame.Offset = context.Ebp;
  stack_frame.AddrStack.Offset = context.Esp;
#endif
  stack_frame.AddrPC.Mode = AddrModeFlat;
  stack_frame.AddrFrame.Mode = AddrModeFlat;
  stack_frame.AddrStack.Mode = AddrModeFlat;
  int frames_count = 0;

  // Collect stack frames.
  int frames_size = frames.length();
  while (frames_count < frames_size) {
    ok = _StackWalk64(
        IMAGE_FILE_MACHINE_I386,    // MachineType
        process_handle,             // hProcess
        thread_handle,              // hThread
        &stack_frame,               // StackFrame
        &context,                   // ContextRecord
        NULL,                       // ReadMemoryRoutine
        _SymFunctionTableAccess64,  // FunctionTableAccessRoutine
        _SymGetModuleBase64,        // GetModuleBaseRoutine
        NULL);                      // TranslateAddress
    if (!ok) break;

    // Store the address.
    ASSERT((stack_frame.AddrPC.Offset >> 32) == 0);  // 32-bit address.
    frames[frames_count].address =
        reinterpret_cast<void*>(stack_frame.AddrPC.Offset);

    // Try to locate a symbol for this frame.
    DWORD64 symbol_displacement;
    SmartArrayPointer<IMAGEHLP_SYMBOL64> symbol(
        NewArray<IMAGEHLP_SYMBOL64>(kStackWalkMaxNameLen));
    if (symbol.is_empty()) return kStackWalkError;  // Out of memory.
    memset(*symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + kStackWalkMaxNameLen);
    (*symbol)->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    (*symbol)->MaxNameLength = kStackWalkMaxNameLen;
    ok = _SymGetSymFromAddr64(process_handle,             // hProcess
                              stack_frame.AddrPC.Offset,  // Address
                              &symbol_displacement,       // Displacement
                              *symbol);                   // Symbol
    if (ok) {
      // Try to locate more source information for the symbol.
      IMAGEHLP_LINE64 Line;
      memset(&Line, 0, sizeof(Line));
      Line.SizeOfStruct = sizeof(Line);
      DWORD line_displacement;
      ok = _SymGetLineFromAddr64(
          process_handle,             // hProcess
          stack_frame.AddrPC.Offset,  // dwAddr
          &line_displacement,         // pdwDisplacement
          &Line);                     // Line
      // Format a text representation of the frame based on the information
      // available.
      if (ok) {
        SNPrintF(MutableCStrVector(frames[frames_count].text,
                                   kStackWalkMaxTextLen),
                 "%s %s:%d:%d",
                 (*symbol)->Name, Line.FileName, Line.LineNumber,
                 line_displacement);
      } else {
        SNPrintF(MutableCStrVector(frames[frames_count].text,
                                   kStackWalkMaxTextLen),
                 "%s",
                 (*symbol)->Name);
      }
      // Make sure line termination is in place.
      frames[frames_count].text[kStackWalkMaxTextLen - 1] = '\0';
    } else {
      // No text representation of this frame
      frames[frames_count].text[0] = '\0';

      // Continue if we are just missing a module (for non C/C++ frames a
      // module will never be found).
      int err = GetLastError();
      if (err != ERROR_MOD_NOT_FOUND) {
        break;
      }
    }

    frames_count++;
  }

  // Return the number of frames filled in.
  return frames_count;
}

// Restore warnings to previous settings.
#pragma warning(pop)

#else  // __MINGW32__
void OS::LogSharedLibraryAddresses() { }
void OS::SignalCodeMovingGC() { }
int OS::StackWalk(Vector<OS::StackFrame> frames) { return 0; }
#endif  // __MINGW32__


uint64_t OS::CpuFeaturesImpliedByPlatform() {
  return 0;  // Windows runs on anything.
}


double OS::nan_value() {
#ifdef _MSC_VER
  // Positive Quiet NaN with no payload (aka. Indeterminate) has all bits
  // in mask set, so value equals mask.
  static const __int64 nanval = kQuietNaNMask;
  return *reinterpret_cast<const double*>(&nanval);
#else  // _MSC_VER
  return NAN;
#endif  // _MSC_VER
}


int OS::ActivationFrameAlignment() {
#ifdef _WIN64
  return 16;  // Windows 64-bit ABI requires the stack to be 16-byte aligned.
#else
  return 8;  // Floating-point math runs faster with 8-byte alignment.
#endif
}


void OS::ReleaseStore(volatile AtomicWord* ptr, AtomicWord value) {
  MemoryBarrier();
  *ptr = value;
}


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  ASSERT(IsAligned(alignment, static_cast<intptr_t>(OS::AllocateAlignment())));
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* address = ReserveRegion(request_size);
  if (address == NULL) return;
  Address base = RoundUp(static_cast<Address>(address), alignment);
  // Try reducing the size by freeing and then reallocating a specific area.
  bool result = ReleaseRegion(address, request_size);
  USE(result);
  ASSERT(result);
  address = VirtualAlloc(base, size, MEM_RESERVE, PAGE_NOACCESS);
  if (address != NULL) {
    request_size = size;
    ASSERT(base == static_cast<Address>(address));
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
    bool result = ReleaseRegion(address_, size_);
    ASSERT(result);
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
  if (CommitRegion(address, size, is_executable)) {
    UpdateAllocatedSpaceLimits(address, static_cast<int>(size));
    return true;
  }
  return false;
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  ASSERT(IsReserved());
  return UncommitRegion(address, size);
}


void* VirtualMemory::ReserveRegion(size_t size) {
  return RandomizedVirtualAlloc(size, MEM_RESERVE, PAGE_NOACCESS);
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (NULL == VirtualAlloc(base, size, MEM_COMMIT, prot)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(base, static_cast<int>(size));
  return true;
}


bool VirtualMemory::Guard(void* address) {
  if (NULL == VirtualAlloc(address,
                           OS::CommitPageSize(),
                           MEM_COMMIT,
                           PAGE_READONLY | PAGE_GUARD)) {
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
  thread->Run();
  return 0;
}


class Thread::PlatformData : public Malloced {
 public:
  explicit PlatformData(HANDLE thread) : thread_(thread) {}
  HANDLE thread_;
  unsigned thread_id_;
};


// Initialize a Win32 thread object. The thread has an invalid thread
// handle until it is started.

Thread::Thread(const Options& options)
    : stack_size_(options.stack_size()) {
  data_ = new PlatformData(kNoThread);
  set_name(options.name());
}


void Thread::set_name(const char* name) {
  OS::StrNCpy(Vector<char>(name_, sizeof(name_)), name, strlen(name));
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
  ASSERT(result != TLS_OUT_OF_INDEXES);
  return static_cast<LocalStorageKey>(result);
}


void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  BOOL result = TlsFree(static_cast<DWORD>(key));
  USE(result);
  ASSERT(result);
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  return TlsGetValue(static_cast<DWORD>(key));
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  BOOL result = TlsSetValue(static_cast<DWORD>(key), value);
  USE(result);
  ASSERT(result);
}



void Thread::YieldCPU() {
  Sleep(0);
}


// ----------------------------------------------------------------------------
// Win32 mutex support.
//
// On Win32 mutexes are implemented using CRITICAL_SECTION objects. These are
// faster than Win32 Mutex objects because they are implemented using user mode
// atomic instructions. Therefore we only do ring transitions if there is lock
// contention.

class Win32Mutex : public Mutex {
 public:
  Win32Mutex() { InitializeCriticalSection(&cs_); }

  virtual ~Win32Mutex() { DeleteCriticalSection(&cs_); }

  virtual int Lock() {
    EnterCriticalSection(&cs_);
    return 0;
  }

  virtual int Unlock() {
    LeaveCriticalSection(&cs_);
    return 0;
  }


  virtual bool TryLock() {
    // Returns non-zero if critical section is entered successfully entered.
    return TryEnterCriticalSection(&cs_);
  }

 private:
  CRITICAL_SECTION cs_;  // Critical section used for mutex
};


Mutex* OS::CreateMutex() {
  return new Win32Mutex();
}


// ----------------------------------------------------------------------------
// Win32 semaphore support.
//
// On Win32 semaphores are implemented using Win32 Semaphore objects. The
// semaphores are anonymous. Also, the semaphores are initialized to have
// no upper limit on count.


class Win32Semaphore : public Semaphore {
 public:
  explicit Win32Semaphore(int count) {
    sem = ::CreateSemaphoreA(NULL, count, 0x7fffffff, NULL);
  }

  ~Win32Semaphore() {
    CloseHandle(sem);
  }

  void Wait() {
    WaitForSingleObject(sem, INFINITE);
  }

  bool Wait(int timeout) {
    // Timeout in Windows API is in milliseconds.
    DWORD millis_timeout = timeout / 1000;
    return WaitForSingleObject(sem, millis_timeout) != WAIT_TIMEOUT;
  }

  void Signal() {
    LONG dummy;
    ReleaseSemaphore(sem, 1, &dummy);
  }

 private:
  HANDLE sem;
};


Semaphore* OS::CreateSemaphore(int count) {
  return new Win32Semaphore(count);
}


// ----------------------------------------------------------------------------
// Win32 socket support.
//

class Win32Socket : public Socket {
 public:
  explicit Win32Socket() {
    // Create the socket.
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  }
  explicit Win32Socket(SOCKET socket): socket_(socket) { }
  virtual ~Win32Socket() { Shutdown(); }

  // Server initialization.
  bool Bind(const int port);
  bool Listen(int backlog) const;
  Socket* Accept() const;

  // Client initialization.
  bool Connect(const char* host, const char* port);

  // Shutdown socket for both read and write.
  bool Shutdown();

  // Data Transimission
  int Send(const char* data, int len) const;
  int Receive(char* data, int len) const;

  bool SetReuseAddress(bool reuse_address);

  bool IsValid() const { return socket_ != INVALID_SOCKET; }

 private:
  SOCKET socket_;
};


bool Win32Socket::Bind(const int port) {
  if (!IsValid())  {
    return false;
  }

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  int status = bind(socket_,
                    reinterpret_cast<struct sockaddr *>(&addr),
                    sizeof(addr));
  return status == 0;
}


bool Win32Socket::Listen(int backlog) const {
  if (!IsValid()) {
    return false;
  }

  int status = listen(socket_, backlog);
  return status == 0;
}


Socket* Win32Socket::Accept() const {
  if (!IsValid()) {
    return NULL;
  }

  SOCKET socket = accept(socket_, NULL, NULL);
  if (socket == INVALID_SOCKET) {
    return NULL;
  } else {
    return new Win32Socket(socket);
  }
}


bool Win32Socket::Connect(const char* host, const char* port) {
  if (!IsValid()) {
    return false;
  }

  // Lookup host and port.
  struct addrinfo *result = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  int status = getaddrinfo(host, port, &hints, &result);
  if (status != 0) {
    return false;
  }

  // Connect.
  status = connect(socket_,
                   result->ai_addr,
                   static_cast<int>(result->ai_addrlen));
  freeaddrinfo(result);
  return status == 0;
}


bool Win32Socket::Shutdown() {
  if (IsValid()) {
    // Shutdown socket for both read and write.
    int status = shutdown(socket_, SD_BOTH);
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    return status == SOCKET_ERROR;
  }
  return true;
}


int Win32Socket::Send(const char* data, int len) const {
  if (len <= 0) return 0;
  int written = 0;
  while (written < len) {
    int status = send(socket_, data + written, len - written, 0);
    if (status == 0) {
      break;
    } else if (status > 0) {
      written += status;
    } else {
      return 0;
    }
  }
  return written;
}


int Win32Socket::Receive(char* data, int len) const {
  if (len <= 0) return 0;
  int status = recv(socket_, data, len, 0);
  return (status == SOCKET_ERROR) ? 0 : status;
}


bool Win32Socket::SetReuseAddress(bool reuse_address) {
  BOOL on = reuse_address ? true : false;
  int status = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                          reinterpret_cast<char*>(&on), sizeof(on));
  return status == SOCKET_ERROR;
}


bool Socket::SetUp() {
  // Initialize Winsock32
  int err;
  WSADATA winsock_data;
  WORD version_requested = MAKEWORD(1, 0);
  err = WSAStartup(version_requested, &winsock_data);
  if (err != 0) {
    PrintF("Unable to initialize Winsock, err = %d\n", Socket::LastError());
  }

  return err == 0;
}


int Socket::LastError() {
  return WSAGetLastError();
}


uint16_t Socket::HToN(uint16_t value) {
  return htons(value);
}


uint16_t Socket::NToH(uint16_t value) {
  return ntohs(value);
}


uint32_t Socket::HToN(uint32_t value) {
  return htonl(value);
}


uint32_t Socket::NToH(uint32_t value) {
  return ntohl(value);
}


Socket* OS::CreateSocket() {
  return new Win32Socket();
}


// ----------------------------------------------------------------------------
// Win32 profiler support.

class Sampler::PlatformData : public Malloced {
 public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need to make a copy of the handle because we are
  // going to use it in the sampler thread. Using GetThreadHandle() will
  // not work in this case. We're using OpenThread because DuplicateHandle
  // for some reason doesn't work in Chrome's sandbox.
  PlatformData() : profiled_thread_(OpenThread(THREAD_GET_CONTEXT |
                                               THREAD_SUSPEND_RESUME |
                                               THREAD_QUERY_INFORMATION,
                                               false,
                                               GetCurrentThreadId())) {}

  ~PlatformData() {
    if (profiled_thread_ != NULL) {
      CloseHandle(profiled_thread_);
      profiled_thread_ = NULL;
    }
  }

  HANDLE profiled_thread() { return profiled_thread_; }

 private:
  HANDLE profiled_thread_;
};


class SamplerThread : public Thread {
 public:
  static const int kSamplerThreadStackSize = 64 * KB;

  explicit SamplerThread(int interval)
      : Thread(Thread::Options("SamplerThread", kSamplerThreadStackSize)),
        interval_(interval) {}

  static void SetUp() { if (!mutex_) mutex_ = OS::CreateMutex(); }
  static void TearDown() { delete mutex_; }

  static void AddActiveSampler(Sampler* sampler) {
    ScopedLock lock(mutex_);
    SamplerRegistry::AddActiveSampler(sampler);
    if (instance_ == NULL) {
      instance_ = new SamplerThread(sampler->interval());
      instance_->Start();
    } else {
      ASSERT(instance_->interval_ == sampler->interval());
    }
  }

  static void RemoveActiveSampler(Sampler* sampler) {
    ScopedLock lock(mutex_);
    SamplerRegistry::RemoveActiveSampler(sampler);
    if (SamplerRegistry::GetState() == SamplerRegistry::HAS_NO_SAMPLERS) {
      RuntimeProfiler::StopRuntimeProfilerThreadBeforeShutdown(instance_);
      delete instance_;
      instance_ = NULL;
    }
  }

  // Implement Thread::Run().
  virtual void Run() {
    SamplerRegistry::State state;
    while ((state = SamplerRegistry::GetState()) !=
           SamplerRegistry::HAS_NO_SAMPLERS) {
      bool cpu_profiling_enabled =
          (state == SamplerRegistry::HAS_CPU_PROFILING_SAMPLERS);
      bool runtime_profiler_enabled = RuntimeProfiler::IsEnabled();
      // When CPU profiling is enabled both JavaScript and C++ code is
      // profiled. We must not suspend.
      if (!cpu_profiling_enabled) {
        if (rate_limiter_.SuspendIfNecessary()) continue;
      }
      if (cpu_profiling_enabled) {
        if (!SamplerRegistry::IterateActiveSamplers(&DoCpuProfile, this)) {
          return;
        }
      }
      if (runtime_profiler_enabled) {
        if (!SamplerRegistry::IterateActiveSamplers(&DoRuntimeProfile, NULL)) {
          return;
        }
      }
      OS::Sleep(interval_);
    }
  }

  static void DoCpuProfile(Sampler* sampler, void* raw_sampler_thread) {
    if (!sampler->isolate()->IsInitialized()) return;
    if (!sampler->IsProfiling()) return;
    SamplerThread* sampler_thread =
        reinterpret_cast<SamplerThread*>(raw_sampler_thread);
    sampler_thread->SampleContext(sampler);
  }

  static void DoRuntimeProfile(Sampler* sampler, void* ignored) {
    if (!sampler->isolate()->IsInitialized()) return;
    sampler->isolate()->runtime_profiler()->NotifyTick();
  }

  void SampleContext(Sampler* sampler) {
    HANDLE profiled_thread = sampler->platform_data()->profiled_thread();
    if (profiled_thread == NULL) return;

    // Context used for sampling the register state of the profiled thread.
    CONTEXT context;
    memset(&context, 0, sizeof(context));

    TickSample sample_obj;
    TickSample* sample = CpuProfiler::TickSampleEvent(sampler->isolate());
    if (sample == NULL) sample = &sample_obj;

    static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (SuspendThread(profiled_thread) == kSuspendFailed) return;
    sample->state = sampler->isolate()->current_vm_state();

    context.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(profiled_thread, &context) != 0) {
#if V8_HOST_ARCH_X64
      sample->pc = reinterpret_cast<Address>(context.Rip);
      sample->sp = reinterpret_cast<Address>(context.Rsp);
      sample->fp = reinterpret_cast<Address>(context.Rbp);
#else
      sample->pc = reinterpret_cast<Address>(context.Eip);
      sample->sp = reinterpret_cast<Address>(context.Esp);
      sample->fp = reinterpret_cast<Address>(context.Ebp);
#endif
      sampler->SampleStack(sample);
      sampler->Tick(sample);
    }
    ResumeThread(profiled_thread);
  }

  const int interval_;
  RuntimeProfilerRateLimiter rate_limiter_;

  // Protects the process wide state below.
  static Mutex* mutex_;
  static SamplerThread* instance_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};


Mutex* SamplerThread::mutex_ = NULL;
SamplerThread* SamplerThread::instance_ = NULL;


void OS::SetUp() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly can cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srand(static_cast<unsigned int>(seed));
  limit_mutex = CreateMutex();
  SamplerThread::SetUp();
}


void OS::TearDown() {
  SamplerThread::TearDown();
  delete limit_mutex;
}


Sampler::Sampler(Isolate* isolate, int interval)
    : isolate_(isolate),
      interval_(interval),
      profiling_(false),
      active_(false),
      samples_taken_(0) {
  data_ = new PlatformData;
}


Sampler::~Sampler() {
  ASSERT(!IsActive());
  delete data_;
}


void Sampler::Start() {
  ASSERT(!IsActive());
  SetActive(true);
  SamplerThread::AddActiveSampler(this);
}


void Sampler::Stop() {
  ASSERT(IsActive());
  SamplerThread::RemoveActiveSampler(this);
  SetActive(false);
}


} }  // namespace v8::internal
