// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"

namespace {

class InspectorProtocolTestEnvironment final : public ::testing::Environment {};

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleMock(&argc, argv);
  testing::AddGlobalTestEnvironment(new InspectorProtocolTestEnvironment);
  return RUN_ALL_TESTS();
}
