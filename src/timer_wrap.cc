#include "timer_wrap.h"  // NOLINT(build/include_inline)
#include "timer_wrap-inl.h"

#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "uv.h"

namespace node {

void TimerWrap::Stop() {
  if (timer_.data == nullptr) return;
  uv_timer_stop(&timer_);
}

void TimerWrap::Close() {
  timer_.data = nullptr;
  env_->CloseHandle(reinterpret_cast<uv_handle_t*>(&timer_), TimerClosedCb);
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
  t->fn_();
}

void TimerWrapHandle::Stop() {
  if (timer_ != nullptr)
    return timer_->Stop();
}

void TimerWrapHandle::Close() {
  if (timer_ != nullptr) {
    timer_->env()->RemoveCleanupHook(CleanupHook, this);
    timer_->Close();
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

void TimerWrapHandle::MemoryInfo(MemoryTracker* tracker) const {
  if (timer_ != nullptr)
    tracker->TrackField("timer", *timer_);
}

void TimerWrapHandle::CleanupHook(void* data) {
  static_cast<TimerWrapHandle*>(data)->Close();
}

}  // namespace node
