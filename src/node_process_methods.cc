#include "base_object-inl.h"
#include "base_object.h"
#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_process.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <vector>

#if HAVE_INSPECTOR
#include "inspector_io.h"
#endif

#include <limits.h>  // PATH_MAX
#include <stdio.h>

#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#define umask _umask
typedef int mode_t;
#else
#include <pthread.h>
#include <sys/resource.h>  // getrlimit, setrlimit
#include <termios.h>  // tcgetattr, tcsetattr
#endif

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::BigUint64Array;
using v8::Context;
using v8::Float64Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Name;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Uint32Array;
using v8::Value;

namespace per_process {
Mutex umask_mutex;
}   // namespace per_process

// Microseconds in a second, as a float, used in CPUUsage() below
#define MICROS_PER_SEC 1e6
// used in Hrtime() below
#define NANOS_PER_SEC 1000000000

#ifdef _WIN32
/* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
#define CHDIR_BUFSIZE (MAX_PATH * 4)
#else
#define CHDIR_BUFSIZE (PATH_MAX)
#endif

static void Abort(const FunctionCallbackInfo<Value>& args) {
  Abort();
}

static void Chdir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->owns_process_state());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value path(env->isolate(), args[0]);
  int err = uv_chdir(*path);
  if (err) {
    // Also include the original working directory, since that will usually
    // be helpful information when debugging a `chdir()` failure.
    char buf[CHDIR_BUFSIZE];
    size_t cwd_len = sizeof(buf);
    uv_cwd(buf, &cwd_len);
    return env->ThrowUVException(err, "chdir", nullptr, buf, *path);
  }
}

// CPUUsage use libuv's uv_getrusage() this-process resource usage accessor,
// to access ru_utime (user CPU time used) and ru_stime (system CPU time used),
// which are uv_timeval_t structs (long tv_sec, long tv_usec).
// Returns those values as Float64 microseconds in the elements of the array
// passed to the function.
static void CPUUsage(const FunctionCallbackInfo<Value>& args) {
  uv_rusage_t rusage;

  // Call libuv to get the values we'll return.
  int err = uv_getrusage(&rusage);
  if (err) {
    // On error, return the strerror version of the error code.
    Local<String> errmsg = OneByteString(args.GetIsolate(), uv_strerror(err));
    return args.GetReturnValue().Set(errmsg);
  }

  // Get the double array pointer from the Float64Array argument.
  CHECK(args[0]->IsFloat64Array());
  Local<Float64Array> array = args[0].As<Float64Array>();
  CHECK_EQ(array->Length(), 2);
  Local<ArrayBuffer> ab = array->Buffer();
  double* fields = static_cast<double*>(ab->GetContents().Data());

  // Set the Float64Array elements to be user / system values in microseconds.
  fields[0] = MICROS_PER_SEC * rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec;
  fields[1] = MICROS_PER_SEC * rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec;
}

static void Cwd(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  char buf[CHDIR_BUFSIZE];
  size_t cwd_len = sizeof(buf);
  int err = uv_cwd(buf, &cwd_len);
  if (err)
    return env->ThrowUVException(err, "uv_cwd");

  Local<String> cwd = String::NewFromUtf8(env->isolate(),
                                          buf,
                                          NewStringType::kNormal,
                                          cwd_len).ToLocalChecked();
  args.GetReturnValue().Set(cwd);
}


// Hrtime exposes libuv's uv_hrtime() high-resolution timer.

// This is the legacy version of hrtime before BigInt was introduced in
// JavaScript.
// The value returned by uv_hrtime() is a 64-bit int representing nanoseconds,
// so this function instead fills in an Uint32Array with 3 entries,
// to avoid any integer overflow possibility.
// The first two entries contain the second part of the value
// broken into the upper/lower 32 bits to be converted back in JS,
// because there is no Uint64Array in JS.
// The third entry contains the remaining nanosecond part of the value.
static void Hrtime(const FunctionCallbackInfo<Value>& args) {
  uint64_t t = uv_hrtime();

  Local<ArrayBuffer> ab = args[0].As<Uint32Array>()->Buffer();
  uint32_t* fields = static_cast<uint32_t*>(ab->GetContents().Data());

  fields[0] = (t / NANOS_PER_SEC) >> 32;
  fields[1] = (t / NANOS_PER_SEC) & 0xffffffff;
  fields[2] = t % NANOS_PER_SEC;
}

static void HrtimeBigInt(const FunctionCallbackInfo<Value>& args) {
  Local<ArrayBuffer> ab = args[0].As<BigUint64Array>()->Buffer();
  uint64_t* fields = static_cast<uint64_t*>(ab->GetContents().Data());
  fields[0] = uv_hrtime();
}

static void Kill(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  if (args.Length() != 2)
    return env->ThrowError("Bad argument.");

  int pid;
  if (!args[0]->Int32Value(context).To(&pid)) return;
  int sig;
  if (!args[1]->Int32Value(context).To(&sig)) return;
  int err = uv_kill(pid, sig);
  args.GetReturnValue().Set(err);
}

static void MemoryUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  size_t rss;
  int err = uv_resident_set_memory(&rss);
  if (err)
    return env->ThrowUVException(err, "uv_resident_set_memory");

  Isolate* isolate = env->isolate();
  // V8 memory usage
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);

  // Get the double array pointer from the Float64Array argument.
  CHECK(args[0]->IsFloat64Array());
  Local<Float64Array> array = args[0].As<Float64Array>();
  CHECK_EQ(array->Length(), 4);
  Local<ArrayBuffer> ab = array->Buffer();
  double* fields = static_cast<double*>(ab->GetContents().Data());

  fields[0] = rss;
  fields[1] = v8_heap_stats.total_heap_size();
  fields[2] = v8_heap_stats.used_heap_size();
  fields[3] = v8_heap_stats.external_memory();
}

void RawDebug(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  Utf8Value message(args.GetIsolate(), args[0]);
  PrintErrorString("%s\n", *message);
  fflush(stderr);
}

static void StartProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->StartProfilerIdleNotifier();
}

static void StopProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->StopProfilerIdleNotifier();
}

static void Umask(const FunctionCallbackInfo<Value>& args) {
  uint32_t old;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsUndefined() || args[0]->IsUint32());
  Mutex::ScopedLock scoped_lock(per_process::umask_mutex);

  if (args[0]->IsUndefined()) {
    old = umask(0);
    umask(static_cast<mode_t>(old));
  } else {
    int oct = args[0].As<Uint32>()->Value();
    old = umask(static_cast<mode_t>(oct));
  }

  args.GetReturnValue().Set(old);
}

static void Uptime(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  double uptime;

  uv_update_time(env->event_loop());
  uptime = uv_now(env->event_loop()) - per_process::prog_start_time;

  args.GetReturnValue().Set(uptime / 1000);
}

static void GetActiveRequests(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  std::vector<Local<Value>> request_v;
  for (auto w : *env->req_wrap_queue()) {
    if (w->persistent().IsEmpty())
      continue;
    request_v.push_back(w->GetOwner());
  }

  args.GetReturnValue().Set(
      Array::New(env->isolate(), request_v.data(), request_v.size()));
}

// Non-static, friend of HandleWrap. Could have been a HandleWrap method but
// implemented here for consistency with GetActiveRequests().
void GetActiveHandles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  std::vector<Local<Value>> handle_v;
  for (auto w : *env->handle_wrap_queue()) {
    if (!HandleWrap::HasRef(w))
      continue;
    handle_v.push_back(w->GetOwner());
  }
  args.GetReturnValue().Set(
      Array::New(env->isolate(), handle_v.data(), handle_v.size()));
}

#ifdef __POSIX__
static void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 1) {
    return env->ThrowError("Invalid number of arguments.");
  }

  CHECK(args[0]->IsNumber());
  pid_t pid = args[0].As<Integer>()->Value();
  int r = kill(pid, SIGUSR1);

  if (r != 0) {
    return env->ThrowErrnoException(errno, "kill");
  }
}
#endif  // __POSIX__

#ifdef _WIN32
static int GetDebugSignalHandlerMappingName(DWORD pid,
                                            wchar_t* buf,
                                            size_t buf_len) {
  return _snwprintf(buf, buf_len, L"node-debug-handler-%u", pid);
}

static void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 1) {
    env->ThrowError("Invalid number of arguments.");
    return;
  }

  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE mapping = nullptr;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = nullptr;
  DWORD pid = 0;

  OnScopeLeave cleanup([&]() {
    if (process != nullptr) CloseHandle(process);
    if (thread != nullptr) CloseHandle(thread);
    if (handler != nullptr) UnmapViewOfFile(handler);
    if (mapping != nullptr) CloseHandle(mapping);
  });

  CHECK(args[0]->IsNumber());
  pid = args[0].As<Integer>()->Value();

  process =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  FALSE,
                  pid);
  if (process == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "OpenProcess"));
    return;
  }

  if (GetDebugSignalHandlerMappingName(
          pid, mapping_name, arraysize(mapping_name)) < 0) {
    env->ThrowErrnoException(errno, "sprintf");
    return;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "OpenFileMappingW"));
    return;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof *handler));
  if (handler == nullptr || *handler == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "MapViewOfFile"));
    return;
  }

  thread =
      CreateRemoteThread(process, nullptr, 0, *handler, nullptr, 0, nullptr);
  if (thread == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "CreateRemoteThread"));
    return;
  }

  // Wait for the thread to terminate
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "WaitForSingleObject"));
    return;
  }
}
#endif  // _WIN32

static void DebugEnd(const FunctionCallbackInfo<Value>& args) {
#if HAVE_INSPECTOR
  Environment* env = Environment::GetCurrent(args);
  if (env->inspector_agent()->IsListening()) {
    env->inspector_agent()->Stop();
  }
#endif
}

static void ReallyExit(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  WaitForInspectorDisconnect(env);
  int code = args[0]->Int32Value(env->context()).FromMaybe(0);
  env->Exit(code);
}

static void InitializeProcessMethods(Local<Object> target,
                                     Local<Value> unused,
                                     Local<Context> context,
                                     void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // define various internal methods
  if (env->owns_process_state()) {
    env->SetMethod(target, "_debugProcess", DebugProcess);
    env->SetMethod(target, "_debugEnd", DebugEnd);
    env->SetMethod(target, "abort", Abort);
    env->SetMethod(target, "chdir", Chdir);
  }

  env->SetMethod(
      target, "_startProfilerIdleNotifier", StartProfilerIdleNotifier);
  env->SetMethod(target, "_stopProfilerIdleNotifier", StopProfilerIdleNotifier);

  env->SetMethod(target, "umask", Umask);
  env->SetMethod(target, "_rawDebug", RawDebug);
  env->SetMethod(target, "memoryUsage", MemoryUsage);
  env->SetMethod(target, "cpuUsage", CPUUsage);
  env->SetMethod(target, "hrtime", Hrtime);
  env->SetMethod(target, "hrtimeBigInt", HrtimeBigInt);

  env->SetMethod(target, "_getActiveRequests", GetActiveRequests);
  env->SetMethod(target, "_getActiveHandles", GetActiveHandles);
  env->SetMethod(target, "_kill", Kill);

  env->SetMethodNoSideEffect(target, "cwd", Cwd);
  env->SetMethod(target, "dlopen", binding::DLOpen);
  env->SetMethod(target, "reallyExit", ReallyExit);
  env->SetMethodNoSideEffect(target, "uptime", Uptime);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(process_methods,
                                   node::InitializeProcessMethods)
