#ifndef SRC_NODE_WORKER_H_
#define SRC_NODE_WORKER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_messaging.h"
#include <unordered_map>

namespace node {
namespace worker {

// A worker thread, as represented in its parent thread.
class Worker : public AsyncWrap {
 public:
  Worker(Environment* env, v8::Local<v8::Object> wrap, const std::string& url);
  ~Worker();

  // Run the worker. This is only called from the worker thread.
  void Run();

  // Forcibly exit the thread with a specified exit code. This may be called
  // from any thread.
  void Exit(int code);

  // Wait for the worker thread to stop (in a blocking manner).
  void JoinThread();

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize(
        "isolate_data", sizeof(IsolateData), "IsolateData");
    tracker->TrackFieldWithSize("env", sizeof(Environment), "Environment");
    tracker->TrackField("thread_exit_async", *thread_exit_async_);
    tracker->TrackField("parent_port", parent_port_);
  }

  SET_MEMORY_INFO_NAME(Worker)
  SET_SELF_SIZE(Worker)

  bool is_stopped() const;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StartThread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StopThread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMessagePort(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  void OnThreadStopped();
  void DisposeIsolate();

  uv_loop_t loop_;
  DeleteFnPtr<IsolateData, FreeIsolateData> isolate_data_;
  DeleteFnPtr<Environment, FreeEnvironment> env_;
  const std::string url_;
  v8::Isolate* isolate_ = nullptr;
  DeleteFnPtr<ArrayBufferAllocator, FreeArrayBufferAllocator>
      array_buffer_allocator_;
  uv_thread_t tid_;

  // This mutex protects access to all variables listed below it.
  mutable Mutex mutex_;

  // Currently only used for telling the parent thread that the child
  // thread exited.
  std::unique_ptr<uv_async_t> thread_exit_async_;
  bool scheduled_on_thread_stopped_ = false;

  // This mutex only protects stopped_. If both locks are acquired, this needs
  // to be the latter one.
  mutable Mutex stopped_mutex_;
  bool stopped_ = true;

  bool thread_joined_ = true;
  int exit_code_ = 0;
  uint64_t thread_id_ = -1;

  std::unique_ptr<MessagePortData> child_port_data_;

  // The child port is always kept alive by the child Environment's persistent
  // handle to it.
  MessagePort* child_port_ = nullptr;
  // This is always kept alive because the JS object associated with the Worker
  // instance refers to it via its [kPort] property.
  MessagePort* parent_port_ = nullptr;
};

}  // namespace worker
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS


#endif  // SRC_NODE_WORKER_H_
