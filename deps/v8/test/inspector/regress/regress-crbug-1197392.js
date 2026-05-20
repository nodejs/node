// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sampling-heap-profiler-suppress-randomness

let {contextGroup, Protocol} = InspectorTest.start('Regression test for crbug.com/1197392');

InspectorTest.runAsyncTestSuite([
  async function testInvalidSamplingInterval() {
    await Protocol.HeapProfiler.enable();
    const message = await Protocol.HeapProfiler.startSampling({samplingInterval: 0});
    InspectorTest.logMessage(message);
    await Protocol.HeapProfiler.disable();
  }
]);
