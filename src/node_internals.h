// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_INTERNALS_H_
#define SRC_NODE_INTERNALS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "node.h"
#include "node_binding.h"
#include "node_mutex.h"
#include "tracing/trace_event.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <cstdint>
#include <cstdlib>

#include <string>
#include <vector>

// Custom constants used by both node_constants.cc and node_zlib.cc
#define Z_MIN_WINDOWBITS 8
#define Z_MAX_WINDOWBITS 15
#define Z_DEFAULT_WINDOWBITS 15

struct sockaddr;

namespace node {

namespace native_module {
class NativeModuleLoader;
}

namespace per_process {
extern Mutex env_var_mutex;
extern uint64_t node_start_time;
}  // namespace per_process

// Forward declaration
class Environment;

// Convert a struct sockaddr to a { address: '1.2.3.4', port: 1234 } JS object.
// Sets address and port properties on the info object and returns it.
// If |info| is omitted, a new object is returned.
v8::Local<v8::Object> AddressToJS(
    Environment* env,
    const sockaddr* addr,
    v8::Local<v8::Object> info = v8::Local<v8::Object>());

template <typename T, int (*F)(const typename T::HandleType*, sockaddr*, int*)>
void GetSockOrPeerName(const v8::FunctionCallbackInfo<v8::Value>& args) {
  T* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  CHECK(args[0]->IsObject());
  sockaddr_storage storage;
  int addrlen = sizeof(storage);
  sockaddr* const addr = reinterpret_cast<sockaddr*>(&storage);
  const int err = F(&wrap->handle_, addr, &addrlen);
  if (err == 0)
    AddressToJS(wrap->env(), addr, args[0].As<v8::Object>());
  args.GetReturnValue().Set(err);
}

void PrintStackTrace(v8::Isolate* isolate, v8::Local<v8::StackTrace> stack);
void PrintCaughtException(v8::Isolate* isolate,
                          v8::Local<v8::Context> context,
                          const v8::TryCatch& try_catch);

void ResetStdio();  // Safe to call more than once and from signal handlers.
#ifdef __POSIX__
void SignalExit(int signal, siginfo_t* info, void* ucontext);
#endif

std::string GetProcessTitle(const char* default_title);
std::string GetHumanReadableProcessName();

void InitializeContextRuntime(v8::Local<v8::Context>);
bool InitializePrimordials(v8::Local<v8::Context> context);

namespace task_queue {
void PromiseRejectCallback(v8::PromiseRejectMessage message);
}  // namespace task_queue

class NodeArrayBufferAllocator : public ArrayBufferAllocator {
 public:
  inline uint32_t* zero_fill_field() { return &zero_fill_field_; }

  void* Allocate(size_t size) override;  // Defined in src/node.cc
  void* AllocateUninitialized(size_t size) override;
  void Free(void* data, size_t size) override;
  void* Reallocate(void* data, size_t old_size, size_t size) override;
  virtual void RegisterPointer(void* data, size_t size) {
    total_mem_usage_.fetch_add(size, std::memory_order_relaxed);
  }
  virtual void UnregisterPointer(void* data, size_t size) {
    total_mem_usage_.fetch_sub(size, std::memory_order_relaxed);
  }

  NodeArrayBufferAllocator* GetImpl() final { return this; }
  inline uint64_t total_mem_usage() const {
    return total_mem_usage_.load(std::memory_order_relaxed);
  }

 private:
  uint32_t zero_fill_field_ = 1;  // Boolean but exposed as uint32 to JS land.
  std::atomic<size_t> total_mem_usage_ {0};
};

class DebuggingArrayBufferAllocator final : public NodeArrayBufferAllocator {
 public:
  ~DebuggingArrayBufferAllocator() override;
  void* Allocate(size_t size) override;
  void* AllocateUninitialized(size_t size) override;
  void Free(void* data, size_t size) override;
  void* Reallocate(void* data, size_t old_size, size_t size) override;
  void RegisterPointer(void* data, size_t size) override;
  void UnregisterPointer(void* data, size_t size) override;

 private:
  void RegisterPointerInternal(void* data, size_t size);
  void UnregisterPointerInternal(void* data, size_t size);
  Mutex mutex_;
  std::unordered_map<void*, size_t> allocations_;
};

namespace Buffer {
v8::MaybeLocal<v8::Object> Copy(Environment* env, const char* data, size_t len);
v8::MaybeLocal<v8::Object> New(Environment* env, size_t size);
// Takes ownership of |data|.
v8::MaybeLocal<v8::Object> New(Environment* env,
                               char* data,
                               size_t length,
                               void (*callback)(char* data, void* hint),
                               void* hint);
// Takes ownership of |data|.  Must allocate |data| with the current Isolate's
// ArrayBuffer::Allocator().
v8::MaybeLocal<v8::Object> New(Environment* env,
                               char* data,
                               size_t length);
// Creates a Buffer instance over an existing ArrayBuffer.
v8::MaybeLocal<v8::Uint8Array> New(Environment* env,
                                   v8::Local<v8::ArrayBuffer> ab,
                                   size_t byte_offset,
                                   size_t length);
// Construct a Buffer from a MaybeStackBuffer (and also its subclasses like
// Utf8Value and TwoByteValue).
// If |buf| is invalidated, an empty MaybeLocal is returned, and nothing is
// changed.
// If |buf| contains actual data, this method takes ownership of |buf|'s
// underlying buffer. However, |buf| itself can be reused even after this call,
// but its capacity, if increased through AllocateSufficientStorage, is not
// guaranteed to stay the same.
template <typename T>
static v8::MaybeLocal<v8::Object> New(Environment* env,
                                      MaybeStackBuffer<T>* buf) {
  v8::MaybeLocal<v8::Object> ret;
  char* src = reinterpret_cast<char*>(buf->out());
  const size_t len_in_bytes = buf->length() * sizeof(buf->out()[0]);

  if (buf->IsAllocated())
    ret = New(env, src, len_in_bytes);
  else if (!buf->IsInvalidated())
    ret = Copy(env, src, len_in_bytes);

  if (ret.IsEmpty())
    return ret;

  if (buf->IsAllocated())
    buf->Release();

  return ret;
}
}  // namespace Buffer

v8::MaybeLocal<v8::Value> InternalMakeCallback(
    Environment* env,
    v8::Local<v8::Object> resource,
    v8::Local<v8::Object> recv,
    const v8::Local<v8::Function> callback,
    int argc,
    v8::Local<v8::Value> argv[],
    async_context asyncContext);

class InternalCallbackScope {
 public:
  enum Flags {
    kNoFlags = 0,
    // Indicates whether 'before' and 'after' hooks should be skipped.
    kSkipAsyncHooks = 1,
    // Indicates whether nextTick and microtask queues should be skipped.
    // This should only be used when there is no call into JS in this scope.
    // (The HTTP parser also uses it for some weird backwards
    // compatibility issues, but it shouldn't.)
    kSkipTaskQueues = 2
  };
  InternalCallbackScope(Environment* env,
                        v8::Local<v8::Object> object,
                        const async_context& asyncContext,
                        int flags = kNoFlags);
  // Utility that can be used by AsyncWrap classes.
  explicit InternalCallbackScope(AsyncWrap* async_wrap, int flags = 0);
  ~InternalCallbackScope();
  void Close();

  inline bool Failed() const { return failed_; }
  inline void MarkAsFailed() { failed_ = true; }

 private:
  Environment* env_;
  async_context async_context_;
  v8::Local<v8::Object> object_;
  bool skip_hooks_;
  bool skip_task_queues_;
  bool failed_ = false;
  bool pushed_ids_ = false;
  bool closed_ = false;
};

class DebugSealHandleScope {
 public:
  explicit inline DebugSealHandleScope(v8::Isolate* isolate = nullptr)
#ifdef DEBUG
    : actual_scope_(isolate != nullptr ? isolate : v8::Isolate::GetCurrent())
#endif
  {}

 private:
#ifdef DEBUG
  v8::SealHandleScope actual_scope_;
#endif
};

class ThreadPoolWork {
 public:
  explicit inline ThreadPoolWork(Environment* env) : env_(env) {
    CHECK_NOT_NULL(env);
  }
  inline virtual ~ThreadPoolWork() = default;

  inline void ScheduleWork();
  inline int CancelWork();

  virtual void DoThreadPoolWork() = 0;
  virtual void AfterThreadPoolWork(int status) = 0;

  Environment* env() const { return env_; }

 private:
  Environment* env_;
  uv_work_t work_req_;
};

#define TRACING_CATEGORY_NODE "node"
#define TRACING_CATEGORY_NODE1(one)                                           \
    TRACING_CATEGORY_NODE ","                                                 \
    TRACING_CATEGORY_NODE "." #one
#define TRACING_CATEGORY_NODE2(one, two)                                      \
    TRACING_CATEGORY_NODE ","                                                 \
    TRACING_CATEGORY_NODE "." #one ","                                        \
    TRACING_CATEGORY_NODE "." #one "." #two

// Functions defined in node.cc that are exposed via the bootstrapper object

#if defined(__POSIX__) && !defined(__ANDROID__) && !defined(__CloudABI__)
#define NODE_IMPLEMENTS_POSIX_CREDENTIALS 1
#endif  // __POSIX__ && !defined(__ANDROID__) && !defined(__CloudABI__)

namespace credentials {
bool SafeGetenv(const char* key, std::string* text, Environment* env = nullptr);
}  // namespace credentials

void DefineZlibConstants(v8::Local<v8::Object> target);
v8::Isolate* NewIsolate(v8::Isolate::CreateParams* params,
                        uv_loop_t* event_loop,
                        MultiIsolatePlatform* platform);
// This overload automatically picks the right 'main_script_id' if no callback
// was provided by the embedder.
v8::MaybeLocal<v8::Value> StartExecution(Environment* env,
                                         StartExecutionCallback cb = nullptr);
v8::MaybeLocal<v8::Object> GetPerContextExports(v8::Local<v8::Context> context);
v8::MaybeLocal<v8::Value> ExecuteBootstrapper(
    Environment* env,
    const char* id,
    std::vector<v8::Local<v8::String>>* parameters,
    std::vector<v8::Local<v8::Value>>* arguments);
void MarkBootstrapComplete(const v8::FunctionCallbackInfo<v8::Value>& args);

struct InitializationResult {
  int exit_code = 0;
  std::vector<std::string> args;
  std::vector<std::string> exec_args;
  bool early_return = false;
};
InitializationResult InitializeOncePerProcess(int argc, char** argv);
void TearDownOncePerProcess();
void SetIsolateErrorHandlers(v8::Isolate* isolate, const IsolateSettings& s);
void SetIsolateMiscHandlers(v8::Isolate* isolate, const IsolateSettings& s);
void SetIsolateCreateParamsForNode(v8::Isolate::CreateParams* params);

#if HAVE_INSPECTOR
namespace profiler {
void StartProfilers(Environment* env);
}
#endif  // HAVE_INSPECTOR

#ifdef __POSIX__
static constexpr unsigned kMaxSignal = 32;
#endif

bool HasSignalJSHandler(int signum);

#ifdef _WIN32
typedef SYSTEMTIME TIME_TYPE;
#else  // UNIX, OSX
typedef struct tm TIME_TYPE;
#endif

double GetCurrentTimeInMicroseconds();
int WriteFileSync(const char* path, uv_buf_t buf);
int WriteFileSync(v8::Isolate* isolate,
                  const char* path,
                  v8::Local<v8::String> string);

class DiagnosticFilename {
 public:
  static void LocalTime(TIME_TYPE* tm_struct);

  inline DiagnosticFilename(Environment* env,
                            const char* prefix,
                            const char* ext);

  inline DiagnosticFilename(uint64_t thread_id,
                            const char* prefix,
                            const char* ext);

  inline const char* operator*() const;

 private:
  static std::string MakeFilename(
      uint64_t thread_id,
      const char* prefix,
      const char* ext);

  std::string filename_;
};

class TraceEventScope {
 public:
  TraceEventScope(const char* category,
                  const char* name,
                  void* id) : category_(category), name_(name), id_(id) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(category_, name_, id_);
  }
  ~TraceEventScope() {
    TRACE_EVENT_NESTABLE_ASYNC_END0(category_, name_, id_);
  }

 private:
  const char* category_;
  const char* name_;
  void* id_;
};

namespace heap {

void DeleteHeapSnapshot(const v8::HeapSnapshot* snapshot);
using HeapSnapshotPointer =
  DeleteFnPtr<const v8::HeapSnapshot, DeleteHeapSnapshot>;

BaseObjectPtr<AsyncWrap> CreateHeapSnapshotStream(
    Environment* env, HeapSnapshotPointer&& snapshot);
}  // namespace heap

namespace fs {
std::string Basename(const std::string& str, const std::string& extension);
}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_INTERNALS_H_
