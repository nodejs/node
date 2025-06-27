// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_COMPILER_TEST_UTILS_H_
#define V8_UNITTESTS_COMPILER_COMPILER_TEST_UTILS_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace compiler {

// The TARGET_TEST(Case, Name) macro works just like
// TEST(Case, Name), except that the test is disabled
// if the platform is not a supported TurboFan target.
#define TARGET_TEST(Case, Name) TEST(Case, Name)


// The TARGET_TEST_F(Case, Name) macro works just like
// TEST_F(Case, Name), except that the test is disabled
// if the platform is not a supported TurboFan target.
#define TARGET_TEST_F(Case, Name) TEST_F(Case, Name)


// The TARGET_TEST_P(Case, Name) macro works just like
// TEST_P(Case, Name), except that the test is disabled
// if the platform is not a supported TurboFan target.
#define TARGET_TEST_P(Case, Name) TEST_P(Case, Name)


// The TARGET_TYPED_TEST(Case, Name) macro works just like
// TYPED_TEST(Case, Name), except that the test is disabled
// if the platform is not a supported TurboFan target.
#define TARGET_TYPED_TEST(Case, Name) TYPED_TEST(Case, Name)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_COMPILER_TEST_UTILS_H_
