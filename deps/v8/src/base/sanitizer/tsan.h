// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_SANITIZER_TSAN_H_
#define V8_BASE_SANITIZER_TSAN_H_

// ThreadSanitizer support.

#if defined(THREAD_SANITIZER)

#define DISABLE_TSAN __attribute__((no_sanitize_thread))

extern "C" {
void AnnotateIgnoreReadsBegin(const char* file, int line);
void AnnotateIgnoreReadsEnd(const char* file, int line);
void __tsan_acquire(void* addr);
void __tsan_release(void* addr);
}

#define TSAN_IGNORE_READS_BEGIN AnnotateIgnoreReadsBegin(__FILE__, __LINE__)
#define TSAN_IGNORE_READS_END AnnotateIgnoreReadsEnd(__FILE__, __LINE__)
#define TSAN_ACQUIRE(addr) __tsan_acquire(reinterpret_cast<void*>(addr))
#define TSAN_RELEASE(addr) __tsan_release(reinterpret_cast<void*>(addr))

#else  // !defined(THREAD_SANITIZER)

#define DISABLE_TSAN
#define TSAN_IGNORE_READS_BEGIN ((void)0)
#define TSAN_IGNORE_READS_END ((void)0)
#define TSAN_ACQUIRE(addr) ((void)0)
#define TSAN_RELEASE(addr) ((void)0)

#endif  // !defined(THREAD_SANITIZER)

#endif  // V8_BASE_SANITIZER_TSAN_H_
