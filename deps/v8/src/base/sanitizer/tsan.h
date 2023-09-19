// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ThreadSanitizer support.

#ifndef V8_BASE_SANITIZER_TSAN_H_
#define V8_BASE_SANITIZER_TSAN_H_

#if defined(THREAD_SANITIZER)

#define DISABLE_TSAN __attribute__((no_sanitize_thread))

#else  // !defined(THREAD_SANITIZER)

#define DISABLE_TSAN

#endif  // !defined(THREAD_SANITIZER)

#endif  // V8_BASE_SANITIZER_TSAN_H_
