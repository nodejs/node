#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "timer_wrap.h"
#include "uv.h"

namespace node {

TimerWrap::TimerWrap(Environment* env, TimerCb fn, void* user_data)
    : env_(env),
      fn_(fn),
      user_data_(user_data) {
  uv_timer_init(env->event_loop(), &timer_);
  timer_.data = this;
}

void TimerWrap::Stop(bool close) {
  if (timer_.data == nullptr) return;
  uv_timer_stop(&timer_);
  if (LIKELY(close)) {
    timer_.data = nullptr;
    env_->CloseHandle(reinterpret_cast<uv_handle_t*>(&timer_), TimerClosedCb);
  }
}

void TimerWrap::TimerClosedCb(uv_handle_t* handle) {
  std::unique_ptr<TimerWrap> ptr(
      ContainerOf(&TimerWrap::timer_,
                  reinterpret_cast<uv_timer_t*>(handle)));
}

void TimerWrap::Update(uint64_t interval, uint64_t repeat) {
  if (timer_.data == nullptr) return;
  uv_timer_start(&timer_, OnTimeout, interval, repeat);
}

void TimerWrap::Ref() {
  if (timer_.data == nullptr) return;
  uv_ref(reinterpret_cast<uv_handle_t*>(&timer_));
}

void TimerWrap::Unref() {
  if (timer_.data == nullptr) return;
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
}

void TimerWrap::OnTimeout(uv_timer_t* timer) {
  TimerWrap* t = ContainerOf(&TimerWrap::timer_, timer);
  t->fn_(t->user_data_);
}

TimerWrapHandle::TimerWrapHandle(
    Environment* env,
    TimerWrap::TimerCb fn,
    void* user_data) {
  timer_ = new TimerWrap(env, fn, user_data);
  env->AddCleanupHook(CleanupHook, this);
}

void TimerWrapHandle::Stop(bool close) {
  if (UNLIKELY(!close))
    return timer_->Stop(close);

  if (timer_ != nullptr) {
    timer_->env()->RemoveCleanupHook(CleanupHook, this);
    timer_->Stop();
  }
  timer_ = nullptr;
}

void TimerWrapHandle::Ref() {
  if (timer_ != nullptr)
    timer_->Ref();
}

void TimerWrapHandle::Unref() {
  if (timer_ != nullptr)
    timer_->Unref();
}

void TimerWrapHandle::Update(uint64_t interval, uint64_t repeat) {
  if (timer_ != nullptr)
    timer_->Update(interval, repeat);
}

void TimerWrapHandle::CleanupHook(void* data) {
  static_cast<TimerWrapHandle*>(data)->Stop();
}

void TimerWrapHandle::MemoryInfo(node::MemoryTracker* tracker) const {
  if (timer_ != nullptr)
    tracker->TrackField("timer", *timer_);
}

}  // namespace node
