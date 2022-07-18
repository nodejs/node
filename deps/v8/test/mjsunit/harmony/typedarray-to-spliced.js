// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-change-array-by-copy --harmony-rab-gsab
// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/typedarray-helpers.js');

function AssertToSplicedResult(input, ...args) {
  // Use Array#splice to test against TypedArray#toSpliced.
  const origArrayCopy = [...input];
  const s = input.toSpliced.apply(input, args);
  let arraySpliced = [...input];
  Array.prototype.splice.apply(arraySpliced, args);

  // The in-place spliced version should be pairwise equal to the toSpliced,
  // modulo being an actual Array if the input is generic.
  assertEquals(new input.constructor(arraySpliced), s);

  // The original input should be unchanged.
  assertEquals(new input.constructor(origArrayCopy), input);

  // The result of toSpliced() is a copy.
  assertFalse(s === input);
}

function TestToSplicedHelper(input) {
  const itemsToInsert = ["5","6"];
  const startIndices = [0, 1, -1, -100, Infinity, -Infinity];
  const deleteCounts = [0, 1, 2, 3];

  AssertToSplicedResult(input);

  for (let startIndex of startIndices) {
    AssertToSplicedResult(input, startIndex);
    for (let deleteCount of deleteCounts) {
      AssertToSplicedResult(input, startIndex, deleteCount);
      AssertToSplicedResult(input, startIndex, deleteCount, ...itemsToInsert);
    }
  }
}

function ResetRABTA(ta) {
  ta.buffer.resize(4 * ta.constructor.BYTES_PER_ELEMENT);
  for (let i = 0; i < 4; i++) {
    ta[i] = i + "";
  }
}

(function TestSurface() {
  for (let TA of ctors) {
    assertEquals(2, TA.prototype.toSpliced.length);
    assertEquals("toSpliced", TA.prototype.toSpliced.name);
  }
})();

(function TestBasic() {
  for (let TA of ctors) {
    let a = new TA(4);
    for (let i = 0; i < 4; i++) {
      a[i] = i + "";
    }
    TestToSplicedHelper(a);
  }
})();

(function TestNonTypedArray() {
  for (let TA of ctors) {
    assertThrows(() => { TA.prototype.toSpliced.call([1,2,3,4]); }, TypeError);
  }
})();

(function TestBigIntConversion() {
  for (let TA of [BigInt64Array, BigUint64Array]) {
    const ta = new TA(4);
    assertThrows(() => { print(ta.toSpliced(0, 0, 42)); }, TypeError);
  }
})();

(function TestResizableBuffer() {
  for (let TA of ctors) {
    const rab = CreateResizableArrayBuffer(4 * TA.BYTES_PER_ELEMENT,
                                           8 * TA.BYTES_PER_ELEMENT);
    const fixedLength = new TA(rab, 0, 4);
    const fixedLengthWithOffset = new TA(rab, 2 * TA.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new TA(rab, 0);
    const lengthTrackingWithOffset = new TA(rab, 2 * TA.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new TA(rab);
    for (let i = 0; i < 4; i++) {
      taWrite[i] = i + "";
    }

    // Orig. array: [0, 1, 2, 3]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, ...] << lengthTracking
    //                    [2, 3, ...] << lengthTrackingWithOffset

    TestToSplicedHelper(fixedLength);
    TestToSplicedHelper(fixedLengthWithOffset);
    TestToSplicedHelper(lengthTracking);
    TestToSplicedHelper(lengthTrackingWithOffset);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * TA.BYTES_PER_ELEMENT);
    WriteToTypedArray(taWrite, 0, 0);

    assertThrows(() => { fixedLength.toSpliced(); }, TypeError);
    assertThrows(() => { fixedLengthWithOffset.toSpliced(); }, TypeError);
    TestToSplicedHelper(lengthTracking);
    assertThrows(() => { lengthTrackingWithOffset.toSpliced(); }, TypeError);

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.toSpliced(); }, TypeError);
    assertThrows(() => { fixedLengthWithOffset.toSpliced(); }, TypeError);
    TestToSplicedHelper(lengthTracking);
    assertThrows(() => { lengthTrackingWithOffset.toSpliced(); }, TypeError);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * TA.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3, 4, 5]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset

    TestToSplicedHelper(fixedLength);
    TestToSplicedHelper(fixedLengthWithOffset);
    TestToSplicedHelper(lengthTracking);
    TestToSplicedHelper(lengthTrackingWithOffset);
  }
})();

(function TestParameterConversionShrinks() {
  for (let TA of ctors) {
    const rab = CreateResizableArrayBuffer(4 * TA.BYTES_PER_ELEMENT,
                                           8 * TA.BYTES_PER_ELEMENT);
    const lengthTracking = new TA(rab);

    const undefinedThrows = (lengthTracking instanceof BigInt64Array ||
                             lengthTracking instanceof BigUint64Array);

    ResetRABTA(lengthTracking);
    {
      // First part indices 0-3; no second part
      // Reloaded length 2
      // First part length 4, partial fast copy of [0,1]
      // Second part length 0
      const evilThree = { valueOf: () => {
        rab.resize(2 * TA.BYTES_PER_ELEMENT);
        return 3;
      }};
      if (undefinedThrows) {
        assertThrows(() => { lengthTracking.toSpliced(evilThree); },
                     TypeError);
      } else {
        const s = lengthTracking.toSpliced(evilThree);
        assertEquals(new TA(["0","1",undefined]), s);
        assertFalse(s === lengthTracking);
      }
    }

    ResetRABTA(lengthTracking);
    {
      // No first part; second part indices 1-3
      // Reloaded length 2
      // First part length 0, degenerate case of entire fast copy
      // Second part length 1, partial fast copy of [1]
      const evilOne = { valueOf: () => {
        rab.resize(2 * TA.BYTES_PER_ELEMENT);
        return 1;
      }};
      if (undefinedThrows) {
        assertThrows(() => { lengthTracking.toSpliced(0, evilOne); },
                     TypeError);
      } else {
        const s = lengthTracking.toSpliced(0, evilOne);
        assertEquals(new TA(["1",undefined,undefined]), s);
        assertFalse(s === lengthTracking);
      }
    }

    ResetRABTA(lengthTracking);
    {
      // First part length 1; second part indices 2-3
      // Reloaded length 3
      // First part length 1, entire fast copy
      // Second part length 1, partial fast copy of [2]
      const evilFive = { valueOf: () => {
        rab.resize(3 * TA.BYTES_PER_ELEMENT);
        return "5";
      }};
      if (undefinedThrows) {
        assertThrows(() => { lengthTracking.toSpliced(1, 1, evilFive); },
                     TypeError);
      } else {
        const s = lengthTracking.toSpliced(1, 1, evilFive);
        assertEquals(new TA(["0","5","2",undefined]), s);
        assertFalse(s === lengthTracking);
      }
    }
  }
})();

(function TestParameterConversionGrows() {
  for (let TA of ctors) {
    const rab = CreateResizableArrayBuffer(4 * TA.BYTES_PER_ELEMENT,
                                           8 * TA.BYTES_PER_ELEMENT);
    const lengthTracking = new TA(rab);

    ResetRABTA(lengthTracking);
    {
      const evilThree = { valueOf: () => {
        rab.resize(6 * TA.BYTES_PER_ELEMENT);
        return 3;
      }};
      const s = lengthTracking.toSpliced(evilThree);
      assertEquals(new TA(["0","1","2"]), s);
      assertFalse(s === lengthTracking);
    }

    ResetRABTA(lengthTracking);
    {
      const evilOne = { valueOf: () => {
        rab.resize(6 * TA.BYTES_PER_ELEMENT);
        return 1;
      }};
      const s = lengthTracking.toSpliced(0, evilOne);
      assertEquals(new TA(["1","2","3"]), s);
      assertFalse(s === lengthTracking);
    }

    ResetRABTA(lengthTracking);
    {
      const evilFive = { valueOf: () => {
        rab.resize(6 * TA.BYTES_PER_ELEMENT);
        return "5";
      }};
      const s = lengthTracking.toSpliced(1, 1, evilFive);
      assertEquals(new TA(["0","5","2","3"]), s);
      assertFalse(s === lengthTracking);
    }
  }
})();

(function TestParameterConversionDetaches() {
  function CreateTA(TA) {
    let ta = new TA(4);
    for (let i = 0; i < 4; i++) {
      ta[i] = i + "";
    }
    return ta;
  }

  for (let TA of ctors) {
    let ta = CreateTA(TA);
    const undefinedThrows = (ta instanceof BigInt64Array ||
                             ta instanceof BigUint64Array);

    {
      const evilThree = { valueOf: () => {
        %ArrayBufferDetach(ta.buffer);
        return 3;
      }};
      if (undefinedThrows) {
        assertThrows(() => { ta.toSpliced(evilThree); },
                     TypeError);
      } else {
        const s = ta.toSpliced(evilThree);
        assertEquals(new TA([undefined,undefined,undefined]), s);
        assertFalse(s === ta);
      }
    }

    ta = CreateTA(TA);
    {
      const evilOne = { valueOf: () => {
        %ArrayBufferDetach(ta.buffer);
        return 1;
      }};
      if (undefinedThrows) {
        assertThrows(() => { ta.toSpliced(0, evilOne); },
                     TypeError);
      } else {
        const s = ta.toSpliced(0, evilOne);
        assertEquals(new TA([undefined,undefined,undefined]), s);
        assertFalse(s === ta);
      }
    }

    ta = CreateTA(TA);
    {
      const evilFive = { valueOf: () => {
        %ArrayBufferDetach(ta.buffer);
        return "5";
      }};
      if (undefinedThrows) {
        assertThrows(() => { ta.toSpliced(1, 1, evilFive); },
                     TypeError);
      } else {
        const s = ta.toSpliced(1, 1, evilFive);
        assertEquals(new TA([undefined,"5",undefined,undefined]), s);
        assertFalse(s === ta);
      }
    }
  }
})();

(function TestGrowableSAB() {
  for (let TA of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * TA.BYTES_PER_ELEMENT,
                                           8 * TA.BYTES_PER_ELEMENT);
    const fixedLength = new TA(gsab, 0, 4);
    const fixedLengthWithOffset = new TA(gsab, 2 * TA.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new TA(gsab, 0);
    const lengthTrackingWithOffset = new TA(gsab, 2 * TA.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new TA(gsab);
    for (let i = 0; i < 4; i++) {
      taWrite[i] = (2 * i) + "";
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset
    TestToSplicedHelper(fixedLength);
    TestToSplicedHelper(fixedLengthWithOffset);
    TestToSplicedHelper(lengthTracking);
    TestToSplicedHelper(lengthTrackingWithOffset);

    // Grow.
    gsab.grow(6 * TA.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset
    TestToSplicedHelper(fixedLength);
    TestToSplicedHelper(fixedLengthWithOffset);
    TestToSplicedHelper(lengthTracking);
    TestToSplicedHelper(lengthTrackingWithOffset);
  }
})();

(function TestDetached() {
  for (let TA of ctors) {
    let a = new TA(4);
    %ArrayBufferDetach(a.buffer);
    assertThrows(() => { a.toSpliced(); }, TypeError);
  }
})();

(function TestNoSpecies() {
  class MyUint8Array extends Uint8Array {
    static get [Symbol.species]() { return MyUint8Array; }
  }
  assertEquals(Uint8Array, (new MyUint8Array()).toSpliced().constructor);
})();
