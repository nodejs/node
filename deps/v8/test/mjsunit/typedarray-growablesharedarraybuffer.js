// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax
// Flags: --harmony-relative-indexing-methods --harmony-array-find-last

"use strict";

d8.file.execute('test/mjsunit/typedarray-helpers.js');

(function TypedArrayPrototype() {
  const gsab = CreateGrowableSharedArrayBuffer(40, 80);
  const sab = new SharedArrayBuffer(80);

  for (let ctor of ctors) {
    const ta_gsab = new ctor(gsab, 0, 3);
    const ta_sab = new ctor(sab, 0, 3);
    assertEquals(ta_gsab.__proto__, ta_sab.__proto__);
  }
})();

(function TypedArrayLengthAndByteLength() {
  const gsab = CreateGrowableSharedArrayBuffer(40, 80);

  for (let ctor of ctors) {
    const ta = new ctor(gsab, 0, 3);
    assertEquals(gsab, ta.buffer);
    assertEquals(3, ta.length);
    assertEquals(3 * ctor.BYTES_PER_ELEMENT, ta.byteLength);

    const empty_ta = new ctor(gsab, 0, 0);
    assertEquals(gsab, empty_ta.buffer);
    assertEquals(0, empty_ta.length);
    assertEquals(0, empty_ta.byteLength);

    const ta_with_offset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 3);
    assertEquals(gsab, ta_with_offset.buffer);
    assertEquals(3, ta_with_offset.length);
    assertEquals(3 * ctor.BYTES_PER_ELEMENT, ta_with_offset.byteLength);

    const empty_ta_with_offset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 0);
    assertEquals(gsab, empty_ta_with_offset.buffer);
    assertEquals(0, empty_ta_with_offset.length);
    assertEquals(0, empty_ta_with_offset.byteLength);

    const length_tracking_ta = new ctor(gsab);
    assertEquals(gsab, length_tracking_ta.buffer);
    assertEquals(40 / ctor.BYTES_PER_ELEMENT, length_tracking_ta.length);
    assertEquals(40, length_tracking_ta.byteLength);

    const offset = 8;
    const length_tracking_ta_with_offset = new ctor(gsab, offset);
    assertEquals(gsab, length_tracking_ta_with_offset.buffer);
    assertEquals((40 - offset) / ctor.BYTES_PER_ELEMENT,
                 length_tracking_ta_with_offset.length);
    assertEquals(40 - offset, length_tracking_ta_with_offset.byteLength);

    const length_tracking_ta_zero = new ctor(gsab, 40);
    assertEquals(gsab, length_tracking_ta_zero.buffer);
    assertEquals(0, length_tracking_ta_zero.length);
    assertEquals(0, length_tracking_ta_zero.byteLength);
  }
})();

(function ConstructInvalid() {
  const gsab = CreateGrowableSharedArrayBuffer(40, 80);

  for (let ctor of ctors) {
    // Length too big.
    assertThrows(() => { new ctor(gsab, 0, 40 / ctor.BYTES_PER_ELEMENT + 1); },
                 RangeError);

    // Offset too close to the end.
    assertThrows(() => { new ctor(gsab, 40 - ctor.BYTES_PER_ELEMENT, 2); },
                 RangeError);

    // Offset beyond end.
    assertThrows(() => { new ctor(gsab, 40, 1); }, RangeError);

    if (ctor.BYTES_PER_ELEMENT > 1) {
      // Offset not a multiple of ctor.BYTES_PER_ELEMENT.
      assertThrows(() => { new ctor(gsab, 1, 1); }, RangeError);
      assertThrows(() => { new ctor(gsab, 1); }, RangeError);
    }
  }

  // Verify the error messages.
  assertThrows(() => { new Int16Array(gsab, 1, 1); }, RangeError,
               /start offset of Int16Array should be a multiple of 2/);

  assertThrows(() => { new Int16Array(gsab, 38, 2); }, RangeError,
               /Invalid typed array length: 2/);
})();

(function ConstructFromTypedArray() {
  AllBigIntMatchedCtorCombinations((targetCtor, sourceCtor) => {
    const gsab = CreateGrowableSharedArrayBuffer(
        4 * sourceCtor.BYTES_PER_ELEMENT,
        8 * sourceCtor.BYTES_PER_ELEMENT);
    const fixedLength = new sourceCtor(gsab, 0, 4);
    const fixedLengthWithOffset = new sourceCtor(
        gsab, 2 * sourceCtor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new sourceCtor(gsab, 0);
    const lengthTrackingWithOffset = new sourceCtor(
        gsab, 2 * sourceCtor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taFull = new sourceCtor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taFull, i, i + 1);
    }

    // Orig. array: [1, 2, 3, 4]
    //              [1, 2, 3, 4] << fixedLength
    //                    [3, 4] << fixedLengthWithOffset
    //              [1, 2, 3, 4, ...] << lengthTracking
    //                    [3, 4, ...] << lengthTrackingWithOffset

    assertEquals([1, 2, 3, 4], ToNumbers(new targetCtor(fixedLength)));
    assertEquals([3, 4], ToNumbers(new targetCtor(fixedLengthWithOffset)));
    assertEquals([1, 2, 3, 4], ToNumbers(new targetCtor(lengthTracking)));
    assertEquals([3, 4], ToNumbers(new targetCtor(lengthTrackingWithOffset)));

    // Grow.
    gsab.grow(6 * sourceCtor.BYTES_PER_ELEMENT);

    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taFull, i, i + 1);
    }

    // Orig. array: [1, 2, 3, 4, 5, 6]
    //              [1, 2, 3, 4] << fixedLength
    //                    [3, 4] << fixedLengthWithOffset
    //              [1, 2, 3, 4, 5, 6, ...] << lengthTracking
    //                    [3, 4, 5, 6, ...] << lengthTrackingWithOffset

    assertEquals([1, 2, 3, 4], ToNumbers(new targetCtor(fixedLength)));
    assertEquals([3, 4], ToNumbers(new targetCtor(fixedLengthWithOffset)));
    assertEquals([1, 2, 3, 4, 5, 6],
                 ToNumbers(new targetCtor(lengthTracking)));
    assertEquals([3, 4, 5, 6],
                 ToNumbers(new targetCtor(lengthTrackingWithOffset)));
  });
})();

(function ConstructFromTypedArraySpeciesConstructorNotCalled() {
  class MySharedArrayBuffer extends SharedArrayBuffer {
    constructor(...params) {
      super(...params);
    }
    static get [Symbol.species]() {
      throw new Error('This should not be called!');
    }
  };

  AllBigIntMatchedCtorCombinations((targetCtor, sourceCtor) => {
    const gsab = new MySharedArrayBuffer(
      4 * sourceCtor.BYTES_PER_ELEMENT,
      {maxByteLength: 8 * sourceCtor.BYTES_PER_ELEMENT});
    // Write some data into the array.
    const taWrite = new sourceCtor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    const fixedLength = new sourceCtor(gsab, 0, 4);
    assertEquals([0, 2, 4, 6], ToNumbers(new targetCtor(fixedLength)));

    const fixedLengthWithOffset = new sourceCtor(
        gsab, 2 * sourceCtor.BYTES_PER_ELEMENT, 2);
    assertEquals([4, 6], ToNumbers(new targetCtor(fixedLengthWithOffset)));

    const lengthTracking = new sourceCtor(gsab, 0);
    assertEquals([0, 2, 4, 6], ToNumbers(new targetCtor(lengthTracking)));

    const lengthTrackingWithOffset = new sourceCtor(
      gsab, 2 * sourceCtor.BYTES_PER_ELEMENT);
    assertEquals([4, 6], ToNumbers(new targetCtor(lengthTrackingWithOffset)));
  });
})();

(function TypedArrayLengthWhenGrown1() {
  const gsab = CreateGrowableSharedArrayBuffer(16, 40);

  // Create TAs which cover the bytes 0-7.
  let tas_and_lengths = [];
  for (let ctor of ctors) {
    const length = 8 / ctor.BYTES_PER_ELEMENT;
    tas_and_lengths.push([new ctor(gsab, 0, length), length]);
  }

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  gsab.grow(20);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  gsab.grow(40);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }
})();

// The previous test with offsets.
(function TypedArrayLengthWhenGrown2() {
  const gsab = CreateGrowableSharedArrayBuffer(20, 40);

  // Create TAs which cover the bytes 8-15.
  let tas_and_lengths = [];
  for (let ctor of ctors) {
    const length = 8 / ctor.BYTES_PER_ELEMENT;
    tas_and_lengths.push([new ctor(gsab, 8, length), length]);
  }

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  gsab.grow(20);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  gsab.grow(40);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }
})();

(function LengthTracking1() {
  const gsab = CreateGrowableSharedArrayBuffer(16, 40);

  let tas = [];
  for (let ctor of ctors) {
    tas.push(new ctor(gsab));
  }

  for (let ta of tas) {
    assertEquals(16 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(16, ta.byteLength);
  }

  gsab.grow(24);
  for (let ta of tas) {
    assertEquals(24 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(24, ta.byteLength);
  }

  // Grow to a number which is not a multiple of all byte_lengths.
  gsab.grow(26);
  for (let ta of tas) {
    const expected_length = Math.floor(26 / ta.BYTES_PER_ELEMENT);
    assertEquals(expected_length, ta.length);
    assertEquals(expected_length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  gsab.grow(40);

  for (let ta of tas) {
    assertEquals(40 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(40, ta.byteLength);
  }
})();

// The previous test with offsets.
(function LengthTracking2() {
  const gsab = CreateGrowableSharedArrayBuffer(16, 40);

  const offset = 8;
  let tas = [];
  for (let ctor of ctors) {
    tas.push(new ctor(gsab, offset));
  }

  for (let ta of tas) {
    assertEquals((16 - offset) / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(16 - offset, ta.byteLength);
  }

  gsab.grow(24);
  for (let ta of tas) {
    assertEquals((24 - offset) / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(24 - offset, ta.byteLength);
  }

  // Grow to a number which is not a multiple of all byte_lengths.
  gsab.grow(26);
  for (let ta of tas) {
    const expected_length = Math.floor((26 - offset)/ ta.BYTES_PER_ELEMENT);
    assertEquals(expected_length, ta.length);
    assertEquals(expected_length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  gsab.grow(40);

  for (let ta of tas) {
    assertEquals((40 - offset) / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(40 - offset, ta.byteLength);
  }
})();

(function LoadWithFeedback() {
  function ReadElement2(ta) {
    return ta[2];
  }
  %EnsureFeedbackVectorForFunction(ReadElement2);

  const gsab = CreateGrowableSharedArrayBuffer(16, 40);

  const i8a = new Int8Array(gsab, 0, 4);
  for (let i = 0; i < 3; ++i) {
    assertEquals(0, ReadElement2(i8a));
  }

  // Within-bounds write
  for (let i = 0; i < 4; ++i) {
    i8a[i] = i;
  }

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertEquals(2, ReadElement2(i8a));
  }

  gsab.grow(20);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertEquals(2, ReadElement2(i8a));
  }

  gsab.grow(40);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertEquals(2, ReadElement2(i8a));
  }
})();

(function LoadAndStoreWithFeedback() {
  function ReadElement(ta, i) {
    return ta[i];
  }

  function HasElement(ta, i) {
    return i in ta;
  }

  function WriteElement(ta, i, v) {
    ta[i] = v;
  }

  %EnsureFeedbackVectorForFunction(ReadElement);
  %EnsureFeedbackVectorForFunction(HasElement);
  %EnsureFeedbackVectorForFunction(WriteElement);

  const gsab = CreateGrowableSharedArrayBuffer(16, 40);

  const i8a = new Int8Array(gsab); // length-tracking
  assertEquals(16, i8a.length);

  // Within-bounds read
  for (let i = 0; i < i8a.length; ++i) {
    assertEquals(0, ReadElement(i8a, i));
    assertTrue(HasElement(i8a, i));
  }
  assertFalse(HasElement(i8a, 17));

  // Within-bounds write
  for (let i = 0; i < i8a.length; ++i) {
    WriteElement(i8a, i, i);
  }

  // Within-bounds read
  for (let i = 0; i < i8a.length; ++i) {
    assertEquals(i, ReadElement(i8a, i));
  }

  let old_length = i8a.length;
  gsab.grow(20);
  assertEquals(20, i8a.length);

  for (let i = 0; i < i8a.length; ++i) {
    if (i < old_length) {
      assertEquals(i, ReadElement(i8a, i));
    } else {
      assertEquals(0, ReadElement(i8a, i));
    }
    assertTrue(HasElement(i8a, i));
  }
  assertFalse(HasElement(i8a, 21));

  // Within-bounds write
  for (let i = 0; i < i8a.length; ++i) {
    WriteElement(i8a, i, i + 1);
  }

  // Within-bounds read
  for (let i = 0; i < i8a.length; ++i) {
    assertEquals(i + 1, ReadElement(i8a, i));
  }
})();

(function HasWithOffsetsWithFeedback() {
  function GetElements(ta) {
    let result = '';
    for (let i = 0; i < 8; ++i) {
      result += (i in ta) + ',';
      //           ^ feedback will be here
    }
    return result;
  }
  %EnsureFeedbackVectorForFunction(GetElements);

  const gsab = CreateGrowableSharedArrayBuffer(4, 8);
  const fixedLength = new Int8Array(gsab, 0, 4);
  const fixedLengthWithOffset = new Int8Array(gsab, 1, 3);
  const lengthTracking = new Int8Array(gsab, 0);
  const lengthTrackingWithOffset = new Int8Array(gsab, 1);

  assertEquals('true,true,true,true,false,false,false,false,',
               GetElements(fixedLength));
  assertEquals('true,true,true,false,false,false,false,false,',
              GetElements(fixedLengthWithOffset));
  assertEquals('true,true,true,true,false,false,false,false,',
              GetElements(lengthTracking));
  assertEquals('true,true,true,false,false,false,false,false,',
              GetElements(lengthTrackingWithOffset));

  gsab.grow(8);

  assertEquals('true,true,true,true,false,false,false,false,',
               GetElements(fixedLength));
  assertEquals('true,true,true,false,false,false,false,false,',
               GetElements(fixedLengthWithOffset));
  assertEquals('true,true,true,true,true,true,true,true,',
               GetElements(lengthTracking));
  assertEquals('true,true,true,true,true,true,true,false,',
               GetElements(lengthTrackingWithOffset));
})();

(function EnumerateElements() {
  let gsab = CreateGrowableSharedArrayBuffer(100, 200);
  for (let ctor of ctors) {
    const ta = new ctor(gsab, 0, 3);
    let keys = '';
    for (const key in ta) {
      keys += key;
    }
    assertEquals('012', keys);
  }
}());

(function IterateTypedArray() {
  const no_elements = 10;
  const offset = 2;

  function TestIteration(ta, expected) {
    let values = [];
    for (const value of ta) {
      values.push(Number(value));
    }
    assertEquals(expected, values);
  }

  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    // We can use the same GSAB for all the TAs below, since we won't modify it
    // after writing the initial values.
    const gsab = CreateGrowableSharedArrayBuffer(buffer_byte_length,
                                                 2 * buffer_byte_length);
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Write some data into the array.
    let ta_write = new ctor(gsab);
    for (let i = 0; i < no_elements; ++i) {
      WriteToTypedArray(ta_write, i, i % 128);
    }

    // Create various different styles of TypedArrays with the GSAB as the
    // backing store and iterate them.
    const ta = new ctor(gsab, 0, 3);
    TestIteration(ta, [0, 1, 2]);

    const empty_ta = new ctor(gsab, 0, 0);
    TestIteration(empty_ta, []);

    const ta_with_offset = new ctor(gsab, byte_offset, 3);
    TestIteration(ta_with_offset, [2, 3, 4]);

    const empty_ta_with_offset = new ctor(gsab, byte_offset, 0);
    TestIteration(empty_ta_with_offset, []);

    const length_tracking_ta = new ctor(gsab);
    {
      let expected = [];
      for (let i = 0; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      TestIteration(length_tracking_ta, expected);
    }

    const length_tracking_ta_with_offset = new ctor(gsab, byte_offset);
    {
      let expected = [];
      for (let i = offset; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      TestIteration(length_tracking_ta_with_offset, expected);
    }

    const empty_length_tracking_ta_with_offset = new ctor(gsab, buffer_byte_length);
    TestIteration(empty_length_tracking_ta_with_offset, []);
  }
}());

// Helpers for iteration tests.
function CreateGsab(buffer_byte_length, ctor) {
  const gsab = CreateGrowableSharedArrayBuffer(buffer_byte_length,
                                               2 * buffer_byte_length);
  // Write some data into the array.
  let ta_write = new ctor(gsab);
  for (let i = 0; i < buffer_byte_length / ctor.BYTES_PER_ELEMENT; ++i) {
    WriteToTypedArray(ta_write, i, i % 128);
  }
  return gsab;
}

function TestIterationAndGrow(ta, expected, gsab, grow_after,
                              new_byte_length) {
  let values = [];
  let grown = false;
  for (const value of ta) {
    if (value instanceof Array) {
      // When iterating via entries(), the values will be arrays [key, value].
      values.push([value[0], Number(value[1])]);
    } else {
      values.push(Number(value));
    }
    if (!grown && values.length == grow_after) {
      gsab.grow(new_byte_length);
      grown = true;
    }
  }
  assertEquals(expected, values);
  assertTrue(grown);
}

(function IterateTypedArrayAndGrowMidIteration() {
  const no_elements = 10;
  const offset = 2;

  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Create various different styles of TypedArrays with the gsab as the
    // backing store and iterate them.

    // Fixed-length TAs aren't affected by resizing.
    let gsab = CreateGsab(buffer_byte_length, ctor);
    const ta = new ctor(gsab, 0, 3);
    TestIterationAndGrow(ta, [0, 1, 2], gsab, 2, buffer_byte_length * 2);

    gsab = CreateGsab(buffer_byte_length, ctor);
    const ta_with_offset = new ctor(gsab, byte_offset, 3);
    TestIterationAndGrow(ta_with_offset, [2, 3, 4], gsab, 2,
                         buffer_byte_length * 2);

    gsab = CreateGsab(buffer_byte_length, ctor);
    const length_tracking_ta = new ctor(gsab);
    {
      let expected = [];
      for (let i = 0; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      // After resizing, the new memory contains zeros.
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }

      TestIterationAndGrow(length_tracking_ta, expected, gsab, 2,
                           buffer_byte_length * 2);
    }

    gsab = CreateGsab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(gsab, byte_offset);
    {
      let expected = [];
      for (let i = offset; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }
      TestIterationAndGrow(length_tracking_ta_with_offset, expected, gsab, 2,
                           buffer_byte_length * 2);
    }
  }
}());

(function IterateTypedArrayAndGrowJustBeforeIterationWouldEnd() {
  const no_elements = 10;
  const offset = 2;

  // We need to recreate the gsab between all TA tests, since we grow it.
  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Create various different styles of TypedArrays with the gsab as the
    // backing store and iterate them.

    let gsab = CreateGsab(buffer_byte_length, ctor);
    const length_tracking_ta = new ctor(gsab);
    {
      let expected = [];
      for (let i = 0; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      // After resizing, the new memory contains zeros.
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }

      TestIterationAndGrow(length_tracking_ta, expected, gsab, no_elements,
                           buffer_byte_length * 2);
    }

    gsab = CreateGsab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(gsab, byte_offset);
    {
      let expected = [];
      for (let i = offset; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }
      TestIterationAndGrow(length_tracking_ta_with_offset, expected, gsab,
                           no_elements - offset, buffer_byte_length * 2);
    }
  }
}());

(function Destructuring() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    let ta_write = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(ta_write, i, i);
    }

    {
      let [a, b, c, d, e] = fixedLength;
      assertEquals([0, 1, 2, 3], ToNumbers([a, b, c, d]));
      assertEquals(undefined, e);
    }

    {
      let [a, b, c] = fixedLengthWithOffset;
      assertEquals([2, 3], ToNumbers([a, b]));
      assertEquals(undefined, c);
    }

    {
      let [a, b, c, d, e] = lengthTracking;
      assertEquals([0, 1, 2, 3], ToNumbers([a, b, c, d]));
      assertEquals(undefined, e);
    }

    {
      let [a, b, c] = lengthTrackingWithOffset;
      assertEquals([2, 3], ToNumbers([a, b]));
      assertEquals(undefined, c);
    }

    // Grow. The new memory is zeroed.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

    {
      let [a, b, c, d, e] = fixedLength;
      assertEquals([0, 1, 2, 3], ToNumbers([a, b, c, d]));
      assertEquals(undefined, e);
    }

    {
      let [a, b, c] = fixedLengthWithOffset;
      assertEquals([2, 3], ToNumbers([a, b]));
      assertEquals(undefined, c);
    }

    {
      let [a, b, c, d, e, f, g] = lengthTracking;
      assertEquals([0, 1, 2, 3, 0, 0], ToNumbers([a, b, c, d, e, f]));
      assertEquals(undefined, g);
    }

    {
      let [a, b, c, d, e] = lengthTrackingWithOffset;
      assertEquals([2, 3, 0, 0], ToNumbers([a, b, c, d]));
      assertEquals(undefined, e);
    }
  }
}());

(function TestFill() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    assertEquals([0, 0, 0, 0], ReadDataFromBuffer(gsab, ctor));

    FillHelper(fixedLength, 1);
    assertEquals([1, 1, 1, 1], ReadDataFromBuffer(gsab, ctor));

    FillHelper(fixedLengthWithOffset, 2);
    assertEquals([1, 1, 2, 2], ReadDataFromBuffer(gsab, ctor));

    FillHelper(lengthTracking, 3);
    assertEquals([3, 3, 3, 3], ReadDataFromBuffer(gsab, ctor));

    FillHelper(lengthTrackingWithOffset, 4);
    assertEquals([3, 3, 4, 4], ReadDataFromBuffer(gsab, ctor));

    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

    FillHelper(fixedLength, 13);
    assertEquals([13, 13, 13, 13, 0, 0], ReadDataFromBuffer(gsab, ctor));

    FillHelper(fixedLengthWithOffset, 14);
    assertEquals([13, 13, 14, 14, 0, 0], ReadDataFromBuffer(gsab, ctor));

    FillHelper(lengthTracking, 15);
    assertEquals([15, 15, 15, 15, 15, 15], ReadDataFromBuffer(gsab, ctor));

    FillHelper(lengthTrackingWithOffset, 16);
    assertEquals([15, 15, 16, 16, 16, 16], ReadDataFromBuffer(gsab, ctor));

    // Filling with non-undefined start & end.
    FillHelper(fixedLength, 17, 1, 3);
    assertEquals([15, 17, 17, 16, 16, 16], ReadDataFromBuffer(gsab, ctor));

    FillHelper(fixedLengthWithOffset, 18, 1, 2);
    assertEquals([15, 17, 17, 18, 16, 16], ReadDataFromBuffer(gsab, ctor));

    FillHelper(lengthTracking, 19, 1, 3);
    assertEquals([15, 19, 19, 18, 16, 16], ReadDataFromBuffer(gsab, ctor));

    FillHelper(lengthTrackingWithOffset, 20, 1, 2);
    assertEquals([15, 19, 19, 20, 16, 16], ReadDataFromBuffer(gsab, ctor));
  }
})();

(function At() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    let ta_write = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(ta_write, i, i);
    }

    assertEquals(3, AtHelper(fixedLength, -1));
    assertEquals(3, AtHelper(lengthTracking, -1));
    assertEquals(3, AtHelper(fixedLengthWithOffset, -1));
    assertEquals(3, AtHelper(lengthTrackingWithOffset, -1));

    // Grow. New memory is zeroed.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    assertEquals(3, AtHelper(fixedLength, -1));
    assertEquals(0, AtHelper(lengthTracking, -1));
    assertEquals(3, AtHelper(fixedLengthWithOffset, -1));
    assertEquals(0, AtHelper(lengthTrackingWithOffset, -1));
  }
})();

(function Slice() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    const fixedLengthSlice = fixedLength.slice();
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLengthSlice));
    assertTrue(fixedLengthSlice.buffer instanceof ArrayBuffer);
    assertFalse(fixedLengthSlice.buffer instanceof SharedArrayBuffer);
    assertFalse(fixedLengthSlice.buffer.resizable);

    const fixedLengthWithOffsetSlice = fixedLengthWithOffset.slice();
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffsetSlice));
    assertTrue(fixedLengthWithOffsetSlice.buffer instanceof ArrayBuffer);
    assertFalse(fixedLengthWithOffsetSlice.buffer instanceof SharedArrayBuffer);
    assertFalse(fixedLengthWithOffsetSlice.buffer.resizable);

    const lengthTrackingSlice = lengthTracking.slice();
    assertEquals([0, 1, 2, 3], ToNumbers(lengthTrackingSlice));
    assertTrue(lengthTrackingSlice.buffer instanceof ArrayBuffer);
    assertFalse(lengthTrackingSlice.buffer instanceof SharedArrayBuffer);
    assertFalse(lengthTrackingSlice.buffer.resizable);

    const lengthTrackingWithOffsetSlice = lengthTrackingWithOffset.slice();
    assertEquals([2, 3], ToNumbers(lengthTrackingWithOffsetSlice));
    assertTrue(lengthTrackingWithOffsetSlice.buffer instanceof ArrayBuffer);
    assertFalse(lengthTrackingWithOffsetSlice.buffer instanceof
        SharedArrayBuffer);
    assertFalse(lengthTrackingWithOffsetSlice.buffer.resizable);

    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLength.slice()));
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffset.slice()));
    assertEquals([0, 1, 2, 3, 0, 0], ToNumbers(lengthTracking.slice()));
    assertEquals([2, 3, 0, 0], ToNumbers(lengthTrackingWithOffset.slice()));

    // Verify that the previously created slices aren't affected by the growing.
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLengthSlice));
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffsetSlice));
    assertEquals([0, 1, 2, 3], ToNumbers(lengthTrackingSlice));
    assertEquals([2, 3], ToNumbers(lengthTrackingWithOffsetSlice));
  }
})();

(function SliceSpeciesCreateResizes() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 1);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const fixedLength = new MyArray(gsab, 0, 4);
    resizeWhenConstructorCalled = true;
    const a = fixedLength.slice();
    assertEquals(4, a.length);
    assertEquals([1, 1, 1, 1], ToNumbers(a));

    assertEquals(6 * ctor.BYTES_PER_ELEMENT, gsab.byteLength);
  }

  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 1);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const lengthTracking = new MyArray(gsab);
    resizeWhenConstructorCalled = true;
    const a = lengthTracking.slice();
    assertEquals(6 * ctor.BYTES_PER_ELEMENT, gsab.byteLength);
    // The length of the resulting TypedArray is determined before
    // TypedArraySpeciesCreate is called, and it doesn't change.
    assertEquals(4, a.length);
    assertEquals([1, 1, 1, 1], ToNumbers(a));
  }
})();

(function CopyWithin() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, ...] << lengthTracking
    //                    [2, 3, ...] << lengthTrackingWithOffset

    fixedLength.copyWithin(0, 2);
    assertEquals([2, 3, 2, 3], ToNumbers(fixedLength));

    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    fixedLengthWithOffset.copyWithin(0, 1);
    assertEquals([3, 3], ToNumbers(fixedLengthWithOffset));

    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    lengthTracking.copyWithin(0, 2);
    assertEquals([2, 3, 2, 3], ToNumbers(lengthTracking));

    lengthTrackingWithOffset.copyWithin(0, 1);
    assertEquals([3, 3], ToNumbers(lengthTrackingWithOffset));

    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3, 4, 5]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset

    fixedLength.copyWithin(0, 2);
    assertEquals([2, 3, 2, 3], ToNumbers(fixedLength));

    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    fixedLengthWithOffset.copyWithin(0, 1);
    assertEquals([3, 3], ToNumbers(fixedLengthWithOffset));

    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //        target ^     ^ start
    lengthTracking.copyWithin(0, 2);
    assertEquals([2, 3, 4, 5, 4, 5], ToNumbers(lengthTracking));

    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset
    //              target ^  ^ start
    lengthTrackingWithOffset.copyWithin(0, 1);
    assertEquals([3, 4, 5, 5], ToNumbers(lengthTrackingWithOffset));
  }
})();

(function CopyWithinParameterConversionGrows() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i);
    }

    const evil = { valueOf: () => { gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
                                    WriteToTypedArray(lengthTracking, 4, 4);
                                    WriteToTypedArray(lengthTracking, 5, 5);
                                    return 0;} };
    // Orig. array: [0, 1, 2, 3]  [4, 5]
    //               ^     ^       ^ new elements
    //          target     start
    lengthTracking.copyWithin(evil, 2);
    assertEquals([2, 3, 2, 3, 4, 5], ToNumbers(lengthTracking));
  }
})();

(function EntriesKeysValues() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    assertEquals([0, 2, 4, 6], ToNumbersWithEntries(fixedLength));
    assertEquals([0, 2, 4, 6], ValuesToNumbers(fixedLength));
    assertEquals([0, 1, 2, 3], Keys(fixedLength));

    assertEquals([4, 6], ToNumbersWithEntries(fixedLengthWithOffset));
    assertEquals([4, 6], ValuesToNumbers(fixedLengthWithOffset));
    assertEquals([0, 1], Keys(fixedLengthWithOffset));

    assertEquals([0, 2, 4, 6], ToNumbersWithEntries(lengthTracking));
    assertEquals([0, 2, 4, 6], ValuesToNumbers(lengthTracking));
    assertEquals([0, 1, 2, 3], Keys(lengthTracking));

    assertEquals([4, 6], ToNumbersWithEntries(lengthTrackingWithOffset));
    assertEquals([4, 6], ValuesToNumbers(lengthTrackingWithOffset));
    assertEquals([0, 1], Keys(lengthTrackingWithOffset));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertEquals([0, 2, 4, 6], ToNumbersWithEntries(fixedLength));
    assertEquals([0, 2, 4, 6], ValuesToNumbers(fixedLength));
    assertEquals([0, 1, 2, 3], Keys(fixedLength));

    assertEquals([4, 6], ToNumbersWithEntries(fixedLengthWithOffset));
    assertEquals([4, 6], ValuesToNumbers(fixedLengthWithOffset));
    assertEquals([0, 1], Keys(fixedLengthWithOffset));

    assertEquals([0, 2, 4, 6, 8, 10], ToNumbersWithEntries(lengthTracking));
    assertEquals([0, 2, 4, 6, 8, 10], ValuesToNumbers(lengthTracking));
    assertEquals([0, 1, 2, 3, 4, 5], Keys(lengthTracking));

    assertEquals([4, 6, 8, 10], ToNumbersWithEntries(lengthTrackingWithOffset));
    assertEquals([4, 6, 8, 10], ValuesToNumbers(lengthTrackingWithOffset));
    assertEquals([0, 1, 2, 3], Keys(lengthTrackingWithOffset));
  }
})();

(function EntriesKeysValuesGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  // Iterating with entries() (the 4 loops below).
  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);

    // The fixed length array is not affected by resizing.
    TestIterationAndGrow(fixedLength.entries(),
                         [[0, 0], [1, 2], [2, 4], [3, 6]],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array is not affected by resizing.
    TestIterationAndGrow(fixedLengthWithOffset.entries(),
                         [[0, 4], [1, 6]],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);

    TestIterationAndGrow(lengthTracking.entries(),
                         [[0, 0], [1, 2], [2, 4], [3, 6], [4, 0], [5, 0]],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndGrow(lengthTrackingWithOffset.entries(),
                         [[0, 4], [1, 6], [2, 0], [3, 0]],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  // Iterating with keys() (the 4 loops below).
  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);

    // The fixed length array is not affected by resizing.
    TestIterationAndGrow(fixedLength.keys(),
                         [0, 1, 2, 3],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array is not affected by resizing.
    TestIterationAndGrow(fixedLengthWithOffset.keys(),
                         [0, 1],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);

    TestIterationAndGrow(lengthTracking.keys(),
                         [0, 1, 2, 3, 4, 5],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndGrow(lengthTrackingWithOffset.keys(),
                         [0, 1, 2, 3],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  // Iterating with values() (the 4 loops below).
  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);

    // The fixed length array is not affected by resizing.
    TestIterationAndGrow(fixedLength.values(),
                         [0, 2, 4, 6],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array is not affected by resizing.
    TestIterationAndGrow(fixedLengthWithOffset.values(),
                         [4, 6],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);

    TestIterationAndGrow(lengthTracking.values(),
                         [0, 2, 4, 6, 0, 0],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndGrow(lengthTrackingWithOffset.values(),
                         [4, 6, 0, 0],
                         gsab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }
})();

(function EverySome() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    function div3(n) {
      return Number(n) % 3 == 0;
    }

    function even(n) {
      return Number(n) % 2 == 0;
    }

    function over10(n) {
      return Number(n) > 10;
    }

    assertFalse(fixedLength.every(div3));
    assertTrue(fixedLength.every(even));
    assertTrue(fixedLength.some(div3));
    assertFalse(fixedLength.some(over10));

    assertFalse(fixedLengthWithOffset.every(div3));
    assertTrue(fixedLengthWithOffset.every(even));
    assertTrue(fixedLengthWithOffset.some(div3));
    assertFalse(fixedLengthWithOffset.some(over10));

    assertFalse(lengthTracking.every(div3));
    assertTrue(lengthTracking.every(even));
    assertTrue(lengthTracking.some(div3));
    assertFalse(lengthTracking.some(over10));

    assertFalse(lengthTrackingWithOffset.every(div3));
    assertTrue(lengthTrackingWithOffset.every(even));
    assertTrue(lengthTrackingWithOffset.some(div3));
    assertFalse(lengthTrackingWithOffset.some(over10));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertFalse(fixedLength.every(div3));
    assertTrue(fixedLength.every(even));
    assertTrue(fixedLength.some(div3));
    assertFalse(fixedLength.some(over10));

    assertFalse(fixedLengthWithOffset.every(div3));
    assertTrue(fixedLengthWithOffset.every(even));
    assertTrue(fixedLengthWithOffset.some(div3));
    assertFalse(fixedLengthWithOffset.some(over10));

    assertFalse(lengthTracking.every(div3));
    assertTrue(lengthTracking.every(even));
    assertTrue(lengthTracking.some(div3));
    assertFalse(lengthTracking.some(over10));

    assertFalse(lengthTrackingWithOffset.every(div3));
    assertTrue(lengthTrackingWithOffset.every(even));
    assertTrue(lengthTrackingWithOffset.some(div3));
    assertFalse(lengthTrackingWithOffset.some(over10));
  }
})();

(function EveryGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return true;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(fixedLength.every(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(fixedLengthWithOffset.every(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(lengthTracking.every(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(lengthTrackingWithOffset.every(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }
})();

(function SomeGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(fixedLength.some(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    gsab = gsab;
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(fixedLengthWithOffset.some(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(lengthTracking.some(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(lengthTrackingWithOffset.some(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }
})();

(function FindFindIndexFindLastFindLastIndex() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    function isTwoOrFour(n) {
      return n == 2 || n == 4;
    }

    assertEquals(2, Number(fixedLength.find(isTwoOrFour)));
    assertEquals(4, Number(fixedLengthWithOffset.find(isTwoOrFour)));
    assertEquals(2, Number(lengthTracking.find(isTwoOrFour)));
    assertEquals(4, Number(lengthTrackingWithOffset.find(isTwoOrFour)));

    assertEquals(1, fixedLength.findIndex(isTwoOrFour));
    assertEquals(0, fixedLengthWithOffset.findIndex(isTwoOrFour));
    assertEquals(1, lengthTracking.findIndex(isTwoOrFour));
    assertEquals(0, lengthTrackingWithOffset.findIndex(isTwoOrFour));

    assertEquals(4, Number(fixedLength.findLast(isTwoOrFour)));
    assertEquals(4, Number(fixedLengthWithOffset.findLast(isTwoOrFour)));
    assertEquals(4, Number(lengthTracking.findLast(isTwoOrFour)));
    assertEquals(4, Number(lengthTrackingWithOffset.findLast(isTwoOrFour)));

    assertEquals(2, fixedLength.findLastIndex(isTwoOrFour));
    assertEquals(0, fixedLengthWithOffset.findLastIndex(isTwoOrFour));
    assertEquals(2, lengthTracking.findLastIndex(isTwoOrFour));
    assertEquals(0, lengthTrackingWithOffset.findLastIndex(isTwoOrFour));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 0);
    }
    WriteToTypedArray(taWrite, 4, 2);
    WriteToTypedArray(taWrite, 5, 4);

    // Orig. array: [0, 0, 0, 0, 2, 4]
    //              [0, 0, 0, 0] << fixedLength
    //                    [0, 0] << fixedLengthWithOffset
    //              [0, 0, 0, 0, 2, 4, ...] << lengthTracking
    //                    [0, 0, 2, 4, ...] << lengthTrackingWithOffset

    assertEquals(undefined, fixedLength.find(isTwoOrFour));
    assertEquals(undefined, fixedLengthWithOffset.find(isTwoOrFour));
    assertEquals(2, Number(lengthTracking.find(isTwoOrFour)));
    assertEquals(2, Number(lengthTrackingWithOffset.find(isTwoOrFour)));

    assertEquals(-1, fixedLength.findIndex(isTwoOrFour));
    assertEquals(-1, fixedLengthWithOffset.findIndex(isTwoOrFour));
    assertEquals(4, lengthTracking.findIndex(isTwoOrFour));
    assertEquals(2, lengthTrackingWithOffset.findIndex(isTwoOrFour));

    assertEquals(undefined, fixedLength.findLast(isTwoOrFour));
    assertEquals(undefined, fixedLengthWithOffset.findLast(isTwoOrFour));
    assertEquals(4, Number(lengthTracking.findLast(isTwoOrFour)));
    assertEquals(4, Number(lengthTrackingWithOffset.findLast(isTwoOrFour)));

    assertEquals(-1, fixedLength.findLastIndex(isTwoOrFour));
    assertEquals(-1, fixedLengthWithOffset.findLastIndex(isTwoOrFour));
    assertEquals(5, lengthTracking.findLastIndex(isTwoOrFour));
    assertEquals(3, lengthTrackingWithOffset.findLastIndex(isTwoOrFour));
  }
})();

(function FindGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLength.find(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLengthWithOffset.find(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTracking.find(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTrackingWithOffset.find(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }
})();

(function FindIndexGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLength.findIndex(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLengthWithOffset.findIndex(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findIndex(CollectValuesAndGrow));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTrackingWithOffset.findIndex(CollectValuesAndGrow));
    assertEquals([4, 6], values);
  }
})();

(function FindLastGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLength.findLast(CollectValuesAndGrow));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLengthWithOffset.findLast(CollectValuesAndGrow));
    assertEquals([6, 4], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTracking.findLast(CollectValuesAndGrow));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTrackingWithOffset.findLast(CollectValuesAndGrow));
    assertEquals([6, 4], values);
  }
})();

(function FindLastIndexGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLength.findLastIndex(CollectValuesAndGrow));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLengthWithOffset.findLastIndex(CollectValuesAndGrow));
    assertEquals([6, 4], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findLastIndex(CollectValuesAndGrow));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTrackingWithOffset.findLastIndex(CollectValuesAndGrow));
    assertEquals([6, 4], values);
  }
})();

(function Filter() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, ...] << lengthTracking
    //                    [2, 3, ...] << lengthTrackingWithOffset

    function isEven(n) {
      return n != undefined && Number(n) % 2 == 0;
    }

    assertEquals([0, 2], ToNumbers(fixedLength.filter(isEven)));
    assertEquals([2], ToNumbers(fixedLengthWithOffset.filter(isEven)));
    assertEquals([0, 2], ToNumbers(lengthTracking.filter(isEven)));
    assertEquals([2], ToNumbers(lengthTrackingWithOffset.filter(isEven)));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3, 4, 5]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset

    assertEquals([0, 2], ToNumbers(fixedLength.filter(isEven)));
    assertEquals([2], ToNumbers(fixedLengthWithOffset.filter(isEven)));
    assertEquals([0, 2, 4], ToNumbers(lengthTracking.filter(isEven)));
    assertEquals([2, 4], ToNumbers(lengthTrackingWithOffset.filter(isEven)));
  }
})();

(function FilterGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndGrow(n) {
    if (n == undefined) {
      values.push(n);
    } else {
      values.push(Number(n));
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(fixedLength.filter(CollectValuesAndGrow)));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(fixedLengthWithOffset.filter(CollectValuesAndGrow)));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    values = [];
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(lengthTracking.filter(CollectValuesAndGrow)));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(lengthTrackingWithOffset.filter(CollectValuesAndGrow)));
    assertEquals([4, 6], values);
  }
})();

(function ForEachReduceReduceRight() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    function Helper(array) {
      const forEachValues = [];
      const reduceValues = [];
      const reduceRightValues = [];

      array.forEach((n) => { forEachValues.push(n);});

      array.reduce((acc, n) => {
        reduceValues.push(n);
      }, "initial value");

      array.reduceRight((acc, n) => {
        reduceRightValues.push(n);
      }, "initial value");

      assertEquals(reduceValues, forEachValues);
      reduceRightValues.reverse();
      assertEquals(reduceValues, reduceRightValues);
      return ToNumbers(forEachValues);
    }

    assertEquals([0, 2, 4, 6], Helper(fixedLength));
    assertEquals([4, 6], Helper(fixedLengthWithOffset));
    assertEquals([0, 2, 4, 6], Helper(lengthTracking));
    assertEquals([4, 6], Helper(lengthTrackingWithOffset));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertEquals([0, 2, 4, 6], Helper(fixedLength));
    assertEquals([4, 6], Helper(fixedLengthWithOffset));
    assertEquals([0, 2, 4, 6, 8, 10], Helper(lengthTracking));
    assertEquals([4, 6, 8, 10], Helper(lengthTrackingWithOffset));
  }
})();

(function ForEachReduceReduceRightGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
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
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ForEachHelper(fixedLength));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ForEachHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ForEachHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ForEachHelper(lengthTrackingWithOffset));
  }

  // Test for reduce.

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ReduceHelper(fixedLength));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ReduceHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ReduceHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ReduceHelper(lengthTrackingWithOffset));
  }

  // Test for reduceRight.

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4, 2, 0], ReduceRightHelper(fixedLength));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4], ReduceRightHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4, 2, 0], ReduceRightHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4], ReduceRightHelper(lengthTrackingWithOffset));
  }
})();

(function Includes() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    assertTrue(IncludesHelper(fixedLength, 2));
    assertFalse(IncludesHelper(fixedLength, undefined));
    assertTrue(IncludesHelper(fixedLength, 2, 1));
    assertFalse(IncludesHelper(fixedLength, 2, 2));
    assertTrue(IncludesHelper(fixedLength, 2, -3));
    assertFalse(IncludesHelper(fixedLength, 2, -2));

    assertFalse(IncludesHelper(fixedLengthWithOffset, 2));
    assertTrue(IncludesHelper(fixedLengthWithOffset, 4));
    assertFalse(IncludesHelper(fixedLengthWithOffset, undefined));
    assertTrue(IncludesHelper(fixedLengthWithOffset, 4, 0));
    assertFalse(IncludesHelper(fixedLengthWithOffset, 4, 1));
    assertTrue(IncludesHelper(fixedLengthWithOffset, 4, -2));
    assertFalse(IncludesHelper(fixedLengthWithOffset, 4, -1));

    assertTrue(IncludesHelper(lengthTracking, 2));
    assertFalse(IncludesHelper(lengthTracking, undefined));
    assertTrue(IncludesHelper(lengthTracking, 2, 1));
    assertFalse(IncludesHelper(lengthTracking, 2, 2));
    assertTrue(IncludesHelper(lengthTracking, 2, -3));
    assertFalse(IncludesHelper(lengthTracking, 2, -2));

    assertFalse(IncludesHelper(lengthTrackingWithOffset, 2));
    assertTrue(IncludesHelper(lengthTrackingWithOffset, 4));
    assertFalse(IncludesHelper(lengthTrackingWithOffset, undefined));
    assertTrue(IncludesHelper(lengthTrackingWithOffset, 4, 0));
    assertFalse(IncludesHelper(lengthTrackingWithOffset, 4, 1));
    assertTrue(IncludesHelper(lengthTrackingWithOffset, 4, -2));
    assertFalse(IncludesHelper(lengthTrackingWithOffset, 4, -1));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertTrue(IncludesHelper(fixedLength, 2));
    assertFalse(IncludesHelper(fixedLength, undefined));
    assertFalse(IncludesHelper(fixedLength, 8));

    assertFalse(IncludesHelper(fixedLengthWithOffset, 2));
    assertTrue(IncludesHelper(fixedLengthWithOffset, 4));
    assertFalse(IncludesHelper(fixedLengthWithOffset, undefined));
    assertFalse(IncludesHelper(fixedLengthWithOffset, 8));

    assertTrue(IncludesHelper(lengthTracking, 2));
    assertFalse(IncludesHelper(lengthTracking, undefined));
    assertTrue(IncludesHelper(lengthTracking, 8));

    assertFalse(IncludesHelper(lengthTrackingWithOffset, 2));
    assertTrue(IncludesHelper(lengthTrackingWithOffset, 4));
    assertFalse(IncludesHelper(lengthTrackingWithOffset, undefined));
    assertTrue(IncludesHelper(lengthTrackingWithOffset, 8));
  }
})();

(function IncludesParameterConversionGrows() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, 1);
    }

    let evil = { valueOf: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertFalse(IncludesHelper(lengthTracking, 0));
    // The TA grew but we only look at the data until the original length.
    assertFalse(IncludesHelper(lengthTracking, 0, evil));
  }

  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    WriteToTypedArray(lengthTracking, 0, 1);

    let evil = { valueOf: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return -4;
    }};
    assertTrue(IncludesHelper(lengthTracking, 1, -4));
    // The TA grew but the start index conversion is done based on the original
    // length.
    assertTrue(IncludesHelper(lengthTracking, 1, evil));
  }
})();

(function IncludesSpecialValues() {
  for (let ctor of floatCtors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    lengthTracking[0] = -Infinity;
    lengthTracking[1] = Infinity;
    lengthTracking[2] = NaN;
    assertTrue(lengthTracking.includes(-Infinity));
    assertTrue(lengthTracking.includes(Infinity));
    assertTrue(lengthTracking.includes(NaN));
  }
})();

(function IndexOfLastIndexOf() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, Math.floor(i / 2));
    }

    // Orig. array: [0, 0, 1, 1]
    //              [0, 0, 1, 1] << fixedLength
    //                    [1, 1] << fixedLengthWithOffset
    //              [0, 0, 1, 1, ...] << lengthTracking
    //                    [1, 1, ...] << lengthTrackingWithOffset

    assertEquals(0, IndexOfHelper(fixedLength, 0));
    assertEquals(1, IndexOfHelper(fixedLength, 0, 1));
    assertEquals(-1, IndexOfHelper(fixedLength, 0, 2));
    assertEquals(-1, IndexOfHelper(fixedLength, 0, -2));
    assertEquals(1, IndexOfHelper(fixedLength, 0, -3));
    assertEquals(2, IndexOfHelper(fixedLength, 1, 1));
    assertEquals(2, IndexOfHelper(fixedLength, 1, -3));
    assertEquals(2, IndexOfHelper(fixedLength, 1, -2));
    assertEquals(-1, IndexOfHelper(fixedLength, undefined));

    assertEquals(1, LastIndexOfHelper(fixedLength, 0));
    assertEquals(1,  LastIndexOfHelper(fixedLength, 0, 1));
    assertEquals(1,  LastIndexOfHelper(fixedLength, 0, 2));
    assertEquals(1,  LastIndexOfHelper(fixedLength, 0, -2));
    assertEquals(1,  LastIndexOfHelper(fixedLength, 0, -3));
    assertEquals(-1,  LastIndexOfHelper(fixedLength, 1, 1));
    assertEquals(2,  LastIndexOfHelper(fixedLength, 1, -2));
    assertEquals(-1,  LastIndexOfHelper(fixedLength, 1, -3));
    assertEquals(-1,  LastIndexOfHelper(fixedLength, undefined));

    assertEquals(-1, IndexOfHelper(fixedLengthWithOffset, 0));
    assertEquals(0, IndexOfHelper(fixedLengthWithOffset, 1));
    assertEquals(0, IndexOfHelper(fixedLengthWithOffset, 1, -2));
    assertEquals(1, IndexOfHelper(fixedLengthWithOffset, 1, -1));
    assertEquals(-1, IndexOfHelper(fixedLengthWithOffset, undefined));

    assertEquals(-1, LastIndexOfHelper(fixedLengthWithOffset, 0));
    assertEquals(1, LastIndexOfHelper(fixedLengthWithOffset, 1));
    assertEquals(0, LastIndexOfHelper(fixedLengthWithOffset, 1, -2));
    assertEquals(1, LastIndexOfHelper(fixedLengthWithOffset, 1, -1));
    assertEquals(-1, LastIndexOfHelper(fixedLengthWithOffset, undefined));

    assertEquals(0, IndexOfHelper(lengthTracking, 0));
    assertEquals(-1, IndexOfHelper(lengthTracking, 0, 2));
    assertEquals(2, IndexOfHelper(lengthTracking, 1, -3));
    assertEquals(-1, IndexOfHelper(lengthTracking, undefined));

    assertEquals(1, LastIndexOfHelper(lengthTracking, 0));
    assertEquals(1, LastIndexOfHelper(lengthTracking, 0, 2));
    assertEquals(1, LastIndexOfHelper(lengthTracking, 0, -3));
    assertEquals(-1, LastIndexOfHelper(lengthTracking, 1, 1));
    assertEquals(2, LastIndexOfHelper(lengthTracking, 1, 2));
    assertEquals(-1, LastIndexOfHelper(lengthTracking, 1, -3));
    assertEquals(-1, LastIndexOfHelper(lengthTracking, undefined));

    assertEquals(-1, IndexOfHelper(lengthTrackingWithOffset, 0));
    assertEquals(0, IndexOfHelper(lengthTrackingWithOffset, 1));
    assertEquals(1, IndexOfHelper(lengthTrackingWithOffset, 1, 1));
    assertEquals(0, IndexOfHelper(lengthTrackingWithOffset, 1, -2));
    assertEquals(-1, IndexOfHelper(lengthTrackingWithOffset, undefined));

    assertEquals(-1, LastIndexOfHelper(lengthTrackingWithOffset, 0));
    assertEquals(1, LastIndexOfHelper(lengthTrackingWithOffset, 1));
    assertEquals(1, LastIndexOfHelper(lengthTrackingWithOffset, 1, 1));
    assertEquals(0, LastIndexOfHelper(lengthTrackingWithOffset, 1, -2));
    assertEquals(1, LastIndexOfHelper(lengthTrackingWithOffset, 1, -1));
    assertEquals(-1, LastIndexOfHelper(lengthTrackingWithOffset, undefined));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, Math.floor(i / 2));
    }

    // Orig. array: [0, 0, 1, 1, 2, 2]
    //              [0, 0, 1, 1] << fixedLength
    //                    [1, 1] << fixedLengthWithOffset
    //              [0, 0, 1, 1, 2, 2, ...] << lengthTracking
    //                    [1, 1, 2, 2, ...] << lengthTrackingWithOffset

    assertEquals(2, IndexOfHelper(fixedLength, 1));
    assertEquals(-1, IndexOfHelper(fixedLength, 2));
    assertEquals(-1, IndexOfHelper(fixedLength, undefined));

    assertEquals(3, LastIndexOfHelper(fixedLength, 1));
    assertEquals(-1, LastIndexOfHelper(fixedLength, 2));
    assertEquals(-1, LastIndexOfHelper(fixedLength, undefined));

    assertEquals(-1, IndexOfHelper(fixedLengthWithOffset, 0));
    assertEquals(0, IndexOfHelper(fixedLengthWithOffset, 1));
    assertEquals(-1, IndexOfHelper(fixedLengthWithOffset, 2));
    assertEquals(-1, IndexOfHelper(fixedLengthWithOffset, undefined));

    assertEquals(-1, LastIndexOfHelper(fixedLengthWithOffset, 0));
    assertEquals(1, LastIndexOfHelper(fixedLengthWithOffset, 1));
    assertEquals(-1, LastIndexOfHelper(fixedLengthWithOffset, 2));
    assertEquals(-1, LastIndexOfHelper(fixedLengthWithOffset, undefined));

    assertEquals(2, IndexOfHelper(lengthTracking, 1));
    assertEquals(4, IndexOfHelper(lengthTracking, 2));
    assertEquals(-1, IndexOfHelper(lengthTracking, undefined));

    assertEquals(3, LastIndexOfHelper(lengthTracking, 1));
    assertEquals(5, LastIndexOfHelper(lengthTracking, 2));
    assertEquals(-1, LastIndexOfHelper(lengthTracking, undefined));

    assertEquals(-1, IndexOfHelper(lengthTrackingWithOffset, 0));
    assertEquals(0, IndexOfHelper(lengthTrackingWithOffset, 1));
    assertEquals(2, IndexOfHelper(lengthTrackingWithOffset, 2));
    assertEquals(-1, IndexOfHelper(lengthTrackingWithOffset, undefined));

    assertEquals(-1, LastIndexOfHelper(lengthTrackingWithOffset, 0));
    assertEquals(1, LastIndexOfHelper(lengthTrackingWithOffset, 1));
    assertEquals(3, LastIndexOfHelper(lengthTrackingWithOffset, 2));
    assertEquals(-1, LastIndexOfHelper(lengthTrackingWithOffset, undefined));
  }
})();

(function IndexOfParameterConversionGrows() {
  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, 1);
    }

    let evil = { valueOf: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertEquals(-1, IndexOfHelper(lengthTracking, 0));
    // The TA grew but we only look at the data until the original length.
    assertEquals(-1, IndexOfHelper(lengthTracking, 0, evil));
  }

  // Growing + length-tracking TA, index conversion.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    WriteToTypedArray(lengthTracking, 0, 1);

    let evil = { valueOf: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return -4;
    }};
    assertEquals(0, IndexOfHelper(lengthTracking, 1, -4));
    // The TA grew but the start index conversion is done based on the original
    // length.
    assertEquals(0, IndexOfHelper(lengthTracking, 1, evil));
  }
})();

(function LastIndexOfParameterConversionGrows() {
  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, 1);
    }

    let evil = { valueOf: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return -1;
    }};
    assertEquals(-1, LastIndexOfHelper(lengthTracking, 0));
    // Because lastIndexOf iterates from the given index downwards, it's not
    // possible to test that "we only look at the data until the original
    // length" without also testing that the index conversion happening with the
    // original length.
    assertEquals(-1, LastIndexOfHelper(lengthTracking, 0, evil));
  }

  // Growing + length-tracking TA, index conversion.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);

    let evil = { valueOf: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return -4;
    }};
    assertEquals(0, LastIndexOfHelper(lengthTracking, 0, -4));
    // The TA grew but the start index conversion is done based on the original
    // length.
    assertEquals(0, LastIndexOfHelper(lengthTracking, 0, evil));
  }
})();

(function IndexOfLastIndexOfSpecialValues() {
  for (let ctor of floatCtors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);
    lengthTracking[0] = -Infinity;
    lengthTracking[1] = -Infinity;
    lengthTracking[2] = Infinity;
    lengthTracking[3] = Infinity;
    lengthTracking[4] = NaN;
    lengthTracking[5] = NaN;
    assertEquals(0, lengthTracking.indexOf(-Infinity));
    assertEquals(1, lengthTracking.lastIndexOf(-Infinity));
    assertEquals(2, lengthTracking.indexOf(Infinity));
    assertEquals(3, lengthTracking.lastIndexOf(Infinity));
    // NaN is never found.
    assertEquals(-1, lengthTracking.indexOf(NaN));
    assertEquals(-1, lengthTracking.lastIndexOf(NaN));
  }
})();

(function JoinToLocaleString() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    assertEquals('0,2,4,6', fixedLength.join());
    assertEquals('0,2,4,6', fixedLength.toLocaleString());
    assertEquals('4,6', fixedLengthWithOffset.join());
    assertEquals('4,6', fixedLengthWithOffset.toLocaleString());
    assertEquals('0,2,4,6', lengthTracking.join());
    assertEquals('0,2,4,6', lengthTracking.toLocaleString());
    assertEquals('4,6', lengthTrackingWithOffset.join());
    assertEquals('4,6', lengthTrackingWithOffset.toLocaleString());

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertEquals('0,2,4,6', fixedLength.join());
    assertEquals('0,2,4,6', fixedLength.toLocaleString());
    assertEquals('4,6', fixedLengthWithOffset.join());
    assertEquals('4,6', fixedLengthWithOffset.toLocaleString());
    assertEquals('0,2,4,6,8,10', lengthTracking.join());
    assertEquals('0,2,4,6,8,10', lengthTracking.toLocaleString());
    assertEquals('4,6,8,10', lengthTrackingWithOffset.join());
    assertEquals('4,6,8,10', lengthTrackingWithOffset.toLocaleString());
 }
})();

(function JoinParameterConversionGrows() {
  // Growing + fixed-length TA.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);

    let evil = { toString: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return '.';
    }};
    assertEquals('0.0.0.0', fixedLength.join(evil));
  }

  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);

    let evil = { toString: () => {
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      return '.';
    }};
    // We iterate 4 elements, since it was the starting length.
    assertEquals('0.0.0.0', lengthTracking.join(evil));
  }
})();

(function ToLocaleStringNumberPrototypeToLocaleStringGrows() {
  const oldNumberPrototypeToLocaleString = Number.prototype.toLocaleString;
  const oldBigIntPrototypeToLocaleString = BigInt.prototype.toLocaleString;

  // Growing + fixed-length TA.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);

    let growAfter = 2;
    Number.prototype.toLocaleString = function() {
      --growAfter;
      if (growAfter == 0) {
        gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    }
    BigInt.prototype.toLocaleString = function() {
      --growAfter;
      if (growAfter == 0) {
        gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    }

    // We iterate 4 elements since it was the starting length. Resizing doesn't
    // affect the TA.
    assertEquals('0,0,0,0', fixedLength.toLocaleString());
  }

  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab);

    let growAfter = 2;
    Number.prototype.toLocaleString = function() {
      --growAfter;
      if (growAfter == 0) {
        gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    }
    BigInt.prototype.toLocaleString = function() {
      --growAfter;
      if (growAfter == 0) {
        gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    }

    // We iterate 4 elements since it was the starting length.
    assertEquals('0,0,0,0', lengthTracking.toLocaleString());
  }

  Number.prototype.toLocaleString = oldNumberPrototypeToLocaleString;
  BigInt.prototype.toLocaleString = oldBigIntPrototypeToLocaleString;
})();

(function TestMap() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    function Helper(array) {
      const values = [];
      function GatherValues(n, ix) {
        assertEquals(values.length, ix);
        values.push(n);
        if (typeof n == 'bigint') {
          return n + 1n;
        }
        return n + 1;
      }
      const newValues = array.map(GatherValues);
      for (let i = 0; i < values.length; ++i) {
        if (typeof values[i] == 'bigint') {
          assertEquals(newValues[i], values[i] + 1n);
        } else {
          assertEquals(newValues[i], values[i] + 1);
        }
      }
      return ToNumbers(values);
    }

    assertEquals([0, 2, 4, 6], Helper(fixedLength));
    assertEquals([4, 6], Helper(fixedLengthWithOffset));
    assertEquals([0, 2, 4, 6], Helper(lengthTracking));
    assertEquals([4, 6], Helper(lengthTrackingWithOffset));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertEquals([0, 2, 4, 6], Helper(fixedLength));
    assertEquals([4, 6], Helper(fixedLengthWithOffset));
    assertEquals([0, 2, 4, 6, 8, 10], Helper(lengthTracking));
    assertEquals([4, 6, 8, 10], Helper(lengthTrackingWithOffset));
  }
})();

(function MapGrowMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let values;
  let gsab;
  let growAfter;
  let growTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == growAfter) {
      gsab.grow(growTo);
    }
    return n;
  }

  function Helper(array) {
    values = [];
    array.map(CollectValuesAndResize);
    return values;
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], Helper(fixedLength));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], Helper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    growAfter = 2;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], Helper(lengthTracking));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    growAfter = 1;
    growTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], Helper(lengthTrackingWithOffset));
  }
})();

(function MapSpeciesCreateGrows() {
  let values;
  let gsab;
  function CollectValues(n, ix, ta) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    // We still need to return a valid BigInt / non-BigInt, even if
    // n is `undefined`.
    if (IsBigIntTypedArray(ta)) {
      return 0n;
    }
    return 0;
  }

  function Helper(array) {
    values = [];
    array.map(CollectValues);
    return values;
  }

  for (let ctor of ctors) {
    gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const fixedLength = new MyArray(gsab, 0, 4);
    resizeWhenConstructorCalled = true;
    assertEquals([0, 1, 2, 3], Helper(fixedLength));
    assertEquals(6 * ctor.BYTES_PER_ELEMENT, gsab.byteLength);
  }

  for (let ctor of ctors) {
    gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const lengthTracking = new MyArray(gsab);
    resizeWhenConstructorCalled = true;
    assertEquals([0, 1, 2, 3], Helper(lengthTracking));
    assertEquals(6 * ctor.BYTES_PER_ELEMENT, gsab.byteLength);
  }
})();

(function Reverse() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    const wholeArrayView = new ctor(gsab);
    function WriteData() {
      // Write some data into the array.
      for (let i = 0; i < wholeArrayView.length; ++i) {
        WriteToTypedArray(wholeArrayView, i, 2 * i);
      }
    }
    WriteData();

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    fixedLength.reverse();
    assertEquals([6, 4, 2, 0], ToNumbers(wholeArrayView));
    fixedLengthWithOffset.reverse();
    assertEquals([6, 4, 0, 2], ToNumbers(wholeArrayView));
    lengthTracking.reverse();
    assertEquals([2, 0, 4, 6], ToNumbers(wholeArrayView));
    lengthTrackingWithOffset.reverse();
    assertEquals([2, 0, 6, 4], ToNumbers(wholeArrayView));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    WriteData();

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    fixedLength.reverse();
    assertEquals([6, 4, 2, 0, 8, 10], ToNumbers(wholeArrayView));
    fixedLengthWithOffset.reverse();
    assertEquals([6, 4, 0, 2, 8, 10], ToNumbers(wholeArrayView));
    lengthTracking.reverse();
    assertEquals([10, 8, 2, 0, 4, 6], ToNumbers(wholeArrayView));
    lengthTrackingWithOffset.reverse();
    assertEquals([10, 8, 6, 4, 0, 2], ToNumbers(wholeArrayView));
  }
})();

(function SetWithGrowableTarget() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taFull = new ctor(gsab);

    // Orig. array: [0, 0, 0, 0]
    //              [0, 0, 0, 0] << fixedLength
    //                    [0, 0] << fixedLengthWithOffset
    //              [0, 0, 0, 0, ...] << lengthTracking
    //                    [0, 0, ...] << lengthTrackingWithOffset

    SetHelper(fixedLength, [1, 2]);
    assertEquals([1, 2, 0, 0], ToNumbers(taFull));
    SetHelper(fixedLength, [3, 4], 1);
    assertEquals([1, 3, 4, 0], ToNumbers(taFull));
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0, 0])}, RangeError);
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0], 1)}, RangeError);
    assertEquals([1, 3, 4, 0], ToNumbers(taFull));

    SetHelper(fixedLengthWithOffset, [5, 6]);
    assertEquals([1, 3, 5, 6], ToNumbers(taFull));
    SetHelper(fixedLengthWithOffset, [7], 1);
    assertEquals([1, 3, 5, 7], ToNumbers(taFull));
    assertThrows(() => { SetHelper(fixedLengthWithOffset, [0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(fixedLengthWithOffset, [0, 0], 1)},
                 RangeError);
    assertEquals([1, 3, 5, 7], ToNumbers(taFull));

    SetHelper(lengthTracking, [8, 9]);
    assertEquals([8, 9, 5, 7], ToNumbers(taFull));
    SetHelper(lengthTracking, [10, 11], 1);
    assertEquals([8, 10, 11, 7], ToNumbers(taFull));
    assertThrows(() => { SetHelper(lengthTracking, [0, 0, 0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(lengthTracking, [0, 0, 0, 0], 1)},
                 RangeError);
    assertEquals([8, 10, 11, 7], ToNumbers(taFull));

    SetHelper(lengthTrackingWithOffset, [12, 13]);
    assertEquals([8, 10, 12, 13], ToNumbers(taFull));
    SetHelper(lengthTrackingWithOffset, [14], 1);
    assertEquals([8, 10, 12, 14], ToNumbers(taFull));
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0], 1)},
                 RangeError);
    assertEquals([8, 10, 12, 14], ToNumbers(taFull));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [8, 10, 12, 14, 0, 0]
    //              [8, 10, 12, 14] << fixedLength
    //                     [12, 14] << fixedLengthWithOffset
    //              [8, 10, 12, 14, 0, 0, ...] << lengthTracking
    //                     [12, 14, 0, 0, ...] << lengthTrackingWithOffset
    SetHelper(fixedLength, [21, 22]);
    assertEquals([21, 22, 12, 14, 0, 0], ToNumbers(taFull));
    SetHelper(fixedLength, [23, 24], 1);
    assertEquals([21, 23, 24, 14, 0, 0], ToNumbers(taFull));
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0, 0])}, RangeError);
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0], 1)}, RangeError);
    assertEquals([21, 23, 24, 14, 0, 0], ToNumbers(taFull));

    SetHelper(fixedLengthWithOffset, [25, 26]);
    assertEquals([21, 23, 25, 26, 0, 0], ToNumbers(taFull));
    SetHelper(fixedLengthWithOffset, [27], 1);
    assertEquals([21, 23, 25, 27, 0, 0], ToNumbers(taFull));
    assertThrows(() => { SetHelper(fixedLengthWithOffset, [0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(fixedLengthWithOffset, [0, 0], 1)},
                 RangeError);
    assertEquals([21, 23, 25, 27, 0, 0], ToNumbers(taFull));

    SetHelper(lengthTracking, [28, 29, 30, 31, 32, 33]);
    assertEquals([28, 29, 30, 31, 32, 33], ToNumbers(taFull));
    SetHelper(lengthTracking, [34, 35, 36, 37, 38], 1);
    assertEquals([28, 34, 35, 36, 37, 38], ToNumbers(taFull));
    assertThrows(() => { SetHelper(lengthTracking, [0, 0, 0, 0, 0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(lengthTracking, [0, 0, 0, 0, 0, 0], 1)},
                 RangeError);
    assertEquals([28, 34, 35, 36, 37, 38], ToNumbers(taFull));

    SetHelper(lengthTrackingWithOffset, [39, 40, 41, 42]);
    assertEquals([28, 34, 39, 40, 41, 42], ToNumbers(taFull));
    SetHelper(lengthTrackingWithOffset, [43, 44, 45], 1);
    assertEquals([28, 34, 39, 43, 44, 45], ToNumbers(taFull));
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0, 0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0, 0, 0], 1)},
                 RangeError);
    assertEquals([28, 34, 39, 43, 44, 45], ToNumbers(taFull));
  }
})();

(function SetSourceLengthGetterGrowsTarget() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let gsab;
  let growTo;
  function CreateSourceProxy(length) {
    return new Proxy({}, {
      get(target, prop, receiver) {
        if (prop == 'length') {
          gsab.grow(growTo);
          return length;
        }
        return true; // Can be converted to both BigInt and Number.
      }
    });
  }

  // Test that we still throw for lengthTracking TAs if the source length is
  // too large, even though we resized in the length getter (we check against
  // the original length).
  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    assertThrows(() => { lengthTracking.set(CreateSourceProxy(6)); },
                 RangeError);
    assertEquals([0, 2, 4, 6, 0, 0], ToNumbers(new ctor(gsab)));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    assertThrows(() => { lengthTrackingWithOffset.set(CreateSourceProxy(6)); },
                 RangeError);
    assertEquals([0, 2, 4, 6, 0, 0], ToNumbers(new ctor(gsab)));
  }
})();

(function SetGrowTargetMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  let gsab;
  // Growing will happen when we're calling Get for the `growAt`:th data
  // element, but we haven't yet written it to the target.
  let growAt;
  let growTo;
  function CreateSourceProxy(length) {
    let requestedIndices = [];
    return new Proxy({}, {
      get(target, prop, receiver) {
        if (prop == 'length') {
          return length;
        }
        requestedIndices.push(prop);
        if (requestedIndices.length == growAt) {
          gsab.grow(growTo);
        }
        return true; // Can be converted to both BigInt and Number.
      }
    });
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);
    growAt = 2;
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    fixedLength.set(CreateSourceProxy(4));
    assertEquals([1, 1, 1, 1], ToNumbers(fixedLength));
    assertEquals([1, 1, 1, 1, 0, 0], ToNumbers(new ctor(gsab)));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    growAt = 1;
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    fixedLengthWithOffset.set(CreateSourceProxy(2));
    assertEquals([1, 1], ToNumbers(fixedLengthWithOffset));
    assertEquals([0, 2, 1, 1, 0, 0], ToNumbers(new ctor(gsab)));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);
    growAt = 2;
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    lengthTracking.set(CreateSourceProxy(2));
    assertEquals([1, 1, 4, 6, 0, 0], ToNumbers(lengthTracking));
    assertEquals([1, 1, 4, 6, 0, 0], ToNumbers(new ctor(gsab)));
  }

  for (let ctor of ctors) {
    gsab = CreateGsabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);
    growAt = 1;
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(2));
    assertEquals([1, 1, 0, 0], ToNumbers(lengthTrackingWithOffset));
    assertEquals([0, 2, 1, 1, 0, 0], ToNumbers(new ctor(gsab)));
  }
})();

(function SetWithGrowableSource() {
  for (let targetIsGrowable of [false, true]) {
    for (let targetCtor of ctors) {
      for (let sourceCtor of ctors) {
        const gsab = CreateGrowableSharedArrayBuffer(
            4 * sourceCtor.BYTES_PER_ELEMENT,
            8 * sourceCtor.BYTES_PER_ELEMENT);
        const fixedLength = new sourceCtor(gsab, 0, 4);
        const fixedLengthWithOffset = new sourceCtor(
            gsab, 2 * sourceCtor.BYTES_PER_ELEMENT, 2);
        const lengthTracking = new sourceCtor(gsab, 0);
        const lengthTrackingWithOffset = new sourceCtor(
            gsab, 2 * sourceCtor.BYTES_PER_ELEMENT);

        // Write some data into the array.
        const taFull = new sourceCtor(gsab);
        for (let i = 0; i < 4; ++i) {
          WriteToTypedArray(taFull, i, i + 1);
        }

        // Orig. array: [1, 2, 3, 4]
        //              [1, 2, 3, 4] << fixedLength
        //                    [3, 4] << fixedLengthWithOffset
        //              [1, 2, 3, 4, ...] << lengthTracking
        //                    [3, 4, ...] << lengthTrackingWithOffset

        const targetAb = targetIsGrowable ?
          new ArrayBuffer(6 * targetCtor.BYTES_PER_ELEMENT) :
          new ArrayBuffer(6 * targetCtor.BYTES_PER_ELEMENT,
                         {maxByteLength: 8 * targetCtor.BYTES_PER_ELEMENT});
        const target = new targetCtor(targetAb);

        if (IsBigIntTypedArray(target) != IsBigIntTypedArray(taFull)) {
          // Can't mix BigInt and non-BigInt types.
          continue;
        }

        SetHelper(target, fixedLength);
        assertEquals([1, 2, 3, 4, 0, 0], ToNumbers(target));

        SetHelper(target, fixedLengthWithOffset);
        assertEquals([3, 4, 3, 4, 0, 0], ToNumbers(target));

        SetHelper(target, lengthTracking, 1);
        assertEquals([3, 1, 2, 3, 4, 0], ToNumbers(target));

        SetHelper(target, lengthTrackingWithOffset, 1);
        assertEquals([3, 3, 4, 3, 4, 0], ToNumbers(target));

        // Grow.
        gsab.grow(6 * sourceCtor.BYTES_PER_ELEMENT);

        for (let i = 0; i < 6; ++i) {
          WriteToTypedArray(taFull, i, i + 1);
        }

        // Orig. array: [1, 2, 3, 4, 5, 6]
        //              [1, 2, 3, 4] << fixedLength
        //                    [3, 4] << fixedLengthWithOffset
        //              [1, 2, 3, 4, 5, 6, ...] << lengthTracking
        //                    [3, 4, 5, 6, ...] << lengthTrackingWithOffset

        SetHelper(target, fixedLength);
        assertEquals([1, 2, 3, 4, 4, 0], ToNumbers(target));

        SetHelper(target, fixedLengthWithOffset);
        assertEquals([3, 4, 3, 4, 4, 0], ToNumbers(target));

        SetHelper(target, lengthTracking, 0);
        assertEquals([1, 2, 3, 4, 5, 6], ToNumbers(target));

        SetHelper(target, lengthTrackingWithOffset, 1);
        assertEquals([1, 3, 4, 5, 6, 6], ToNumbers(target));
      }
    }
  }
})();

(function Subarray() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    const fixedLengthSubFull = fixedLength.subarray(0);
    assertEquals([0, 2, 4, 6], ToNumbers(fixedLengthSubFull));
    const fixedLengthWithOffsetSubFull = fixedLengthWithOffset.subarray(0);
    assertEquals([4, 6], ToNumbers(fixedLengthWithOffsetSubFull));
    const lengthTrackingSubFull = lengthTracking.subarray(0);
    assertEquals([0, 2, 4, 6], ToNumbers(lengthTrackingSubFull));
    const lengthTrackingWithOffsetSubFull =
        lengthTrackingWithOffset.subarray(0);
    assertEquals([4, 6], ToNumbers(lengthTrackingWithOffsetSubFull));

    // Relative offsets
    assertEquals([4, 6], ToNumbers(fixedLength.subarray(-2)));
    assertEquals([6], ToNumbers(fixedLengthWithOffset.subarray(-1)));
    assertEquals([4, 6], ToNumbers(lengthTracking.subarray(-2)));
    assertEquals([6], ToNumbers(lengthTrackingWithOffset.subarray(-1)));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assertEquals([0, 2, 4, 6], ToNumbers(fixedLength.subarray(0)));
    assertEquals([4, 6], ToNumbers(fixedLengthWithOffset.subarray(0)));
    assertEquals([0, 2, 4, 6, 8, 10], ToNumbers(lengthTracking.subarray(0)));
    assertEquals([4, 6, 8, 10],
                 ToNumbers(lengthTrackingWithOffset.subarray(0)));

    assertEquals(4, fixedLengthSubFull.length);
    assertEquals(2, fixedLengthWithOffsetSubFull.length);

    // TODO(v8:11111): Are subarrays of length-tracking TAs also
    // length-tracking? See
    // https://github.com/tc39/proposal-resizablearraybuffer/issues/91
    assertEquals(4, lengthTrackingSubFull.length);
    assertEquals(2, lengthTrackingWithOffsetSubFull.length);
  }
})();

(function SubarrayParameterConversionGrows() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //              [0, 2, 4, 6, ...] << lengthTracking
  function CreateGsabForTest(ctor) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    // Write some data into the array.
    const taWrite = new ctor(gsab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }
    return gsab;
  }

  // Growing + fixed-length TA. Growing won't affect anything.
  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const fixedLength = new ctor(gsab, 0, 4);

    const evil = { valueOf: () => { gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
                                    return 0;}};
    assertEquals([0, 2, 4, 6], ToNumbers(fixedLength.subarray(evil)));
  }

  // Growing + length-tracking TA. The length computation is done with the
  // original length.
  for (let ctor of ctors) {
    const gsab = CreateGsabForTest(ctor);
    const lengthTracking = new ctor(gsab, 0);

    const evil = { valueOf: () => { gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
                                    return 0;}};

    assertEquals([0, 2, 4, 6], ToNumbers(lengthTracking.subarray(evil)));
  }
})();

(function SortWithDefaultComparison() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    const taFull = new ctor(gsab, 0);
    function WriteUnsortedData() {
      // Write some data into the array.
      for (let i = 0; i < taFull.length; ++i) {
        WriteToTypedArray(taFull, i, 10 - 2 * i);
      }
    }
    // Orig. array: [10, 8, 6, 4]
    //              [10, 8, 6, 4] << fixedLength
    //                     [6, 4] << fixedLengthWithOffset
    //              [10, 8, 6, 4, ...] << lengthTracking
    //                     [6, 4, ...] << lengthTrackingWithOffset

    WriteUnsortedData();
    fixedLength.sort();
    assertEquals([4, 6, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    fixedLengthWithOffset.sort();
    assertEquals([10, 8, 4, 6], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([4, 6, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTrackingWithOffset.sort();
    assertEquals([10, 8, 4, 6], ToNumbers(taFull));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [10, 8, 6, 4, 2, 0]
    //              [10, 8, 6, 4] << fixedLength
    //                     [6, 4] << fixedLengthWithOffset
    //              [10, 8, 6, 4, 2, 0, ...] << lengthTracking
    //                     [6, 4, 2, 0, ...] << lengthTrackingWithOffset

    WriteUnsortedData();
    fixedLength.sort();
    assertEquals([4, 6, 8, 10, 2, 0], ToNumbers(taFull));

    WriteUnsortedData();
    fixedLengthWithOffset.sort();
    assertEquals([10, 8, 4, 6, 2, 0], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([0, 2, 4, 6, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTrackingWithOffset.sort();
    assertEquals([10, 8, 0, 2, 4, 6], ToNumbers(taFull));
  }
})();

(function SortWithCustomComparison() {
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(gsab, 2 * ctor.BYTES_PER_ELEMENT);

    const taFull = new ctor(gsab, 0);
    function WriteUnsortedData() {
      // Write some data into the array.
      for (let i = 0; i < taFull.length; ++i) {
        WriteToTypedArray(taFull, i, 10 - i);
      }
    }
    function CustomComparison(a, b) {
      // Sort all odd numbers before even numbers.
      a = Number(a);
      b = Number(b);
      if (a % 2 == 1 && b % 2 == 0) {
        return -1;
      }
      if (a % 2 == 0 && b % 2 == 1) {
        return 1;
      }
      if (a < b) {
        return -1;
      }
      if (a > b) {
        return 1;
      }
      return 0;
    }

    // Orig. array: [10, 9, 8, 7]
    //              [10, 9, 8, 7] << fixedLength
    //                     [8, 7] << fixedLengthWithOffset
    //              [10, 9, 8, 7, ...] << lengthTracking
    //                     [8, 7, ...] << lengthTrackingWithOffset

    WriteUnsortedData();
    fixedLength.sort(CustomComparison);
    assertEquals([7, 9, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    fixedLengthWithOffset.sort(CustomComparison);
    assertEquals([10, 9, 7, 8], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTracking.sort(CustomComparison);
    assertEquals([7, 9, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTrackingWithOffset.sort(CustomComparison);
    assertEquals([10, 9, 7, 8], ToNumbers(taFull));

    // Grow.
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [10, 9, 8, 7, 6, 5]
    //              [10, 9, 8, 7] << fixedLength
    //                     [8, 7] << fixedLengthWithOffset
    //              [10, 9, 8, 7, 6, 5, ...] << lengthTracking
    //                     [8, 7, 6, 5, ...] << lengthTrackingWithOffset

    WriteUnsortedData();
    fixedLength.sort(CustomComparison);
    assertEquals([7, 9, 8, 10, 6, 5], ToNumbers(taFull));

    WriteUnsortedData();
    fixedLengthWithOffset.sort(CustomComparison);
    assertEquals([10, 9, 7, 8, 6, 5], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTracking.sort(CustomComparison);
    assertEquals([5, 7, 9, 6, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTrackingWithOffset.sort(CustomComparison);
    assertEquals([10, 9, 5, 7, 6, 8], ToNumbers(taFull));
  }
})();

(function SortCallbackGrows() {
  function WriteUnsortedData(taFull) {
    for (let i = 0; i < taFull.length; ++i) {
      WriteToTypedArray(taFull, i, 10 - i);
    }
  }

  let gsab;
  let growTo;
  function CustomComparison(a, b) {
    gsab.grow(growTo);
    if (a < b) {
      return -1;
    }
    if (a > b) {
      return 1;
    }
    return 0;
  }

  // Fixed length TA.
  for (let ctor of ctors) {
    gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    const fixedLength = new ctor(gsab, 0, 4);
    const taFull = new ctor(gsab, 0);
    WriteUnsortedData(taFull);

    fixedLength.sort(CustomComparison);

    // Growing doesn't affect the sorting.
    assertEquals([7, 8, 9, 10, 0, 0], ToNumbers(taFull));
  }

  // Length-tracking TA.
  for (let ctor of ctors) {
    gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    growTo = 6 * ctor.BYTES_PER_ELEMENT;
    const lengthTracking = new ctor(gsab, 0);
    const taFull = new ctor(gsab, 0);
    WriteUnsortedData(taFull);

    lengthTracking.sort(CustomComparison);

    // Growing doesn't affect the sorting. Only the elements that were part of
    // the original TA are sorted.
    assertEquals([7, 8, 9, 10, 0, 0], ToNumbers(taFull));
  }
})();

(function ObjectDefinePropertyDefineProperties() {
  for (let helper of
      [ObjectDefinePropertyHelper, ObjectDefinePropertiesHelper]) {
    for (let ctor of ctors) {
      const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                   8 * ctor.BYTES_PER_ELEMENT);
      const fixedLength = new ctor(gsab, 0, 4);
      const fixedLengthWithOffset = new ctor(
          gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
      const lengthTracking = new ctor(gsab, 0);
      const lengthTrackingWithOffset = new ctor(
          gsab, 2 * ctor.BYTES_PER_ELEMENT);
      const taFull = new ctor(gsab, 0);

      // Orig. array: [0, 0, 0, 0]
      //              [0, 0, 0, 0] << fixedLength
      //                    [0, 0] << fixedLengthWithOffset
      //              [0, 0, 0, 0, ...] << lengthTracking
      //                    [0, 0, ...] << lengthTrackingWithOffset

      helper(fixedLength, 0, 1);
      assertEquals([1, 0, 0, 0], ToNumbers(taFull));
      helper(fixedLengthWithOffset, 0, 2);
      assertEquals([1, 0, 2, 0], ToNumbers(taFull));
      helper(lengthTracking, 1, 3);
      assertEquals([1, 3, 2, 0], ToNumbers(taFull));
      helper(lengthTrackingWithOffset, 1, 4);
      assertEquals([1, 3, 2, 4], ToNumbers(taFull));

      assertThrows(() => { helper(fixedLength, 4, 8); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 2, 8); }, TypeError);
      assertThrows(() => { helper(lengthTracking, 4, 8); }, TypeError);
      assertThrows(() => { helper(lengthTrackingWithOffset, 2, 8); },
                   TypeError);

      // Grow.
      gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

      helper(fixedLength, 0, 9);
      assertEquals([9, 3, 2, 4, 0, 0], ToNumbers(taFull));
      helper(fixedLengthWithOffset, 0, 10);
      assertEquals([9, 3, 10, 4, 0, 0], ToNumbers(taFull));
      helper(lengthTracking, 1, 11);
      assertEquals([9, 11, 10, 4, 0, 0], ToNumbers(taFull));
      helper(lengthTrackingWithOffset, 2, 12);
      assertEquals([9, 11, 10, 4, 12, 0], ToNumbers(taFull));

      // Trying to define properties out of the fixed-length bounds throws.
      assertThrows(() => { helper(fixedLength, 5, 13); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 3, 13); }, TypeError);
      assertEquals([9, 11, 10, 4, 12, 0], ToNumbers(taFull));

      helper(lengthTracking, 4, 14);
      assertEquals([9, 11, 10, 4, 14, 0], ToNumbers(taFull));
      helper(lengthTrackingWithOffset, 3, 15);
      assertEquals([9, 11, 10, 4, 14, 15], ToNumbers(taFull));

      assertThrows(() => { helper(fixedLength, 6, 8); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 4, 8); }, TypeError);
      assertThrows(() => { helper(lengthTracking, 6, 8); }, TypeError);
      assertThrows(() => { helper(lengthTrackingWithOffset, 4, 8); },
                   TypeError);

    }
  }
})();

(function ObjectDefinePropertyParameterConversionGrows() {
  const helper = ObjectDefinePropertyHelper;
  // Length tracking.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab, 0);
    const evil = {toString: () => {
        gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
        return 4;  // Index valid after resize.
    }};
    helper(lengthTracking, evil, 8);
    assertEquals([0, 0, 0, 0, 8, 0], ToNumbers(lengthTracking));
  }
})();

(function ObjectFreeze() {
  // Freezing non-OOB non-zero-length TAs throws.
  for (let ctor of ctors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 4);
    const fixedLengthWithOffset = new ctor(
        gsab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(gsab, 0);
    const lengthTrackingWithOffset = new ctor(
        gsab, 2 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { Object.freeze(fixedLength); }, TypeError);
    assertThrows(() => { Object.freeze(fixedLengthWithOffset); }, TypeError);
    assertThrows(() => { Object.freeze(lengthTracking); }, TypeError);
    assertThrows(() => { Object.freeze(lengthTrackingWithOffset); }, TypeError);
  }
  // Freezing zero-length TAs doesn't throw.
  for (let ctor of ctors) {
    const gsab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(gsab, 0, 0);
    const fixedLengthWithOffset = new ctor(
        gsab, 2 * ctor.BYTES_PER_ELEMENT, 0);
    // Zero-length because the offset is at the end:
    const lengthTrackingWithOffset = new ctor(
        gsab, 4 * ctor.BYTES_PER_ELEMENT);

    Object.freeze(fixedLength);
    Object.freeze(fixedLengthWithOffset);
    Object.freeze(lengthTrackingWithOffset);
  }
})();
