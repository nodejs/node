#ifndef SRC_NODE_REPORT_H_
#define SRC_NODE_REPORT_H_

#include "nan.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <time.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

namespace nodereport {

using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Function;
using v8::Object;
using v8::Number;
using v8::String;
using v8::Value;
using v8::StackTrace;
using v8::StackFrame;
using v8::MaybeLocal;

// Bit-flags for node-report trigger options
#define NR_EXCEPTION  0x01
#define NR_FATALERROR 0x02
#define NR_SIGNAL     0x04
#define NR_APICALL    0x08

// Maximum file and path name lengths
#define NR_MAXNAME 64
#define NR_MAXPATH 1024

enum DumpEvent {kException, kFatalError, kSignal_JS, kSignal_UV, kJavaScript};

#ifdef _WIN32
typedef SYSTEMTIME TIME_TYPE;
#else  // UNIX, OSX
typedef struct tm TIME_TYPE;
#endif

// NODEREPORT_VERSION is defined in binding.gyp
#if !defined(NODEREPORT_VERSION)
#define NODEREPORT_VERSION "dev"
#endif
#define UNKNOWN_NODEVERSION_STRING "Unable to determine Node.js version\n"

// Function declarations - functions in src/node_report.cc
void TriggerNodeReport(Isolate* isolate, DumpEvent event, const char* message, const char* location, char* name, v8::MaybeLocal<v8::Value> error);
void GetNodeReport(Isolate* isolate, DumpEvent event, const char* message, const char* location, v8::MaybeLocal<v8::Value> error, std::ostream& out);

// Function declarations - utility functions in src/utilities.cc
unsigned int ProcessNodeReportEvents(const char* args);
unsigned int ProcessNodeReportSignal(const char* args);
void ProcessNodeReportFileName(const char* args);
void ProcessNodeReportDirectory(const char* args);
unsigned int ProcessNodeReportVerboseSwitch(const char* args);
void SetLoadTime();
void SetVersionString(Isolate* isolate);
void SetCommandLine();
void reportEndpoints(uv_handle_t* h, std::ostringstream& out);
void reportPath(uv_handle_t* h, std::ostringstream& out);
void walkHandle(uv_handle_t* h, void* arg);
void WriteInteger(std::ostream& out, size_t value);

// Global variable declarations - definitions are in src/node-report.c
extern char report_filename[NR_MAXNAME + 1];
extern char report_directory[NR_MAXPATH + 1];
extern std::string version_string;
extern std::string commandline_string;
extern TIME_TYPE loadtime_tm_struct;
extern time_t load_time;

// Local implementation of secure_getenv()
inline const char* secure_getenv(const char* key) {
#ifndef _WIN32
  if (getuid() != geteuid() || getgid() != getegid())
    return nullptr;
#endif
  return getenv(key);
}

// Emulate arraysize() on Windows pre Visual Studio 2015
#if defined(_MSC_VER) && _MSC_VER < 1900
#define arraysize(a) (sizeof(a) / sizeof(*a))
#else
template <typename T, size_t N>
constexpr size_t arraysize(const T(&)[N]) { return N; }
#endif  // defined( _MSC_VER ) && (_MSC_VER < 1900)

// Emulate snprintf() on Windows pre Visual Studio 2015
#if defined( _MSC_VER ) && (_MSC_VER < 1900)
#include <stdarg.h>
inline static int snprintf(char* buffer, size_t n, const char* format, ...) {
  va_list argp;
  va_start(argp, format);
  int ret = _vscprintf(format, argp);
  vsnprintf_s(buffer, n, _TRUNCATE, format, argp);
  va_end(argp);
  return ret;
}

#define __func__ __FUNCTION__
#endif  // defined( _MSC_VER ) && (_MSC_VER < 1900)

}  // namespace nodereport

#endif  // SRC_NODE_REPORT_H_
