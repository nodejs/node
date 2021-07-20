// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax

"use strict";

function resizeHelper(ab, value) {
  const return_value = ab.resize(value);
  assertEquals(undefined, return_value);
  assertEquals(value, ab.byteLength);
}

function growHelper(ab, value) {
  const return_value = ab.grow(value);
  assertEquals(undefined, return_value);
  assertEquals(value, ab.byteLength);
}

(function TestRABBasics() {
  const rab = new ResizableArrayBuffer(10, 20);
  assertTrue(rab instanceof ResizableArrayBuffer);
  assertFalse(rab instanceof GrowableSharedArrayBuffer);
  assertFalse(rab instanceof ArrayBuffer);
  assertFalse(rab instanceof SharedArrayBuffer);
  assertEquals(10, rab.byteLength);
  assertEquals(20, rab.maxByteLength);
})();

(function TestRABCtorByteLengthEqualsMax() {
  const rab = new ResizableArrayBuffer(10, 10);
  assertEquals(10, rab.byteLength);
  assertEquals(10, rab.maxByteLength);
})();

(function TestRABCtorByteLengthZero() {
  const rab = new ResizableArrayBuffer(0, 10);
  assertEquals(0, rab.byteLength);
  assertEquals(10, rab.maxByteLength);
})();

(function TestRABCtorByteLengthAndMaxZero() {
  const rab = new ResizableArrayBuffer(0, 0);
  assertEquals(0, rab.byteLength);
  assertEquals(0, rab.maxByteLength);
})();

(function TestRABCtorNoMaxByteLength() {
  assertThrows(() => { new ResizableArrayBuffer(10); }, RangeError);
  // But this is fine; undefined is converted to 0.
  const rab = new ResizableArrayBuffer(0);
  assertEquals(0, rab.byteLength);
  assertEquals(0, rab.maxByteLength);
})();

(function TestAllocatingOutrageouslyMuchThrows() {
  assertThrows(() => { new ResizableArrayBuffer(0, 2 ** 100);}, RangeError);
})();

(function TestRABCtorOperationOrder() {
  let log = '';
  const mock_length = {valueOf: function() {
      log += 'valueof length, '; return 10; }};
  const mock_max_length = {valueOf: function() {
      log += 'valueof max_length, '; return 10; }};
  new ResizableArrayBuffer(mock_length, mock_max_length);

  assertEquals('valueof length, valueof max_length, ', log);
})();

(function TestGSABCtorOperationOrder() {
  let log = '';
  const mock_length = {valueOf: function() {
      log += 'valueof length, '; return 10; }};
  const mock_max_length = {valueOf: function() {
      log += 'valueof max_length, '; return 10; }};
  new ResizableArrayBuffer(mock_length, mock_max_length);

  assertEquals('valueof length, valueof max_length, ', log);
})();

(function TestByteLengthGetterReceiverChecks() {
  const name = 'byteLength';
  const ab_getter = Object.getOwnPropertyDescriptor(
      ArrayBuffer.prototype, name).get;
  const sab_getter = Object.getOwnPropertyDescriptor(
      SharedArrayBuffer.prototype, name).get;
  const rab_getter = Object.getOwnPropertyDescriptor(
      ResizableArrayBuffer.prototype, name).get;
  const gsab_getter = Object.getOwnPropertyDescriptor(
      GrowableSharedArrayBuffer.prototype, name).get;

  const ab = new ArrayBuffer(40);
  const sab = new SharedArrayBuffer(40);
  const rab = new ResizableArrayBuffer(40, 40);
  const gsab = new GrowableSharedArrayBuffer(40, 40);

  assertEquals(40, ab_getter.call(ab));
  assertEquals(40, sab_getter.call(sab));
  assertEquals(40, rab_getter.call(rab));
  assertEquals(40, gsab_getter.call(gsab));

  assertThrows(() => { ab_getter.call(sab);});
  assertThrows(() => { ab_getter.call(rab);});
  assertThrows(() => { ab_getter.call(gsab);});

  assertThrows(() => { sab_getter.call(ab);});
  assertThrows(() => { sab_getter.call(rab);});
  assertThrows(() => { sab_getter.call(gsab);});

  assertThrows(() => { rab_getter.call(ab);});
  assertThrows(() => { rab_getter.call(sab);});
  assertThrows(() => { rab_getter.call(gsab);});

  assertThrows(() => { gsab_getter.call(ab);});
  assertThrows(() => { gsab_getter.call(sab);});
  assertThrows(() => { gsab_getter.call(rab);});
})();

(function TestMaxByteLengthGetterReceiverChecks() {
  const name = 'maxByteLength';
  const rab_getter = Object.getOwnPropertyDescriptor(
      ResizableArrayBuffer.prototype, name).get;
  const gsab_getter = Object.getOwnPropertyDescriptor(
      GrowableSharedArrayBuffer.prototype, name).get;

  const ab = new ArrayBuffer(40);
  const sab = new SharedArrayBuffer(40);
  const rab = new ResizableArrayBuffer(20, 40);
  const gsab = new GrowableSharedArrayBuffer(20, 40);

  assertEquals(40, rab_getter.call(rab));
  assertEquals(40, gsab_getter.call(gsab));

  assertThrows(() => { rab_getter.call(ab);});
  assertThrows(() => { rab_getter.call(sab);});
  assertThrows(() => { rab_getter.call(gsab);});

  assertThrows(() => { gsab_getter.call(ab);});
  assertThrows(() => { gsab_getter.call(sab);});
  assertThrows(() => { gsab_getter.call(rab);});
})();

(function TestResizeAndGrowReceiverChecks() {
  const rab_resize = ResizableArrayBuffer.prototype.resize;
  const gsab_grow = GrowableSharedArrayBuffer.prototype.grow;

  const ab = new ArrayBuffer(40);
  const sab = new SharedArrayBuffer(40);
  const rab = new ResizableArrayBuffer(10, 40);
  const gsab = new GrowableSharedArrayBuffer(10, 40);

  rab_resize.call(rab, 20);
  gsab_grow.call(gsab, 20);
  assertThrows(() => { rab_resize.call(ab, 30);});
  assertThrows(() => { rab_resize.call(sab, 30);});
  assertThrows(() => { rab_resize.call(gsab, 30);});

  assertThrows(() => { gsab_grow.call(ab, 30);});
  assertThrows(() => { gsab_grow.call(sab, 30);});
  assertThrows(() => { gsab_grow.call(rab, 30);});
})();

(function TestRABResizeToMax() {
  const rab = new ResizableArrayBuffer(10, 20);
  resizeHelper(rab, 20);
})();

(function TestRABResizeToSameSize() {
  const rab = new ResizableArrayBuffer(10, 20);
  resizeHelper(rab, 10);
})();

(function TestRABResizeToSmaller() {
  const rab = new ResizableArrayBuffer(10, 20);
  resizeHelper(rab, 5);
})();

(function TestRABResizeToZero() {
  const rab = new ResizableArrayBuffer(10, 20);
  resizeHelper(rab, 0);
})();

(function TestRABResizeZeroToZero() {
  const rab = new ResizableArrayBuffer(0, 20);
  resizeHelper(rab, 0);
})();

(function TestRABGrowBeyondMaxThrows() {
  const rab = new ResizableArrayBuffer(10, 20);
  assertEquals(10, rab.byteLength);
  assertThrows(() => {rab.grow(21)});
  assertEquals(10, rab.byteLength);
})();

(function TestRABResizeMultipleTimes() {
  const rab = new ResizableArrayBuffer(10, 20);
  const sizes = [15, 7, 7, 0, 8, 20, 20, 10];
  for (let s of sizes) {
    resizeHelper(rab, s);
  }
})();

(function TestRABResizeParameters() {
  const rab = new ResizableArrayBuffer(10, 20);
  rab.resize('15');
  assertEquals(15, rab.byteLength);
  rab.resize({valueOf: function() { return 16; }});
  assertEquals(16, rab.byteLength);
  rab.resize(17.9);
  assertEquals(17, rab.byteLength);
})();

(function TestRABResizeInvalidParameters() {
  const rab = new ResizableArrayBuffer(10, 20);
  assertThrows(() => { rab.resize(-1) }, RangeError);
  assertThrows(() => { rab.resize({valueOf: function() {
      throw new Error('length param'); }})});
  assertEquals(10, rab.byteLength);

  // Various non-numbers are converted to NaN which is treated as 0.
  rab.resize('string');
  assertEquals(0, rab.byteLength);
  rab.resize();
  assertEquals(0, rab.byteLength);
})();

(function TestRABResizeDetached() {
  const rab = new ResizableArrayBuffer(10, 20);
  %ArrayBufferDetach(rab);
  assertThrows(() => { rab.resize(15) }, TypeError);
})();

(function DetachInsideResizeParameterConversion() {
  const rab = new ResizableArrayBuffer(40, 80);

  const evil = {
    valueOf: () => { %ArrayBufferDetach(rab); return 20; }
  };

  assertThrows(() => { rab.resize(evil); });
})();

(function ResizeInsideResizeParameterConversion() {
  const rab = new ResizableArrayBuffer(40, 80);

  const evil = {
    valueOf: () => { rab.resize(10); return 20; }
  };

  rab.resize(evil);
  assertEquals(20, rab.byteLength);
})();


(function TestRABNewMemoryAfterResizeInitializedToZero() {
  const maybe_page_size = 4096;
  const rab = new ResizableArrayBuffer(maybe_page_size, 2 * maybe_page_size);
  const i8a = new Int8Array(rab);
  rab.resize(2 * maybe_page_size);
  for (let i = 0; i < 2 * maybe_page_size; ++i) {
    assertEquals(0, i8a[i]);
  }
})();

(function TestRABMemoryInitializedToZeroAfterShrinkAndGrow() {
  const maybe_page_size = 4096;
  const rab = new ResizableArrayBuffer(maybe_page_size, 2 * maybe_page_size);
  const i8a = new Int8Array(rab);
  for (let i = 0; i < maybe_page_size; ++i) {
    i8a[i] = 1;
  }
  rab.resize(maybe_page_size / 2);
  rab.resize(maybe_page_size);
  for (let i = maybe_page_size / 2; i < maybe_page_size; ++i) {
    assertEquals(0, i8a[i]);
  }
})();

(function TestGSABBasics() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertFalse(gsab instanceof ResizableArrayBuffer);
  assertTrue(gsab instanceof GrowableSharedArrayBuffer);
  assertFalse(gsab instanceof ArrayBuffer);
  assertFalse(gsab instanceof SharedArrayBuffer);
  assertEquals(10, gsab.byteLength);
  assertEquals(20, gsab.maxByteLength);
})();

(function TestGSABCtorByteLengthEqualsMax() {
  const gsab = new GrowableSharedArrayBuffer(10, 10);
  assertEquals(10, gsab.byteLength);
  assertEquals(10, gsab.maxByteLength);
})();

(function TestGSABCtorByteLengthZero() {
  const gsab = new GrowableSharedArrayBuffer(0, 10);
  assertEquals(0, gsab.byteLength);
  assertEquals(10, gsab.maxByteLength);
})();

(function TestGSABCtorByteLengthAndMaxZero() {
  const gsab = new GrowableSharedArrayBuffer(0, 0);
  assertEquals(0, gsab.byteLength);
  assertEquals(0, gsab.maxByteLength);
})();

(function TestGSABCtorNoMaxByteLength() {
  assertThrows(() => { new GrowableSharedArrayBuffer(10); }, RangeError);
  // But this is fine; undefined is converted to 0.
  const gsab = new GrowableSharedArrayBuffer(0);
  assertEquals(0, gsab.byteLength);
  assertEquals(0, gsab.maxByteLength);
})();

(function TestAllocatingOutrageouslyMuchThrows() {
  assertThrows(() => { new GrowableSharedArrayBuffer(0, 2 ** 100);},
               RangeError);
})();

(function TestGSABGrowToMax() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  growHelper(gsab, 20);
})();

(function TestGSABGrowToSameSize() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  growHelper(gsab, 10);
})();

(function TestGSABGrowToSmallerThrows() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  assertThrows(() => {gsab.grow(5)});
  assertEquals(10, gsab.byteLength);
})();

(function TestGSABGrowToZeroThrows() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  assertThrows(() => {gsab.grow(0)});
  assertEquals(10, gsab.byteLength);
})();

(function TestGSABGrowBeyondMaxThrows() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  assertThrows(() => {gsab.grow(21)});
  assertEquals(10, gsab.byteLength);
})();

(function TestGSABGrowMultipleTimes() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  const sizes = [15, 7, 7, 0, 8, 20, 20, 10];
  for (let s of sizes) {
    const current_size = gsab.byteLength;
    if (s < gsab.byteLength) {
      assertThrows(() => {gsab.grow(s)});
      assertEquals(current_size, gsab.byteLength);
    } else {
      growHelper(gsab, s);
    }
  }
})();

(function TestGSABGrowParameters() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  gsab.grow('15');
  assertEquals(15, gsab.byteLength);
  gsab.grow({valueOf: function() { return 16; }});
  assertEquals(16, gsab.byteLength);
  gsab.grow(17.9);
  assertEquals(17, gsab.byteLength);
})();

(function TestGSABGrowInvalidParameters() {
  const gsab = new GrowableSharedArrayBuffer(0, 20);
  assertThrows(() => { gsab.grow(-1) }, RangeError);
  assertThrows(() => { gsab.grow({valueOf: function() {
      throw new Error('length param'); }})});
  assertEquals(0, gsab.byteLength);

  // Various non-numbers are converted to NaN which is treated as 0.
  gsab.grow('string');
  assertEquals(0, gsab.byteLength);
  gsab.grow();
  assertEquals(0, gsab.byteLength);
})();

(function TestGSABMemoryInitializedToZeroAfterGrow() {
  const maybe_page_size = 4096;
  const gsab = new GrowableSharedArrayBuffer(maybe_page_size,
                                             2 * maybe_page_size);
  const i8a = new Int8Array(gsab);
  gsab.grow(2 * maybe_page_size);
  assertEquals(2 * maybe_page_size, i8a.length);
  for (let i = 0; i < 2 * maybe_page_size; ++i) {
    assertEquals(0, i8a[i]);
  }
})();

(function GrowGSABOnADifferentThread() {
  const gsab = new GrowableSharedArrayBuffer(10, 20);
  assertEquals(10, gsab.byteLength);
  function workerCode() {
    function assert(thing) {
      if (!thing) {
        postMessage('error');
      }
    }
    onmessage = function(params) {
      const gsab = params.gsab;
      assert(!(gsab instanceof ResizableArrayBuffer));
      assert(gsab instanceof GrowableSharedArrayBuffer);
      assert(!(gsab instanceof ArrayBuffer));
      assert(!(gsab instanceof SharedArrayBuffer));
      assert(10 == gsab.byteLength);
      gsab.grow(15);
      postMessage('ok');
    }
  }
  const w = new Worker(workerCode, {type: 'function'});
  w.postMessage({gsab: gsab});
  assertEquals('ok', w.getMessage());
  assertEquals(15, gsab.byteLength);
})();
