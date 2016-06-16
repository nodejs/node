#include "node_report.h"
#include "node.h"
#include "node_natives.h"
#include "node_version.h"
#include "v8.h"
#include "time.h"
#include "zlib.h"
#include "ares.h"

#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#if !defined(_MSC_VER)
#include <strings.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <inttypes.h>
#endif

namespace node {

using v8::HeapSpaceStatistics;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::StackFrame;
using v8::StackTrace;
using v8::String;

using v8::V8;

static void PrintStackFromMessage(FILE* fp, Isolate* isolate,
                                  Local<Message> message_handle);
static void PrintStackFromStackTrace(FILE* fp, Isolate* isolate,
                                     DumpEvent event);
static void PrintStackFrame(FILE* fp, Isolate* isolate,
                            Local<StackFrame> frame, int index);
static void PrintGCStatistics(FILE *fp, Isolate* isolate);
static void PrintSystemInformation(FILE *fp, Isolate* isolate);
static void WriteIntegerWithCommas(FILE *fp, size_t value);

static int seq = 0;  // sequence number for NodeReport filenames
const char* v8_states[] = {
  "JS", "GC", "COMPILER", "OTHER", "EXTERNAL", "IDLE"
};

#ifdef __POSIX__
static struct {
  const char* description;
  int id;
} rlimit_strings[] = {
  {"core file size (blocks)       ", RLIMIT_CORE},
  {"data seg size (kbytes)        ", RLIMIT_DATA},
  {"file size (blocks)            ", RLIMIT_FSIZE},
  {"max locked memory (bytes)     ", RLIMIT_MEMLOCK},
  {"max memory size (kbytes)      ", RLIMIT_RSS},
  {"open files                    ", RLIMIT_NOFILE},
  {"stack size (bytes)            ", RLIMIT_STACK},
  {"cpu time (seconds)            ", RLIMIT_CPU},
  {"max user processes            ", RLIMIT_NPROC},
  {"virtual memory (kbytes)       ", RLIMIT_AS}
};
#endif

/*
 * Function to process the --nodereport-events option.
 */
unsigned int ProcessNodeReportArgs(const char *args) {
  if (strlen(args) == 0) {
    fprintf(stderr, "Missing argument for --nodereport-events option\n");
    return 0;
  }
  // Parse the supplied event types
  unsigned int event_flags = 0;
  const char* cursor = args;
  while (*cursor != '\0') {
    if (!strncmp(cursor, "exception", sizeof("exception") - 1)) {
      event_flags |= NR_EXCEPTION;
      cursor += sizeof("exception") - 1;
    } else if (!strncmp(cursor, "fatalerror", sizeof("fatalerror") - 1)) {
       event_flags |= NR_FATALERROR;
       cursor += sizeof("fatalerror") - 1;
    } else if (!strncmp(cursor, "sigusr2", sizeof("sigusr2") - 1)) {
        event_flags |= NR_SIGUSR2;
        cursor += sizeof("sigusr2") - 1;
    } else if (!strncmp(cursor, "sigquit", sizeof("sigquit") - 1)) {
         event_flags |= NR_SIGQUIT;
         cursor += sizeof("sigquit") - 1;
    } else {
      fprintf(stderr,
        "Unrecognised argument for --nodereport-events option: %s\n", cursor);
       return 0;
    }
    if (*cursor == '+') {
      cursor++;  // Hop over the '+' separator
    }
  }
  return event_flags;
}

/*
 * API to write a NodeReport to file
 */
void TriggerNodeReport(Isolate* isolate, DumpEvent event,
                       const char *message, const char *location) {
  TriggerNodeReport(isolate, event, message, location, Local<Message>());
}

/*
 * API to write a NodeReport to file - with additional V8 Message supplied
 */
void TriggerNodeReport(Isolate* isolate, DumpEvent event,
                       const char *message, const char *location,
                       Local<Message> message_handle) {
  // Construct the NodeReport filename, with timestamp, pid and sequence number
  char filename[48] = "NodeReport";
  seq++;

#ifdef _WIN32
  SYSTEMTIME tm_struct;
  GetLocalTime(&tm_struct);
  DWORD pid = GetCurrentProcessId();

  snprintf(&filename[strlen(filename)], sizeof(filename) - strlen(filename),
           ".%4d%02d%02d.%02d%02d%02d.%d.%03d.txt",
           tm_struct.wYear, tm_struct.wMonth, tm_struct.wDay,
           tm_struct.wHour, tm_struct.wMinute, tm_struct.wSecond,
           pid, seq);
#else  // UNIX, OSX
  struct timeval time_val;
  struct tm tm_struct;
  gettimeofday(&time_val, NULL);

  pid_t pid = getpid();

  localtime_r(&time_val.tv_sec, &tm_struct);
  snprintf(&filename[strlen(filename)], sizeof(filename) - strlen(filename),
           ".%4d%02d%02d.%02d%02d%02d.%d.%03d.txt",
           tm_struct.tm_year+1900, tm_struct.tm_mon+1, tm_struct.tm_mday,
           tm_struct.tm_hour, tm_struct.tm_min, tm_struct.tm_sec,
           pid, seq);
#endif

  // Open the NodeReport file for writing
  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    fprintf(stderr, "\nFailed to open Node.js report file: %s (errno: %d)\n",
            filename, errno);
    return;
  } else {
    fprintf(stderr, "\nWriting Node.js report to file: %s\n", filename);
  }

  // Print NodeReport title and event information
  fprintf(fp,
    "========================================================================"
    "\n"
    "==== NodeReport ========================================================"
    "\n");

  fprintf(fp, "\nEvent: %s, location: \"%s\"\n", message, location);
  fprintf(fp, "Filename: %s\n", filename);

  // Print dump event date/time stamp
#ifdef _WIN32
  fprintf(fp, "Event time: %4d/%02d/%02d %02d:%02d:%02d\n",
          tm_struct.wYear, tm_struct.wMonth, tm_struct.wDay,
          tm_struct.wHour, tm_struct.wMinute, tm_struct.wSecond);
#else  // UNIX, OSX
  fprintf(fp, "Event time: %4d/%02d/%02d %02d:%02d:%02d\n",
          tm_struct.tm_year+1900, tm_struct.tm_mon+1, tm_struct.tm_mday,
          tm_struct.tm_hour, tm_struct.tm_min, tm_struct.tm_sec);
#endif

  // Print native process ID
  fprintf(fp, "Process ID: %d\n", pid);

  // Print Node.js and deps component versions
  fprintf(fp, "\nNode.js version: %s\n", NODE_VERSION);
  fprintf(fp, "(v8: %s, libuv: %s, zlib: %s, ares: %s)\n",
        V8::GetVersion(), uv_version_string(), ZLIB_VERSION, ARES_VERSION_STR);

  // Print summary Javascript stack trace
  fprintf(fp, "\n"
    "========================================================================"
    "\n"
    "==== Javascript stack trace ============================================"
    "\n\n");

  switch (event) {
  case kException:
    // Print the stack as stored in the V8 exception Message object. Note we
    // need to have called isolate->SetCaptureStackTraceForUncaughtExceptions()
    if (!message_handle.IsEmpty()) {
      PrintStackFromMessage(fp, isolate, message_handle);
    }
    break;
  case kFatalError:
    // Print the stack using Message::PrintCurrentStackTrace() API
    Message::PrintCurrentStackTrace(isolate, fp);
    break;
  case kSignal_JS:
  case kSignal_UV:
    // Print the stack using StackTrace::StackTrace() and GetStackSample() APIs
    PrintStackFromStackTrace(fp, isolate, event);
    break;
  }  // end switch(event)

  // Print V8 Heap and Garbage Collector information
  PrintGCStatistics(fp, isolate);

  // Print operating system information
  PrintSystemInformation(fp, isolate);

  fprintf(fp, "\n"
    "========================================================================"
    "\n");
  fclose(fp);
  fprintf(stderr, "Node.js report completed\n");
}

/*
 * Function to print stack trace contained in supplied V8 Message object.
 */
static void PrintStackFromMessage(FILE* fp, Isolate* isolate,
                                  Local<Message> message_handle) {
  Local<String> message_string = message_handle->Get();
  node::Utf8Value message_utf8(isolate, message_string);
  Local<StackTrace>stack = message_handle->GetStackTrace();

  if (stack.IsEmpty()) {
    fprintf(fp, "\nNo stack trace available from V8 Message: %s\n",
            *message_utf8);
    return;
  }
  fprintf(fp, "V8 Message: %s\n", *message_utf8);
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    PrintStackFrame(fp, isolate, stack->GetFrame(i), i);
  }
}

/*
 * Function to print stack using StackTrace::StackTrace() and GetStackSample()
 */
static void PrintStackFromStackTrace(FILE* fp, Isolate* isolate,
                                     DumpEvent event) {
  v8::RegisterState state;
  v8::SampleInfo info;
  void* samples[255];

  // Initialise the register state
  state.pc = NULL;
  state.fp = &state;
  state.sp = &state;

  isolate->GetStackSample(state, samples, arraysize(samples), &info);
  CHECK(static_cast<size_t>(info.vm_state) < arraysize(v8_states));
  fprintf(fp, "Javascript VM state: %s\n\n", v8_states[info.vm_state]);
  if (event == kSignal_UV) {
    fprintf(fp,
      "Signal received when event loop idle, no stack trace available\n");
    return;
  }
  Local<StackTrace> stack = StackTrace::CurrentStackTrace(isolate, 255,
                                                        StackTrace::kDetailed);
  if (stack.IsEmpty()) {
    fprintf(fp,
      "\nNo stack trace available from StackTrace::CurrentStackTrace()\n");
    return;
  }
  // Print the stack trace, adding in the pc values from GetStackSample()
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    PrintStackFrame(fp, isolate, stack->GetFrame(i), i);
    if ((size_t)i < info.frames_count) {
      fprintf(fp, "   program counter: %p\n", samples[i]);
    }
  }
}

/*
 * Function to print a Javascript stack frame from a V8 StackFrame object
 */
static void PrintStackFrame(FILE* fp, Isolate* isolate,
                            Local<StackFrame> frame, int i) {
  node::Utf8Value fn_name_s(isolate, frame->GetFunctionName());
  node::Utf8Value script_name(isolate, frame->GetScriptName());
  const int line_number = frame->GetLineNumber();
  const int column = frame->GetColumn();

  if (frame->IsEval()) {
    if (frame->GetScriptId() == Message::kNoScriptIdInfo) {
      fprintf(fp, "at [eval]:%i:%i\n", line_number, column);
    } else {
      fprintf(fp, "at [eval] (%s:%i:%i)\n", *script_name, line_number, column);
    }
    return;
  }

  if (fn_name_s.length() == 0) {
    fprintf(fp, "%d: %s:%i:%i\n", i+1, *script_name, line_number, column);
  } else {
    if (frame->IsConstructor()) {
      fprintf(fp, "%d: %s [constructor] (%s:%i:%i)\n", i+1,
              *fn_name_s, *script_name, line_number, column);
    } else {
      fprintf(fp, "%d: %s (%s:%i:%i)\n", i+1,
              *fn_name_s, *script_name, line_number, column);
    }
  }
}

/*
 * Function to print V8 Javascript heap information.
 *
 * This uses the existing V8 HeapStatistics and HeapSpaceStatistics APIs.
 * The isolate->GetGCStatistics(&heap_stats) internal V8 API could potentially
 * provide some more useful information - the GC history and the handle counts
 */
static void PrintGCStatistics(FILE *fp, Isolate* isolate) {
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);

  fprintf(fp, "\n"
    "========================================================================"
    "\n"
    "==== Javascript Heap and Garbage Collector ============================="
    "\n");
  HeapSpaceStatistics v8_heap_space_stats;
  // Loop through heap spaces
  for (size_t i = 0; i < isolate->NumberOfHeapSpaces(); i++) {
    isolate->GetHeapSpaceStatistics(&v8_heap_space_stats, i);
    fprintf(fp, "\nHeap space name: %s", v8_heap_space_stats.space_name());
    fprintf(fp, "\n    Memory size: ");
    WriteIntegerWithCommas(fp, v8_heap_space_stats.space_size());
    fprintf(fp, " bytes, committed memory: ");
    WriteIntegerWithCommas(fp, v8_heap_space_stats.physical_space_size());
    fprintf(fp, " bytes\n    Capacity: ");
    WriteIntegerWithCommas(fp, v8_heap_space_stats.space_used_size() +
                           v8_heap_space_stats.space_available_size());
    fprintf(fp, " bytes, used: ");
    WriteIntegerWithCommas(fp, v8_heap_space_stats.space_used_size());
    fprintf(fp, " bytes, available: ");
    WriteIntegerWithCommas(fp, v8_heap_space_stats.space_available_size());
    fprintf(fp, " bytes");
  }

  fprintf(fp, "\n\nTotal heap memory size: ");
  WriteIntegerWithCommas(fp, v8_heap_stats.total_heap_size());
  fprintf(fp, " bytes\nTotal heap committed memory: ");
  WriteIntegerWithCommas(fp, v8_heap_stats.total_physical_size());
  fprintf(fp, " bytes\nTotal used heap memory: ");
  WriteIntegerWithCommas(fp, v8_heap_stats.used_heap_size());
  fprintf(fp, " bytes\nTotal available heap memory: ");
  WriteIntegerWithCommas(fp, v8_heap_stats.total_available_size());
  fprintf(fp, " bytes\n\nHeap memory limit: ");
  WriteIntegerWithCommas(fp, v8_heap_stats.heap_size_limit());
  fprintf(fp, "\n");
}

/*
 * Function to print operating system information.
 */
static void PrintSystemInformation(FILE *fp, Isolate* isolate) {
  fprintf(fp, "\n"
    "========================================================================"
    "\n");
  fprintf(fp,
    "==== System Information ================================================"
    "\n");
  fprintf(fp, "\nPlatform: %s\nArchitecture: %s\n", NODE_PLATFORM, NODE_ARCH);

#ifdef __POSIX__
  fprintf(fp,
    "\nResource                             soft limit      hard limit\n");
  struct rlimit limit;

  for (size_t i = 0; i < arraysize(rlimit_strings); i++) {
    if (getrlimit(rlimit_strings[i].id, &limit) == 0) {
      fprintf(fp, "%s ", rlimit_strings[i].description);
      if (limit.rlim_cur == RLIM_INFINITY) {
        fprintf(fp, "       unlimited");
      } else {
        fprintf(fp, "%16" PRIu64, limit.rlim_cur);
      }
      if (limit.rlim_max == RLIM_INFINITY) {
        fprintf(fp, "       unlimited\n");
      } else {
        fprintf(fp, "%16" PRIu64 "\n", limit.rlim_max);
      }
    }
  }
#endif
}

/*
 * Utility function to print out large integer values with commas.
 */
static void WriteIntegerWithCommas(FILE *fp, size_t value) {
  int thousandsStack[8];  // Sufficient for max 64-bit number
  int stackTop = 0;
  int i;
  size_t workingValue = value;

  do {
    thousandsStack[stackTop++] = workingValue % 1000;
    workingValue /= 1000;
  } while (workingValue != 0);

  for (i = stackTop-1; i >= 0; i--) {
    if (i == (stackTop-1)) {
      fprintf(fp, "%u", thousandsStack[i]);
    } else {
      fprintf(fp, "%03u", thousandsStack[i]);
    }
    if (i > 0) {
       fprintf(fp, ",");
    }
  }
}

}  // namespace node
