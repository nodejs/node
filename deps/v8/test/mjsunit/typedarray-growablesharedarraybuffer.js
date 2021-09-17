// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax

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
    values.push(Number(value));
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
