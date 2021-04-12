#ifndef SRC_ASYNC_SIGNAL_H_
#define SRC_ASYNC_SIGNAL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "memory_tracker.h"
#include "uv.h"

namespace node {

// AsyncSignal and AsyncSignalHandler are utilities that wrap
// uv_async_t and correctly manage the lifecycle of each to help
// eliminate duplicate code.
class AsyncSignal final : public MemoryRetainer {
 public:
  using Callback = std::function<void()>;

  AsyncSignal(Environment*, const Callback& fn);
  AsyncSignal(const AsyncSignal&) = delete;
  AsyncSignal(const AsyncSignal&&) = delete;
  AsyncSignal& operator=(const AsyncSignal&) = delete;
  AsyncSignal& operator==(const AsyncSignal&&) = delete;

  inline Environment* env() const { return env_; }

  void Close();
  void Send();
  void Ref();
  void Unref();

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(AsyncSignal)
  SET_SELF_SIZE(AsyncSignal)

 private:
  static void ClosedCb(uv_handle_t* handle);
  static void OnSignal(uv_async_t* timer);
  Environment* env_;
  Callback fn_;
  uv_async_t handle_;
};

class AsyncSignalHandle final : public MemoryRetainer {
 public:
  AsyncSignalHandle(
      Environment* env,
      const AsyncSignal::Callback& fn);

  AsyncSignalHandle(const AsyncSignalHandle&) = delete;

  inline ~AsyncSignalHandle() { Close(); }

  void Close();
  void Send();
  void Ref();
  void Unref();

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(AsyncSignalHandle)
  SET_SELF_SIZE(AsyncSignalHandle)

 private:
  static void CleanupHook(void* data);

  AsyncSignal* signal_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_ASYNC_SIGNAL_H_
