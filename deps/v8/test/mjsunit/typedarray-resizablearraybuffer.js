// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax

"use strict";

d8.file.execute('test/mjsunit/typedarray-helpers.js');

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

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
    values.push(Number(value));
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
