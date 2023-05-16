// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_HANDLE_BASE_H_
#define INCLUDE_V8_HANDLE_BASE_H_

#include "v8-internal.h"  // NOLINT(build/include_directory)

namespace v8 {

namespace internal {

// Helper functions about values contained in handles.
// A value is either an indirect pointer or a direct pointer, depending on
// whether direct local support is enabled.
class ValueHelper final {
 public:
#ifdef V8_ENABLE_DIRECT_LOCAL
  static constexpr Address kTaggedNullAddress = 1;
  static constexpr Address kEmpty = kTaggedNullAddress;
#else
  static constexpr Address kEmpty = kNullAddress;
#endif  // V8_ENABLE_DIRECT_LOCAL

  template <typename T>
  V8_INLINE static bool IsEmpty(T* value) {
    return reinterpret_cast<Address>(value) == kEmpty;
  }

  // Returns a handle's "value" for all kinds of abstract handles. For Local,
  // it is equivalent to `*handle`. The variadic parameters support handle
  // types with extra type parameters, like `Persistent<T, M>`.
  template <template <typename T, typename... Ms> typename H, typename T,
            typename... Ms>
  V8_INLINE static T* HandleAsValue(const H<T, Ms...>& handle) {
    return handle.template value<T>();
  }

#ifdef V8_ENABLE_DIRECT_LOCAL

  template <typename T>
  V8_INLINE static Address ValueAsAddress(const T* value) {
    return reinterpret_cast<Address>(value);
  }

  template <typename T, typename S>
  V8_INLINE static T* SlotAsValue(S* slot) {
    return *reinterpret_cast<T**>(slot);
  }

  template <typename T>
  V8_INLINE static T* ValueAsSlot(T* const& value) {
    return reinterpret_cast<T*>(const_cast<T**>(&value));
  }

#else  // !V8_ENABLE_DIRECT_LOCAL

  template <typename T>
  V8_INLINE static Address ValueAsAddress(const T* value) {
    return *reinterpret_cast<const Address*>(value);
  }

  template <typename T, typename S>
  V8_INLINE static T* SlotAsValue(S* slot) {
    return reinterpret_cast<T*>(slot);
  }

  template <typename T>
  V8_INLINE static T* ValueAsSlot(T* const& value) {
    return value;
  }

#endif  // V8_ENABLE_DIRECT_LOCAL
};

/**
 * Helper functions about handles.
 */
class HandleHelper final {
 public:
  /**
   * Checks whether two handles are equal.
   * They are equal iff they are both empty or they are both non-empty and the
   * objects to which they refer are physically equal.
   *
   * If both handles refer to JS objects, this is the same as strict equality.
   * For primitives, such as numbers or strings, a `false` return value does not
   * indicate that the values aren't equal in the JavaScript sense.
   * Use `Value::StrictEquals()` to check primitives for equality.
   */
  template <typename T1, typename T2>
  V8_INLINE static bool EqualHandles(const T1& lhs, const T2& rhs) {
    if (lhs.IsEmpty()) return rhs.IsEmpty();
    if (rhs.IsEmpty()) return false;
    return lhs.ptr() == rhs.ptr();
  }
};

}  // namespace internal

/**
 * A base class for abstract handles containing indirect pointers.
 * These are useful regardless of whether direct local support is enabled.
 */
class IndirectHandleBase {
 public:
  // Returns true if the handle is empty.
  V8_INLINE bool IsEmpty() const { return location_ == nullptr; }

  // Sets the handle to be empty. IsEmpty() will then return true.
  V8_INLINE void Clear() { location_ = nullptr; }

 protected:
  friend class internal::ValueHelper;
  friend class internal::HandleHelper;

  V8_INLINE IndirectHandleBase() = default;
  V8_INLINE IndirectHandleBase(const IndirectHandleBase& other) = default;
  V8_INLINE IndirectHandleBase& operator=(const IndirectHandleBase& that) =
      default;

  V8_INLINE explicit IndirectHandleBase(internal::Address* location)
      : location_(location) {}

  // Returns the address of the actual heap object (tagged).
  // This method must be called only if the handle is not empty, otherwise it
  // will crash.
  V8_INLINE internal::Address ptr() const { return *location_; }

  // Returns a reference to the slot (indirect pointer).
  V8_INLINE internal::Address* const& slot() const { return location_; }
  V8_INLINE internal::Address*& slot() { return location_; }

  // Returns the handler's "value" (direct or indirect pointer, depending on
  // whether direct local support is enabled).
  template <typename T>
  V8_INLINE T* value() const {
    return internal::ValueHelper::SlotAsValue<T>(slot());
  }

 private:
  internal::Address* location_ = nullptr;
};

#ifdef V8_ENABLE_DIRECT_LOCAL

/**
 * A base class for abstract handles containing direct pointers.
 * These are only possible when conservative stack scanning is enabled.
 */
class DirectHandleBase {
 public:
  // Returns true if the handle is empty.
  V8_INLINE bool IsEmpty() const {
    return ptr_ == internal::ValueHelper::kEmpty;
  }

  // Sets the handle to be empty. IsEmpty() will then return true.
  V8_INLINE void Clear() { ptr_ = internal::ValueHelper::kEmpty; }

 protected:
  friend class internal::ValueHelper;
  friend class internal::HandleHelper;

  V8_INLINE DirectHandleBase() = default;
  V8_INLINE DirectHandleBase(const DirectHandleBase& other) = default;
  V8_INLINE DirectHandleBase& operator=(const DirectHandleBase& that) = default;

  V8_INLINE explicit DirectHandleBase(internal::Address ptr) : ptr_(ptr) {}

  // Returns the address of the referenced object.
  V8_INLINE internal::Address ptr() const { return ptr_; }

  // Returns the handler's "value" (direct pointer, as direct local support
  // is guaranteed to be enabled here).
  template <typename T>
  V8_INLINE T* value() const {
    return reinterpret_cast<T*>(ptr_);
  }

 private:
  internal::Address ptr_ = internal::ValueHelper::kEmpty;
};

#endif  // V8_ENABLE_DIRECT_LOCAL

}  // namespace v8

#endif  // INCLUDE_V8_HANDLE_BASE_H_
