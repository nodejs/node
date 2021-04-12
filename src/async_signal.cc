#include "async_signal.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "util-inl.h"

namespace node {

AsyncSignal::AsyncSignal(Environment* env, const Callback& fn)
    : env_(env), fn_(fn) {
  CHECK_EQ(uv_async_init(env->event_loop(), &handle_, OnSignal), 0);
  handle_.data = this;
}

void AsyncSignal::Close() {
  handle_.data = nullptr;
  env_->CloseHandle(reinterpret_cast<uv_handle_t*>(&handle_), ClosedCb);
}

void AsyncSignal::ClosedCb(uv_handle_t* handle) {
  std::unique_ptr<AsyncSignal> ptr(
      ContainerOf(&AsyncSignal::handle_,
                  reinterpret_cast<uv_async_t*>(handle)));
}

void AsyncSignal::Send() {
  if (handle_.data == nullptr) return;
  uv_async_send(&handle_);
}

void AsyncSignal::Ref() {
  if (handle_.data == nullptr) return;
  uv_ref(reinterpret_cast<uv_handle_t*>(&handle_));
}

void AsyncSignal::Unref() {
  if (handle_.data == nullptr) return;
  uv_unref(reinterpret_cast<uv_handle_t*>(&handle_));
}

void AsyncSignal::OnSignal(uv_async_t* handle) {
  AsyncSignal* t = ContainerOf(&AsyncSignal::handle_, handle);
  t->fn_();
}

AsyncSignalHandle::AsyncSignalHandle(
    Environment* env,
    const AsyncSignal::Callback& fn)
    : signal_(new AsyncSignal(env, fn)) {
  env->AddCleanupHook(CleanupHook, this);
  Unref();
}

void AsyncSignalHandle::Close() {
  if (signal_ != nullptr) {
    signal_->env()->RemoveCleanupHook(CleanupHook, this);
    signal_->Close();
  }
  signal_ = nullptr;
}

void AsyncSignalHandle::Send() {
  if (signal_ != nullptr)
    signal_->Send();
}

void AsyncSignalHandle::Ref() {
  if (signal_ != nullptr)
    signal_->Ref();
}

void AsyncSignalHandle::Unref() {
  if (signal_ != nullptr)
    signal_->Unref();
}

void AsyncSignalHandle::MemoryInfo(MemoryTracker* tracker) const {
  if (signal_ != nullptr)
    tracker->TrackField("signal", *signal_);
}

void AsyncSignalHandle::CleanupHook(void* data) {
  static_cast<AsyncSignalHandle*>(data)->Close();
}

}  // namespace node
