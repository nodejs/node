//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Smart pointer wrapper around cpp::unique_ptr enforcing null checks.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CHECKED_PTR_H
#define LLVM_LIBC_SRC___SUPPORT_CHECKED_PTR_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/CPP/utility.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace detail {

class LIBC_TRIVIAL_ABI CheckedState {
#ifndef NDEBUG
  bool checked = false;

public:
  LIBC_INLINE constexpr CheckedState() = default;

  // Moving disarms `other` and starts `this` unchecked so the new owner must
  // explicitly check its value.
  LIBC_INLINE constexpr CheckedState(CheckedState &&other) {
    other.checked = true;
  }

  // Overwriting an existing CheckedState requires it to have been checked,
  // disarms `other`, and resets `this` to unchecked.
  LIBC_INLINE constexpr CheckedState &operator=(CheckedState &&other) {
    assert_checked();
    // Self-move-assignment leaves the (checked) state unchanged.
    if (this == &other)
      return *this;
    other.checked = true;
    checked = false;
    return *this;
  }

  LIBC_INLINE ~CheckedState() { assert_checked(); }

  LIBC_INLINE constexpr void mark_checked() { checked = true; }
  LIBC_INLINE constexpr void assert_checked() const { LIBC_ASSERT(checked); }
#else
public:
  LIBC_INLINE constexpr CheckedState() = default;
  LIBC_INLINE constexpr CheckedState(CheckedState &&) = default;
  LIBC_INLINE constexpr CheckedState &operator=(CheckedState &&) = default;

  LIBC_INLINE constexpr void mark_checked() {}
  LIBC_INLINE constexpr void assert_checked() const {}
#endif
};

} // namespace detail

/// A smart pointer wrapper around cpp::unique_ptr that enforces (in !NDEBUG
/// builds) that callers check pointer nullness before accessing or destroying
/// the pointer.
///
/// A pointer (including a null one) counts as checked once its operator bool
/// has been called. In !NDEBUG builds, the following operations assert that the
/// pointer has been checked:
/// - dereferencing (operator*, operator->, operator[]),
/// - extracting the pointer via take_unique(),
/// - destruction,
/// - being overwritten by a move assignment.
///
/// Moving from a CheckedPtr disarms the source and leaves the destination
/// unchecked, forcing the new owner to check it.
///
/// In NDEBUG builds, nothing is tracked and CheckedPtr behaves like
/// cpp::unique_ptr.
template <typename T, typename Deleter = cpp::default_delete<T>>
class [[nodiscard]] LIBC_TRIVIAL_ABI CheckedPtr {
  using PtrType = cpp::unique_ptr<T, Deleter>;
  LIBC_NO_UNIQUE_ADDRESS detail::CheckedState state;
  PtrType ptr;

  template <typename U, typename E> friend class CheckedPtr;

public:
  using deleter_type = typename PtrType::deleter_type;
  using element_type = typename PtrType::element_type;
  using pointer = typename PtrType::pointer;

  LIBC_INLINE constexpr explicit CheckedPtr(pointer p) : ptr(p) {}
  LIBC_INLINE constexpr CheckedPtr(pointer p, const deleter_type &d)
      : ptr(p, d) {}
  LIBC_INLINE constexpr CheckedPtr(pointer p, deleter_type &&d)
      : ptr(p, cpp::move(d)) {}

  LIBC_INLINE constexpr CheckedPtr(CheckedPtr &&other) = default;
  LIBC_INLINE constexpr CheckedPtr &operator=(CheckedPtr &&other) = default;

  CheckedPtr(const CheckedPtr &) = delete;
  CheckedPtr &operator=(const CheckedPtr &) = delete;

  // Conversion move constructor (available iff the underlying cpp::unique_ptr
  // conversion is)
  template <typename U, typename E,
            cpp::enable_if_t<
                cpp::is_convertible_v<cpp::unique_ptr<U, E>, PtrType>, int> = 0>
  LIBC_INLINE constexpr CheckedPtr(CheckedPtr<U, E> &&other)
      : state(cpp::move(other.state)), ptr(cpp::move(other.ptr)) {}

  // Conversion move assignment (available iff the underlying cpp::unique_ptr
  // assignment is)
  template <
      typename U, typename E,
      cpp::enable_if_t<
          cpp::is_assignable_v<PtrType &, cpp::unique_ptr<U, E> &&>, int> = 0>
  LIBC_INLINE constexpr CheckedPtr &operator=(CheckedPtr<U, E> &&other) {
    state = cpp::move(other.state);
    ptr = cpp::move(other.ptr);
    return *this;
  }

  /// Returns true if the pointer is non-null, and marks it as checked.
  ///
  /// This is intentionally non-const: recording the check in a const member
  /// function would require a mutable flag, and concurrent checks through const
  /// references would then race on it. As a consequence, a CheckedPtr cannot be
  /// checked through a const reference (but it can be used through one once it
  /// has been checked).
  LIBC_INLINE constexpr explicit operator bool() {
    state.mark_checked();
    return static_cast<bool>(ptr);
  }

  template <typename U = T, cpp::enable_if_t<!cpp::is_array_v<U>, int> = 0>
  LIBC_INLINE constexpr cpp::add_lvalue_reference_t<U> operator*() const {
    state.assert_checked();
    return *ptr;
  }

  template <typename U = T, cpp::enable_if_t<!cpp::is_array_v<U>, int> = 0>
  LIBC_INLINE constexpr pointer operator->() const {
    state.assert_checked();
    return ptr.get();
  }

  // The return type depends on U (rather than using element_type) so that it is
  // not formed for non-array types, e.g. CheckedPtr<void, D>.
  template <typename U = T, cpp::enable_if_t<cpp::is_array_v<U>, int> = 0>
  LIBC_INLINE constexpr cpp::remove_extent_t<U> &operator[](size_t i) const {
    state.assert_checked();
    return ptr[i];
  }

  /// Moves the owned pointer into a plain cpp::unique_ptr, leaving this
  /// CheckedPtr null. Asserts that the pointer has been checked.
  [[nodiscard]] LIBC_INLINE constexpr PtrType take_unique() {
    state.assert_checked();
    return cpp::move(ptr);
  }
};

/// Allocates and constructs an object of type T from `args`. Returns a null
/// CheckedPtr if the allocation fails.
template <typename T, typename... Args,
          cpp::enable_if_t<!cpp::is_array_v<T>, int> = 0>
LIBC_INLINE CheckedPtr<T> make_unique(Args &&...args) {
  // There is no need to inspect `ac`: the AllocChecker operator new is
  // noexcept, so on failure no object is constructed and the new-expression
  // evaluates to nullptr, which the caller is then forced to check for.
  AllocChecker ac;
  return CheckedPtr<T>(new (ac) T(cpp::forward<Args>(args)...));
}

/// Allocates an array of `n` value-initialized elements. T must be an array of
/// unknown bound (e.g. `make_unique<int[]>(n)`). Returns a null CheckedPtr if
/// the allocation fails. Note some allocators return a nullptr for zero-sized
/// allocations (n=0), which makes it impossible to identify an allocation
/// failure in this case.
template <
    typename T,
    cpp::enable_if_t<cpp::is_same_v<T, cpp::remove_extent_t<T>[]>, int> = 0>
LIBC_INLINE CheckedPtr<T> make_unique(size_t n) {
  using Elem = cpp::remove_extent_t<T>;
  // See the non-array overload for why `ac` is not inspected.
  AllocChecker ac;
  return CheckedPtr<T>(new (ac) Elem[n]());
}

/// Arrays of known bound (e.g. `make_unique<int[4]>()`) are not supported.
template <typename T, typename... Args,
          cpp::enable_if_t<cpp::is_array_v<T> &&
                               !cpp::is_same_v<T, cpp::remove_extent_t<T>[]>,
                           int> = 0>
void make_unique(Args &&...) = delete;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CHECKED_PTR_H
