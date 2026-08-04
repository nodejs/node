#ifndef SRC_NODE_MUTEX_H_
#define SRC_NODE_MUTEX_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util.h"
#include "uv.h"

#include <memory>  // std::shared_ptr<T>
#include <utility>  // std::forward<T>

namespace node {

template <typename Traits> class ConditionVariableBase;
template <typename Traits> class MutexBase;
struct LibuvMutexTraits;
struct LibuvRwlockTraits;

using ConditionVariable = ConditionVariableBase<LibuvMutexTraits>;
using Mutex = MutexBase<LibuvMutexTraits>;
using RwLock = MutexBase<LibuvRwlockTraits>;

template <typename T, typename MutexT = Mutex>
class ExclusiveAccess {
 public:
  ExclusiveAccess() = default;

  template <typename... Args>
  explicit ExclusiveAccess(Args&&... args)
      : item_(std::forward<Args>(args)...) {}

  ExclusiveAccess(const ExclusiveAccess&) = delete;
  ExclusiveAccess& operator=(const ExclusiveAccess&) = delete;

  class Scoped {
   public:
    // ExclusiveAccess will commonly be used in conjunction with std::shared_ptr
    // and without this constructor it's too easy to forget to keep a reference
    // around to the shared_ptr while operating on the ExclusiveAccess object.
    explicit Scoped(const std::shared_ptr<ExclusiveAccess>& shared)
        : shared_(shared)
        , scoped_lock_(shared->mutex_)
        , pointer_(&shared->item_) {}

    explicit Scoped(ExclusiveAccess* exclusive_access)
        : shared_()
        , scoped_lock_(exclusive_access->mutex_)
        , pointer_(&exclusive_access->item_) {}

    T& operator*() const { return *pointer_; }
    T* operator->() const { return pointer_; }

    Scoped(const Scoped&) = delete;
    Scoped& operator=(const Scoped&) = delete;

   private:
    std::shared_ptr<ExclusiveAccess> shared_;
    typename MutexT::ScopedLock scoped_lock_;
    T* const pointer_;
  };

 private:
  friend class ScopedLock;
  MutexT mutex_;
  T item_;
};

template <typename Traits>
class MutexBase {
 public:
  inline MutexBase();
  inline ~MutexBase();
  inline void Lock();
  inline void Unlock();
  inline void RdLock();
  inline void RdUnlock();

  MutexBase(const MutexBase&) = delete;
  MutexBase& operator=(const MutexBase&) = delete;

  class ScopedLock;
  class ScopedUnlock;

  class ScopedLock {
   public:
    inline explicit ScopedLock(const MutexBase& mutex);
    inline explicit ScopedLock(const ScopedUnlock& scoped_unlock);
    inline ~ScopedLock();

    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

   private:
    template <typename> friend class ConditionVariableBase;
    friend class ScopedUnlock;
    const MutexBase& mutex_;
  };

  class ScopedReadLock {
   public:
    inline explicit ScopedReadLock(const MutexBase& mutex);
    inline ~ScopedReadLock();

    ScopedReadLock(const ScopedReadLock&) = delete;
    ScopedReadLock& operator=(const ScopedReadLock&) = delete;

   private:
    template <typename> friend class ConditionVariableBase;
    const MutexBase& mutex_;
  };

  using ScopedWriteLock = ScopedLock;

  class ScopedUnlock {
   public:
    inline explicit ScopedUnlock(const ScopedLock& scoped_lock);
    inline ~ScopedUnlock();

    ScopedUnlock(const ScopedUnlock&) = delete;
    ScopedUnlock& operator=(const ScopedUnlock&) = delete;

   private:
    friend class ScopedLock;
    const MutexBase& mutex_;
  };

 private:
  template <typename> friend class ConditionVariableBase;
  mutable typename Traits::MutexT mutex_;
};

template <typename Traits>
class ConditionVariableBase {
 public:
  using ScopedLock = typename MutexBase<Traits>::ScopedLock;

  inline ConditionVariableBase();
  inline ~ConditionVariableBase();
  inline void Broadcast(const ScopedLock&);
  inline void Signal(const ScopedLock&);
  inline void Wait(const ScopedLock& scoped_lock);

  ConditionVariableBase(const ConditionVariableBase&) = delete;
  ConditionVariableBase& operator=(const ConditionVariableBase&) = delete;

 private:
  typename Traits::CondT cond_;
};

struct LibuvMutexTraits {
  using CondT = uv_cond_t;
  using MutexT = uv_mutex_t;

  static inline int cond_init(CondT* cond) {
    return uv_cond_init(cond);
  }

  static inline int mutex_init(MutexT* mutex) {
    return uv_mutex_init(mutex);
  }

  static inline void cond_broadcast(CondT* cond) {
    uv_cond_broadcast(cond);
  }

  static inline void cond_destroy(CondT* cond) {
    uv_cond_destroy(cond);
  }

  static inline void cond_signal(CondT* cond) {
    uv_cond_signal(cond);
  }

  static inline void cond_wait(CondT* cond, MutexT* mutex) {
    uv_cond_wait(cond, mutex);
  }

  static inline void mutex_destroy(MutexT* mutex) {
    uv_mutex_destroy(mutex);
  }

  static inline void mutex_lock(MutexT* mutex) {
    uv_mutex_lock(mutex);
  }

  static inline void mutex_unlock(MutexT* mutex) {
    uv_mutex_unlock(mutex);
  }

  static inline void mutex_rdlock(MutexT* mutex) {
    uv_mutex_lock(mutex);
  }

  static inline void mutex_rdunlock(MutexT* mutex) {
    uv_mutex_unlock(mutex);
  }
};

struct LibuvRwlockTraits {
  using MutexT = uv_rwlock_t;

  static inline int mutex_init(MutexT* mutex) {
    return uv_rwlock_init(mutex);
  }

  static inline void mutex_destroy(MutexT* mutex) {
    uv_rwlock_destroy(mutex);
  }

  static inline void mutex_lock(MutexT* mutex) {
    uv_rwlock_wrlock(mutex);
  }

  static inline void mutex_unlock(MutexT* mutex) {
    uv_rwlock_wrunlock(mutex);
  }

  static inline void mutex_rdlock(MutexT* mutex) {
    uv_rwlock_rdlock(mutex);
  }

  static inline void mutex_rdunlock(MutexT* mutex) {
    uv_rwlock_rdunlock(mutex);
  }
};

template <typename Traits>
ConditionVariableBase<Traits>::ConditionVariableBase() {
  CHECK_EQ(0, Traits::cond_init(&cond_));
}

template <typename Traits>
ConditionVariableBase<Traits>::~ConditionVariableBase() {
  Traits::cond_destroy(&cond_);
}

template <typename Traits>
void ConditionVariableBase<Traits>::Broadcast(const ScopedLock&) {
  Traits::cond_broadcast(&cond_);
}

template <typename Traits>
void ConditionVariableBase<Traits>::Signal(const ScopedLock&) {
  Traits::cond_signal(&cond_);
}

template <typename Traits>
void ConditionVariableBase<Traits>::Wait(const ScopedLock& scoped_lock) {
  Traits::cond_wait(&cond_, &scoped_lock.mutex_.mutex_);
}

template <typename Traits>
MutexBase<Traits>::MutexBase() {
  CHECK_EQ(0, Traits::mutex_init(&mutex_));
}

template <typename Traits>
MutexBase<Traits>::~MutexBase() {
  Traits::mutex_destroy(&mutex_);
}

template <typename Traits>
void MutexBase<Traits>::Lock() {
  Traits::mutex_lock(&mutex_);
}

template <typename Traits>
void MutexBase<Traits>::Unlock() {
  Traits::mutex_unlock(&mutex_);
}

template <typename Traits>
void MutexBase<Traits>::RdLock() {
  Traits::mutex_rdlock(&mutex_);
}

template <typename Traits>
void MutexBase<Traits>::RdUnlock() {
  Traits::mutex_rdunlock(&mutex_);
}

template <typename Traits>
MutexBase<Traits>::ScopedLock::ScopedLock(const MutexBase& mutex)
    : mutex_(mutex) {
  Traits::mutex_lock(&mutex_.mutex_);
}

template <typename Traits>
MutexBase<Traits>::ScopedLock::ScopedLock(const ScopedUnlock& scoped_unlock)
    : MutexBase(scoped_unlock.mutex_) {}

template <typename Traits>
MutexBase<Traits>::ScopedLock::~ScopedLock() {
  Traits::mutex_unlock(&mutex_.mutex_);
}

template <typename Traits>
MutexBase<Traits>::ScopedReadLock::ScopedReadLock(const MutexBase& mutex)
    : mutex_(mutex) {
  Traits::mutex_rdlock(&mutex_.mutex_);
}

template <typename Traits>
MutexBase<Traits>::ScopedReadLock::~ScopedReadLock() {
  Traits::mutex_rdunlock(&mutex_.mutex_);
}

template <typename Traits>
MutexBase<Traits>::ScopedUnlock::ScopedUnlock(const ScopedLock& scoped_lock)
    : mutex_(scoped_lock.mutex_) {
  Traits::mutex_unlock(&mutex_.mutex_);
}

template <typename Traits>
MutexBase<Traits>::ScopedUnlock::~ScopedUnlock() {
  Traits::mutex_lock(&mutex_.mutex_);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_MUTEX_H_
