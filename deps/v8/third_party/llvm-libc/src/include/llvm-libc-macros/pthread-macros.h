//===-- Definition of pthread macros --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_PTHREAD_MACRO_H
#define LLVM_LIBC_MACROS_PTHREAD_MACRO_H

#define PTHREAD_NULL {0}

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_MUTEX_STALLED 0
#define PTHREAD_MUTEX_ROBUST 1

#define PTHREAD_BARRIER_SERIAL_THREAD -1

#define PTHREAD_ONCE_INIT {0}

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1

#ifdef __linux__
#define PTHREAD_MUTEX_INITIALIZER                                              \
  {                                                                            \
      /* .__ftxw = */ {0},     /* .__priority_inherit = */ 0,                  \
      /* .__recursive = */ 0,  /* .__robust = */ 0,                            \
      /* .__pshared = */ 0,    /* .__owner = */ 0,                             \
      /* .__lock_count = */ 0,                                                 \
  }
#else
#define PTHREAD_MUTEX_INITIALIZER                                              \
  {                                                                            \
      /* .__ftxw = */ {0},     /* .__priority_inherit = */ 0,                  \
      /* .__recursive = */ 0,  /* .__robust = */ 0,                            \
      /* .__pshared = */ 0,    /* .__owner = */ 0,                             \
      /* .__lock_count = */ 0,                                                 \
  }
#endif

#define PTHREAD_COND_INITIALIZER                                               \
  {                                                                            \
      /* .__waiter_queue = */ {{NULL, NULL}},                                  \
      /* .__futex = */ {0},                                                    \
      /* .__is_shared = */ 0,                                                  \
      /* .__is_realtime = */ 1,                                                \
      /* .__padding = */ {0},                                                  \
  }

#define PTHREAD_RWLOCK_INITIALIZER                                             \
  {                                                                            \
      /* .__raw = */ {                                                         \
          /* .__is_pshared = */ 0,                                             \
          /* .__preference = */ 0,                                             \
          /* .__state = */ 0,                                                  \
          /* .__wait_queue_mutex = */ {0},                                     \
          /* .__pending_readers = */ {0},                                      \
          /* .__pending_writers = */ {0},                                      \
          /* .__reader_serialization = */ {0},                                 \
          /* .__writer_serialization = */ {0},                                 \
      },                                                                       \
      /* .__write_tid = */ 0,                                                  \
  }

// glibc extensions
#define PTHREAD_STACK_MIN (1 << 14) // 16KB
#define PTHREAD_RWLOCK_PREFER_READER_NP 0
#define PTHREAD_RWLOCK_PREFER_WRITER_NP 1
#define PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP 2

#endif // LLVM_LIBC_MACROS_PTHREAD_MACRO_H
