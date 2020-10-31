/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <jni.h>
#include <stdlib.h>
#include <unistd.h>

namespace {

// Must be kept in sync with heapprofd_test_cts.cc
constexpr int kIndividualAllocSz = 4153;
constexpr int kAllocationIntervalUs = 10 * 1000;

__attribute__((noreturn)) void perfetto_test_allocations() {
  for (;;) {
    // volatile & use avoids builtin malloc optimizations
    volatile char* x = static_cast<char*>(malloc(kIndividualAllocSz));
    if (x) {
      x[0] = '\0';
      free(const_cast<char*>(x));
    }
    usleep(kAllocationIntervalUs);
  }
}

// Runs continuously as a target for the sampling perf profiler tests.
__attribute__((noreturn)) void perfetto_busy_wait() {
  for (volatile unsigned i = 0;; i++) {
  }
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_cts_app_MainActivity_runNative(JNIEnv*, jclass) {
  perfetto_test_allocations();
}

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_cts_app_BusyWaitActivity_runNativeBusyWait(JNIEnv*,
                                                                 jclass) {
  perfetto_busy_wait();
}
