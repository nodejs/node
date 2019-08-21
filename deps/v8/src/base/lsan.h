// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LeakSanitizer support.

#ifndef V8_BASE_LSAN_H_
#define V8_BASE_LSAN_H_

#include <type_traits>

// There is no compile time flag for LSan, to enable this whenever ASan is
// enabled. Note that LSan can be used as part of ASan with 'detect_leaks=1'.
// On windows, LSan is not implemented yet, so disable it there.
#if defined(V8_USE_ADDRESS_SANITIZER) && !defined(V8_OS_WIN)

#include <sanitizer/lsan_interface.h>

#define LSAN_IGNORE_OBJECT(ptr) __lsan_ignore_object(ptr)

#else  // defined(V8_USE_ADDRESS_SANITIZER) && !defined(V8_OS_WIN)

#define LSAN_IGNORE_OBJECT(ptr)                                         \
  static_assert(std::is_convertible<decltype(ptr), const void*>::value, \
                "LSAN_IGNORE_OBJECT can only be used with pointer types")

#endif  // defined(V8_USE_ADDRESS_SANITIZER) && !defined(V8_OS_WIN)

#endif  // V8_BASE_LSAN_H_
