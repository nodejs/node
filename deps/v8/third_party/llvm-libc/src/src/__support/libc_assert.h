//===-- Definition of a libc internal assert macro --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_LIBC_ASSERT_H
#define LLVM_LIBC_SRC___SUPPORT_LIBC_ASSERT_H

#if defined(LIBC_COPT_USE_C_ASSERT) || !defined(LIBC_FULL_BUILD)

// The build is configured to just use the public <assert.h> API
// for libc's internal assertions.

#ifndef LIBC_ASSERT
#include <assert.h>

#define LIBC_ASSERT(COND) assert(COND)
#endif // LIBC_ASSERT

#else // Not LIBC_COPT_USE_C_ASSERT

#include "src/__support/OSUtil/exit.h"
#include "src/__support/OSUtil/io.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h" // For LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/macro-utils.h"
#include "src/__support/macros/optimization.h" // For LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

// This is intended to be removed in a future patch to use a similar design to
// below, but it's necessary for the external assert.
LIBC_INLINE void report_assertion_failure(const char *assertion,
                                          const char *filename, unsigned line,
                                          const char *funcname) {
  const IntegerToString<unsigned> line_buffer(line);
  write_to_stderr(filename);
  write_to_stderr(":");
  write_to_stderr(line_buffer.view());
  write_to_stderr(": Assertion failed: '");
  write_to_stderr(assertion);
  write_to_stderr("' in function: '");
  write_to_stderr(funcname);
  write_to_stderr("'\n");
}

} // namespace LIBC_NAMESPACE_DECL

#ifdef LIBC_ASSERT
#error "Unexpected: LIBC_ASSERT macro already defined"
#endif

// The public "assert" macro calls abort on failure. Should it be same here?
// The libc internal assert can fire from anywhere inside the libc. So, to
// avoid potential chicken-and-egg problems, it is simple to do an exit
// on assertion failure instead of calling abort. We also don't want to use
// __builtin_trap as it could potentially be implemented using illegal
// instructions which can be very misleading when debugging.
#ifdef NDEBUG
#define LIBC_ASSERT(COND)                                                      \
  do {                                                                         \
  } while (false)
#else

#define LIBC_ASSERT(COND)                                                      \
  do {                                                                         \
    if (LIBC_UNLIKELY(!(COND))) {                                              \
      LIBC_NAMESPACE::write_to_stderr(__FILE__ ":" LLVM_LIBC_STRINGIFY(        \
          __LINE__) ": Assertion failed: '" #COND "' in function: '");         \
      LIBC_NAMESPACE::write_to_stderr(__PRETTY_FUNCTION__);                    \
      LIBC_NAMESPACE::write_to_stderr("'\n");                                  \
      LIBC_NAMESPACE::internal::exit(0xFF);                                    \
    }                                                                          \
  } while (false)
#endif // NDEBUG

#endif // LIBC_COPT_USE_C_ASSERT

#endif // LLVM_LIBC_SRC___SUPPORT_LIBC_ASSERT_H
