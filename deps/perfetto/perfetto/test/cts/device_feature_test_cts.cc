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

#include <sys/sysinfo.h>

#include "test/gtest_and_gmock.h"

namespace perfetto {

TEST(PerfettoDeviceFeatureTest, TestMaxCpusForAtraceChmod) {
  // Check that there are no more than 16 CPUs so that the assumption in the
  // atrace.rc for clearing CPU buffers is valid.
  ASSERT_LE(sysconf(_SC_NPROCESSORS_CONF), 16);
}

}  // namespace perfetto
