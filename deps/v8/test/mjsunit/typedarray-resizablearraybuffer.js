// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax
// Flags: --harmony-relative-indexing-methods --harmony-array-find-last

"use strict";

d8.file.execute('test/mjsunit/typedarray-helpers.js');

(function TypedArrayPrototype() {
  const rab = CreateResizableArrayBuffer(40, 80);
  const ab = new ArrayBuffer(80);

  for (let ctor of ctors) {
    const ta_rab = new ctor(rab, 0, 3);
    const ta_ab = new ctor(ab, 0, 3);
    assertEquals(ta_rab.__proto__, ta_ab.__proto__);
  }
})();

(function TypedArrayLengthAndByteLength() {
  const rab = CreateResizableArrayBuffer(40, 80);

  for (let ctor of ctors) {
    const ta = new ctor(rab, 0, 3);
    assertEquals(rab, ta.buffer);
    assertEquals(3, ta.length);
    assertEquals(3 * ctor.BYTES_PER_ELEMENT, ta.byteLength);

    const empty_ta = new ctor(rab, 0, 0);
    assertEquals(rab, empty_ta.buffer);
    assertEquals(0, empty_ta.length);
    assertEquals(0, empty_ta.byteLength);

    const ta_with_offset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 3);
    assertEquals(rab, ta_with_offset.buffer);
    assertEquals(3, ta_with_offset.length);
    assertEquals(3 * ctor.BYTES_PER_ELEMENT, ta_with_offset.byteLength);

    const empty_ta_with_offset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 0);
    assertEquals(rab, empty_ta_with_offset.buffer);
    assertEquals(0, empty_ta_with_offset.length);
    assertEquals(0, empty_ta_with_offset.byteLength);

    const length_tracking_ta = new ctor(rab);
    assertEquals(rab, length_tracking_ta.buffer);
    assertEquals(40 / ctor.BYTES_PER_ELEMENT, length_tracking_ta.length);
    assertEquals(40, length_tracking_ta.byteLength);

    const offset = 8;
    const length_tracking_ta_with_offset = new ctor(rab, offset);
    assertEquals(rab, length_tracking_ta_with_offset.buffer);
    assertEquals((40 - offset) / ctor.BYTES_PER_ELEMENT,
                 length_tracking_ta_with_offset.length);
    assertEquals(40 - offset, length_tracking_ta_with_offset.byteLength);

    const empty_length_tracking_ta_with_offset = new ctor(rab, 40);
    assertEquals(rab, empty_length_tracking_ta_with_offset.buffer);
    assertEquals(0, empty_length_tracking_ta_with_offset.length);
    assertEquals(0, empty_length_tracking_ta_with_offset.byteLength);
  }
})();

(function ConstructInvalid() {
  const rab = CreateResizableArrayBuffer(40, 80);

  for (let ctor of ctors) {
    // Length too big.
    assertThrows(() => { new ctor(rab, 0, 40 / ctor.BYTES_PER_ELEMENT + 1); },
                 RangeError);

    // Offset too close to the end.
    assertThrows(() => { new ctor(rab, 40 - ctor.BYTES_PER_ELEMENT, 2); },
                 RangeError);

    // Offset beyond end.
    assertThrows(() => { new ctor(rab, 40, 1); }, RangeError);

    if (ctor.BYTES_PER_ELEMENT > 1) {
      // Offset not a multiple of the byte size.
      assertThrows(() => { new ctor(rab, 1, 1); }, RangeError);
      assertThrows(() => { new ctor(rab, 1); }, RangeError);
    }
  }

  // Verify the error messages.
  assertThrows(() => { new Int16Array(rab, 1, 1); }, RangeError,
               /start offset of Int16Array should be a multiple of 2/);

  assertThrows(() => { new Int16Array(rab, 38, 2); }, RangeError,
               /Invalid typed array length: 2/);
})();

(function TypedArrayLengthWhenResizedOutOfBounds1() {
  const rab = CreateResizableArrayBuffer(16, 40);

  // Create TAs which cover the bytes 0-7.
  let tas_and_lengths = [];
  for (let ctor of ctors) {
    const length = 8 / ctor.BYTES_PER_ELEMENT;
    tas_and_lengths.push([new ctor(rab, 0, length), length]);
  }

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  rab.resize(2);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
  }

  // Resize the rab so that it just barely covers the needed 8 bytes.
  rab.resize(8);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  rab.resize(40);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }
})();

// The previous test with offsets.
(function TypedArrayLengthWhenResizedOutOfBounds2() {
  const rab = CreateResizableArrayBuffer(20, 40);

  // Create TAs which cover the bytes 8-15.
  let tas_and_lengths = [];
  for (let ctor of ctors) {
    const length = 8 / ctor.BYTES_PER_ELEMENT;
    tas_and_lengths.push([new ctor(rab, 8, length), length]);
  }

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
    assertEquals(8, ta.byteOffset);
  }

  rab.resize(10);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
    assertEquals(0, ta.byteOffset);
  }

  // Resize the rab so that it just barely covers the needed 8 bytes.
  rab.resize(16);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
    assertEquals(8, ta.byteOffset);
  }

  rab.resize(40);

  for (let [ta, length] of tas_and_lengths) {
    assertEquals(length, ta.length);
    assertEquals(length * ta.BYTES_PER_ELEMENT, ta.byteLength);
    assertEquals(8, ta.byteOffset);
  }
})();

(function LengthTracking1() {
  const rab = CreateResizableArrayBuffer(16, 40);

  let tas = [];
  for (let ctor of ctors) {
    tas.push(new ctor(rab));
  }

  for (let ta of tas) {
    assertEquals(16 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(16, ta.byteLength);
  }

  rab.resize(40);
  for (let ta of tas) {
    assertEquals(40 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(40, ta.byteLength);
  }

  // Resize to a number which is not a multiple of all byte_lengths.
  rab.resize(19);
  for (let ta of tas) {
    const expected_length = Math.floor(19 / ta.BYTES_PER_ELEMENT);
    assertEquals(expected_length, ta.length);
    assertEquals(expected_length * ta.BYTES_PER_ELEMENT, ta.byteLength);
  }

  rab.resize(1);

  for (let ta of tas) {
    if (ta.BYTES_PER_ELEMENT == 1) {
      assertEquals(1, ta.length);
      assertEquals(1, ta.byteLength);
    } else {
      assertEquals(0, ta.length);
      assertEquals(0, ta.byteLength);
    }
  }

  rab.resize(0);

  for (let ta of tas) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
  }

  rab.resize(8);

  for (let ta of tas) {
    assertEquals(8 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(8, ta.byteLength);
  }

  rab.resize(40);

  for (let ta of tas) {
    assertEquals(40 / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(40, ta.byteLength);
  }
})();

// The previous test with offsets.
(function LengthTracking2() {
  const rab = CreateResizableArrayBuffer(16, 40);

  const offset = 8;
  let tas = [];
  for (let ctor of ctors) {
    tas.push(new ctor(rab, offset));
  }

  for (let ta of tas) {
    assertEquals((16 - offset) / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(16 - offset, ta.byteLength);
    assertEquals(offset, ta.byteOffset);
  }

  rab.resize(40);
  for (let ta of tas) {
    assertEquals((40 - offset) / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(40 - offset, ta.byteLength);
    assertEquals(offset, ta.byteOffset);
  }

  // Resize to a number which is not a multiple of all byte_lengths.
  rab.resize(20);
  for (let ta of tas) {
    const expected_length = Math.floor((20 - offset)/ ta.BYTES_PER_ELEMENT);
    assertEquals(expected_length, ta.length);
    assertEquals(expected_length * ta.BYTES_PER_ELEMENT, ta.byteLength);
    assertEquals(offset, ta.byteOffset);
  }

  // Resize so that all TypedArrays go out of bounds (because of the offset).
  rab.resize(7);

  for (let ta of tas) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
    assertEquals(0, ta.byteOffset);
  }

  rab.resize(0);

  for (let ta of tas) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
    assertEquals(0, ta.byteOffset);
  }

  rab.resize(8);

  for (let ta of tas) {
    assertEquals(0, ta.length);
    assertEquals(0, ta.byteLength);
    assertEquals(offset, ta.byteOffset);
  }

  // Resize so that the TypedArrays which have element size > 1 go out of bounds
  // (because less than 1 full element would fit).
  rab.resize(offset + 1);

  for (let ta of tas) {
    if (ta.BYTES_PER_ELEMENT == 1) {
      assertEquals(1, ta.length);
      assertEquals(1, ta.byteLength);
      assertEquals(offset, ta.byteOffset);
    } else {
      assertEquals(0, ta.length);
      assertEquals(0, ta.byteLength);
      assertEquals(offset, ta.byteOffset);
    }
  }

  rab.resize(40);

  for (let ta of tas) {
    assertEquals((40 - offset) / ta.BYTES_PER_ELEMENT, ta.length);
    assertEquals(40 - offset, ta.byteLength);
    assertEquals(offset, ta.byteOffset);
  }
})();

(function AccessOutOfBoundsTypedArray() {
  for (let ctor of ctors) {
    if (ctor.BYTES_PER_ELEMENT != 1) {
      continue;
    }
    const rab = CreateResizableArrayBuffer(16, 40);
    const array = new ctor(rab, 0, 4);

    // Initial values
    for (let i = 0; i < 4; ++i) {
      assertEquals(0, array[i]);
    }

    // Within-bounds write
    for (let i = 0; i < 4; ++i) {
      array[i] = i;
    }

    // Within-bounds read
    for (let i = 0; i < 4; ++i) {
      assertEquals(i, array[i]);
    }

    rab.resize(2);

    // OOB read. If the RAB isn't large enough to fit the entire TypedArray,
    // the length of the TypedArray is treated as 0.
    for (let i = 0; i < 4; ++i) {
      assertEquals(undefined, array[i]);
    }

    // OOB write (has no effect)
    for (let i = 0; i < 4; ++i) {
      array[i] = 10;
    }

    rab.resize(4);

    // Within-bounds read
    for (let i = 0; i < 2; ++i) {
      assertEquals(i, array[i]);
    }
    // The shrunk-and-regrown part got zeroed.
    for (let i = 2; i < 4; ++i) {
      assertEquals(0, array[i]);
    }

    rab.resize(40);

    // Within-bounds read
    for (let i = 0; i < 2; ++i) {
      assertEquals(i, array[i]);
    }
    for (let i = 2; i < 4; ++i) {
      assertEquals(0, array[i]);
    }
  }
})();

(function OutOfBoundsTypedArrayAndHas() {
  for (let ctor of ctors) {
    if (ctor.BYTES_PER_ELEMENT != 1) {
      continue;
    }
    const rab = CreateResizableArrayBuffer(16, 40);
    const array = new ctor(rab, 0, 4);

    // Within-bounds read
    for (let i = 0; i < 4; ++i) {
      assertTrue(i in array);
    }

    rab.resize(2);

    // OOB read. If the RAB isn't large enough to fit the entire TypedArray,
    // the length of the TypedArray is treated as 0.
    for (let i = 0; i < 4; ++i) {
      assertFalse(i in array);
    }

    rab.resize(4);

    // Within-bounds read
    for (let i = 0; i < 4; ++i) {
      assertTrue(i in array);
    }

    rab.resize(40);

    // Within-bounds read
    for (let i = 0; i < 4; ++i) {
      assertTrue(i in array);
    }
  }
})();

(function LoadFromOutOfBoundsTypedArrayWithFeedback() {
  function ReadElement2(ta) {
    return ta[2];
  }
  %EnsureFeedbackVectorForFunction(ReadElement2);

  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);
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

  rab.resize(2);

  // OOB read
  for (let i = 0; i < 3; ++i) {
    assertEquals(undefined, ReadElement2(i8a));
  }

  rab.resize(4);

  // Within-bounds read (the memory got zeroed)
  for (let i = 0; i < 3; ++i) {
    assertEquals(0, ReadElement2(i8a));
  }

  i8a[2] = 3;
  for (let i = 0; i < 3; ++i) {
    assertEquals(3, ReadElement2(i8a));
  }

  rab.resize(40);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertEquals(3, ReadElement2(i8a));
  }
})();

(function HasAndOutOfBoundsTypedArrayWithFeedback() {
  function HasElement2(ta) {
    return 2 in ta;
  }
  %EnsureFeedbackVectorForFunction(HasElement2);

  const rab = CreateResizableArrayBuffer(16, 40);

  const i8a = new Int8Array(rab, 0, 4);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertTrue(HasElement2(i8a));
  }

  rab.resize(2);

  // OOB read
  for (let i = 0; i < 3; ++i) {
    assertFalse(HasElement2(i8a));
  }
  rab.resize(4);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertTrue(HasElement2(i8a));
  }

  rab.resize(40);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertTrue(HasElement2(i8a));
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

  const rab = CreateResizableArrayBuffer(4, 8);
  const fixedLength = new Int8Array(rab, 0, 4);
  const fixedLengthWithOffset = new Int8Array(rab, 1, 3);
  const lengthTracking = new Int8Array(rab, 0);
  const lengthTrackingWithOffset = new Int8Array(rab, 1);

  assertEquals('true,true,true,true,false,false,false,false,',
               GetElements(fixedLength));
  assertEquals('true,true,true,false,false,false,false,false,',
              GetElements(fixedLengthWithOffset));
  assertEquals('true,true,true,true,false,false,false,false,',
              GetElements(lengthTracking));
  assertEquals('true,true,true,false,false,false,false,false,',
              GetElements(lengthTrackingWithOffset));

  rab.resize(2);

  assertEquals('false,false,false,false,false,false,false,false,',
               GetElements(fixedLength));
  assertEquals('false,false,false,false,false,false,false,false,',
              GetElements(fixedLengthWithOffset));
  assertEquals('true,true,false,false,false,false,false,false,',
              GetElements(lengthTracking));
  assertEquals('true,false,false,false,false,false,false,false,',
              GetElements(lengthTrackingWithOffset));

  // Resize beyond the offset of the length tracking arrays.
  rab.resize(1);
  assertEquals('false,false,false,false,false,false,false,false,',
               GetElements(fixedLength));
  assertEquals('false,false,false,false,false,false,false,false,',
              GetElements(fixedLengthWithOffset));
  assertEquals('true,false,false,false,false,false,false,false,',
              GetElements(lengthTracking));
  assertEquals('false,false,false,false,false,false,false,false,',
              GetElements(lengthTrackingWithOffset));

  rab.resize(8);

  assertEquals('true,true,true,true,false,false,false,false,',
               GetElements(fixedLength));
  assertEquals('true,true,true,false,false,false,false,false,',
               GetElements(fixedLengthWithOffset));
  assertEquals('true,true,true,true,true,true,true,true,',
               GetElements(lengthTracking));
  assertEquals('true,true,true,true,true,true,true,false,',
               GetElements(lengthTrackingWithOffset));
})();

(function StoreToOutOfBoundsTypedArrayWithFeedback() {
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
  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertEquals(3, i8a[2]);
  }

  rab.resize(2);

  // OOB write (has no effect)
  for (let i = 0; i < 3; ++i) {
    WriteElement2(i8a, 4);
  }

  rab.resize(4);

  // Within-bounds read (the memory got zeroed)
  for (let i = 0; i < 3; ++i) {
    assertEquals(0, i8a[2]);
  }

  // Within-bounds write
  for (let i = 0; i < 3; ++i) {
    WriteElement2(i8a, 5);
  }

  rab.resize(40);

  // Within-bounds read
  for (let i = 0; i < 3; ++i) {
    assertEquals(5, i8a[2]);
  }
})();

(function OOBBehavesLikeDetached() {
  function ReadElement2(ta) {
    return ta[2];
  }
  function HasElement2(ta) {
    return 2 in ta;
  }

  const rab = CreateResizableArrayBuffer(16, 40);
  const i8a = new Int8Array(rab, 0, 4);
  i8a.__proto__ = {2: 'wrong value'};
  i8a[2] = 10;
  assertEquals(10, ReadElement2(i8a));
  assertTrue(HasElement2(i8a));

  rab.resize(0);
  assertEquals(undefined, ReadElement2(i8a));
  assertFalse(HasElement2(i8a));
})();

(function OOBBehavesLikeDetachedWithFeedback() {
  function ReadElement2(ta) {
    return ta[2];
  }
  function HasElement2(ta) {
    return 2 in ta;
  }
  %EnsureFeedbackVectorForFunction(ReadElement2);
  %EnsureFeedbackVectorForFunction(HasElement2);

  const rab = CreateResizableArrayBuffer(16, 40);
  const i8a = new Int8Array(rab, 0, 4);
  i8a.__proto__ = {2: 'wrong value'};
  i8a[2] = 10;
  for (let i = 0; i < 3; ++i) {
    assertEquals(10, ReadElement2(i8a));
    assertTrue(HasElement2(i8a));
  }
  rab.resize(0);

  for (let i = 0; i < 3; ++i) {
    assertEquals(undefined, ReadElement2(i8a));
    assertFalse(HasElement2(i8a));
  }
})();

(function EnumerateElements() {
  let rab = CreateResizableArrayBuffer(100, 200);
  for (let ctor of ctors) {
    const ta = new ctor(rab, 0, 3);
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
    // We can use the same RAB for all the TAs below, since we won't modify it
    // after writing the initial values.
    const rab = CreateResizableArrayBuffer(buffer_byte_length,
                                         2 * buffer_byte_length);
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Write some data into the array.
    let ta_write = new ctor(rab);
    for (let i = 0; i < no_elements; ++i) {
      WriteToTypedArray(ta_write, i, i % 128);
    }

    // Create various different styles of TypedArrays with the RAB as the
    // backing store and iterate them.
    const ta = new ctor(rab, 0, 3);
    TestIteration(ta, [0, 1, 2]);

    const empty_ta = new ctor(rab, 0, 0);
    TestIteration(empty_ta, []);

    const ta_with_offset = new ctor(rab, byte_offset, 3);
    TestIteration(ta_with_offset, [2, 3, 4]);

    const empty_ta_with_offset = new ctor(rab, byte_offset, 0);
    TestIteration(empty_ta_with_offset, []);

    const length_tracking_ta = new ctor(rab);
    {
      let expected = [];
      for (let i = 0; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      TestIteration(length_tracking_ta, expected);
    }

    const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
    {
      let expected = [];
      for (let i = offset; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      TestIteration(length_tracking_ta_with_offset, expected);
    }

    const empty_length_tracking_ta_with_offset = new ctor(rab, buffer_byte_length);
    TestIteration(empty_length_tracking_ta_with_offset, []);
  }
}());

// Helpers for iteration tests.
function CreateRab(buffer_byte_length, ctor) {
  const rab = CreateResizableArrayBuffer(buffer_byte_length,
                                         2 * buffer_byte_length);
  // Write some data into the array.
  let ta_write = new ctor(rab);
  for (let i = 0; i < buffer_byte_length / ctor.BYTES_PER_ELEMENT; ++i) {
    WriteToTypedArray(ta_write, i, i % 128);
  }
  return rab;
}

function TestIterationAndResize(ta, expected, rab, resize_after,
                                new_byte_length) {
  let values = [];
  let resized = false;
  for (const value of ta) {
    if (value instanceof Array) {
      // When iterating via entries(), the values will be arrays [key, value].
      values.push([value[0], Number(value[1])]);
    } else {
      values.push(Number(value));
    }
    if (!resized && values.length == resize_after) {
      rab.resize(new_byte_length);
      resized = true;
    }
  }
  assertEquals(expected, values);
  assertTrue(resized);
}

(function IterateTypedArrayAndGrowMidIteration() {
  const no_elements = 10;
  const offset = 2;

  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Create various different styles of TypedArrays with the RAB as the
    // backing store and iterate them.

    // Fixed-length TAs aren't affected by resizing.
    let rab = CreateRab(buffer_byte_length, ctor);
    const ta = new ctor(rab, 0, 3);
    TestIterationAndResize(ta, [0, 1, 2], rab, 2, buffer_byte_length * 2);

    rab = CreateRab(buffer_byte_length, ctor);
    const ta_with_offset = new ctor(rab, byte_offset, 3);
    TestIterationAndResize(ta_with_offset, [2, 3, 4], rab, 2,
                           buffer_byte_length * 2);

    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta = new ctor(rab);
    {
      let expected = [];
      for (let i = 0; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      // After resizing, the new memory contains zeros.
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }

      TestIterationAndResize(length_tracking_ta, expected, rab, 2,
                             buffer_byte_length * 2);
    }

    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
    {
      let expected = [];
      for (let i = offset; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }
      TestIterationAndResize(length_tracking_ta_with_offset, expected, rab, 2,
                             buffer_byte_length * 2);
    }
  }
}());

(function IterateTypedArrayAndGrowJustBeforeIterationWouldEnd() {
  const no_elements = 10;
  const offset = 2;

  // We need to recreate the RAB between all TA tests, since we grow it.
  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Create various different styles of TypedArrays with the RAB as the
    // backing store and iterate them.

    let rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta = new ctor(rab);
    {
      let expected = [];
      for (let i = 0; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      // After resizing, the new memory contains zeros.
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }

      TestIterationAndResize(length_tracking_ta, expected, rab, no_elements,
                             buffer_byte_length * 2);
    }

    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
    {
      let expected = [];
      for (let i = offset; i < no_elements; ++i) {
        expected.push(i % 128);
      }
      for (let i = 0; i < no_elements; ++i) {
        expected.push(0);
      }
      TestIterationAndResize(length_tracking_ta_with_offset, expected, rab,
                             no_elements - offset, buffer_byte_length * 2);
    }
  }
}());

(function IterateTypedArrayAndShrinkMidIteration() {
  const no_elements = 10;
  const offset = 2;

  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Create various different styles of TypedArrays with the RAB as the
    // backing store and iterate them.

    // Fixed-length TAs aren't affected by shrinking if they stay in-bounds.
    // They appear detached after shrinking out of bounds.
    let rab = CreateRab(buffer_byte_length, ctor);
    const ta1 = new ctor(rab, 0, 3);
    TestIterationAndResize(ta1, [0, 1, 2], rab, 2, buffer_byte_length / 2);

    rab = CreateRab(buffer_byte_length, ctor);
    const ta2 = new ctor(rab, 0, 3);
    assertThrows(() => { TestIterationAndResize(ta2, null, rab, 2, 1)});

    rab = CreateRab(buffer_byte_length, ctor);
    const ta_with_offset1 = new ctor(rab, byte_offset, 3);
    TestIterationAndResize(ta_with_offset1, [2, 3, 4], rab, 2,
                           buffer_byte_length / 2);

    rab = CreateRab(buffer_byte_length, ctor);
    const ta_with_offset2 = new ctor(rab, byte_offset, 3);
    assertThrows(() => {
        TestIterationAndResize(ta_with_offset2, null, rab, 2, 0); });

    // Length-tracking TA with offset 0 doesn't throw, but its length gracefully
    // reduces too.
    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta = new ctor(rab);
    TestIterationAndResize(length_tracking_ta, [0, 1, 2, 3, 4], rab, 2,
                           buffer_byte_length / 2);

    // Length-tracking TA appears detached when the buffer is resized beyond the
    // offset.
    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
    assertThrows(() => {
        TestIterationAndResize(length_tracking_ta_with_offset, null, rab, 2,
                               byte_offset / 2);

    // Length-tracking TA reduces its length gracefully when the buffer is
    // resized to barely cover the offset.
    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
    TestIterationAndResize(length_tracking_ta_with_offset, [3, 4], rab, 2,
                           byte_offset);
    });
  }
}());

(function IterateTypedArrayAndShrinkToZeroMidIteration() {
  const no_elements = 10;
  const offset = 2;

  for (let ctor of ctors) {
    const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
    const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

    // Create various different styles of TypedArrays with the RAB as the
    // backing store and iterate them.

    // Fixed-length TAs appear detached after shrinking out of bounds.
    let rab = CreateRab(buffer_byte_length, ctor);
    const ta = new ctor(rab, 0, 3);
    assertThrows(() => { TestIterationAndResize(ta, null, rab, 2, 0)});

    rab = CreateRab(buffer_byte_length, ctor);
    const ta_with_offset = new ctor(rab, byte_offset, 3);
    assertThrows(() => {
        TestIterationAndResize(ta_with_offset, null, rab, 2, 0); });

    // Length-tracking TA with offset 0 doesn't throw, but its length gracefully
    // goes to zero too.
    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta = new ctor(rab);
    TestIterationAndResize(length_tracking_ta, [0, 1], rab, 2, 0);

    // Length-tracking TA which is resized beyond the offset appars detached.
    rab = CreateRab(buffer_byte_length, ctor);
    const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
    assertThrows(() => {
        TestIterationAndResize(length_tracking_ta_with_offset, null, rab, 2, 0);
    });
  }
}());

(function Destructuring() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    let ta_write = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { let [a, b, c] = fixedLength; }, TypeError);
    assertThrows(() => { let [a, b, c] = fixedLengthWithOffset; }, TypeError);

    {
      let [a, b, c, d] = lengthTracking;
      assertEquals([0, 1, 2], ToNumbers([a, b, c]));
      assertEquals(undefined, d);
    }

    {
      let [a, b] = lengthTrackingWithOffset;
      assertEquals([2], ToNumbers([a]));
      assertEquals(undefined, b);
    }

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { let [a, b, c] = fixedLength; }, TypeError);
    assertThrows(() => { let [a, b, c] = fixedLengthWithOffset; }, TypeError);
    assertThrows(() => { let [a, b, c] = lengthTrackingWithOffset; },
                 TypeError);

    {
      let [a, b] = lengthTracking;
      assertEquals([0], ToNumbers([a]));
      assertEquals(undefined, b);
    }

    // Shrink to 0.
    rab.resize(0);

    assertThrows(() => { let [a, b, c] = fixedLength; }, TypeError);
    assertThrows(() => { let [a, b, c] = fixedLengthWithOffset; }, TypeError);
    assertThrows(() => { let [a, b, c] = lengthTrackingWithOffset; },
                 TypeError);

    {
      let [a] = lengthTracking;
      assertEquals(undefined, a);
    }

    // Grow so that all TAs are back in-bounds. The new memory is zeroed.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);

    {
      let [a, b, c, d, e] = fixedLength;
      assertEquals([0, 0, 0, 0], ToNumbers([a, b, c, d]));
      assertEquals(undefined, e);
    }

    {
      let [a, b, c] = fixedLengthWithOffset;
      assertEquals([0, 0], ToNumbers([a, b]));
      assertEquals(undefined, c);
    }

    {
      let [a, b, c, d, e, f, g] = lengthTracking;
      assertEquals([0, 0, 0, 0, 0, 0], ToNumbers([a, b, c, d, e, f]));
      assertEquals(undefined, g);
    }

    {
      let [a, b, c, d, e] = lengthTrackingWithOffset;
      assertEquals([0, 0, 0, 0], ToNumbers([a, b, c, d]));
      assertEquals(undefined, e);
    }
  }
}());

(function TestFill() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    assertEquals([0, 0, 0, 0], ReadDataFromBuffer(rab, ctor));

    FillHelper(fixedLength, 1);
    assertEquals([1, 1, 1, 1], ReadDataFromBuffer(rab, ctor));

    FillHelper(fixedLengthWithOffset, 2);
    assertEquals([1, 1, 2, 2], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTracking, 3);
    assertEquals([3, 3, 3, 3], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTrackingWithOffset, 4);
    assertEquals([3, 3, 4, 4], ReadDataFromBuffer(rab, ctor));

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => FillHelper(fixedLength, 5), TypeError);
    assertEquals([3, 3, 4], ReadDataFromBuffer(rab, ctor));

    assertThrows(() => FillHelper(fixedLengthWithOffset, 6), TypeError);
    assertEquals([3, 3, 4], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTracking, 7);
    assertEquals([7, 7, 7], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTrackingWithOffset, 8);
    assertEquals([7, 7, 8], ReadDataFromBuffer(rab, ctor));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => FillHelper(fixedLength, 9), TypeError);
    assertEquals([7], ReadDataFromBuffer(rab, ctor));

    assertThrows(() => FillHelper(fixedLengthWithOffset, 10), TypeError);
    assertEquals([7], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTracking, 11);
    assertEquals([11], ReadDataFromBuffer(rab, ctor));

    assertThrows(() => FillHelper(lengthTrackingWithOffset, 12), TypeError);
    assertEquals([11], ReadDataFromBuffer(rab, ctor));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);

    FillHelper(fixedLength, 13);
    assertEquals([13, 13, 13, 13, 0, 0], ReadDataFromBuffer(rab, ctor));

    FillHelper(fixedLengthWithOffset, 14);
    assertEquals([13, 13, 14, 14, 0, 0], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTracking, 15);
    assertEquals([15, 15, 15, 15, 15, 15], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTrackingWithOffset, 16);
    assertEquals([15, 15, 16, 16, 16, 16], ReadDataFromBuffer(rab, ctor));

    // Filling with non-undefined start & end.
    FillHelper(fixedLength, 17, 1, 3);
    assertEquals([15, 17, 17, 16, 16, 16], ReadDataFromBuffer(rab, ctor));

    FillHelper(fixedLengthWithOffset, 18, 1, 2);
    assertEquals([15, 17, 17, 18, 16, 16], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTracking, 19, 1, 3);
    assertEquals([15, 19, 19, 18, 16, 16], ReadDataFromBuffer(rab, ctor));

    FillHelper(lengthTrackingWithOffset, 20, 1, 2);
    assertEquals([15, 19, 19, 20, 16, 16], ReadDataFromBuffer(rab, ctor));
  }
})();

(function FillParameterConversionResizes() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => { rab.resize(2); return 0;}};
    assertThrows(() => { FillHelper(fixedLength, evil, 1, 2); }, TypeError);
    rab.resize(4 * ctor.BYTES_PER_ELEMENT);
    assertThrows(() => { FillHelper(fixedLength, 3, evil, 2); }, TypeError);
    rab.resize(4 * ctor.BYTES_PER_ELEMENT);
    assertThrows(() => { FillHelper(fixedLength, 3, 1, evil); }, TypeError);
  }
})();

(function At() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    let ta_write = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(ta_write, i, i);
    }

    assertEquals(3, AtHelper(fixedLength, -1));
    assertEquals(3, AtHelper(lengthTracking, -1));
    assertEquals(3, AtHelper(fixedLengthWithOffset, -1));
    assertEquals(3, AtHelper(lengthTrackingWithOffset, -1));

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { AtHelper(fixedLength, -1); });
    assertThrows(() => { AtHelper(fixedLengthWithOffset, -1); });

    assertEquals(2, AtHelper(lengthTracking, -1));
    assertEquals(2, AtHelper(lengthTrackingWithOffset, -1));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { AtHelper(fixedLength, -1); });
    assertThrows(() => { AtHelper(fixedLengthWithOffset, -1); });
    assertEquals(0, AtHelper(lengthTracking, -1));
    assertThrows(() => { AtHelper(lengthTrackingWithOffset, -1); });

    // Grow so that all TAs are back in-bounds. New memory is zeroed.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    assertEquals(0, AtHelper(fixedLength, -1));
    assertEquals(0, AtHelper(lengthTracking, -1));
    assertEquals(0, AtHelper(fixedLengthWithOffset, -1));
    assertEquals(0, AtHelper(lengthTrackingWithOffset, -1));
  }
})();

(function AtParameterConversionResizes() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => { rab.resize(2); return 0;}};
    assertEquals(undefined, AtHelper(fixedLength, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => { rab.resize(2); return -1;}};
    // The TypedArray is *not* out of bounds since it's length-tracking.
    assertEquals(undefined, AtHelper(lengthTracking, evil));
  }
})();

(function Slice() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    const fixedLengthSlice = fixedLength.slice();
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLengthSlice));
    assertFalse(fixedLengthSlice.buffer.resizable);

    const fixedLengthWithOffsetSlice = fixedLengthWithOffset.slice();
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffsetSlice));
    assertFalse(fixedLengthWithOffsetSlice.buffer.resizable);

    const lengthTrackingSlice = lengthTracking.slice();
    assertEquals([0, 1, 2, 3], ToNumbers(lengthTrackingSlice));
    assertFalse(lengthTrackingSlice.buffer.resizable);

    const lengthTrackingWithOffsetSlice = lengthTrackingWithOffset.slice();
    assertEquals([2, 3], ToNumbers(lengthTrackingWithOffsetSlice));
    assertFalse(lengthTrackingWithOffsetSlice.buffer.resizable);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.slice(); });
    assertThrows(() => { fixedLengthWithOffset.slice(); });
    assertEquals([0, 1, 2], ToNumbers(lengthTracking.slice()));
    assertEquals([2], ToNumbers(lengthTrackingWithOffset.slice()));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.slice(); });
    assertThrows(() => { fixedLengthWithOffset.slice(); });
    assertEquals([0], ToNumbers(lengthTracking.slice()));
    assertThrows(() => { lengthTrackingWithOffset.slice(); });

     // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.slice(); });
    assertThrows(() => { fixedLengthWithOffset.slice(); });
    assertEquals([], ToNumbers(lengthTracking.slice()));
    assertThrows(() => { lengthTrackingWithOffset.slice(); });

    // Verify that the previously created slices aren't affected by the
    // shrinking.
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLengthSlice));
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffsetSlice));
    assertEquals([0, 1, 2, 3], ToNumbers(lengthTrackingSlice));
    assertEquals([2, 3], ToNumbers(lengthTrackingWithOffsetSlice));

    // Grow so that all TAs are back in-bounds. New memory is zeroed.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    assertEquals([0, 0, 0, 0], ToNumbers(fixedLength.slice()));
    assertEquals([0, 0], ToNumbers(fixedLengthWithOffset.slice()));
    assertEquals([0, 0, 0, 0, 0, 0], ToNumbers(lengthTracking.slice()));
    assertEquals([0, 0, 0, 0], ToNumbers(lengthTrackingWithOffset.slice()));
  }
})();

(function SliceSpeciesCreateResizes() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const fixedLength = new MyArray(rab, 0, 4);
    resizeWhenConstructorCalled = true;
    assertThrows(() => { fixedLength.slice(); }, TypeError);
    assertEquals(2 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 1);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const lengthTracking = new MyArray(rab);
    resizeWhenConstructorCalled = true;
    const a = lengthTracking.slice();
    assertEquals(2 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
    // The length of the resulting TypedArray is determined before
    // TypedArraySpeciesCreate is called, and it doesn't change.
    assertEquals(4, a.length);
    assertEquals([1, 1, 0, 0], ToNumbers(a));
  }

  // Test that the (start, end) parameters are computed based on the original
  // length.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 1);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const lengthTracking = new MyArray(rab);
    resizeWhenConstructorCalled = true;
    const a = lengthTracking.slice(-3, -1);
    assertEquals(2 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
    // The length of the resulting TypedArray is determined before
    // TypedArraySpeciesCreate is called, and it doesn't change.
    assertEquals(2, a.length);
    assertEquals([1, 0], ToNumbers(a));
  }

  // Test where the buffer gets resized "between elements".
  {
    const rab = CreateResizableArrayBuffer(8, 16);

    // Fill the buffer with 1-bits.
    const taWrite = new Uint8Array(rab);
    for (let i = 0; i < 8; ++i) {
      WriteToTypedArray(taWrite, i, 255);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends Uint16Array {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          // Resize so that the size is not a multiple of the element size.
          rab.resize(5);
        }
      }
    };

    const lengthTracking = new MyArray(rab);
    assertEquals([65535, 65535, 65535, 65535], ToNumbers(lengthTracking));
    resizeWhenConstructorCalled = true;
    const a = lengthTracking.slice();
    assertEquals(5, rab.byteLength);
    assertEquals(4, a.length); // The old length is used.
    assertEquals(65535, a[0]);
    assertEquals(65535, a[1]);
    assertEquals(0, a[2]);
    assertEquals(0, a[3]);
  }
})();

(function CopyWithin() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 3; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2]
    //              [0, 1, 2, ...] << lengthTracking
    //                    [2, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.copyWithin(0, 1); });
    assertThrows(() => { fixedLengthWithOffset.copyWithin(0, 1); });
    lengthTracking.copyWithin(0, 1);
    assertEquals([1, 2, 2], ToNumbers(lengthTracking));
    lengthTrackingWithOffset.copyWithin(0, 1);
    assertEquals([2], ToNumbers(lengthTrackingWithOffset));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    WriteToTypedArray(taWrite, 0, 0);

    assertThrows(() => { fixedLength.copyWithin(0, 1, 1); });
    assertThrows(() => { fixedLengthWithOffset.copyWithin(0, 1, 1); });
    lengthTracking.copyWithin(0, 0, 1);
    assertEquals([0], ToNumbers(lengthTracking));
    assertThrows(() => { lengthTrackingWithOffset.copyWithin(0, 1, 1); });

     // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.copyWithin(0, 1, 1); });
    assertThrows(() => { fixedLengthWithOffset.copyWithin(0, 1, 1); });
    lengthTracking.copyWithin(0, 0, 1);
    assertEquals([], ToNumbers(lengthTracking));
    assertThrows(() => { lengthTrackingWithOffset.copyWithin(0, 1, 1); });

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function CopyWithinParameterConversionShrinks() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    const evil = { valueOf: () => { rab.resize(2 * ctor.BYTES_PER_ELEMENT);
                                    return 2;}};
    assertThrows(() => { fixedLength.copyWithin(evil, 0, 1); }, TypeError);
    rab.resize(4 * ctor.BYTES_PER_ELEMENT);
    assertThrows(() => { fixedLength.copyWithin(0, evil, 3); }, TypeError);
    rab.resize(4 * ctor.BYTES_PER_ELEMENT);
    assertThrows(() => { fixedLength.copyWithin(0, 1, evil); }, TypeError);
  }
})();

(function CopyWithinParameterConversionGrows() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i);
    }

    const evil = { valueOf: () => { rab.resize(6 * ctor.BYTES_PER_ELEMENT);
                                    WriteToTypedArray(lengthTracking, 4, 4);
                                    WriteToTypedArray(lengthTracking, 5, 5);
                                    return 0;} };
    // Orig. array: [0, 1, 2, 3]  [4, 5]
    //               ^     ^       ^ new elements
    //          target     start
    lengthTracking.copyWithin(evil, 2);
    assertEquals([2, 3, 2, 3, 4, 5], ToNumbers(lengthTracking));

    rab.resize(4 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i);
    }

    // Orig. array: [0, 1, 2, 3]  [4, 5]
    //               ^     ^       ^ new elements
    //           start     target
    lengthTracking.copyWithin(2, evil);
    assertEquals([0, 1, 0, 1, 4, 5], ToNumbers(lengthTracking));
  }
})();

(function EntriesKeysValues() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.entries(); });
    assertThrows(() => { fixedLength.values(); });
    assertThrows(() => { fixedLength.keys(); });
    assertThrows(() => { fixedLengthWithOffset.entries(); });
    assertThrows(() => { fixedLengthWithOffset.values(); });
    assertThrows(() => { fixedLengthWithOffset.keys(); });

    assertEquals([0, 2, 4], ToNumbersWithEntries(lengthTracking));
    assertEquals([0, 2, 4], ValuesToNumbers(lengthTracking));
    assertEquals([0, 1, 2], Keys(lengthTracking));

    assertEquals([4], ToNumbersWithEntries(lengthTrackingWithOffset));
    assertEquals([4], ValuesToNumbers(lengthTrackingWithOffset));
    assertEquals([0], Keys(lengthTrackingWithOffset));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.entries(); });
    assertThrows(() => { fixedLength.values(); });
    assertThrows(() => { fixedLength.keys(); });
    assertThrows(() => { fixedLengthWithOffset.entries(); });
    assertThrows(() => { fixedLengthWithOffset.values(); });
    assertThrows(() => { fixedLengthWithOffset.keys(); });
    assertThrows(() => { lengthTrackingWithOffset.entries(); });
    assertThrows(() => { lengthTrackingWithOffset.values(); });
    assertThrows(() => { lengthTrackingWithOffset.keys(); });

    assertEquals([0], ToNumbersWithEntries(lengthTracking));
    assertEquals([0], ValuesToNumbers(lengthTracking));
    assertEquals([0], Keys(lengthTracking));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.entries(); });
    assertThrows(() => { fixedLength.values(); });
    assertThrows(() => { fixedLength.keys(); });
    assertThrows(() => { fixedLengthWithOffset.entries(); });
    assertThrows(() => { fixedLengthWithOffset.values(); });
    assertThrows(() => { fixedLengthWithOffset.keys(); });
    assertThrows(() => { lengthTrackingWithOffset.entries(); });
    assertThrows(() => { lengthTrackingWithOffset.values(); });
    assertThrows(() => { lengthTrackingWithOffset.keys(); });

    assertEquals([], ToNumbersWithEntries(lengthTracking));
    assertEquals([], ValuesToNumbers(lengthTracking));
    assertEquals([], Keys(lengthTracking));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

  // Iterating with entries() (the 4 loops below).
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // The fixed length array is not affected by resizing.
    TestIterationAndResize(fixedLength.entries(),
                           [[0, 0], [1, 2], [2, 4], [3, 6]],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array is not affected by resizing.
    TestIterationAndResize(fixedLengthWithOffset.entries(),
                           [[0, 4], [1, 6]],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    TestIterationAndResize(lengthTracking.entries(),
                           [[0, 0], [1, 2], [2, 4], [3, 6], [4, 0], [5, 0]],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndResize(lengthTrackingWithOffset.entries(),
                           [[0, 4], [1, 6], [2, 0], [3, 0]],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  // Iterating with keys() (the 4 loops below).
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // The fixed length array is not affected by resizing.
    TestIterationAndResize(fixedLength.keys(),
                           [0, 1, 2, 3],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array is not affected by resizing.
    TestIterationAndResize(fixedLengthWithOffset.keys(),
                           [0, 1],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    TestIterationAndResize(lengthTracking.keys(),
                           [0, 1, 2, 3, 4, 5],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndResize(lengthTrackingWithOffset.keys(),
                           [0, 1, 2, 3],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  // Iterating with values() (the 4 loops below).
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // The fixed length array is not affected by resizing.
    TestIterationAndResize(fixedLength.values(),
                           [0, 2, 4, 6],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array is not affected by resizing.
    TestIterationAndResize(fixedLengthWithOffset.values(),
                           [4, 6],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    TestIterationAndResize(lengthTracking.values(),
                           [0, 2, 4, 6, 0, 0],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndResize(lengthTrackingWithOffset.values(),
                           [4, 6, 0, 0],
                           rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
  }
})();

(function EntriesKeysValuesShrinkMidIteration() {
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

  // Iterating with entries() (the 4 loops below).
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // The fixed length array goes out of bounds when the RAB is resized.
    assertThrows(() => { TestIterationAndResize(
                             fixedLength.entries(),
                             null,
                             rab, 2, 3 * ctor.BYTES_PER_ELEMENT); });
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array goes out of bounds when the RAB is resized.
    assertThrows(() => { TestIterationAndResize(
                             fixedLengthWithOffset.entries(),
                             null,
                             rab, 1, 3 * ctor.BYTES_PER_ELEMENT); });
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    TestIterationAndResize(lengthTracking.entries(),
                           [[0, 0], [1, 2], [2, 4]],
                           rab, 2, 3 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndResize(lengthTrackingWithOffset.entries(),
                           [[0, 4], [1, 6]],
                           rab, 2, 3 * ctor.BYTES_PER_ELEMENT);
  }

  // Iterating with keys() (the 4 loops below).
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // The fixed length array goes out of bounds when the RAB is resized.
    assertThrows(() => { TestIterationAndResize(
                             fixedLength.keys(),
                             null,
                             rab, 2, 3 * ctor.BYTES_PER_ELEMENT); });
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array goes out of bounds when the RAB is resized.
    assertThrows(() => { TestIterationAndResize(
                             fixedLengthWithOffset.keys(),
                             null,
                             rab, 2, 3 * ctor.BYTES_PER_ELEMENT); });
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    TestIterationAndResize(lengthTracking.keys(),
                           [0, 1, 2],
                           rab, 2, 3 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndResize(lengthTrackingWithOffset.keys(),
                           [0, 1],
                           rab, 2, 3 * ctor.BYTES_PER_ELEMENT);
  }

  // Iterating with values() (the 4 loops below).
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // The fixed length array goes out of bounds when the RAB is resized.
    assertThrows(() => { TestIterationAndResize(
                             fixedLength.values(),
                             null,
                             rab, 2, 3 * ctor.BYTES_PER_ELEMENT); });
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);

    // The fixed length array goes out of bounds when the RAB is resized.
    assertThrows(() => { TestIterationAndResize(
                             fixedLengthWithOffset.values(),
                             null,
                             rab, 2, 3 * ctor.BYTES_PER_ELEMENT); });
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    TestIterationAndResize(lengthTracking.values(),
                           [0, 2, 4],
                           rab, 2, 3 * ctor.BYTES_PER_ELEMENT);
  }

  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    TestIterationAndResize(lengthTrackingWithOffset.values(),
                           [4, 6],
                           rab, 2, 3 * ctor.BYTES_PER_ELEMENT);
  }
})();

(function EverySome() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.every(div3); });
    assertThrows(() => { fixedLength.some(div3); });

    assertThrows(() => { fixedLengthWithOffset.every(div3); });
    assertThrows(() => { fixedLengthWithOffset.some(div3); });

    assertFalse(lengthTracking.every(div3));
    assertTrue(lengthTracking.every(even));
    assertTrue(lengthTracking.some(div3));
    assertFalse(lengthTracking.some(over10));

    assertFalse(lengthTrackingWithOffset.every(div3));
    assertTrue(lengthTrackingWithOffset.every(even));
    assertFalse(lengthTrackingWithOffset.some(div3));
    assertFalse(lengthTrackingWithOffset.some(over10));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.every(div3); });
    assertThrows(() => { fixedLength.some(div3); });

    assertThrows(() => { fixedLengthWithOffset.every(div3); });
    assertThrows(() => { fixedLengthWithOffset.some(div3); });

    assertTrue(lengthTracking.every(div3));
    assertTrue(lengthTracking.every(even));
    assertTrue(lengthTracking.some(div3));
    assertFalse(lengthTracking.some(over10));

    assertThrows(() => { lengthTrackingWithOffset.every(div3); });
    assertThrows(() => { lengthTrackingWithOffset.some(div3); });

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.every(div3); });
    assertThrows(() => { fixedLength.some(div3); });

    assertThrows(() => { fixedLengthWithOffset.every(div3); });
    assertThrows(() => { fixedLengthWithOffset.some(div3); });

    assertTrue(lengthTracking.every(div3));
    assertTrue(lengthTracking.every(even));
    assertFalse(lengthTracking.some(div3));
    assertFalse(lengthTracking.some(over10));

    assertThrows(() => { lengthTrackingWithOffset.every(div3); });
    assertThrows(() => { lengthTrackingWithOffset.some(div3); });

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function EveryShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return true;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertTrue(fixedLength.every(CollectValuesAndResize));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertTrue(fixedLengthWithOffset.every(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertTrue(lengthTracking.every(CollectValuesAndResize));
    assertEquals([0, 2, 4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertTrue(lengthTrackingWithOffset.every(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }
})();

(function EveryGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return true;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(fixedLength.every(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(fixedLengthWithOffset.every(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(lengthTracking.every(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertTrue(lengthTrackingWithOffset.every(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }
})();

(function SomeShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertFalse(fixedLength.some(CollectValuesAndResize));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertFalse(fixedLengthWithOffset.some(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertFalse(lengthTracking.some(CollectValuesAndResize));
    assertEquals([0, 2, 4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertFalse(lengthTrackingWithOffset.some(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }
})();

(function SomeGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(fixedLength.some(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(fixedLengthWithOffset.some(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(lengthTracking.some(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    rab = rab;
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertFalse(lengthTrackingWithOffset.some(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }
})();

(function FindFindIndexFindLastFindLastIndex() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.find(isTwoOrFour); });
    assertThrows(() => { fixedLength.findIndex(isTwoOrFour); });
    assertThrows(() => { fixedLength.findLast(isTwoOrFour); });
    assertThrows(() => { fixedLength.findLastIndex(isTwoOrFour); });

    assertThrows(() => { fixedLengthWithOffset.find(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findIndex(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findLast(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findLastIndex(isTwoOrFour); });

    assertEquals(2, Number(lengthTracking.find(isTwoOrFour)));
    assertEquals(4, Number(lengthTrackingWithOffset.find(isTwoOrFour)));

    assertEquals(1, lengthTracking.findIndex(isTwoOrFour));
    assertEquals(0, lengthTrackingWithOffset.findIndex(isTwoOrFour));

    assertEquals(4, Number(lengthTracking.findLast(isTwoOrFour)));
    assertEquals(4, Number(lengthTrackingWithOffset.findLast(isTwoOrFour)));

    assertEquals(2, lengthTracking.findLastIndex(isTwoOrFour));
    assertEquals(0, lengthTrackingWithOffset.findLastIndex(isTwoOrFour));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.find(isTwoOrFour); });
    assertThrows(() => { fixedLength.findIndex(isTwoOrFour); });
    assertThrows(() => { fixedLength.findLast(isTwoOrFour); });
    assertThrows(() => { fixedLength.findLastIndex(isTwoOrFour); });

    assertThrows(() => { fixedLengthWithOffset.find(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findIndex(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findLast(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findLastIndex(isTwoOrFour); });

    assertThrows(() => { lengthTrackingWithOffset.find(isTwoOrFour); });
    assertThrows(() => { lengthTrackingWithOffset.findIndex(isTwoOrFour); });
    assertThrows(() => { lengthTrackingWithOffset.findLast(isTwoOrFour); });
    assertThrows(() => { lengthTrackingWithOffset.findLastIndex(isTwoOrFour); });

    assertEquals(undefined, lengthTracking.find(isTwoOrFour));
    assertEquals(-1, lengthTracking.findIndex(isTwoOrFour));
    assertEquals(undefined, lengthTracking.findLast(isTwoOrFour));
    assertEquals(-1, lengthTracking.findLastIndex(isTwoOrFour));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.find(isTwoOrFour); });
    assertThrows(() => { fixedLength.findIndex(isTwoOrFour); });
    assertThrows(() => { fixedLength.findLast(isTwoOrFour); });
    assertThrows(() => { fixedLength.findLastIndex(isTwoOrFour); });

    assertThrows(() => { fixedLengthWithOffset.find(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findIndex(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findLast(isTwoOrFour); });
    assertThrows(() => { fixedLengthWithOffset.findLastIndex(isTwoOrFour); });

    assertThrows(() => { lengthTrackingWithOffset.find(isTwoOrFour); });
    assertThrows(() => { lengthTrackingWithOffset.findIndex(isTwoOrFour); });
    assertThrows(() => { lengthTrackingWithOffset.findLast(isTwoOrFour); });
    assertThrows(() => { lengthTrackingWithOffset.findLastIndex(isTwoOrFour); });

    assertEquals(undefined, lengthTracking.find(isTwoOrFour));
    assertEquals(-1, lengthTracking.findIndex(isTwoOrFour));
    assertEquals(undefined, lengthTracking.findLast(isTwoOrFour));
    assertEquals(-1, lengthTracking.findLastIndex(isTwoOrFour));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function FindShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLength.find(CollectValuesAndResize));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLengthWithOffset.find(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTracking.find(CollectValuesAndResize));
    assertEquals([0, 2, 4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTrackingWithOffset.find(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }
})();

(function FindGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLength.find(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLengthWithOffset.find(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTracking.find(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTrackingWithOffset.find(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }
})();

(function FindIndexShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLength.findIndex(CollectValuesAndResize));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLengthWithOffset.findIndex(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findIndex(CollectValuesAndResize));
    assertEquals([0, 2, 4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTrackingWithOffset.findIndex(CollectValuesAndResize));
    assertEquals([4, undefined], values);
  }
})();

(function FindIndexGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLength.findIndex(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLengthWithOffset.findIndex(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findIndex(CollectValuesAndResize));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTrackingWithOffset.findIndex(CollectValuesAndResize));
    assertEquals([4, 6], values);
  }
})();

(function FindLastShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLength.findLast(CollectValuesAndResize));
    assertEquals([6, 4, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLengthWithOffset.findLast(CollectValuesAndResize));
    assertEquals([6, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTracking.findLast(CollectValuesAndResize));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTrackingWithOffset.findLast(CollectValuesAndResize));
    assertEquals([6, 4], values);
  }
})();

(function FindLastGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLength.findLast(CollectValuesAndResize));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, fixedLengthWithOffset.findLast(CollectValuesAndResize));
    assertEquals([6, 4], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTracking.findLast(CollectValuesAndResize));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(undefined, lengthTrackingWithOffset.findLast(CollectValuesAndResize));
    assertEquals([6, 4], values);
  }
})();

(function FindLastIndexShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLength.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLengthWithOffset.findLastIndex(CollectValuesAndResize));
    assertEquals([6, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 1;
    resizeTo = 2 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findLastIndex(CollectValuesAndResize));
    assertEquals([6, undefined, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTrackingWithOffset.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4], values);
  }
})();

(function FindLastIndexGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLength.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, fixedLengthWithOffset.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTracking.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4, 2, 0], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals(-1, lengthTrackingWithOffset.findLastIndex(CollectValuesAndResize));
    assertEquals([6, 4], values);
  }
})();

(function Filter() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 1, 2]
    //              [0, 1, 2, ...] << lengthTracking
    //                    [2, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.filter(isEven); });
    assertThrows(() => { fixedLengthWithOffset.filter(isEven); });

    assertEquals([0, 2], ToNumbers(lengthTracking.filter(isEven)));
    assertEquals([2], ToNumbers(lengthTrackingWithOffset.filter(isEven)));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.filter(isEven); });
    assertThrows(() => { fixedLengthWithOffset.filter(isEven); });
    assertThrows(() => { lengthTrackingWithOffset.filter(isEven); });

    assertEquals([0], ToNumbers(lengthTracking.filter(isEven)));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.filter(isEven); });
    assertThrows(() => { fixedLengthWithOffset.filter(isEven); });
    assertThrows(() => { lengthTrackingWithOffset.filter(isEven); });

    assertEquals([], ToNumbers(lengthTracking.filter(isEven)));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(fixedLength.filter(CollectValuesAndResize)));
    assertEquals([0, 2, undefined, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(fixedLengthWithOffset.filter(CollectValuesAndResize)));
    assertEquals([4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(lengthTracking.filter(CollectValuesAndResize)));
    assertEquals([0, 2, 4, undefined], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(lengthTrackingWithOffset.filter(CollectValuesAndResize)));
    assertEquals([4, undefined], values);
  }
})();

(function FilterGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
    }
    return false;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(fixedLength.filter(CollectValuesAndResize)));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(fixedLengthWithOffset.filter(CollectValuesAndResize)));
    assertEquals([4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    values = [];
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(lengthTracking.filter(CollectValuesAndResize)));
    assertEquals([0, 2, 4, 6], values);
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    values = [];
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([], ToNumbers(lengthTrackingWithOffset.filter(CollectValuesAndResize)));
    assertEquals([4, 6], values);
  }
})();

(function ForEachReduceReduceRight() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { Helper(fixedLength); });
    assertThrows(() => { Helper(fixedLengthWithOffset); });

    assertEquals([0, 2, 4], Helper(lengthTracking));
    assertEquals([4], Helper(lengthTrackingWithOffset));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { Helper(fixedLength); });
    assertThrows(() => { Helper(fixedLengthWithOffset); });
    assertThrows(() => { Helper(lengthTrackingWithOffset); });

    assertEquals([0], Helper(lengthTracking));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { Helper(fixedLength); });
    assertThrows(() => { Helper(fixedLengthWithOffset); });
    assertThrows(() => { Helper(lengthTrackingWithOffset); });

    assertEquals([], Helper(lengthTracking));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function ForEachReduceReduceRightShrinkMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
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
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, undefined, undefined], ForEachHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, undefined], ForEachHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, undefined], ForEachHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, undefined], ForEachHelper(lengthTrackingWithOffset));
  }

  // Tests for reduce.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, undefined, undefined], ReduceHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, undefined], ReduceHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, undefined], ReduceHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, undefined], ReduceHelper(lengthTrackingWithOffset));
  }

  // Tests for reduceRight.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4, undefined, undefined], ReduceRightHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, undefined], ReduceRightHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    // Unaffected by the shrinking, since we've already iterated past the point.
    assertEquals([6, 4, 2, 0], ReduceRightHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 1;
    resizeTo = 2 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, undefined, 2, 0], ReduceRightHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    // Unaffected by the shrinking, since we've already iterated past the point.
    assertEquals([6, 4], ReduceRightHelper(lengthTrackingWithOffset));
  }
})();

(function ForEachReduceReduceRightGrowMidIteration() {
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
  let resizeAfter;
  let resizeTo;
  function CollectValuesAndResize(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
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
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ForEachHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ForEachHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ForEachHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ForEachHelper(lengthTrackingWithOffset));
  }

  // Test for reduce.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ReduceHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ReduceHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], ReduceHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], ReduceHelper(lengthTrackingWithOffset));
  }

  // Test for reduceRight.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4, 2, 0], ReduceRightHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4], ReduceRightHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4, 2, 0], ReduceRightHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([6, 4], ReduceRightHelper(lengthTrackingWithOffset));
  }
})();

(function Includes() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { IncludesHelper(fixedLength, 2); });
    assertThrows(() => { IncludesHelper(fixedLengthWithOffset, 2); });

    assertTrue(IncludesHelper(lengthTracking, 2));
    assertFalse(IncludesHelper(lengthTracking, undefined));

    assertFalse(IncludesHelper(lengthTrackingWithOffset, 2));
    assertTrue(IncludesHelper(lengthTrackingWithOffset, 4));
    assertFalse(IncludesHelper(lengthTrackingWithOffset, undefined));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { IncludesHelper(fixedLength, 2); });
    assertThrows(() => { IncludesHelper(fixedLengthWithOffset, 2); });
    assertThrows(() => { IncludesHelper(lengthTrackingWithOffset, 2); });

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { IncludesHelper(fixedLength, 2); });
    assertThrows(() => { IncludesHelper(fixedLengthWithOffset, 2); });
    assertThrows(() => { IncludesHelper(lengthTrackingWithOffset, 2); });

    assertFalse(IncludesHelper(lengthTracking, 2));
    assertFalse(IncludesHelper(lengthTracking, undefined));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function IncludesParameterConversionResizes() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertFalse(IncludesHelper(fixedLength, undefined));
    // The TA is OOB so it includes only "undefined".
    assertTrue(IncludesHelper(fixedLength, undefined, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertTrue(IncludesHelper(fixedLength, 0));
    // The TA is OOB so it includes only "undefined".
    assertFalse(IncludesHelper(fixedLength, 0, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertFalse(IncludesHelper(lengthTracking, undefined));
    // "includes" iterates until the original length and sees "undefined"s.
    assertTrue(IncludesHelper(lengthTracking, undefined, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, 1);
    }

    let evil = { valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertFalse(IncludesHelper(lengthTracking, 0));
    // The TA grew but we only look at the data until the original length.
    assertFalse(IncludesHelper(lengthTracking, 0, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    WriteToTypedArray(lengthTracking, 0, 1);

    let evil = { valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    lengthTracking[0] = -Infinity;
    lengthTracking[1] = Infinity;
    lengthTracking[2] = NaN;
    assertTrue(lengthTracking.includes(-Infinity));
    assertTrue(lengthTracking.includes(Infinity));
    assertTrue(lengthTracking.includes(NaN));
  }
})();
