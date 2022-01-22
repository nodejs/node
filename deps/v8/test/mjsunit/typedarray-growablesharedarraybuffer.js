// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax
// Flags: --harmony-relative-indexing-methods --harmony-array-find-last

"use strict";

d8.file.execute('test/mjsunit/typedarray-helpers.js');

function CreateGrowableSharedArrayBuffer(byteLength, maxByteLength) {
  return new SharedArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

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
  const floatCtors = [
    Float32Array,
    Float64Array,
    MyFloat32Array
  ];
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
