#ifndef SRC_NODE_MUTEX_H_
#define SRC_NODE_MUTEX_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util.h"
#include "uv.h"

namespace node {

template <typename Traits> class ConditionVariableBase;
template <typename Traits> class MutexBase;
struct LibuvMutexTraits;

using ConditionVariable = ConditionVariableBase<LibuvMutexTraits>;
using Mutex = MutexBase<LibuvMutexTraits>;

template <typename Traits>
class MutexBase {
 public:
  inline MutexBase();
  inline ~MutexBase();
  inline void Lock();
  inline void Unlock();

  class ScopedLock;
  class ScopedUnlock;

  class ScopedLock {
   public:
    inline explicit ScopedLock(const MutexBase& mutex);
    inline explicit ScopedLock(const ScopedUnlock& scoped_unlock);
    inline ~ScopedLock();

   private:
    template <typename> friend class ConditionVariableBase;
    friend class ScopedUnlock;
    const MutexBase& mutex_;
    DISALLOW_COPY_AND_ASSIGN(ScopedLock);
  };

  class ScopedUnlock {
   public:
    inline explicit ScopedUnlock(const ScopedLock& scoped_lock);
    inline ~ScopedUnlock();

   private:
    friend class ScopedLock;
    const MutexBase& mutex_;
    DISALLOW_COPY_AND_ASSIGN(ScopedUnlock);
  };

 private:
  template <typename> friend class ConditionVariableBase;
  mutable typename Traits::MutexT mutex_;
  DISALLOW_COPY_AND_ASSIGN(MutexBase);
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

 private:
  typename Traits::CondT cond_;
  DISALLOW_COPY_AND_ASSIGN(ConditionVariableBase);
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
