// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char** argv) {
  testing::InitGoogleMock(&argc, argv);  // Also inits GoogleTest.
  return RUN_ALL_TESTS();
}
