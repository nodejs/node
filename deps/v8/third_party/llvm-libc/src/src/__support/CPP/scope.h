//===-- Standalone implementation of experimental/scope ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_SCOPE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_SCOPE_H

#include "src/__support/CPP/utility/forward.h"
#include "src/__support/CPP/utility/move.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// A reimplementation of std::experimental::scope_exit from the C++ library
// fundamentals TS v3
template <typename EF> class scope_exit {
  EF exit_function;
  bool execute_on_destruction;

public:
  template <typename Fn>
  LIBC_INLINE explicit scope_exit(Fn &&fn)
      : exit_function(cpp::forward<Fn>(fn)), execute_on_destruction(true) {}

  LIBC_INLINE scope_exit(scope_exit &&other)
      : exit_function(cpp::move(other.exit_function)),
        execute_on_destruction(other.execute_on_destruction) {
    other.release();
  }

  scope_exit(const scope_exit &) = delete;
  scope_exit &operator=(const scope_exit &) = delete;
  scope_exit &operator=(scope_exit &&) = delete;

  LIBC_INLINE ~scope_exit() {
    if (execute_on_destruction)
      exit_function();
  }

  LIBC_INLINE void release() { execute_on_destruction = false; }
};

template <typename EF> scope_exit(EF) -> scope_exit<EF>;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_SCOPE_H
