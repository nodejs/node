// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#ifndef WIN32_LEAN_AND_MEAN
// WIN32_LEAN_AND_MEAN implies NOCRYPT and NOGDI.
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOKERNEL
#define NOKERNEL
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOSERVICE
#define NOSERVICE
#endif
#ifndef NOSOUND
#define NOSOUND
#endif
#ifndef NOMCX
#define NOMCX
#endif
// Require Windows XP or higher (this is required for the RtlCaptureContext
// function to be present).
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif

#include <windows.h>

#include <time.h>  // For LocalOffset() implementation.
#include <mmsystem.h>  // For timeGetTime().
#ifdef __MINGW32__
// Require Windows XP or higher when compiling with MinGW. This is for MinGW
// header files to expose getaddrinfo.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif  // __MINGW32__
#ifndef __MINGW32__
#include <dbghelp.h>  // For SymLoadModule64 and al.
#endif  // __MINGW32__
#include <limits.h>  // For INT_MAX and al.
#include <tlhelp32.h>  // For Module32First and al.

// These additional WIN32 includes have to be right here as the #undef's below
// makes it impossible to have them elsewhere.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>  // for _beginthreadex()
#include <stdlib.h>

#undef VOID
#undef DELETE
#undef IN
#undef THIS
#undef CONST
#undef NAN
#undef GetObject
#undef CreateMutex
#undef CreateSemaphore

#include "v8.h"

#include "platform.h"

// Extra POSIX/ANSI routines for Win32 when when using Visual Studio C++. Please
// refer to The Open Group Base Specification for specification of the correct
// semantics for these functions.
// (http://www.opengroup.org/onlinepubs/000095399/)
#ifdef _MSC_VER

namespace v8 {
namespace internal {

// Test for finite value - usually defined in math.h
int isfinite(double x) {
  return _finite(x);
}

}  // namespace v8
}  // namespace internal

// Test for a NaN (not a number) value - usually defined in math.h
int isnan(double x) {
  return _isnan(x);
}


// Test for infinity - usually defined in math.h
int isinf(double x) {
  return (_fpclass(x) & (_FPCLASS_PINF | _FPCLASS_NINF)) != 0;
}


// Test if x is less than y and both nominal - usually defined in math.h
int isless(double x, double y) {
  return isnan(x) || isnan(y) ? 0 : x < y;
}


// Test if x is greater than y and both nominal - usually defined in math.h
int isgreater(double x, double y) {
  return isnan(x) || isnan(y) ? 0 : x > y;
}


// Classify floating point number - usually defined in math.h
int fpclassify(double x) {
  // Use the MS-specific _fpclass() for classification.
  int flags = _fpclass(x);

  // Determine class. We cannot use a switch statement because
  // the _FPCLASS_ constants are defined as flags.
  if (flags & (_FPCLASS_PN | _FPCLASS_NN)) return FP_NORMAL;
  if (flags & (_FPCLASS_PZ | _FPCLASS_NZ)) return FP_ZERO;
  if (flags & (_FPCLASS_PD | _FPCLASS_ND)) return FP_SUBNORMAL;
  if (flags & (_FPCLASS_PINF | _FPCLASS_NINF)) return FP_INFINITE;

  // All cases should be covered by the code above.
  ASSERT(flags & (_FPCLASS_SNAN | _FPCLASS_QNAN));
  return FP_NAN;
}


// Test sign - usually defined in math.h
int signbit(double x) {
  // We need to take care of the special case of both positive
  // and negative versions of zero.
  if (x == 0)
    return _fpclass(x) & _FPCLASS_NZ;
  else
    return x < 0;
}


// Case-insensitive bounded string comparisons. Use stricmp() on Win32. Usually
// defined in strings.h.
int strncasecmp(const char* s1, const char* s2, int n) {
  return _strnicmp(s1, s2, n);
}

#endif  // _MSC_VER


// Extra functions for MinGW. Most of these are the _s functions which are in
// the Microsoft Visual Studio C++ CRT.
#ifdef __MINGW32__

int localtime_s(tm* out_tm, const time_t* time) {
  tm* posix_local_time_struct = localtime(time);
  if (posix_local_time_struct == NULL) return 1;
  *out_tm = *posix_local_time_struct;
  return 0;
}


// Not sure this the correct interpretation of _mkgmtime
time_t _mkgmtime(tm* timeptr) {
  return mktime(timeptr);
}


int fopen_s(FILE** pFile, const char* filename, const char* mode) {
  *pFile = fopen(filename, mode);
  return *pFile != NULL ? 0 : 1;
}


int _vsnprintf_s(char* buffer, size_t sizeOfBuffer, size_t count,
                 const char* format, va_list argptr) {
  return _vsnprintf(buffer, sizeOfBuffer, format, argptr);
}
#define _TRUNCATE 0


int strncpy_s(char* strDest, size_t numberOfElements,
              const char* strSource, size_t count) {
  strncpy(strDest, strSource, count);
  return 0;
}

#endif  // __MINGW32__

// Generate a pseudo-random number in the range 0-2^31-1. Usually
// defined in stdlib.h. Missing in both Microsoft Visual Studio C++ and MinGW.
int random() {
  return rand();
}


namespace v8 {
namespace internal {

double ceiling(double x) {
  return ceil(x);
}

#ifdef _WIN64
typedef double (*ModuloFunction)(double, double);

// Defined in codegen-x64.cc.
ModuloFunction CreateModuloFunction();

double modulo(double x, double y) {
  static ModuloFunction function = CreateModuloFunction();
  return function(x, y);
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

// ----------------------------------------------------------------------------
// The Time class represents time on win32. A timestamp is represented as
// a 64-bit integer in 100 nano-seconds since January 1, 1601 (UTC). JavaScript
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
  OS::SNPrintF(Vector<char>(std_tz_name_, kTzNameSize),
               "%S",
               tzinfo_.StandardName);
  std_tz_name_[kTzNameSize - 1] = '\0';
  OS::SNPrintF(Vector<char>(dst_tz_name_, kTzNameSize),
               "%S",
               tzinfo_.DaylightName);
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
  // Convert local time in struct to POSIX time as if it were a UTC time.
  time_t local_posix_time = _mkgmtime(&posix_local_time_struct);
  Time localtime(1000.0 * local_posix_time);

  return localtime.Diff(&rounded_to_second);
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


void OS::Setup() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly can cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srand(static_cast<unsigned int>(seed));
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


// Open log file in binary mode to avoid /n -> /r/n conversion.
const char* OS::LogFileOpenMode = "wb";


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
  int result = strncpy_s(dest.start(), dest.length(), src, n);
  USE(result);
  ASSERT(result == 0);
}


// We keep the lowest and highest addresses mapped as a quick way of
// determining that pointers are outside the heap (used mostly in assertions
// and verification).  The estimate is conservative, ie, not all addresses in
// 'allocated' space are actually allocated to our heap.  The range is
// [lowest, highest), inclusive on the low and and exclusive on the high end.
static void* lowest_ever_allocated = reinterpret_cast<void*>(-1);
static void* highest_ever_allocated = reinterpret_cast<void*>(0);


static void UpdateAllocatedSpaceLimits(void* address, int size) {
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


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
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

  // VirtualAlloc rounds allocated size to page size automatically.
  size_t msize = RoundUp(requested, static_cast<int>(GetPageSize()));
  intptr_t address = NULL;

  // Windows XP SP2 allows Data Excution Prevention (DEP).
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

  // For exectutable pages try and randomize the allocation address
  if (prot == PAGE_EXECUTE_READWRITE && msize >= Page::kPageSize) {
      address = (V8::Random() << kPageSizeBits) | kAllocationRandomAddressMin;
      address &= kAllocationRandomAddressMax;
  }

  LPVOID mbase = VirtualAlloc(reinterpret_cast<void *>(address),
                              msize,
                              MEM_COMMIT | MEM_RESERVE,
                              prot);
  if (mbase == NULL && address != NULL)
    mbase = VirtualAlloc(NULL, msize, MEM_COMMIT | MEM_RESERVE, prot);

  if (mbase == NULL) {
    LOG(StringEvent("OS::Allocate", "VirtualAlloc failed"));
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


#ifdef ENABLE_HEAP_PROTECTION

void OS::Protect(void* address, size_t size) {
  // TODO(1240712): VirtualProtect has a return value which is ignored here.
  DWORD old_protect;
  VirtualProtect(address, size, PAGE_READONLY, &old_protect);
}


void OS::Unprotect(void* address, size_t size, bool is_executable) {
  // TODO(1240712): VirtualProtect has a return value which is ignored here.
  DWORD new_protect = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  DWORD old_protect;
  VirtualProtect(address, size, new_protect, &old_protect);
}

#endif


void OS::Sleep(int milliseconds) {
  ::Sleep(milliseconds);
}


void OS::Abort() {
  if (!IsDebuggerPresent()) {
#ifdef _MSC_VER
    // Make the MSVCRT do a silent abort.
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
    _set_abort_behavior(0, _CALL_REPORTFAULT);
#endif  // _MSC_VER
    abort();
  } else {
    DebugBreak();
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
  Win32MemoryMappedFile(HANDLE file, HANDLE file_mapping, void* memory)
    : file_(file), file_mapping_(file_mapping), memory_(memory) { }
  virtual ~Win32MemoryMappedFile();
  virtual void* memory() { return memory_; }
 private:
  HANDLE file_;
  HANDLE file_mapping_;
  void* memory_;
};


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
  return new Win32MemoryMappedFile(file, file_mapping, memory);
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
                      FALSE);          // fInvadeProcess
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
    LOG(SharedLibraryEvent(
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
    SmartPointer<IMAGEHLP_SYMBOL64> symbol(
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


bool VirtualMemory::IsReserved() {
  return address_ != NULL;
}


VirtualMemory::VirtualMemory(size_t size) {
  address_ = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
  size_ = size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    if (0 == VirtualFree(address(), 0, MEM_RELEASE)) address_ = NULL;
  }
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (NULL == VirtualAlloc(address, size, MEM_COMMIT, prot)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(address, static_cast<int>(size));
  return true;
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  ASSERT(IsReserved());
  return VirtualFree(address, size, MEM_DECOMMIT) != FALSE;
}


// ----------------------------------------------------------------------------
// Win32 thread support.

// Definition of invalid thread handle and id.
static const HANDLE kNoThread = INVALID_HANDLE_VALUE;
static const DWORD kNoThreadId = 0;


class ThreadHandle::PlatformData : public Malloced {
 public:
  explicit PlatformData(ThreadHandle::Kind kind) {
    Initialize(kind);
  }

  void Initialize(ThreadHandle::Kind kind) {
    switch (kind) {
      case ThreadHandle::SELF: tid_ = GetCurrentThreadId(); break;
      case ThreadHandle::INVALID: tid_ = kNoThreadId; break;
    }
  }
  DWORD tid_;  // Win32 thread identifier.
};


// Entry point for threads. The supplied argument is a pointer to the thread
// object. The entry function dispatches to the run method in the thread
// object. It is important that this function has __stdcall calling
// convention.
static unsigned int __stdcall ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // This is also initialized by the last parameter to _beginthreadex() but we
  // don't know which thread will run first (the original thread or the new
  // one) so we initialize it here too.
  thread->thread_handle_data()->tid_ = GetCurrentThreadId();
  thread->Run();
  return 0;
}


// Initialize thread handle to invalid handle.
ThreadHandle::ThreadHandle(ThreadHandle::Kind kind) {
  data_ = new PlatformData(kind);
}


ThreadHandle::~ThreadHandle() {
  delete data_;
}


// The thread is running if it has the same id as the current thread.
bool ThreadHandle::IsSelf() const {
  return GetCurrentThreadId() == data_->tid_;
}


// Test for invalid thread handle.
bool ThreadHandle::IsValid() const {
  return data_->tid_ != kNoThreadId;
}


void ThreadHandle::Initialize(ThreadHandle::Kind kind) {
  data_->Initialize(kind);
}


class Thread::PlatformData : public Malloced {
 public:
  explicit PlatformData(HANDLE thread) : thread_(thread) {}
  HANDLE thread_;
};


// Initialize a Win32 thread object. The thread has an invalid thread
// handle until it is started.

Thread::Thread() : ThreadHandle(ThreadHandle::INVALID) {
  data_ = new PlatformData(kNoThread);
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
                     0,
                     ThreadEntry,
                     this,
                     0,
                     reinterpret_cast<unsigned int*>(
                         &thread_handle_data()->tid_)));
  ASSERT(IsValid());
}


// Wait for thread to terminate.
void Thread::Join() {
  WaitForSingleObject(data_->thread_, INFINITE);
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

  ~Win32Mutex() { DeleteCriticalSection(&cs_); }

  int Lock() {
    EnterCriticalSection(&cs_);
    return 0;
  }

  int Unlock() {
    LeaveCriticalSection(&cs_);
    return 0;
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
  int status = send(socket_, data, len, 0);
  return status;
}


int Win32Socket::Receive(char* data, int len) const {
  int status = recv(socket_, data, len, 0);
  return status;
}


bool Win32Socket::SetReuseAddress(bool reuse_address) {
  BOOL on = reuse_address ? TRUE : FALSE;
  int status = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                          reinterpret_cast<char*>(&on), sizeof(on));
  return status == SOCKET_ERROR;
}


bool Socket::Setup() {
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


#ifdef ENABLE_LOGGING_AND_PROFILING

// ----------------------------------------------------------------------------
// Win32 profiler support.
//
// On win32 we use a sampler thread with high priority to sample the program
// counter for the profiled thread.

class Sampler::PlatformData : public Malloced {
 public:
  explicit PlatformData(Sampler* sampler) {
    sampler_ = sampler;
    sampler_thread_ = INVALID_HANDLE_VALUE;
    profiled_thread_ = INVALID_HANDLE_VALUE;
  }

  Sampler* sampler_;
  HANDLE sampler_thread_;
  HANDLE profiled_thread_;

  // Sampler thread handler.
  void Runner() {
    // Context used for sampling the register state of the profiled thread.
    CONTEXT context;
    memset(&context, 0, sizeof(context));
    // Loop until the sampler is disengaged, keeping the specified
    // sampling frequency.
    for ( ; sampler_->IsActive(); Sleep(sampler_->interval_)) {
      TickSample sample_obj;
      TickSample* sample = CpuProfiler::TickSampleEvent();
      if (sample == NULL) sample = &sample_obj;

      // If the sampler runs in sync with the JS thread, we try to
      // suspend it. If we fail, we skip the current sample.
      if (sampler_->IsSynchronous()) {
        static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
        if (SuspendThread(profiled_thread_) == kSuspendFailed) continue;
      }

      // We always sample the VM state.
      sample->state = VMState::current_state();

      // If profiling, we record the pc and sp of the profiled thread.
      if (sampler_->IsProfiling()) {
        context.ContextFlags = CONTEXT_FULL;
        if (GetThreadContext(profiled_thread_, &context) != 0) {
#if V8_HOST_ARCH_X64
          sample->pc = reinterpret_cast<Address>(context.Rip);
          sample->sp = reinterpret_cast<Address>(context.Rsp);
          sample->fp = reinterpret_cast<Address>(context.Rbp);
#else
          sample->pc = reinterpret_cast<Address>(context.Eip);
          sample->sp = reinterpret_cast<Address>(context.Esp);
          sample->fp = reinterpret_cast<Address>(context.Ebp);
#endif
          sampler_->SampleStack(sample);
        }
      }

      // Invoke tick handler with program counter and stack pointer.
      sampler_->Tick(sample);

      // If the sampler runs in sync with the JS thread, we have to
      // remember to resume it.
      if (sampler_->IsSynchronous()) ResumeThread(profiled_thread_);
    }
  }
};


// Entry point for sampler thread.
static unsigned int __stdcall SamplerEntry(void* arg) {
  Sampler::PlatformData* data =
      reinterpret_cast<Sampler::PlatformData*>(arg);
  data->Runner();
  return 0;
}


// Initialize a profile sampler.
Sampler::Sampler(int interval, bool profiling)
    : interval_(interval),
      profiling_(profiling),
      synchronous_(profiling),
      active_(false) {
  data_ = new PlatformData(this);
}


Sampler::~Sampler() {
  delete data_;
}


// Start profiling.
void Sampler::Start() {
  // If we are starting a synchronous sampler, we need to be able to
  // access the calling thread.
  if (IsSynchronous()) {
    // Get a handle to the calling thread. This is the thread that we are
    // going to profile. We need to make a copy of the handle because we are
    // going to use it in the sampler thread. Using GetThreadHandle() will
    // not work in this case. We're using OpenThread because DuplicateHandle
    // for some reason doesn't work in Chrome's sandbox.
    data_->profiled_thread_ = OpenThread(THREAD_GET_CONTEXT |
                                         THREAD_SUSPEND_RESUME |
                                         THREAD_QUERY_INFORMATION,
                                         FALSE,
                                         GetCurrentThreadId());
    BOOL ok = data_->profiled_thread_ != NULL;
    if (!ok) return;
  }

  // Start sampler thread.
  unsigned int tid;
  active_ = true;
  data_->sampler_thread_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, SamplerEntry, data_, 0, &tid));
  // Set thread to high priority to increase sampling accuracy.
  SetThreadPriority(data_->sampler_thread_, THREAD_PRIORITY_TIME_CRITICAL);
}


// Stop profiling.
void Sampler::Stop() {
  // Seting active to false triggers termination of the sampler
  // thread.
  active_ = false;

  // Wait for sampler thread to terminate.
  WaitForSingleObject(data_->sampler_thread_, INFINITE);

  // Release the thread handles
  CloseHandle(data_->sampler_thread_);
  CloseHandle(data_->profiled_thread_);
}


#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal
