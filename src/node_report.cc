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

using v8::HandleScope;
using v8::HeapStatistics;
using v8::HeapSpaceStatistics;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Object;
using v8::String;
using v8::StackFrame;
using v8::StackTrace;
using v8::V8;

static void PrintStackFromMessage(FILE* fd, Isolate* isolate, Local<Message> message_handle);
static void PrintStackFromStackTrace(FILE* fd, Isolate* isolate, DumpEvent event);
static void PrintStackFrame(FILE* fd, Isolate* isolate, Local<StackFrame> frame, int index);
static void PrintGCStatistics(FILE *fd, Isolate* isolate);
static void PrintSystemInformation(FILE *fd, Isolate* isolate);
static void WriteIntegerWithCommas(FILE *fd, size_t value);

static int seq = 0; // sequence number for NodeReport filenames
const char* v8_states[] = {"JS","GC","COMPILER","OTHER","EXTERNAL","IDLE"};

#ifdef __POSIX__
static struct {const char* description; int id;} rlimit_strings[] = {
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
 * API to write a triage dump (NodeReport) to file
 */
void TriggerNodeReport(Isolate* isolate, DumpEvent event, const char *message, const char *location) {
  TriggerNodeReport(isolate, event, message, location, Local<Message>());
}

/*
 * API to write a triage dump (NodeReport) to file - with additional V8 Message supplied
 */
void TriggerNodeReport(Isolate* isolate, DumpEvent event, const char *message, const char *location, Local<Message> message_handle) {

  // Construct the NodeReport filename, with timestamp, pid and sequence number
  char filename[48] = "NodeReport";
  seq++; // TODO: sequence number atomic?

#ifdef _WIN32
  SYSTEMTIME tm_struct;
  GetLocalTime(&tm_struct);
  DWORD pid = GetCurrentProcessId();

  sprintf(&filename[10], ".%4d%02d%02d.%02d%02d%02d.%d.%03d.txt",
          tm_struct.wYear,tm_struct.wMonth,tm_struct.wDay,tm_struct.wHour,tm_struct.wMinute,tm_struct.wSecond,pid,seq);
#else // UNIX
  struct timeval time_val;
  struct tm *tm_struct;
  gettimeofday(&time_val, NULL);

  pid_t pid = getpid();

  tm_struct = localtime(&time_val.tv_sec);
  sprintf(&filename[10],".%4d%02d%02d.%02d%02d%02d.%d.%03d.txt",
          tm_struct->tm_year+1900,tm_struct->tm_mon+1,tm_struct->tm_mday,tm_struct->tm_hour,tm_struct->tm_min,tm_struct->tm_sec,pid,seq);
#endif

  // Open the NodeReport file for writing
  FILE *fd = fopen(filename, "w");
  fprintf(stderr,"\nWriting Node.js error report to file: %s\n", filename);

  // Print NodeReport title and event information
  fprintf(fd,"================================================================================\n");
  fprintf(fd,"==== NodeReport ================================================================\n");

  fprintf(fd,"\nEvent: %s, location: \"%s\"\n", message, location);
  fprintf(fd,"Filename: %s\n", filename);

  // Print dump event date/time stamp
#ifdef _WIN32
  fprintf(fd,"Event time: %4d/%02d/%02d %02d:%02d:%02d\n",
          tm_struct.wYear,tm_struct.wMonth,tm_struct.wDay,tm_struct.wHour,tm_struct.wMinute,tm_struct.wSecond);
#else // UNIX
  fprintf(fd,"Event time: %4d/%02d/%02d %02d:%02d:%02d\n",
          tm_struct->tm_year+1900,tm_struct->tm_mon+1,tm_struct->tm_mday,tm_struct->tm_hour,tm_struct->tm_min,tm_struct->tm_sec);
#endif

  // Print native process ID
  fprintf(fd,"Process ID: %d\n", pid);

  // Print Node.js and deps component versions
  fprintf(fd,"\nNode.js version: %s\n", NODE_VERSION);
  fprintf(fd,"(v8: %s, libuv: %s, zlib: %s, ares: %s)\n", V8::GetVersion(), uv_version_string(), ZLIB_VERSION, ARES_VERSION_STR);

  // Print summary Javascript stack trace
  fprintf(fd,"\n================================================================================\n");
  fprintf(fd,"==== Javascript stack trace ====================================================\n\n");

  switch(event) {
  case kException:
    // Print Javascript stack trace as stored in the V8 exception Message object. We need V8 to
    // have saved the stack trace, see isolate->SetCaptureStackTraceForUncaughtExceptions().
    if (!message_handle.IsEmpty()) {
      PrintStackFromMessage(fd, isolate, message_handle);
    }
    break;
  case kFatalError:
    // Print Javascript stack using the Message::PrintCurrentStackTrace() API
    Message::PrintCurrentStackTrace(isolate, fd);
    break;
  case kSignal_JS:
  case kSignal_UV:
    // Print Javascript stack using the StackTrace::StackTrace() and GetStackSample() APIs
    PrintStackFromStackTrace(fd, isolate, event);
    break;
  } // end switch(event)

  // Print V8 Heap and Garbage Collector information
  PrintGCStatistics(fd, isolate);
  
  // Print operating system information
  PrintSystemInformation(fd, isolate);

  // TODO: Obtain and print libuv information, e.g. uv_print_all_handles()
  // TODO: Print native stack
  // TODO: Obtain and print CPU time information e.g. getrusage(), and/or the new V8 CPU time APIs

  fprintf(fd,"\n================================================================================\n");
  fclose(fd);
  fprintf(stderr,"Node.js error report completed\n");
}

/*
 * Function to print stack trace contained in supplied V8 Message object.
 */
static void PrintStackFromMessage(FILE* fd, Isolate* isolate, Local<Message> message_handle) {

  Local<String> message_string = message_handle->Get();
  node::Utf8Value message_utf8(isolate, message_string);
  Local<StackTrace>stack = message_handle->GetStackTrace();

  if (stack.IsEmpty()) {
    fprintf(fd,"\nNo stack trace available from V8 Message: %s\n", *message_utf8);
    return;
  }
  fprintf(fd,"V8 Message: %s\n", *message_utf8);
  for (int i = 0; i < stack->GetFrameCount(); i++) {
	  PrintStackFrame(fd, isolate, stack->GetFrame(i), i);
  }
}

/*
 * Function to print stack trace  the StackTrace::StackTrace() and GetStackSample() APIs
 */
static void PrintStackFromStackTrace(FILE* fd, Isolate* isolate, DumpEvent event) {
  v8::RegisterState state;
  v8::SampleInfo info;
  void* sample[255];

  // Initialise the register state
  state.pc = NULL;
  state.fp = &state;
  state.sp = &state;

  isolate->GetStackSample(state, sample, 255, &info);
  fprintf(fd,"Javascript VM state: %s\n\n", v8_states[info.vm_state]);
  if (event == kSignal_UV) {
    fprintf(fd,"Signal received when event loop idle, no stack trace available\n");
    return;
  }
  Local<StackTrace> stack = StackTrace::CurrentStackTrace(isolate, 255, StackTrace::kDetailed);
  if (stack.IsEmpty()) {
    fprintf(fd,"\nNo stack trace available from StackTrace::CurrentStackTrace()\n");
    return;
  }
  // Print the stack trace, adding in the pc values from the GetStackSample() call
  for (int i=0; i < stack->GetFrameCount(); i++) {
    PrintStackFrame(fd, isolate, stack->GetFrame(i), i);
    if ((size_t)i < info.frames_count) {
      fprintf(fd,"   program counter: %p\n", sample[i]);
    }
  }
}

/*
 * Function to print a Javascript stack frame from a V8 StackFrame object
 */
static void PrintStackFrame(FILE* fd, Isolate* isolate, Local<StackFrame> frame, int i) {
  node::Utf8Value fn_name_s(isolate, frame->GetFunctionName());
  node::Utf8Value script_name(isolate, frame->GetScriptName());
  const int line_number = frame->GetLineNumber();
  const int column = frame->GetColumn();

  if (frame->IsEval()) {
    if (frame->GetScriptId() == Message::kNoScriptIdInfo) {
      fprintf(fd, "at [eval]:%i:%i\n", line_number, column);
    } else {
      fprintf(fd,"at [eval] (%s:%i:%i)\n",*script_name, line_number, column);
    }
    return;
  }

  if (fn_name_s.length() == 0) {
    fprintf(fd, "%d: %s:%i:%i\n", i+1, *script_name, line_number, column);
  } else {
    if (frame->IsConstructor()) {
      fprintf(fd, "%d: %s [constructor] (%s:%i:%i)\n", i+1, *fn_name_s, *script_name, line_number, column);
    } else {
      fprintf(fd, "%d: %s (%s:%i:%i)\n", i+1, *fn_name_s, *script_name, line_number, column);
    }
  }
}

/*
 * Function to print V8 Javascript heap information.
 *
 * This uses the existing V8 HeapStatistics and HeapSpaceStatistics APIs. The isolate->GetGCStatistics(&heap_stats)
 * internal V8 API could potentially provide some more useful information - the GC history strings and the handle counts
 */
static void PrintGCStatistics(FILE *fd, Isolate* isolate) {
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);

  fprintf(fd,"\n================================================================================");
  fprintf(fd,"\n==== Javascript Heap and Garbage Collector =====================================\n");
  HeapSpaceStatistics v8_heap_space_stats;
  // Loop through heap spaces
  for (size_t i = 0; i < isolate->NumberOfHeapSpaces(); i++) {
    isolate->GetHeapSpaceStatistics(&v8_heap_space_stats, i);
    fprintf(fd,"\nHeap space name: %s",v8_heap_space_stats.space_name());
    fprintf(fd,"\n    Memory size: ");
    WriteIntegerWithCommas(fd, v8_heap_space_stats.space_size());
    fprintf(fd," bytes, committed memory: ");
    WriteIntegerWithCommas(fd, v8_heap_space_stats.physical_space_size());
    fprintf(fd," bytes\n    Capacity: ");
    WriteIntegerWithCommas(fd, v8_heap_space_stats.space_used_size() + v8_heap_space_stats.space_available_size());
    fprintf(fd," bytes, used: ");
    WriteIntegerWithCommas(fd, v8_heap_space_stats.space_used_size());
    fprintf(fd," bytes, available: ");
    WriteIntegerWithCommas(fd, v8_heap_space_stats.space_available_size());
    fprintf(fd," bytes");
  }

  fprintf(fd,"\n\nTotal heap memory size: ");
  WriteIntegerWithCommas(fd, v8_heap_stats.total_heap_size());
  fprintf(fd," bytes\nTotal heap committed memory: ");
  WriteIntegerWithCommas(fd, v8_heap_stats.total_physical_size());
  fprintf(fd," bytes\nTotal used heap memory: ");
  WriteIntegerWithCommas(fd, v8_heap_stats.used_heap_size());
  fprintf(fd," bytes\nTotal available heap memory: ");
  WriteIntegerWithCommas(fd, v8_heap_stats.total_available_size());
  fprintf(fd," bytes\n\nHeap memory limit: ");
  WriteIntegerWithCommas(fd, v8_heap_stats.heap_size_limit());
  fprintf(fd,"\n");
}

/*
 * Function to print operating system information (e.g ulimits on UNIX platforms).
 */
static void PrintSystemInformation(FILE *fd, Isolate* isolate) {

  fprintf(fd,"\n================================================================================");
  fprintf(fd,"\n==== System Information ========================================================\n");
  
  fprintf(fd,"\nPlatform: %s\nArchitecture: %s\n", NODE_PLATFORM, NODE_ARCH);

#ifdef __POSIX__
  fprintf(fd,"\nResource                             soft limit      hard limit\n");
  struct rlimit limit;
  
  for (size_t i = 0 ; i < arraysize(rlimit_strings); i++ ) {
    if (getrlimit(rlimit_strings[i].id,&limit) == 0) {
      fprintf(fd,"%s ",rlimit_strings[i].description);
      if (limit.rlim_cur == RLIM_INFINITY) {
        fprintf(fd,"       unlimited");
      } else {
        fprintf(fd,"%16" PRIu64, limit.rlim_cur);
      }
      if (limit.rlim_max == RLIM_INFINITY) {
        fprintf(fd,"       unlimited\n");
      } else {
        fprintf(fd,"%16" PRIu64 "\n", limit.rlim_max);
      }
    }
  }
#endif  
}

/*
 * Utility function to print out integer values with commas between every thousand for readability.
 */
static void WriteIntegerWithCommas(FILE *fd, size_t value) {
  int thousandsStack[7]; /* log1000(2^64-1) = 6.42. We never need more than 7 frames */
  int stackTop = 0;
  int i;
  size_t workingValue = value;

  do {
    thousandsStack[stackTop++] = workingValue % 1000;
    workingValue /= 1000;
  } while (workingValue != 0);

  for (i = stackTop-1; i>=0; i--) {
    if (i == (stackTop -1)) {
      fprintf(fd,"%u",thousandsStack[i]);
    } else {
      fprintf(fd,"%03u",thousandsStack[i]);
    }
    if (i > 0) {
       fprintf(fd,",");
    }
  }
}

} // namespace node
