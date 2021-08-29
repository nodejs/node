// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute('test/mjsunit/mjsunit.js');

function assertLessThanOrEqual(a, b) {
  assertTrue(a <= b, `Expected ${a} <= ${b}`);
}

function checkMeasureMemoryResultField(field) {
  assertTrue('jsMemoryEstimate' in field);
  assertTrue('jsMemoryRange' in field);
  assertEquals('number', typeof field.jsMemoryEstimate);
  assertEquals(2, field.jsMemoryRange.length);
  assertEquals('number', typeof field.jsMemoryRange[0]);
  assertEquals('number', typeof field.jsMemoryRange[1]);
  assertLessThanOrEqual(field.jsMemoryRange[0],
                        field.jsMemoryRange[1]);
  assertLessThanOrEqual(field.jsMemoryRange[0],
                        field.jsMemoryEstimate);
  assertLessThanOrEqual(field.jsMemoryEstimate,
                        field.jsMemoryRange[1]);
}

function checkMeasureMemoryResult(result, detailed) {
  assertTrue('total' in result);
  checkMeasureMemoryResultField(result.total);
  if (detailed) {
    assertTrue('current' in result);
    checkMeasureMemoryResultField(result.current);
    assertTrue('other' in result);
    for (let other of result.other) {
      checkMeasureMemoryResultField(other);
    }
  }
}

if (this.performance && performance.measureMemory) {
  assertPromiseResult((async () => {
    let result = await performance.measureMemory();
    checkMeasureMemoryResult(result, false);
  })());

  assertPromiseResult((async () => {
    let result = await performance.measureMemory({detailed: false});
    checkMeasureMemoryResult(result, false);
  })());

  assertPromiseResult((async () => {
    let result = await performance.measureMemory({detailed: true});
    print(JSON.stringify(result));
    checkMeasureMemoryResult(result, true);
  })());
  // Force a GC to complete memory measurement.
  gc();
}
