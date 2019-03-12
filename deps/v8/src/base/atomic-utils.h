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

// Provides atomic operations for a values stored at some address.
template <typename TAtomicStorageType>
class AsAtomicImpl {
 public:
  using AtomicStorageType = TAtomicStorageType;

  template <typename T>
  static T Acquire_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(AtomicStorageType));
    return cast_helper<T>::to_return_type(
        base::Acquire_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static T Relaxed_Load(T* addr) {
    STATIC_ASSERT(sizeof(T) <= sizeof(AtomicStorageType));
    return cast_helper<T>::to_return_type(
        base::Relaxed_Load(to_storage_addr(addr)));
  }

  template <typename T>
  static void Release_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(AtomicStorageType));
    base::Release_Store(to_storage_addr(addr),
                        cast_helper<T>::to_storage_type(new_value));
  }

  template <typename T>
  static void Relaxed_Store(T* addr,
                            typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(AtomicStorageType));
    base::Relaxed_Store(to_storage_addr(addr),
                        cast_helper<T>::to_storage_type(new_value));
  }

  template <typename T>
  static T Release_CompareAndSwap(
      T* addr, typename std::remove_reference<T>::type old_value,
      typename std::remove_reference<T>::type new_value) {
    STATIC_ASSERT(sizeof(T) <= sizeof(AtomicStorageType));
    return cast_helper<T>::to_return_type(base::Release_CompareAndSwap(
        to_storage_addr(addr), cast_helper<T>::to_storage_type(old_value),
        cast_helper<T>::to_storage_type(new_value)));
  }

  // Atomically sets bits selected by the mask to the given value.
  // Returns false if the bits are already set as needed.
  template <typename T>
  static bool SetBits(T* addr, T bits, T mask) {
    STATIC_ASSERT(sizeof(T) <= sizeof(AtomicStorageType));
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
  template <typename U>
  struct cast_helper {
    static AtomicStorageType to_storage_type(U value) {
      return static_cast<AtomicStorageType>(value);
    }
    static U to_return_type(AtomicStorageType value) {
      return static_cast<U>(value);
    }
  };

  template <typename U>
  struct cast_helper<U*> {
    static AtomicStorageType to_storage_type(U* value) {
      return reinterpret_cast<AtomicStorageType>(value);
    }
    static U* to_return_type(AtomicStorageType value) {
      return reinterpret_cast<U*>(value);
    }
  };

  template <typename T>
  static AtomicStorageType* to_storage_addr(T* value) {
    return reinterpret_cast<AtomicStorageType*>(value);
  }
  template <typename T>
  static const AtomicStorageType* to_storage_addr(const T* value) {
    return reinterpret_cast<const AtomicStorageType*>(value);
  }
};

using AsAtomic8 = AsAtomicImpl<base::Atomic8>;
using AsAtomic32 = AsAtomicImpl<base::Atomic32>;
using AsAtomicWord = AsAtomicImpl<base::AtomicWord>;

// This is similar to AsAtomicWord but it explicitly deletes functionality
// provided atomic access to bit representation of stored values.
template <typename TAtomicStorageType>
class AsAtomicPointerImpl : public AsAtomicImpl<TAtomicStorageType> {
 public:
  template <typename T>
  static bool SetBits(T* addr, T bits, T mask) = delete;
};

using AsAtomicPointer = AsAtomicPointerImpl<base::AtomicWord>;

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
