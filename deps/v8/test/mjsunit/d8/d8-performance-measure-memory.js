// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the performance.measureMemory() function of d8.  This test only makes
// sense with d8.

load('test/mjsunit/mjsunit.js');

function assertLessThanOrEqual(a, b) {
  assertTrue(a <= b, `Expected ${a} <= ${b}`);
}

function checkMeasureMemoryResult(result) {
  assertTrue('total' in result);
  assertTrue('jsMemoryEstimate' in result.total);
  assertTrue('jsMemoryRange' in result.total);
  assertEquals('number', typeof result.total.jsMemoryEstimate);
  assertEquals(2, result.total.jsMemoryRange.length);
  assertEquals('number', typeof result.total.jsMemoryRange[0]);
  assertEquals('number', typeof result.total.jsMemoryRange[1]);
  assertLessThanOrEqual(result.total.jsMemoryRange[0],
                        result.total.jsMemoryRange[1]);
  assertLessThanOrEqual(result.total.jsMemoryRange[0],
                        result.total.jsMemoryEstimate);
  assertLessThanOrEqual(result.total.jsMemoryEstimate,
                        result.total.jsMemoryRange[1]);
}

if (this.performance && performance.measureMemory) {
  assertPromiseResult((async () => {
    let result = await performance.measureMemory();
    checkMeasureMemoryResult(result);
  })());

  assertPromiseResult((async () => {
    let result = await performance.measureMemory({detailed: false});
    checkMeasureMemoryResult(result);
  })());

  assertPromiseResult((async () => {
    let result = await performance.measureMemory({detailed: true});
    // TODO(ulan): Also check the detailed results once measureMemory
    // supports them.
    checkMeasureMemoryResult(result);
  })());
}
