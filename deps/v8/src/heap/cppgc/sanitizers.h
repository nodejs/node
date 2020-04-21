// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_SANITIZERS_H_
#define V8_HEAP_CPPGC_SANITIZERS_H_

#include "src/base/macros.h"

//
// TODO(chromium:1056170): Find a place in base for sanitizer support.
//

#ifdef V8_USE_ADDRESS_SANITIZER

#include <sanitizer/asan_interface.h>

#define NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))

#else  // !V8_USE_ADDRESS_SANITIZER

#define NO_SANITIZE_ADDRESS

#endif  // V8_USE_ADDRESS_SANITIZER

#ifdef V8_USE_MEMORY_SANITIZER

#include <sanitizer/msan_interface.h>

#define MSAN_UNPOISON(addr, size) __msan_unpoison(addr, size)

#else  // !V8_USE_MEMORY_SANITIZER

#define MSAN_UNPOISON(addr, size) ((void)(addr), (void)(size))

#endif  // V8_USE_MEMORY_SANITIZER

#endif  // V8_HEAP_CPPGC_SANITIZERS_H_
