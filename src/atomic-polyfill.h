#ifndef SRC_ATOMIC_POLYFILL_H_
#define SRC_ATOMIC_POLYFILL_H_

#include "util.h"

namespace nonstd {

template <typename T>
struct atomic {
  atomic() = default;
  T exchange(T value) { return __sync_lock_test_and_set(&value_, value); }
  T value_ = T();
  DISALLOW_COPY_AND_ASSIGN(atomic);
};

}  // namespace nonstd

#endif  // SRC_ATOMIC_POLYFILL_H_
