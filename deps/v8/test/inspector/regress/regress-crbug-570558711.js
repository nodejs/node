// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {contextGroup, Protocol} = InspectorTest.start('Regression test for crbug.com/570558711');

InspectorTest.runAsyncTestSuite([
  async function testInvalidSamplingInterval() {
    await Protocol.Profiler.enable();
    let message = await Protocol.Profiler.setSamplingInterval({interval: 0});
    InspectorTest.logMessage(message);
    message = await Protocol.Profiler.setSamplingInterval({interval: -1});
    InspectorTest.logMessage(message);
    await Protocol.Profiler.start();
    await Protocol.Profiler.stop();
    await Protocol.Profiler.disable();
  }
]);
