
#include "node_report.h"
#include "ares.h"
#include "debug_utils.h"
#include "http_parser.h"
#include "nghttp2/nghttp2ver.h"
#include "node_internals.h"
#include "node_metadata.h"
#include "zlib.h"

#include <atomic>
#include <fstream>

#ifdef _WIN32
#include <Lm.h>
#include <Windows.h>
#include <dbghelp.h>
#include <process.h>
#include <psapi.h>
#include <tchar.h>
#include <cwctype>
#else
#include <sys/resource.h>
// Get the standard printf format macros for C99 stdint types.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <cxxabi.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <sys/utsname.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <iomanip>

#ifndef _MSC_VER
#include <strings.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef _WIN32
extern char** environ;
#endif

namespace report {
using node::arraysize;
using node::Environment;
using node::Mutex;
using node::NativeSymbolDebuggingContext;
using node::PerIsolateOptions;
using v8::HeapSpaceStatistics;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::StackTrace;
using v8::String;
using v8::V8;
using v8::Value;

// Internal/static function declarations
static void WriteNodeReport(Isolate* isolate,
                            Environment* env,
                            const char* message,
                            const char* location,
                            const std::string& filename,
                            std::ostream& out,
                            Local<String> stackstr,
                            TIME_TYPE* time);
static void PrintVersionInformation(JSONWriter* writer);
static void PrintJavaScriptStack(JSONWriter* writer,
                                 Isolate* isolate,
                                 Local<String> stackstr,
                                 const char* location);
static void PrintNativeStack(JSONWriter* writer);
#ifndef _WIN32
static void PrintResourceUsage(JSONWriter* writer);
#endif
static void PrintGCStatistics(JSONWriter* writer, Isolate* isolate);
static void PrintSystemInformation(JSONWriter* writer);
static void PrintLoadedLibraries(JSONWriter* writer);
static void PrintComponentVersions(JSONWriter* writer);
static void LocalTime(TIME_TYPE* tm_struct);

// Global variables
static std::atomic_int seq = {0};  // sequence number for report filenames

// External function to trigger a report, writing to file.
// The 'name' parameter is in/out: an input filename is used
// if supplied, and the actual filename is returned.
std::string TriggerNodeReport(Isolate* isolate,
                              Environment* env,
                              const char* message,
                              const char* location,
                              std::string name,
                              Local<String> stackstr) {
  std::ostringstream oss;
  std::string filename;
  std::shared_ptr<PerIsolateOptions> options;
  if (env != nullptr) options = env->isolate_data()->options();

  // Obtain the current time and the pid (platform dependent)
  TIME_TYPE tm_struct;
  LocalTime(&tm_struct);
  uv_pid_t pid = uv_os_getpid();
  // Determine the required report filename. In order of priority:
  //   1) supplied on API 2) configured on startup 3) default generated
  if (!name.empty()) {
    // Filename was specified as API parameter, use that
    oss << name;
  } else if (env != nullptr && options->report_filename.length() > 0) {
    // File name was supplied via start-up option, use that
    oss << options->report_filename;
  } else {
    // Construct the report filename, with timestamp, pid and sequence number
    oss << "report";
    seq++;
#ifdef _WIN32
    oss << "." << std::setfill('0') << std::setw(4) << tm_struct.wYear;
    oss << std::setfill('0') << std::setw(2) << tm_struct.wMonth;
    oss << std::setfill('0') << std::setw(2) << tm_struct.wDay;
    oss << "." << std::setfill('0') << std::setw(2) << tm_struct.wHour;
    oss << std::setfill('0') << std::setw(2) << tm_struct.wMinute;
    oss << std::setfill('0') << std::setw(2) << tm_struct.wSecond;
    oss << "." << pid;
    oss << "." << std::setfill('0') << std::setw(3) << seq.load();
#else  // UNIX, OSX
    oss << "." << std::setfill('0') << std::setw(4) << tm_struct.tm_year + 1900;
    oss << std::setfill('0') << std::setw(2) << tm_struct.tm_mon + 1;
    oss << std::setfill('0') << std::setw(2) << tm_struct.tm_mday;
    oss << "." << std::setfill('0') << std::setw(2) << tm_struct.tm_hour;
    oss << std::setfill('0') << std::setw(2) << tm_struct.tm_min;
    oss << std::setfill('0') << std::setw(2) << tm_struct.tm_sec;
    oss << "." << pid;
    oss << "." << std::setfill('0') << std::setw(3) << seq.load();
#endif
    oss << ".json";
  }

  filename = oss.str();
  // Open the report file stream for writing. Supports stdout/err,
  // user-specified or (default) generated name
  std::ofstream outfile;
  std::ostream* outstream = &std::cout;
  if (filename == "stdout") {
    outstream = &std::cout;
  } else if (filename == "stderr") {
    outstream = &std::cerr;
  } else {
    // Regular file. Append filename to directory path if one was specified
    if (env != nullptr && options->report_directory.length() > 0) {
      std::string pathname = options->report_directory;
      pathname += PATHSEP;
      pathname += filename;
      outfile.open(pathname, std::ios::out | std::ios::binary);
    } else {
      outfile.open(filename, std::ios::out | std::ios::binary);
    }
    // Check for errors on the file open
    if (!outfile.is_open()) {
      if (env != nullptr && options->report_directory.length() > 0) {
        std::cerr << std::endl
                  << "Failed to open Node.js report file: " << filename
                  << " directory: " << options->report_directory
                  << " (errno: " << errno << ")" << std::endl;
      } else {
        std::cerr << std::endl
                  << "Failed to open Node.js report file: " << filename
                  << " (errno: " << errno << ")" << std::endl;
      }
      return "";
    } else {
      std::cerr << std::endl
                << "Writing Node.js report to file: " << filename << std::endl;
    }
  }

  // Pass our stream about by reference, not by copying it.
  std::ostream& out = outfile.is_open() ? outfile : *outstream;

  WriteNodeReport(
      isolate, env, message, location, filename, out, stackstr, &tm_struct);

  // Do not close stdout/stderr, only close files we opened.
  if (outfile.is_open()) {
    outfile.close();
  }

  std::cerr << "Node.js report completed" << std::endl;
  if (name.empty()) return filename;
  return name;
}

// External function to trigger a report, writing to a supplied stream.
void GetNodeReport(Isolate* isolate,
                   Environment* env,
                   const char* message,
                   const char* location,
                   Local<String> stackstr,
                   std::ostream& out) {
  // Obtain the current time and the pid (platform dependent)
  TIME_TYPE tm_struct;
  LocalTime(&tm_struct);
  std::string str = "NA";
  WriteNodeReport(
      isolate, env, message, location, str, out, stackstr, &tm_struct);
}

// Internal function to coordinate and write the various
// sections of the report to the supplied stream
static void WriteNodeReport(Isolate* isolate,
                            Environment* env,
                            const char* message,
                            const char* location,
                            const std::string& filename,
                            std::ostream& out,
                            Local<String> stackstr,
                            TIME_TYPE* tm_struct) {
  std::ostringstream buf;
  uv_pid_t pid = uv_os_getpid();

  // Save formatting for output stream.
  std::ios old_state(nullptr);
  old_state.copyfmt(out);

  // File stream opened OK, now start printing the report content:
  // the title and header information (event, filename, timestamp and pid)

  JSONWriter writer(out);
  writer.json_start();
  writer.json_objectstart("header");

  writer.json_keyvalue("event", message);
  writer.json_keyvalue("location", location);
  if (!filename.empty())
    writer.json_keyvalue("filename", filename);
  else
    writer.json_keyvalue("filename", std::string("''"));

  // Report dump event and module load date/time stamps
  char timebuf[64];
#ifdef _WIN32
  snprintf(timebuf,
           sizeof(timebuf),
           "%4d/%02d/%02d %02d:%02d:%02d",
           tm_struct->wYear,
           tm_struct->wMonth,
           tm_struct->wDay,
           tm_struct->wHour,
           tm_struct->wMinute,
           tm_struct->wSecond);
  writer.json_keyvalue("dumpEventTime", timebuf);
#else  // UNIX, OSX
  snprintf(timebuf,
           sizeof(timebuf),
           "%4d-%02d-%02dT%02d:%02d:%02dZ",
           tm_struct->tm_year + 1900,
           tm_struct->tm_mon + 1,
           tm_struct->tm_mday,
           tm_struct->tm_hour,
           tm_struct->tm_min,
           tm_struct->tm_sec);
  writer.json_keyvalue("dumpEventTime", timebuf);
  struct timeval ts;
  gettimeofday(&ts, nullptr);
  writer.json_keyvalue("dumpEventTimeStamp",
                       std::to_string(ts.tv_sec * 1000 + ts.tv_usec / 1000));
#endif
  // Report native process ID
  buf << pid;
  writer.json_keyvalue("processId", buf.str());
  buf.flush();

  // Report out the command line.
  if (!node::per_process::cli_options->cmdline.empty()) {
    writer.json_arraystart("commandLine");
    for (std::string arg : node::per_process::cli_options->cmdline) {
      writer.json_element(arg);
    }
    writer.json_arrayend();
  }

  // Report Node.js and OS version information
  PrintVersionInformation(&writer);
  writer.json_objectend();

  // Report summary JavaScript stack backtrace
  PrintJavaScriptStack(&writer, isolate, stackstr, location);

  // Report native stack backtrace
  PrintNativeStack(&writer);

  // Report V8 Heap and Garbage Collector information
  PrintGCStatistics(&writer, isolate);

  // Report OS and current thread resource usage
#ifndef _WIN32
  PrintResourceUsage(&writer);
#endif

  writer.json_arraystart("libuv");
  if (env != nullptr)
    uv_walk(env->event_loop(), WalkHandle, static_cast<void*>(&writer));
  else
    uv_walk(uv_default_loop(), WalkHandle, static_cast<void*>(&writer));

  writer.json_arrayend();

  // Report operating system information
  PrintSystemInformation(&writer);

  writer.json_objectend();

  // Restore output stream formatting.
  out.copyfmt(old_state);
}

// Report Node.js version, OS version and machine information.
static void PrintVersionInformation(JSONWriter* writer) {
  std::ostringstream buf;
  // Report Node version
  buf << "v" << NODE_VERSION_STRING;
  writer->json_keyvalue("nodejsVersion", buf.str());
  buf.str("");
#ifdef __GLIBC__
  buf << __GLIBC__ << "." << __GLIBC_MINOR__;
  writer->json_keyvalue("glibcVersion", buf.str());
  buf.str("");
#endif
  // Report Process word size
  buf << sizeof(void*) * 8 << " bit";
  writer->json_keyvalue("wordSize", buf.str());
  buf.str("");

  // Report deps component versions
  PrintComponentVersions(writer);

  // Report operating system and machine information (Windows)
#ifdef _WIN32
  {
    // Level 101 to obtain the server name, type, and associated details.
    // ref: https://docs.microsoft.com/en-us/windows/desktop/
    // api/lmserver/nf-lmserver-netservergetinfo
    const DWORD level = 101;
    LPSERVER_INFO_101 os_info = nullptr;
    NET_API_STATUS nStatus =
        NetServerGetInfo(nullptr, level, reinterpret_cast<LPBYTE*>(&os_info));
    if (nStatus == NERR_Success) {
      LPSTR os_name = "Windows";
      const DWORD major = os_info->sv101_version_major & MAJOR_VERSION_MASK;
      const DWORD type = os_info->sv101_type;
      const bool isServer = (type & SV_TYPE_DOMAIN_CTRL) ||
                            (type & SV_TYPE_DOMAIN_BAKCTRL) ||
                            (type & SV_TYPE_SERVER_NT);
      switch (major) {
        case 5:
          switch (os_info->sv101_version_minor) {
            case 0:
              os_name = "Windows 2000";
              break;
            default:
              os_name = (isServer ? "Windows Server 2003" : "Windows XP");
          }
          break;
        case 6:
          switch (os_info->sv101_version_minor) {
            case 0:
              os_name = (isServer ? "Windows Server 2008" : "Windows Vista");
              break;
            case 1:
              os_name = (isServer ? "Windows Server 2008 R2" : "Windows 7");
              break;
            case 2:
              os_name = (isServer ? "Windows Server 2012" : "Windows 8");
              break;
            case 3:
              os_name = (isServer ? "Windows Server 2012 R2" : "Windows 8.1");
              break;
            default:
              os_name = (isServer ? "Windows Server" : "Windows Client");
          }
          break;
        case 10:
          os_name = (isServer ? "Windows Server 2016" : "Windows 10");
          break;
        default:
          os_name = (isServer ? "Windows Server" : "Windows Client");
      }
      writer->json_keyvalue("osVersion", os_name);

      // Convert and report the machine name and comment fields
      // (these are LPWSTR types)
      size_t count;
      char name_buf[256];
      wcstombs_s(
          &count, name_buf, sizeof(name_buf), os_info->sv101_name, _TRUNCATE);
      if (os_info->sv101_comment != nullptr) {
        char comment_buf[256];
        wcstombs_s(&count,
                   comment_buf,
                   sizeof(comment_buf),
                   os_info->sv101_comment,
                   _TRUNCATE);
        buf << name_buf << " " << comment_buf;
        writer->json_keyvalue("machine", buf.str());
        buf.flush();
      } else {
        writer->json_keyvalue("machine", name_buf);
      }

      if (os_info != nullptr) {
        NetApiBufferFree(os_info);
      }
    } else {
      // NetServerGetInfo() failed, fallback to use GetComputerName() instead
      TCHAR machine_name[256];
      DWORD machine_name_size = 256;
      writer->json_keyvalue("osVersion", "Windows");
      if (GetComputerName(machine_name, &machine_name_size)) {
        writer->json_keyvalue("machine", machine_name);
      }
    }
  }
#else
  // Report operating system and machine information (Unix/OSX)
  struct utsname os_info;
  if (uname(&os_info) >= 0) {
#ifdef _AIX
    buf << os_info.sysname << " " << os_info.version << "." << os_info.release;
    writer->json_keyvalue("osVersion", buf.str());
    buf.flush();
#else
    buf << os_info.sysname << " " << os_info.release << " " << os_info.version;
    writer->json_keyvalue("osVersion", buf.str());
    buf.flush();
#endif
    const char* (*libc_version)();
    *(reinterpret_cast<void**>(&libc_version)) =
        dlsym(RTLD_DEFAULT, "gnu_get_libc_version");
    if (libc_version != nullptr) {
      writer->json_keyvalue("glibc", (*libc_version)());
    }
    buf << os_info.nodename << " " << os_info.machine;
    writer->json_keyvalue("machine", buf.str());
    buf.flush();
  }
#endif
}

// Report the JavaScript stack.
static void PrintJavaScriptStack(JSONWriter* writer,
                                 Isolate* isolate,
                                 Local<String> stackstr,
                                 const char* location) {
  writer->json_objectstart("javascriptStack");

  std::string ss;
  if ((!strcmp(location, "OnFatalError")) ||
      (!strcmp(location, "OnUserSignal"))) {
    ss = "No stack.\nUnavailable.\n";
  } else {
    String::Utf8Value sv(isolate, stackstr);
    ss = std::string(*sv, sv.length());
  }
  int line = ss.find("\n");
  if (line == -1) {
    writer->json_keyvalue("message", ss.c_str());
    writer->json_objectend();
  } else {
    std::string l = ss.substr(0, line);
    writer->json_keyvalue("message", l);
    writer->json_arraystart("stack");
    ss = ss.substr(line + 1);
    line = ss.find("\n");
    while (line != -1) {
      l = ss.substr(0, line);
      l.erase(l.begin(), std::find_if(l.begin(), l.end(), [](int ch) {
                return !std::iswspace(ch);
              }));
      writer->json_element(l);
      ss = ss.substr(line + 1);
      line = ss.find("\n");
    }
  }
  writer->json_arrayend();
  writer->json_objectend();
}

// Report a native stack backtrace
static void PrintNativeStack(JSONWriter* writer) {
  auto sym_ctx = NativeSymbolDebuggingContext::New();
  void* frames[256];
  const int size = sym_ctx->GetStackTrace(frames, arraysize(frames));
  writer->json_arraystart("nativeStack");
  int i;
  std::ostringstream buf;
  for (i = 1; i < size; i++) {
    void* frame = frames[i];
    buf.str("");
    buf << " [pc=" << frame << "] ";
    buf << sym_ctx->LookupSymbol(frame).Display().c_str();
    writer->json_element(buf.str());
  }
  writer->json_arrayend();
}

// Report V8 JavaScript heap information.
// This uses the existing V8 HeapStatistics and HeapSpaceStatistics APIs.
// The isolate->GetGCStatistics(&heap_stats) internal V8 API could potentially
// provide some more useful information - the GC history and the handle counts
static void PrintGCStatistics(JSONWriter* writer, Isolate* isolate) {
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  HeapSpaceStatistics v8_heap_space_stats;

  writer->json_objectstart("javascriptHeap");
  writer->json_keyvalue("totalMemory",
                        std::to_string(v8_heap_stats.total_heap_size()));
  writer->json_keyvalue("totalCommittedMemory",
                        std::to_string(v8_heap_stats.total_physical_size()));
  writer->json_keyvalue("usedMemory",
                        std::to_string(v8_heap_stats.used_heap_size()));
  writer->json_keyvalue("availableMemory",
                        std::to_string(v8_heap_stats.total_available_size()));
  writer->json_keyvalue("memoryLimit",
                        std::to_string(v8_heap_stats.heap_size_limit()));

  writer->json_objectstart("heapSpaces");
  // Loop through heap spaces
  size_t i;
  for (i = 0; i < isolate->NumberOfHeapSpaces() - 1; i++) {
    isolate->GetHeapSpaceStatistics(&v8_heap_space_stats, i);
    writer->json_objectstart(v8_heap_space_stats.space_name());
    writer->json_keyvalue("memorySize",
                          std::to_string(v8_heap_space_stats.space_size()));
    writer->json_keyvalue(
        "committedMemory",
        std::to_string(v8_heap_space_stats.physical_space_size()));
    writer->json_keyvalue(
        "capacity",
        std::to_string(v8_heap_space_stats.space_used_size() +
                       v8_heap_space_stats.space_available_size()));
    writer->json_keyvalue(
        "used", std::to_string(v8_heap_space_stats.space_used_size()));
    writer->json_keyvalue(
        "available",
        std::to_string(v8_heap_space_stats.space_available_size()));
    writer->json_objectend();
  }
  isolate->GetHeapSpaceStatistics(&v8_heap_space_stats, i);
  writer->json_objectstart(v8_heap_space_stats.space_name());
  writer->json_keyvalue("memorySize",
                        std::to_string(v8_heap_space_stats.space_size()));
  writer->json_keyvalue(
      "committedMemory",
      std::to_string(v8_heap_space_stats.physical_space_size()));
  writer->json_keyvalue(
      "capacity",
      std::to_string(v8_heap_space_stats.space_used_size() +
                     v8_heap_space_stats.space_available_size()));
  writer->json_keyvalue("used",
                        std::to_string(v8_heap_space_stats.space_used_size()));
  writer->json_keyvalue(
      "available", std::to_string(v8_heap_space_stats.space_available_size()));
  writer->json_objectend();
  writer->json_objectend();
  writer->json_objectend();
}

#ifndef _WIN32
// Report resource usage (Linux/OSX only).
static void PrintResourceUsage(JSONWriter* writer) {
  char buf[64];
  double cpu_abs;
  double cpu_percentage;
  time_t current_time;  // current time absolute
  time(&current_time);
  size_t boot_time = static_cast<time_t>(node::per_process::prog_start_time /
                                         (1000 * 1000 * 1000));
  auto uptime = difftime(current_time, boot_time);
  if (uptime == 0) uptime = 1;  // avoid division by zero.

  // Process and current thread usage statistics
  struct rusage stats;
  writer->json_objectstart("resourceUsage");
  if (getrusage(RUSAGE_SELF, &stats) == 0) {
#if defined(__APPLE__) || defined(_AIX)
    snprintf(buf,
             sizeof(buf),
             "%ld.%06d",
             stats.ru_utime.tv_sec,
             stats.ru_utime.tv_usec);
    writer->json_keyvalue("userCpuSeconds", buf);
    snprintf(buf,
             sizeof(buf),
             "%ld.%06d",
             stats.ru_stime.tv_sec,
             stats.ru_stime.tv_usec);
    writer->json_keyvalue("kernelCpuSeconds", buf);
#else
    snprintf(buf,
             sizeof(buf),
             "%ld.%06ld",
             stats.ru_utime.tv_sec,
             stats.ru_utime.tv_usec);
    writer->json_keyvalue("userCpuSeconds", buf);
    snprintf(buf,
             sizeof(buf),
             "%ld.%06ld",
             stats.ru_stime.tv_sec,
             stats.ru_stime.tv_usec);
    writer->json_keyvalue("kernelCpuSeconds", buf);
#endif
    cpu_abs = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec +
              stats.ru_stime.tv_sec + 0.000001 * stats.ru_stime.tv_usec;
    cpu_percentage = (cpu_abs / uptime) * 100.0;
    writer->json_keyvalue("cpuConsumptionPercent",
                          std::to_string(cpu_percentage));
    writer->json_keyvalue("maxRss", std::to_string(stats.ru_maxrss * 1024));
    writer->json_objectstart("pageFaults");
    writer->json_keyvalue("IORequired", std::to_string(stats.ru_majflt));
    writer->json_keyvalue("IONotRequired", std::to_string(stats.ru_minflt));
    writer->json_objectend();
    writer->json_objectstart("fsActivity");
    writer->json_keyvalue("reads", std::to_string(stats.ru_inblock));
    writer->json_keyvalue("writes", std::to_string(stats.ru_oublock));
    writer->json_objectend();
  }
  writer->json_objectend();
#ifdef RUSAGE_THREAD
  if (getrusage(RUSAGE_THREAD, &stats) == 0) {
    writer->json_objectstart("uvthreadResourceUsage");
#if defined(__APPLE__) || defined(_AIX)
    snprintf(buf,
             sizeof(buf),
             "%ld.%06d",
             stats.ru_utime.tv_sec,
             stats.ru_utime.tv_usec);
    writer->json_keyvalue("userCpuSeconds", buf);
    snprintf(buf,
             sizeof(buf),
             "%ld.%06d",
             stats.ru_stime.tv_sec,
             stats.ru_stime.tv_usec);
    writer->json_keyvalue("kernelCpuSeconds", buf);
#else
    snprintf(buf,
             sizeof(buf),
             "%ld.%06ld",
             stats.ru_utime.tv_sec,
             stats.ru_utime.tv_usec);
    writer->json_keyvalue("userCpuSeconds", buf);
    snprintf(buf,
             sizeof(buf),
             "%ld.%06ld",
             stats.ru_stime.tv_sec,
             stats.ru_stime.tv_usec);
    writer->json_keyvalue("kernelCpuSeconds", buf);
#endif
    cpu_abs = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec +
              stats.ru_stime.tv_sec + 0.000001 * stats.ru_stime.tv_usec;
    cpu_percentage = (cpu_abs / uptime) * 100.0;
    writer->json_keyvalue("cpuConsumptionPercent",
                          std::to_string(cpu_percentage));
    writer->json_objectstart("fsActivity");
    writer->json_keyvalue("reads", std::to_string(stats.ru_inblock));
    writer->json_keyvalue("writes", std::to_string(stats.ru_oublock));
    writer->json_objectend();
    writer->json_objectend();
  }
#endif
}
#endif

// Report operating system information.
static void PrintSystemInformation(JSONWriter* writer) {
#ifndef _WIN32
  static struct {
    const char* description;
    int id;
  } rlimit_strings[] = {
    {"core_file_size_blocks", RLIMIT_CORE},
    {"data_seg_size_kbytes", RLIMIT_DATA},
    {"file_size_blocks", RLIMIT_FSIZE},
#if !(defined(_AIX) || defined(__sun))
    {"max_locked_memory_bytes", RLIMIT_MEMLOCK},
#endif
#ifndef __sun
    {"max_memory_size_kbytes", RLIMIT_RSS},
#endif
    {"open_files", RLIMIT_NOFILE},
    {"stack_size_bytes", RLIMIT_STACK},
    {"cpu_time_seconds", RLIMIT_CPU},
#ifndef __sun
    {"max_user_processes", RLIMIT_NPROC},
#endif
    {"virtual_memory_kbytes", RLIMIT_AS}
  };
#endif  // _WIN32
  writer->json_objectstart("environmentVariables");
  Mutex::ScopedLock lock(node::per_process::env_var_mutex);
#ifdef _WIN32
  LPWSTR lpszVariable;
  LPWCH lpvEnv;

  // Get pointer to the environment block
  lpvEnv = GetEnvironmentStringsW();
  if (lpvEnv != nullptr) {
    // Variable strings are separated by null bytes,
    // and the block is terminated by a null byte.
    lpszVariable = reinterpret_cast<LPWSTR>(lpvEnv);
    while (*lpszVariable) {
      DWORD size = WideCharToMultiByte(
          CP_UTF8, 0, lpszVariable, -1, nullptr, 0, nullptr, nullptr);
      char* str = new char[size];
      WideCharToMultiByte(
          CP_UTF8, 0, lpszVariable, -1, str, size, nullptr, nullptr);
      std::string env(str);
      int sep = env.rfind("=");
      std::string key = env.substr(0, sep);
      std::string value = env.substr(sep + 1);
      writer->json_keyvalue(key, value);
      lpszVariable += lstrlenW(lpszVariable) + 1;
    }
    FreeEnvironmentStringsW(lpvEnv);
  }
  writer->json_objectend();
#else
  std::string pair;
  for (char** env = environ; *env != nullptr; ++env) {
    std::string pair(*env);
    int separator = pair.find('=');
    std::string key = pair.substr(0, separator);
    std::string str = pair.substr(separator + 1);
    writer->json_keyvalue(key, str);
  }
  writer->json_objectend();

  writer->json_objectstart("userLimits");
  struct rlimit limit;
  std::string soft, hard;

  for (size_t i = 0; i < arraysize(rlimit_strings); i++) {
    if (getrlimit(rlimit_strings[i].id, &limit) == 0) {
      if (limit.rlim_cur == RLIM_INFINITY)
        soft = std::string("unlimited");
      else
        soft = std::to_string(limit.rlim_cur);

      if (limit.rlim_max == RLIM_INFINITY)
        hard = std::string("unlimited");
      else
        hard = std::to_string(limit.rlim_max);

      writer->json_objectstart(rlimit_strings[i].description);
      writer->json_keyvalue("soft", soft);
      writer->json_keyvalue("hard", hard);
      writer->json_objectend();
    }
  }
  writer->json_objectend();
#endif

  PrintLoadedLibraries(writer);
}

// Report a list of loaded native libraries.
static void PrintLoadedLibraries(JSONWriter* writer) {
  writer->json_arraystart("sharedObjects");
  std::vector<std::string> modules =
      NativeSymbolDebuggingContext::GetLoadedLibraries();
  for (auto const& module_name : modules) writer->json_element(module_name);
  writer->json_arrayend();
}

// Obtain and report the node and subcomponent version strings.
static void PrintComponentVersions(JSONWriter* writer) {
  std::stringstream buf;

  writer->json_objectstart("componentVersions");

#define V(key)                                                                 \
  writer->json_keyvalue(#key, node::per_process::metadata.versions.key.c_str());
  NODE_VERSIONS_KEYS(V)
#undef V

  // Some extra information that is not present in node_metadata.
  writer->json_keyvalue("arch", NODE_ARCH);
  writer->json_keyvalue("platform", NODE_PLATFORM);
  writer->json_keyvalue("release", NODE_RELEASE);
  if (NODE_VERSION_IS_LTS != 0)
    writer->json_keyvalue("lts", NODE_VERSION_LTS_CODENAME);
  writer->json_objectend();
}

static void LocalTime(TIME_TYPE* tm_struct) {
#ifdef _WIN32
  GetLocalTime(tm_struct);
#else  // UNIX, OSX
  struct timeval time_val;
  gettimeofday(&time_val, nullptr);
  localtime_r(&time_val.tv_sec, tm_struct);
#endif
}

}  // namespace report
