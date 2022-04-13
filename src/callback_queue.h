#ifndef SRC_CALLBACK_QUEUE_H_
#define SRC_CALLBACK_QUEUE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <atomic>

namespace node {

namespace CallbackFlags {
enum Flags {
  kUnrefed = 0,
  kRefed = 1,
};
}

// A queue of C++ functions that take Args... as arguments and return R
// (this is similar to the signature of std::function).
// New entries are added using `CreateCallback()`/`Push()`, and removed using
// `Shift()`.
// The `refed` flag is left for easier use in situations in which some of these
// should be run even if nothing else is keeping the event loop alive.
template <typename R, typename... Args>
class CallbackQueue {
 public:
  class Callback {
   public:
    explicit inline Callback(CallbackFlags::Flags flags);

    virtual ~Callback() = default;
    virtual R Call(Args... args) = 0;

    inline CallbackFlags::Flags flags() const;

   private:
    inline std::unique_ptr<Callback> get_next();
    inline void set_next(std::unique_ptr<Callback> next);

    CallbackFlags::Flags flags_;
    std::unique_ptr<Callback> next_;

    friend class CallbackQueue;
  };

  template <typename Fn>
  inline std::unique_ptr<Callback> CreateCallback(
      Fn&& fn, CallbackFlags::Flags);

  inline std::unique_ptr<Callback> Shift();
  inline void Push(std::unique_ptr<Callback> cb);
  // ConcatMove adds elements from 'other' to the end of this list, and clears
  // 'other' afterwards.
  inline void ConcatMove(CallbackQueue&& other);

  // size() is atomic and may be called from any thread.
  inline size_t size() const;

 private:
  template <typename Fn>
  class CallbackImpl final : public Callback {
   public:
    CallbackImpl(Fn&& callback, CallbackFlags::Flags flags);
    R Call(Args... args) override;

   private:
    Fn callback_;
  };

  std::atomic<size_t> size_ {0};
  std::unique_ptr<Callback> head_;
  Callback* tail_ = nullptr;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CALLBACK_QUEUE_H_
