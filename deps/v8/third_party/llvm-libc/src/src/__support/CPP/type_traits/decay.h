//===-- decay type_traits ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_DECAY_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_DECAY_H

#include "src/__support/macros/attributes.h"

#include "src/__support/CPP/type_traits/add_pointer.h"
#include "src/__support/CPP/type_traits/conditional.h"
#include "src/__support/CPP/type_traits/is_array.h"
#include "src/__support/CPP/type_traits/is_function.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/CPP/type_traits/remove_extent.h"
#include "src/__support/CPP/type_traits/remove_reference.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// decay
template <class T> class decay {
  using U = cpp::remove_reference_t<T>;

public:
  using type = conditional_t<
      cpp::is_array_v<U>, cpp::add_pointer_t<cpp::remove_extent_t<U>>,
      cpp::conditional_t<cpp::is_function_v<U>, cpp::add_pointer_t<U>,
                         cpp::remove_cv_t<U>>>;
};
template <class T> using decay_t = typename decay<T>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_DECAY_H
