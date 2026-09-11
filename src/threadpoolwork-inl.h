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

#ifndef SRC_THREADPOOLWORK_INL_H_
#define SRC_THREADPOOLWORK_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string_view>
#include "env-inl.h"
#include "node_diagnostics_channel.h"
#include "node_internals.h"
#include "tracing/trace_event.h"
#include "util-inl.h"

namespace node {

void ThreadPoolWork::ScheduleWork() {
  env_->IncreaseWaitingRequestCounter();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACING_CATEGORY_NODE2(threadpoolwork, async), type_, this);

  // Some ThreadPoolWork instances are submitted more than once.
  enqueued_at_ = 0;
  work_start_ = 0;
  work_end_ = 0;

  if (env_->has_threadpool_work_subscribers()) [[unlikely]] {
    enqueued_at_ = uv_hrtime();
  }

  int status = uv_queue_work(
      env_->event_loop(),
      &work_req_,
      [](uv_work_t* req) {
        ThreadPoolWork* self = ContainerOf(&ThreadPoolWork::work_req_, req);
        if (self->IsObserved()) self->work_start_ = uv_hrtime();
        TRACE_EVENT_BEGIN0(TRACING_CATEGORY_NODE2(threadpoolwork, sync),
                           self->type_);
        self->DoThreadPoolWork();
        if (self->IsObserved()) self->work_end_ = uv_hrtime();
        TRACE_EVENT_END0(TRACING_CATEGORY_NODE2(threadpoolwork, sync),
                         self->type_);
      },
      [](uv_work_t* req, int status) {
        ThreadPoolWork* self = ContainerOf(&ThreadPoolWork::work_req_, req);
        self->env_->DecreaseWaitingRequestCounter();
        TRACE_EVENT_NESTABLE_ASYNC_END1(
            TRACING_CATEGORY_NODE2(threadpoolwork, async),
            self->type_,
            self,
            "result",
            status);
        // AfterThreadPoolWork() may unsubscribe or delete `self`.
        if (self->IsObserved()) {
          auto* channel = self->env_->threadpool_work_channel().get();
          if (channel != nullptr) self->PublishDiagnostics(*channel);
        }
        self->AfterThreadPoolWork(status);
      });
  CHECK_EQ(status, 0);
}

void ThreadPoolWork::PublishDiagnostics(diagnostics_channel::Channel& channel) {
  if (!env_->can_call_into_js() || !channel.HasSubscribers()) return;

  v8::Isolate* isolate = env_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = env_->context();

  // Match performance.now() without losing precision.
  auto to_milliseconds = [isolate, origin = env_->time_origin()](
                             uint64_t mark) -> v8::Local<v8::Value> {
    if (mark == 0) return v8::Null(isolate);
    return v8::Number::New(isolate, static_cast<double>(mark - origin) / 1e6);
  };

  v8::Local<v8::DictionaryTemplate> tmpl = env_->threadpool_work_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view names[] = {
        "type", "enqueued", "started", "ended"};
    tmpl = v8::DictionaryTemplate::New(isolate, names);
    env_->set_threadpool_work_template(tmpl);
  }

  v8::MaybeLocal<v8::Value> values[] = {
      OneByteString(isolate, type_, -1, v8::NewStringType::kInternalized),
      to_milliseconds(enqueued_at_),
      to_milliseconds(work_start_),
      to_milliseconds(work_end_),
  };

  v8::Local<v8::Object> message;
  if (!NewDictionaryInstance(context, tmpl, values).ToLocal(&message)) return;
  channel.Publish(env_, message);
}

int ThreadPoolWork::CancelWork() {
  return uv_cancel(reinterpret_cast<uv_req_t*>(&work_req_));
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_THREADPOOLWORK_INL_H_
