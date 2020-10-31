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

#ifndef SRC_BASE_TEST_UTILS_H_
#define SRC_BASE_TEST_UTILS_H_

#include <string>

#include "perfetto/base/logging.h"

#if PERFETTO_DCHECK_IS_ON()

#define EXPECT_DCHECK_DEATH(statement) \
  EXPECT_DEATH_IF_SUPPORTED(statement, "PERFETTO_CHECK")
#define ASSERT_DCHECK_DEATH(statement) \
  ASSERT_DEATH_IF_SUPPORTED(statement, "PERFETTO_CHECK")

#else  // PERFETTO_DCHECK_IS_ON()

// Since PERFETTO_DCHECK_IS_ON() is false these statements should not die (if
// they should/do we should use EXPECT/ASSERT DEATH_TEST_IF_SUPPORTED directly).
// Therefore if the platform supports DEATH_TESTS we can use the handy
// GTEST_EXECUTE_STATEMENT_ which prevents optimizing the code away, and if not
// we just fall back on executing the code directly.
#if defined(GTEST_EXECUTE_STATEMENT_)
#define EXPECT_DCHECK_DEATH(statement) \
    GTEST_EXECUTE_STATEMENT_(statement, "PERFETTO_CHECK")
#define ASSERT_DCHECK_DEATH(statement) \
    GTEST_EXECUTE_STATEMENT_(statement, "PERFETTO_CHECK")
#else
#define EXPECT_DCHECK_DEATH(statement) [&]() { statement }()
#define ASSERT_DCHECK_DEATH(statement) [&]() { statement }()
#endif  //  defined(GTEST_EXECUTE_STATEMENT_)

#endif  // PERFETTO_DCHECK_IS_ON()

namespace perfetto {
namespace base {

std::string GetCurExecutableDir();
std::string GetTestDataPath(const std::string& path);

}  // namespace base
}  // namespace perfetto

#endif  // SRC_BASE_TEST_UTILS_H_
