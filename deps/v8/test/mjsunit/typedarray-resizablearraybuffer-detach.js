// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax --harmony-array-find-last

"use strict";

d8.file.execute('test/mjsunit/typedarray-helpers.js');

(function ConstructorThrowsIfBufferDetached() {
  const rab = CreateResizableArrayBuffer(40, 80);
  %ArrayBufferDetach(rab);

  for (let ctor of ctors) {
    assertThrows(() => { ctor(rab); }, TypeError);
    assertThrows(() => { ctor(rab, 8); }, TypeError);
    assertThrows(() => { ctor(rab, 8, 1); }, TypeError);
  }
})();

(function TypedArrayLengthAndByteLength() {
  const rab = CreateResizableArrayBuffer(40, 80);

  let tas = [];
  for (let ctor of ctors) {
    tas.push(new ctor(rab, 0, 3));
    tas.push(new ctor(rab, 8, 3));
    tas.push(new ctor(rab));
    tas.push(new ctor(rab, 8));
  }

  %ArrayBufferDetach(rab);

  for (let ta of tas) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
  }
})();

(function AccessDetachedTypedArray() {
  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);

  // Initial values
  for (let i = 0; i < 4; ++i) {
    assertEquals(0, i8a[i]);
  }

  // Within-bounds write
  for (let i = 0; i < 4; ++i) {
    i8a[i] = i;
  }

  %ArrayBufferDetach(rab);

  // OOB read
  for (let i = 0; i < 4; ++i) {
    assertEquals(undefined, i8a[i]);
  }

  // OOB write (has no effect)
  for (let i = 0; i < 4; ++i) {
    i8a[i] = 10;
  }

  for (let i = 0; i < 4; ++i) {
    assertEquals(undefined, i8a[i]);
  }
})();

(function LoadFromOutOfScopeTypedArrayWithFeedback() {
  function ReadElement2(ta) {
    return ta[2];
  }
  %EnsureFeedbackVectorForFunction(ReadElement2);

  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);
  assertEquals(0, ReadElement2(i8a));

  // Within-bounds write
  for (let i = 0; i < 4; ++i) {
    i8a[i] = i;
  }

  %ArrayBufferDetach(rab);

  // OOB read
  for (let i = 0; i < 3; ++i) {
    assertEquals(undefined, ReadElement2(i8a));
  }
})();

(function StoreToOutOfScopeTypedArrayWithFeedback() {
  function WriteElement2(ta, i) {
    ta[2] = i;
  }
  %EnsureFeedbackVectorForFunction(WriteElement2);

  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);
  assertEquals(0, i8a[2]);

  // Within-bounds write
  for (let i = 0; i < 3; ++i) {
    WriteElement2(i8a, 3);
  }

  %ArrayBufferDetach(rab);

  // OOB write (has no effect)
  for (let i = 0; i < 3; ++i) {
    WriteElement2(i8a, 4);
  }

  // OOB read
  for (let i = 0; i < 3; ++i) {
    assertEquals(undefined, i8a[2]);
  }
})();

(function FillParameterConversionDetaches() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => { %ArrayBufferDetach(rab); return 1;}};
    // The length is read after converting the first parameter ('value'), so the
    // detaching parameter has to be the 2nd ('start') or 3rd ('end').
    assertThrows(function() {
      FillHelper(fixedLength, 1, 0, evil);
    }, TypeError);
  }
})();

(function CopyWithinParameterConversionDetaches() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => { %ArrayBufferDetach(rab); return 2;}};
    assertThrows(function() {
      fixedLength.copyWithin(evil, 0, 1);
    }, TypeError);
  }
})();

(function EveryDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values = [];
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return true;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertTrue(fixedLength.every(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertTrue(fixedLengthWithOffset.every(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertTrue(lengthTracking.every(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertTrue(lengthTrackingWithOffset.every(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }
})();

(function SomeDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertFalse(fixedLength.some(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertFalse(fixedLengthWithOffset.some(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertFalse(lengthTracking.some(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertFalse(lengthTrackingWithOffset.some(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }
})();

(function FindDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertEquals(undefined, fixedLength.find(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertEquals(undefined, fixedLengthWithOffset.find(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertEquals(undefined, lengthTracking.find(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertEquals(undefined, lengthTrackingWithOffset.find(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }
})();

(function FindIndexDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertEquals(-1, fixedLength.findIndex(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertEquals(-1, fixedLengthWithOffset.findIndex(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertEquals(-1, lengthTracking.findIndex(CollectValuesAndDetach));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertEquals(-1, lengthTrackingWithOffset.findIndex(CollectValuesAndDetach));
    assertEquals([4, undefined], values);
  }
})();

(function FindLastDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertEquals(undefined, fixedLength.findLast(CollectValuesAndDetach));
    assertEquals([6, 4, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertEquals(undefined, fixedLengthWithOffset.findLast(CollectValuesAndDetach));
    assertEquals([6, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertEquals(undefined, lengthTracking.findLast(CollectValuesAndDetach));
    assertEquals([6, 4, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertEquals(undefined, lengthTrackingWithOffset.findLast(CollectValuesAndDetach));
    assertEquals([6, undefined], values);
  }
})();

(function FindLastIndexDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertEquals(-1, fixedLength.findLastIndex(CollectValuesAndDetach));
    assertEquals([6, 4, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertEquals(-1, fixedLengthWithOffset.findLastIndex(CollectValuesAndDetach));
    assertEquals([6, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertEquals(-1, lengthTracking.findLastIndex(CollectValuesAndDetach));
    assertEquals([6, 4, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertEquals(-1, lengthTrackingWithOffset.findLastIndex(CollectValuesAndDetach));
    assertEquals([6, undefined], values);
  }
})();

(function FilterShrinkMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == detachAfter) {
       %ArrayBufferDetach(rab);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    detachAfter = 2;
    assertEquals([], ToNumbers(fixedLength.filter(CollectValuesAndDetach)));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    detachAfter = 1;
    assertEquals([], ToNumbers(fixedLengthWithOffset.filter(CollectValuesAndDetach)));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    detachAfter = 2;
    assertEquals([], ToNumbers(lengthTracking.filter(CollectValuesAndDetach)));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    detachAfter = 1;
    assertEquals([], ToNumbers(lengthTrackingWithOffset.filter(CollectValuesAndDetach)));
    assertEquals([4, undefined], values);
  }
})();

(function ForEachReduceReduceRightDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetach(rab);
    }
    return true;
  }

  function ForEachHelper(array) {
    values = [];
    array.forEach(CollectValuesAndResize);
    return values;
  }

  function ReduceHelper(array) {
    values = [];
    array.reduce((acc, n) => { CollectValuesAndResize(n); }, "initial value");
    return values;
  }

  function ReduceRightHelper(array) {
    values = [];
    array.reduceRight((acc, n) => { CollectValuesAndResize(n); },
                      "initial value");
    return values;
  }

  // Test for forEach.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    detachAfter = 2;
    assertEquals([0, 2, undefined, undefined], ForEachHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    detachAfter = 1;
    assertEquals([4, undefined], ForEachHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    detachAfter = 2;
    assertEquals([0, 2, undefined, undefined], ForEachHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    detachAfter = 1;
    assertEquals([4, undefined], ForEachHelper(lengthTrackingWithOffset));
  }

  // Tests for reduce.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    detachAfter = 2;
    assertEquals([0, 2, undefined, undefined], ReduceHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    detachAfter = 1;
    assertEquals([4, undefined], ReduceHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    detachAfter = 2;
    assertEquals([0, 2, undefined, undefined], ReduceHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    detachAfter = 1;
    assertEquals([4, undefined], ReduceHelper(lengthTrackingWithOffset));
  }

  // Tests for reduceRight.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    detachAfter = 2;
    assertEquals([6, 4, undefined, undefined], ReduceRightHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    detachAfter = 1;
    assertEquals([6, undefined], ReduceRightHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    detachAfter = 2;
   assertEquals([6, 4, undefined, undefined], ReduceRightHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    detachAfter = 1;
    assertEquals([6, undefined], ReduceRightHelper(lengthTrackingWithOffset));
  }
})();

(function IncludesParameterConversionDetaches() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      %ArrayBufferDetach(rab);
      return 0;
    }};
    assertFalse(IncludesHelper(fixedLength, undefined));
    // The TA is detached so it includes only "undefined".
    assertTrue(IncludesHelper(fixedLength, undefined, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      %ArrayBufferDetach(rab);
      return 0;
    }};
    assertTrue(IncludesHelper(fixedLength, 0));
    // The TA is detached so it includes only "undefined".
    assertFalse(IncludesHelper(fixedLength, 0, evil));
  }
})();
