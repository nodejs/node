#ifndef SRC_NODE_TRACE_EVENTS_H_
#define SRC_NODE_TRACE_EVENTS_H_

#ifndef V8_USE_PERFETTO
#error This header should be used with perfetto
#endif  // V8_USE_PERFETTO

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "handle_wrap.h"
#include "env.h"
#include "memory_tracker.h"
#include "v8.h"
#include "uv.h"

#include <atomic>

namespace node {
namespace tracing {

struct JSTracingSessionTraits {
  struct Settings {};

  struct State {
    int id = -1;
  };

  static void Init(
      TracingSession<JSTracingSessionTraits>* session,
      const TracingSession<JSTracingSessionTraits>::Options& options,
      const perfetto::TraceConfig& config);
  static void OnStartTracing(
      TracingSession<JSTracingSessionTraits>* session);
  static void OnStopTracing(
      TracingSession<JSTracingSessionTraits>* session);
};

using JSTracingSessionBase = TracingSession<JSTracingSessionTraits>;

class JSTracingSession : public HandleWrap, public JSTracingSessionBase {
 public:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  JSTracingSession(
      Environment* env,
      v8::Local<v8::Object> object,
      TracingService* service,
      const JSTracingSession::Options& options,
      v8::Local<v8::Function> callback);

  void OnClose() override;

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(JSTracingSession);
  SET_SELF_SIZE(JSTracingSession);

 private:
  enum class FinishState {
    RUNNING,
    DONE,
    WAITING,
    COMPLETE
  };

  void set_finished_state(FinishState state) { finish_state_.store(state); }
  FinishState get_finished_state() { return finish_state_.load(); }
  void Start();
  void Stop();
  void ProcessData();
  static void Handle(uv_check_t* handle);

  uv_check_t check_;
  v8::Global<v8::Function> callback_;

  std::atomic<FinishState> finish_state_ {FinishState::RUNNING};

  friend class JSTracingSessionTraits;
};

}  // namespace tracing
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS


#endif  // SRC_NODE_TRACE_EVENTS_H_
