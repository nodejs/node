#ifndef SRC_NODE_WORKER_H_
#define SRC_NODE_WORKER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>
#include "node_messaging.h"
#include "uv.h"

namespace node {
namespace worker {

class WorkerThreadData;

// A worker thread, as represented in its parent thread.
class Worker : public AsyncWrap {
 public:
  Worker(Environment* env,
         v8::Local<v8::Object> wrap,
         const std::string& url,
         std::shared_ptr<PerIsolateOptions> per_isolate_opts,
         std::vector<std::string>&& exec_argv);
  ~Worker() override;

  // Run the worker. This is only called from the worker thread.
  void Run();

  // Forcibly exit the thread with a specified exit code. This may be called
  // from any thread.
  void Exit(int code);

  // Wait for the worker thread to stop (in a blocking manner).
  void JoinThread();

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("parent_port", parent_port_);
    tracker->TrackInlineField(&on_thread_finished_, "on_thread_finished_");
  }

  SET_MEMORY_INFO_NAME(Worker)
  SET_SELF_SIZE(Worker)

  bool is_stopped() const;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CloneParentEnvVars(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetEnvVars(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StartThread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StopThread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  void OnThreadStopped();
  void CreateEnvMessagePort(Environment* env);
  const std::string url_;

  std::shared_ptr<PerIsolateOptions> per_isolate_opts_;
  std::vector<std::string> exec_argv_;
  std::vector<std::string> argv_;

  MultiIsolatePlatform* platform_;
  v8::Isolate* isolate_ = nullptr;
  bool profiler_idle_notifier_started_;
  uv_thread_t tid_;

#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
  std::unique_ptr<inspector::ParentInspectorHandle> inspector_parent_handle_;
#endif

  // This mutex protects access to all variables listed below it.
  mutable Mutex mutex_;

  bool thread_joined_ = true;
  int exit_code_ = 0;
  uint64_t thread_id_ = -1;
  uintptr_t stack_base_ = 0;

  // Full size of the thread's stack.
  static constexpr size_t kStackSize = 4 * 1024 * 1024;
  // Stack buffer size that is not available to the JS engine.
  static constexpr size_t kStackBufferSize = 192 * 1024;

  std::unique_ptr<MessagePortData> child_port_data_;
  std::shared_ptr<KVStore> env_vars_;

  // The child port is kept alive by the child Environment's persistent
  // handle to it, as long as that child Environment exists.
  MessagePort* child_port_ = nullptr;
  // This is always kept alive because the JS object associated with the Worker
  // instance refers to it via its [kPort] property.
  MessagePort* parent_port_ = nullptr;

  AsyncRequest on_thread_finished_;

  // A raw flag that is used by creator and worker threads to
  // sync up on pre-mature termination of worker  - while in the
  // warmup phase.  Once the worker is fully warmed up, use the
  // async handle of the worker's Environment for the same purpose.
  bool stopped_ = true;

  // The real Environment of the worker object. It has a lesser
  // lifespan than the worker object itself - comes to life
  // when the worker thread creates a new Environment, and gets
  // destroyed alongwith the worker thread.
  Environment* env_ = nullptr;

  friend class WorkerThreadData;
};

}  // namespace worker
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS


#endif  // SRC_NODE_WORKER_H_
