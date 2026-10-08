//===-- Libc-internal alternative to <new> ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_NEW_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_NEW_H

#include "hdr/func/free.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/macro-utils.h"
#include "src/__support/macros/properties/compiler.h"

#include <stddef.h> // For size_t

// Defining members in the std namespace is not preferred. But, we do it here
// so that we can use it to define the operator new which takes std::align_val_t
// argument.
namespace std {

enum class align_val_t : size_t {};

} // namespace std

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <class T> [[nodiscard]] constexpr T *launder(T *p) {
  static_assert(__has_builtin(__builtin_launder),
                "cpp::launder requires __builtin_launder");
  return __builtin_launder(p);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

LIBC_INLINE void *operator new(size_t, void *p) { return p; }

LIBC_INLINE void *operator new[](size_t, void *p) { return p; }

// The ideal situation would be to define the various flavors of operator delete
// inline like we do with operator new above. However, since we need operator
// delete prototypes to match those specified by the C++ standard, we cannot
// define them inline as the C++ standard does not allow inline definitions of
// replacement operator delete implementations. Note also that we assign a
// special linkage name to each of these replacement operator delete functions.
// This is because, if we do not give them a special libc internal linkage name,
// they will replace operator delete for the entire application. Including this
// header file in all libc source files where operator delete is called ensures
// that only libc call sites use these replacement operator delete functions.

#ifndef LIBC_COMPILER_IS_MSVC
#define DELETE_NAME(name)                                                      \
  __asm__(LLVM_LIBC_STRINGIFY(LIBC_NAMESPACE) "_" LLVM_LIBC_STRINGIFY(name))
#else
#define DELETE_NAME(name)
#endif // LIBC_COMPILER_IS_MSVC

void operator delete(void *) noexcept DELETE_NAME(delete);
void operator delete(void *, std::align_val_t) noexcept
    DELETE_NAME(delete_aligned);
void operator delete(void *, size_t) noexcept DELETE_NAME(delete_sized);
void operator delete(void *, size_t, std::align_val_t) noexcept
    DELETE_NAME(delete_sized_aligned);
void operator delete[](void *) noexcept DELETE_NAME(delete_array);
void operator delete[](void *, std::align_val_t) noexcept
    DELETE_NAME(delete_array_aligned);
void operator delete[](void *, size_t) noexcept DELETE_NAME(delete_array_sized);
void operator delete[](void *, size_t, std::align_val_t) noexcept
    DELETE_NAME(delete_array_sized_aligned);

#undef DELETE_NAME

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_NEW_H
