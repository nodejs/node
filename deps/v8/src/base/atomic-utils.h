// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ATOMIC_UTILS_H_
#define V8_BASE_ATOMIC_UTILS_H_

#include <limits.h>
#include <type_traits>

#include "src/base/atomicops.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// Deprecated. Use std::atomic<T> for new code.
// Flag using T atomically. Also accepts void* as T.
template <typename T>
class AtomicValue {
 public:
  AtomicValue() : value_(0) {}

  explicit AtomicValue(T initial)
      : value_(cast_helper<T>::to_storage_type(initial)) {}

  V8_INLINE T Value() const {
    return cast_helper<T>::to_return_type(base::Acquire_Load(&value_));
  }

  V8_INLINE void SetValue(T new_value) {
    base::Release_Store(&value_, cast_helper<T>::to_storage_type(new_value));
  }

 private:
  STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));

  template <typename S>
  struct cast_helper {
    static base::AtomicWord to_storage_type(S value) {
      return static_cast<base::AtomicWord>(value);
    }
    static S to_return_type(base::AtomicWord value) {
      return static_cast<S>(value);
    }
  };

  template <typename S>
  struct cast_helper<S*> {
    static base::AtomicWord to_storage_type(S* value) {
      return reinterpret_cast<base::AtomicWord>(value);
    }
    static S* to_return_type(base::AtomicWord value) {
      return reinterpret_cast<S*>(value);
    }
  };

  base::AtomicWord value_;
};

class AsAtomic32 {
 public:
  template <typename T>
  static T Acquire_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic32));
    return to_return_type<T>(base::Acquire_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static T Relaxed_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic32));
    return to_return_type<T>(base::Relaxed_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static void Release_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic32));
    base::Release_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static void Relaxed_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic32));
    base::Relaxed_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static T Release_CompareAndSwap(
      T* addr, typename std::remove_reference<T>::type old_value,
      typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic32));
    return to_return_type<T>(base::Release_CompareAndSwap(
        to_storage_addr(addr), to_storage_type(old_value),
        to_storage_type(new_value)));
  }

  // Atomically sets bits selected by the mask to the given value.
  // Returns false if the bits are already set as needed.
  template <typename T>
  static bool SetBits(T* addr, T bits, T mask) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic32));
    DCHECK_EQ(bits & ~mask, static_cast<T>(0));
    T old_value;
    T new_value;
    do {
      old_value = Relaxed_Load(addr);
      if ((old_value & mask) == bits) return false;
      new_value = (old_value & ~mask) | bits;
    } while (Release_CompareAndSwap(addr, old_value, new_value) != old_value);
    return true;
  }

 private:
  template <typename T>
  static base::Atomic32 to_storage_type(T value) {
    return static_cast<base::Atomic32>(value);
  }
  template <typename T>
  static T to_return_type(base::Atomic32 value) {
    return static_cast<T>(value);
  }
  template <typename T>
  static base::Atomic32* to_storage_addr(T* value) {
    return reinterpret_cast<base::Atomic32*>(value);
  }
  template <typename T>
  static const base::Atomic32* to_storage_addr(const T* value) {
    return reinterpret_cast<const base::Atomic32*>(value);
  }
};

class AsAtomicWord {
 public:
  template <typename T>
  static T Acquire_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    return to_return_type<T>(base::Acquire_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static T Relaxed_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    return to_return_type<T>(base::Relaxed_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static void Release_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    base::Release_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static void Relaxed_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    base::Relaxed_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static T Release_CompareAndSwap(
      T* addr, typename std::remove_reference<T>::type old_value,
      typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    return to_return_type<T>(base::Release_CompareAndSwap(
        to_storage_addr(addr), to_storage_type(old_value),
        to_storage_type(new_value)));
  }

  // Atomically sets bits selected by the mask to the given value.
  // Returns false if the bits are already set as needed.
  template <typename T>
  static bool SetBits(T* addr, T bits, T mask) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    DCHECK_EQ(bits & ~mask, static_cast<T>(0));
    T old_value;
    T new_value;
    do {
      old_value = Relaxed_Load(addr);
      if ((old_value & mask) == bits) return false;
      new_value = (old_value & ~mask) | bits;
    } while (Release_CompareAndSwap(addr, old_value, new_value) != old_value);
    return true;
  }

 private:
  template <typename T>
  static base::AtomicWord to_storage_type(T value) {
    return static_cast<base::AtomicWord>(value);
  }
  template <typename T>
  static T to_return_type(base::AtomicWord value) {
    return static_cast<T>(value);
  }
  template <typename T>
  static base::AtomicWord* to_storage_addr(T* value) {
    return reinterpret_cast<base::AtomicWord*>(value);
  }
  template <typename T>
  static const base::AtomicWord* to_storage_addr(const T* value) {
    return reinterpret_cast<const base::AtomicWord*>(value);
  }
};

class AsAtomic8 {
 public:
  template <typename T>
  static T Acquire_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic8));
    return to_return_type<T>(base::Acquire_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static T Relaxed_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic8));
    return to_return_type<T>(base::Relaxed_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static void Release_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic8));
    base::Release_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static void Relaxed_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic8));
    base::Relaxed_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static T Release_CompareAndSwap(
      T* addr, typename std::remove_reference<T>::type old_value,
      typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::Atomic8));
    return to_return_type<T>(base::Release_CompareAndSwap(
        to_storage_addr(addr), to_storage_type(old_value),
        to_storage_type(new_value)));
  }

 private:
  template <typename T>
  static base::Atomic8 to_storage_type(T value) {
    return static_cast<base::Atomic8>(value);
  }
  template <typename T>
  static T to_return_type(base::Atomic8 value) {
    return static_cast<T>(value);
  }
  template <typename T>
  static base::Atomic8* to_storage_addr(T* value) {
    return reinterpret_cast<base::Atomic8*>(value);
  }
  template <typename T>
  static const base::Atomic8* to_storage_addr(const T* value) {
    return reinterpret_cast<const base::Atomic8*>(value);
  }
};

class AsAtomicPointer {
 public:
  template <typename T>
  static T Acquire_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    return to_return_type<T>(base::Acquire_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static T Relaxed_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    return to_return_type<T>(base::Relaxed_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static void Release_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    base::Release_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static void Relaxed_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    base::Relaxed_Store(to_storage_addr(addr), to_storage_type(new_value));
  }

  template <typename T>
  static T Release_CompareAndSwap(
      T* addr, typename std::remove_reference<T>::type old_value,
      typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(base::AtomicWord));
    return to_return_type<T>(base::Release_CompareAndSwap(
        to_storage_addr(addr), to_storage_type(old_value),
        to_storage_type(new_value)));
  }

 private:
  template <typename T>
  static base::AtomicWord to_storage_type(T value) {
    return reinterpret_cast<base::AtomicWord>(value);
  }
  template <typename T>
  static T to_return_type(base::AtomicWord value) {
    return reinterpret_cast<T>(value);
  }
  template <typename T>
  static base::AtomicWord* to_storage_addr(T* value) {
    return reinterpret_cast<base::AtomicWord*>(value);
  }
  template <typename T>
  static const base::AtomicWord* to_storage_addr(const T* value) {
    return reinterpret_cast<const base::AtomicWord*>(value);
  }
};

template <typename T,
          typename = typename std::enable_if<std::is_unsigned<T>::value>::type>
inline void CheckedIncrement(std::atomic<T>* number, T amount) {
  const T old = number->fetch_add(amount);
  DCHECK_GE(old + amount, old);
  USE(old);
}

template <typename T,
          typename = typename std::enable_if<std::is_unsigned<T>::value>::type>
inline void CheckedDecrement(std::atomic<T>* number, T amount) {
  const T old = number->fetch_sub(amount);
  DCHECK_GE(old, amount);
  USE(old);
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMIC_UTILS_H_
