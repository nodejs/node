// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  // Don't catch SEH exceptions and continue as the following tests might hang
  // in an broken environment on windows.
  testing::GTEST_FLAG(catch_exceptions) = false;
  testing::InitGoogleMock(&argc, argv);
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::InitializeExternalStartupData(argv[0]);
  return RUN_ALL_TESTS();
}
