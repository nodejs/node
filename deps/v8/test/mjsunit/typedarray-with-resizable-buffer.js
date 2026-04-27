// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestResizeLarger() {
  const rab = new ArrayBuffer(4 * 4, { maxByteLength: 8 * 4 });
  const ta = new Int32Array(rab);
  ta[0] = 10; ta[1] = 100; ta[2] = 1000; ta[3] = 10000;

  let valueOfCalled = false;
  const result = ta.with(-1, {
    valueOf() {
      valueOfCalled = true;
      rab.resize(8 * 4);
      return 42;
    }
  });

  assertTrue(valueOfCalled);
  assertEquals(4, result.length);
  assertEquals([10, 100, 1000, 42], Array.from(result));
})();

(function TestResizeSmallerThrows() {
  const rab = new ArrayBuffer(4 * 4, { maxByteLength: 8 * 4 });
  const ta = new Int32Array(rab);
  ta[0] = 10; ta[1] = 100; ta[2] = 1000; ta[3] = 10000;

  let valueOfCalled = false;
  assertThrows(() => {
    ta.with(-1, {
      valueOf() {
        valueOfCalled = true;
        rab.resize(2 * 4);
        return 42;
      }
    });
  }, RangeError);

  assertTrue(valueOfCalled);
})();

(function TestResizeToZeroThrows() {
  const rab = new ArrayBuffer(4 * 4, { maxByteLength: 8 * 4 });
  const ta = new Int32Array(rab);
  ta[0] = 10; ta[1] = 100; ta[2] = 1000; ta[3] = 10000;

  let valueOfCalled = false;
  assertThrows(() => {
    ta.with(0, {
      valueOf() {
        valueOfCalled = true;
        rab.resize(0);
        return 42;
      }
    });
  }, RangeError);

  assertTrue(valueOfCalled);
})();

(function TestOOBIndexCallsValueOf() {
  const rab = new ArrayBuffer(4 * 4, { maxByteLength: 8 * 4 });
  const ta = new Int32Array(rab);

  let valueOfCalled = false;
  assertThrows(() => {
    ta.with(10, { // OOB index
      valueOf() {
        valueOfCalled = true;
        return 42;
      }
    });
  }, RangeError);

  assertTrue(valueOfCalled);
})();

(function TestValueOfMakesOOBIndexOkay() {
  const rab = new ArrayBuffer(4 * 4, { maxByteLength: 8 * 4 });
  const ta = new Int32Array(rab);
  ta[0] = 10; ta[1] = 100; ta[2] = 1000; ta[3] = 10000;

  let valueOfCalled = false;
  const result = ta.with(6, { // OOB index
      valueOf() {
        // Resizing here makes the index okay
        rab.resize(8 * 4);
        valueOfCalled = true;
        return 42;
      }
    });

  assertTrue(valueOfCalled);
  // The result array is based on the original length, and storing the element
  // beyond the original length does not happen.
  assertEquals(4, result.length);
  assertEquals([10, 100, 1000, 10000], Array.from(result));
})();

(function TestValueOfDoesntMakeOOBIndexOkay() {
  const rab = new ArrayBuffer(4 * 4, { maxByteLength: 8 * 4 });
  const ta = new Int32Array(rab);
  ta[0] = 10; ta[1] = 100; ta[2] = 1000; ta[3] = 10000;

  let valueOfCalled = false;
  assertThrows(() =>  ta.with(10, { // OOB index
      valueOf() {
        // Resizing here is not enough to make the index within bounds
        rab.resize(8 * 4);
        valueOfCalled = true;
        return 42;
      }
    }), RangeError);

  assertTrue(valueOfCalled);
})();
