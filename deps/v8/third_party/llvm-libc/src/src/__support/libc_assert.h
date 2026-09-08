//===-- Definition of a libc internal assert macro --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_LIBC_ASSERT_H
#define LLVM_LIBC_SRC___SUPPORT_LIBC_ASSERT_H

#include "src/__support/macros/attributes.h" // For LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/hardening.h"
#include "src/__support/macros/macro-utils.h"
#include "src/__support/macros/optimization.h" // For LIBC_UNLIKELY
#include "src/__support/macros/properties/os.h"

#ifdef LIBC_FULL_BUILD
#include "src/__support/OSUtil/exit.h"
#include "src/__support/OSUtil/io.h"
#include "src/__support/integer_to_string.h"
#endif

//===----------------------------------------------------------------------===//
// LIBC_REQUIRE(COND, MSG) (always-on assert regardless of NDEBUG)
//===----------------------------------------------------------------------===//
#ifndef LIBC_FULL_BUILD
#ifdef LIBC_TARGET_OS_IS_LINUX
// __assert_fail is in Linux Standard Base (LSB), hence we should always be able
// to use it here.
#include <assert.h>
#define LIBC_REQUIRE(COND, MSG)                                                \
  do {                                                                         \
    if (LIBC_UNLIKELY(!(COND)))                                                \
      __assert_fail(MSG, __FILE__, __LINE__, __PRETTY_FUNCTION__);             \
  } while (false)
#else
// Fallback path will just trap: we cannot reliably do anything else.
#define LIBC_REQUIRE(COND, MSG)                                                \
  do {                                                                         \
    if (LIBC_UNLIKELY(!(COND)))                                                \
      __builtin_trap();                                                        \
  } while (false)
#endif // LIBC_TARGET_OS_IS_LINUX
#else
// FIXME: Calling abort on assertion is actually required by standards like LSB.
// Calling exit also confuses the debugger as exiting will not trigger
// debugger's stop-on-signal behavior by default. Currently, adding abort will
// result in cyclic dependency.
#define LIBC_REQUIRE(COND, MSG)                                                \
  do {                                                                         \
    if (LIBC_UNLIKELY(!(COND))) {                                              \
      LIBC_NAMESPACE::write_to_stderr(__FILE__ ":" LLVM_LIBC_STRINGIFY(        \
          __LINE__) ": Assertion failed: '" MSG "' in function: '");           \
      LIBC_NAMESPACE::write_to_stderr(__PRETTY_FUNCTION__);                    \
      LIBC_NAMESPACE::write_to_stderr("'\n");                                  \
      LIBC_NAMESPACE::internal::exit(0xFF);                                    \
    }                                                                          \
  } while (false)
#endif // LIBC_FULL_BUILD

//===----------------------------------------------------------------------===//
// LIBC_ASSERT(COND) (NDEBUG guarded assertion)
//===----------------------------------------------------------------------===//
#if defined(LIBC_COPT_USE_C_ASSERT) || !defined(LIBC_FULL_BUILD)

// The build is configured to just use the public <assert.h> API
// for libc's internal assertions.
#ifndef LIBC_ASSERT
#include <assert.h>
#define LIBC_ASSERT(COND) assert(COND)
#endif // LIBC_ASSERT

#else // Not LIBC_COPT_USE_C_ASSERT
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

#ifdef NDEBUG
#define LIBC_ASSERT(COND)                                                      \
  do {                                                                         \
  } while (false)
#else
// Forward to LIBC_REQUIRE with the condition stringified.
#define LIBC_ASSERT(COND) LIBC_REQUIRE(COND, #COND)
#endif // NDEBUG

#endif // LIBC_COPT_USE_C_ASSERT

//===----------------------------------------------------------------------===//
// Hardening runtime check
//===----------------------------------------------------------------------===//

#if LIBC_COPT_HARDENING_MODE == LIBC_HARDENING_MODE_NONE
#define LIBC_HEAP_INTEGRITY_CHECK(COND, MSG) ((void)0)
#elif LIBC_COPT_HARDENING_MODE == LIBC_HARDENING_MODE_FAST
#define LIBC_HEAP_INTEGRITY_CHECK(COND, MSG) ((void)0)
#elif LIBC_COPT_HARDENING_MODE == LIBC_HARDENING_MODE_EXTENSIVE
#define LIBC_HEAP_INTEGRITY_CHECK(COND, MSG) LIBC_REQUIRE(COND, MSG)
#elif LIBC_COPT_HARDENING_MODE == LIBC_HARDENING_MODE_DEBUG
#define LIBC_HEAP_INTEGRITY_CHECK(COND, MSG) LIBC_REQUIRE(COND, MSG)
#else
#error "Unsupported hardening mode"
#endif
#endif // LLVM_LIBC_SRC___SUPPORT_LIBC_ASSERT_H
