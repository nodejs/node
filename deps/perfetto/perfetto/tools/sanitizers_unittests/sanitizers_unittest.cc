/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Smoke tests to verify that clang sanitizers are actually working.

#include <pthread.h>
#include <stdint.h>

#include <memory>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

#if defined(ADDRESS_SANITIZER)
TEST(SanitizerTests, ASAN_UserAfterFree) {
  EXPECT_DEATH(
      {
        void* alloc = malloc(16);
        volatile char* mem = reinterpret_cast<volatile char*>(alloc);
        mem[0] = 1;
        mem[15] = 1;
        free(alloc);
        mem[0] = 2;
        abort();
      },
      "AddressSanitizer:.*heap-use-after-free");
}
#endif  // ADDRESS_SANITIZER

#if defined(MEMORY_SANITIZER)
TEST(SanitizerTests, MSAN_UninitializedMemory) {
  EXPECT_DEATH(
      {
        std::unique_ptr<int> mem(new int[10]);
        volatile int* x = reinterpret_cast<volatile int*>(mem.get());
        if (x[rand() % 10] == 42)
          printf("\n");
        abort();
      },
      "MemorySanitizer:.*use-of-uninitialized-value");
}
#endif

// b/141460117: Leak sanitizer tests don't work in debug builds.
#if defined(LEAK_SANITIZER) && defined(NDEBUG)
TEST(SanitizerTests, LSAN_LeakMalloc) {
  EXPECT_DEATH(
      {
        void* alloc = malloc(16);
        reinterpret_cast<volatile char*>(alloc)[0] = 1;
        alloc = malloc(16);
        reinterpret_cast<volatile char*>(alloc)[0] = 2;
        free(alloc);
        exit(0);  // LSan runs on the atexit handler.
      },
      "LeakSanitizer:.*detected memory leaks");
}

TEST(SanitizerTests, LSAN_LeakCppNew) {
  EXPECT_DEATH(
      {
        std::unique_ptr<int> alloc(new int(1));
        *reinterpret_cast<volatile char*>(alloc.get()) = 1;
        alloc.release();
        alloc.reset(new int(2));
        *reinterpret_cast<volatile char*>(alloc.get()) = 2;
        exit(0);  // LSan runs on the atexit handler.
      },
      "LeakSanitizer:.*detected memory leaks");
}
#endif  // LEAK_SANITIZER && defined(NDEBUG)

#if defined(UNDEFINED_SANITIZER)
TEST(SanitizerTests, UBSAN_DivisionByZero) {
  EXPECT_DEATH(
      {
        volatile float div = 1;
        float res = 3 / (div - 1);
        ASSERT_GT(res, -1.0f);  // just use |res| to make the compiler happy.
        abort();
      },
      "error:.*division by zero");
}

TEST(SanitizerTests, UBSAN_ShiftExponent) {
  EXPECT_DEATH(
      {
        volatile uint32_t n = 32;
        volatile uint32_t shift = 31;
        uint64_t res = n << (shift + 3);
        ASSERT_NE(1u, res);  // just use |res| to make the compiler happy.
        abort();
      },
      "error:.*shift exponent");
}
#endif  // UNDEFINED_SANITIZER

#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER) && !defined(LEAK_SANITIZER) &&    \
    !defined(UNDEFINED_SANITIZER)
TEST(SanitizerTests, NoSanitizersConfigured) {
  printf("No sanitizers configured!\n");
}
#endif

}  // namespace
}  // namespace perfetto
