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

#ifndef SRC_TRACING_TEST_API_TEST_SUPPORT_H_
#define SRC_TRACING_TEST_API_TEST_SUPPORT_H_

// This header is intended to be used only for api_integrationtest.cc and solves
// the following problem: api_integrationtest.cc doesn't pull any non-public
// perfetto header, to spot accidental public->non-public dependencies.
// Sometimes, however, we need to use some internal perfetto code for the test
// itself. This header exposes wrapper functions to achieve that without leaking
// internal headers.

//  IMPORTANT: This header must not pull any non-public perfetto header.

#include <stdint.h>

namespace perfetto {
namespace test {

int32_t GetCurrentProcessId();

}  // namespace test
}  // namespace perfetto

#endif  // SRC_TRACING_TEST_API_TEST_SUPPORT_H_
