#ifndef SRC_CALLBACK_QUEUE_INL_H_
#define SRC_CALLBACK_QUEUE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "callback_queue.h"

namespace node {

template <typename R, typename... Args>
template <typename Fn>
std::unique_ptr<typename CallbackQueue<R, Args...>::Callback>
CallbackQueue<R, Args...>::CreateCallback(Fn&& fn, CallbackFlags::Flags flags) {
  return std::make_unique<CallbackImpl<Fn>>(std::move(fn), flags);
}

template <typename R, typename... Args>
std::unique_ptr<typename CallbackQueue<R, Args...>::Callback>
CallbackQueue<R, Args...>::Shift() {
  std::unique_ptr<Callback> ret = std::move(head_);
  if (ret) {
    head_ = ret->get_next();
    if (!head_)
      tail_ = nullptr;  // The queue is now empty.
  }
  size_--;
  return ret;
}

template <typename R, typename... Args>
void CallbackQueue<R, Args...>::Push(std::unique_ptr<Callback> cb) {
  Callback* prev_tail = tail_;

  size_++;
  tail_ = cb.get();
  if (prev_tail != nullptr)
    prev_tail->set_next(std::move(cb));
  else
    head_ = std::move(cb);
}

template <typename R, typename... Args>
void CallbackQueue<R, Args...>::ConcatMove(CallbackQueue<R, Args...>&& other) {
  size_ += other.size_;
  if (tail_ != nullptr)
    tail_->set_next(std::move(other.head_));
  else
    head_ = std::move(other.head_);
  tail_ = other.tail_;
  other.tail_ = nullptr;
  other.size_ = 0;
}

template <typename R, typename... Args>
size_t CallbackQueue<R, Args...>::size() const {
  return size_.load();
}

template <typename R, typename... Args>
CallbackQueue<R, Args...>::Callback::Callback(CallbackFlags::Flags flags)
  : flags_(flags) {}

template <typename R, typename... Args>
CallbackFlags::Flags CallbackQueue<R, Args...>::Callback::flags() const {
  return flags_;
}

template <typename R, typename... Args>
std::unique_ptr<typename CallbackQueue<R, Args...>::Callback>
CallbackQueue<R, Args...>::Callback::get_next() {
  return std::move(next_);
}

template <typename R, typename... Args>
void CallbackQueue<R, Args...>::Callback::set_next(
    std::unique_ptr<Callback> next) {
  next_ = std::move(next);
}

template <typename R, typename... Args>
template <typename Fn>
CallbackQueue<R, Args...>::CallbackImpl<Fn>::CallbackImpl(
    Fn&& callback, CallbackFlags::Flags flags)
  : Callback(flags),
    callback_(std::move(callback)) {}

template <typename R, typename... Args>
template <typename Fn>
R CallbackQueue<R, Args...>::CallbackImpl<Fn>::Call(Args... args) {
  return callback_(std::forward<Args>(args)...);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CALLBACK_QUEUE_INL_H_
