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

#include "perfetto/ext/base/thread_checker.h"

#include <pthread.h>

#include <functional>
#include <memory>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

// We just need two distinct pointers to return to pthread_join().
void* const kTrue = reinterpret_cast<void*>(1);
void* const kFalse = nullptr;

void* RunOnThread(std::function<void*(void)> closure) {
  pthread_t thread;
  auto thread_main = [](void* arg) -> void* {
    pthread_exit((*reinterpret_cast<std::function<void*(void)>*>(arg))());
  };
  EXPECT_EQ(0, pthread_create(&thread, nullptr, thread_main, &closure));
  void* retval = nullptr;
  EXPECT_EQ(0, pthread_join(thread, &retval));
  return retval;
}

TEST(ThreadCheckerTest, Basic) {
  ThreadChecker thread_checker;
  ASSERT_TRUE(thread_checker.CalledOnValidThread());
  void* res = RunOnThread([&thread_checker]() -> void* {
    return thread_checker.CalledOnValidThread() ? kTrue : kFalse;
  });
  ASSERT_TRUE(thread_checker.CalledOnValidThread());
  ASSERT_EQ(kFalse, res);
}

TEST(ThreadCheckerTest, Detach) {
  ThreadChecker thread_checker;
  ASSERT_TRUE(thread_checker.CalledOnValidThread());
  thread_checker.DetachFromThread();
  void* res = RunOnThread([&thread_checker]() -> void* {
    return thread_checker.CalledOnValidThread() ? kTrue : kFalse;
  });
  ASSERT_EQ(kTrue, res);
  ASSERT_FALSE(thread_checker.CalledOnValidThread());
}

TEST(ThreadCheckerTest, CopyConstructor) {
  ThreadChecker thread_checker;
  ThreadChecker copied_thread_checker = thread_checker;
  ASSERT_TRUE(thread_checker.CalledOnValidThread());
  ASSERT_TRUE(copied_thread_checker.CalledOnValidThread());
  void* res = RunOnThread([&copied_thread_checker]() -> void* {
    return copied_thread_checker.CalledOnValidThread() ? kTrue : kFalse;
  });
  ASSERT_EQ(kFalse, res);

  copied_thread_checker.DetachFromThread();
  res = RunOnThread([&thread_checker, &copied_thread_checker]() -> void* {
    return (copied_thread_checker.CalledOnValidThread() &&
            !thread_checker.CalledOnValidThread())
               ? kTrue
               : kFalse;
  });
  ASSERT_EQ(kTrue, res);
}

}  // namespace
}  // namespace base
}  // namespace perfetto
