// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/libplatform/default-platform.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-libplatform.h"

using namespace v8::internal;
using namespace v8::platform;


TEST(DefaultPlatformMessagePump) {
  TaskCounter task_counter;

  DefaultPlatform platform;

  TestTask* task = new TestTask(&task_counter, true);

  CHECK(!platform.PumpMessageLoop(CcTest::isolate()));

  platform.CallOnForegroundThread(CcTest::isolate(), task);

  CHECK_EQ(1, task_counter.GetCount());
  CHECK(platform.PumpMessageLoop(CcTest::isolate()));
  CHECK_EQ(0, task_counter.GetCount());
  CHECK(!platform.PumpMessageLoop(CcTest::isolate()));
}
