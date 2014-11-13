// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-platform.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::InSequence;
using testing::StrictMock;

namespace v8 {
namespace platform {

namespace {

struct MockTask : public Task {
  virtual ~MockTask() { Die(); }
  MOCK_METHOD0(Run, void());
  MOCK_METHOD0(Die, void());
};

}  // namespace


TEST(DefaultPlatformTest, PumpMessageLoop) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatform platform;
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  StrictMock<MockTask>* task = new StrictMock<MockTask>;
  platform.CallOnForegroundThread(isolate, task);
  EXPECT_CALL(*task, Run());
  EXPECT_CALL(*task, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));
}

}  // namespace platform
}  // namespace v8
