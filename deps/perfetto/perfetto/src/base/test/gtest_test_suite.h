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

#ifndef SRC_BASE_TEST_GTEST_TEST_SUITE_H_
#define SRC_BASE_TEST_GTEST_TEST_SUITE_H_

#include "test/gtest_and_gmock.h"

// Define newer TEST_SUITE googletest APIs as aliases of the older APIs where
// necessary. This makes it possible to migrate Perfetto to the newer APIs and
// still use older googletest versions where necessary.
//
// TODO(costan): Remove this header after googletest is rolled in Android.

#if !defined(INSTANTIATE_TEST_SUITE_P)
#define INSTANTIATE_TEST_SUITE_P(...) INSTANTIATE_TEST_CASE_P(__VA_ARGS__)
#endif

#if !defined(INSTANTIATE_TYPED_TEST_SUITE_P)
#define INSTANTIATE_TYPED_TEST_SUITE_P(...) \
  INSTANTIATE_TYPED_TEST_CASE_P(__VA_ARGS__)
#endif

#if !defined(REGISTER_TEST_SUITE_P)
#define REGISTER_TEST_SUITE_P(...) REGISTER_TEST_CASE_P(__VA_ARGS__)
#endif

#if !defined(TYPED_TEST_SUITE)
#define TYPED_TEST_SUITE(...) TYPED_TEST_CASE(__VA_ARGS__)
#endif

#if !defined(TYPED_TEST_SUITE_P)
#define TYPED_TEST_SUITE_P(...) TYPED_TEST_CASE_P(__VA_ARGS__)
#endif

#endif  // SRC_BASE_TEST_GTEST_TEST_SUITE_H_
