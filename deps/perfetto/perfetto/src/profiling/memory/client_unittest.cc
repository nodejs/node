/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/profiling/memory/client.h"

#include <thread>

#include "perfetto/base/thread_utils.h"
#include "perfetto/ext/base/unix_socket.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

TEST(ClientTest, GetThreadStackBase) {
  std::thread th([] {
    const char* stackbase = GetThreadStackBase();
    ASSERT_NE(stackbase, nullptr);
    // The implementation assumes the stack grows from higher addresses to
    // lower. We will need to rework once we encounter architectures where the
    // stack grows the other way.
    EXPECT_GT(stackbase, __builtin_frame_address(0));
  });
  th.join();
}

TEST(ClientTest, IsMainThread) {
  // Our code relies on the fact that getpid() == GetThreadId() if this
  // process/thread is the main thread of the process. This test ensures that is
  // true.
  auto pid = getpid();
  auto main_thread_id = base::GetThreadId();
  EXPECT_EQ(pid, main_thread_id);
  std::thread th(
      [main_thread_id] { EXPECT_NE(main_thread_id, base::GetThreadId()); });
  th.join();
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
