#ifndef SRC_NODE_THREADSAFE_COW_H_
#define SRC_NODE_THREADSAFE_COW_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util.h"
#include "uv.h"

#include <memory>   // std::shared_ptr<T>
#include <utility>  // std::forward<T>

namespace node {

// Copy-on-write utility. Not threadsafe, i.e. there is no synchronization
// of the copy operation with other operations.
template <typename T>
class CopyOnWrite final {
 public:
  template <typename... Args>
  explicit CopyOnWrite(Args&&... args)
      : data_(std::make_shared<T>(std::forward<Args>(args)...)) {}

  CopyOnWrite(const CopyOnWrite<T>& other) = default;
  CopyOnWrite& operator=(const CopyOnWrite<T>& other) = default;
  CopyOnWrite(CopyOnWrite<T>&& other) = default;
  CopyOnWrite& operator=(CopyOnWrite<T>&& other) = default;

  const T* read() const { return data_.get(); }
  T* write();

  const T& operator*() const { return *read(); }
  const T* operator->() const { return read(); }

 private:
  std::shared_ptr<T> data_;
};

// Threadsafe copy-on-write utility. Consumers need to use the Read and
// Write helpers to access the target data structure.
template <typename T>
class ThreadsafeCopyOnWrite final {
 private:
  // Define this early since some of the public members depend on it
  // and some compilers need it to be defined first in that case.
  struct Impl {
    explicit Impl(const T& data) : data(data) {}
    explicit Impl(T&& data) : data(std::move(data)) {}

    Impl(const Impl& other);
    Impl& operator=(const Impl& other) = delete;
    Impl(Impl&& other) = delete;
    Impl& operator=(Impl&& other) = delete;

    RwLock mutex;
    T data;
  };

 public:
  template <typename... Args>
  ThreadsafeCopyOnWrite(Args&&... args)
      : impl_(T(std::forward<Args>(args)...)) {}

  ThreadsafeCopyOnWrite(const ThreadsafeCopyOnWrite<T>& other) = default;
  ThreadsafeCopyOnWrite& operator=(const ThreadsafeCopyOnWrite<T>& other) =
      default;
  ThreadsafeCopyOnWrite(ThreadsafeCopyOnWrite<T>&& other) = default;
  ThreadsafeCopyOnWrite& operator=(ThreadsafeCopyOnWrite<T>&& other) = default;

  class Read {
   public:
    explicit Read(const ThreadsafeCopyOnWrite<T>* cow);

    const T& operator*() const;
    const T* operator->() const;

   private:
    const ThreadsafeCopyOnWrite<T>* cow_;
    RwLock::ScopedReadLock lock_;
  };

  class Write {
   public:
    explicit Write(ThreadsafeCopyOnWrite<T>* cow);

    T& operator*();
    T* operator->();

   private:
    ThreadsafeCopyOnWrite<T>* cow_;
    typename ThreadsafeCopyOnWrite<T>::Impl* impl_;
    RwLock::ScopedLock lock_;
  };

  Read read() const { return Read(this); }
  Write write() { return Write(this); }

 private:
  CopyOnWrite<Impl> impl_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_THREADSAFE_COW_H_
