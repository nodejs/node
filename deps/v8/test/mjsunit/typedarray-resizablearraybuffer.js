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

(function ConstructFromTypedArray() {
  AllBigIntMatchedCtorCombinations((targetCtor, sourceCtor) => {
    const rab = CreateResizableArrayBuffer(
        4 * sourceCtor.BYTES_PER_ELEMENT,
        8 * sourceCtor.BYTES_PER_ELEMENT);
    const fixedLength = new sourceCtor(rab, 0, 4);
    const fixedLengthWithOffset = new sourceCtor(
        rab, 2 * sourceCtor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new sourceCtor(rab, 0);
    const lengthTrackingWithOffset = new sourceCtor(
        rab, 2 * sourceCtor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taFull = new sourceCtor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * sourceCtor.BYTES_PER_ELEMENT);

    // Orig. array: [1, 2, 3]
    //              [1, 2, 3, ...] << lengthTracking
    //                    [3, ...] << lengthTrackingWithOffset

    assertThrows(() => { new targetCtor(fixedLength); }, TypeError);
    assertThrows(() => { new targetCtor(fixedLengthWithOffset); }, TypeError);
    assertEquals([1, 2, 3], ToNumbers(new targetCtor(lengthTracking)));
    assertEquals([3], ToNumbers(new targetCtor(lengthTrackingWithOffset)));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * sourceCtor.BYTES_PER_ELEMENT);

    assertThrows(() => { new targetCtor(fixedLength); }, TypeError);
    assertThrows(() => { new targetCtor(fixedLengthWithOffset); }, TypeError);
    assertEquals([1], ToNumbers(new targetCtor(lengthTracking)));
    assertThrows(() => { new targetCtor(lengthTrackingWithOffset); },
                 TypeError);

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { new targetCtor(fixedLength); }, TypeError);
    assertThrows(() => { new targetCtor(fixedLengthWithOffset); }, TypeError);
    assertEquals([], ToNumbers(new targetCtor(lengthTracking)));
    assertThrows(() => { new targetCtor(lengthTrackingWithOffset); },
                 TypeError);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * sourceCtor.BYTES_PER_ELEMENT);

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

(function IndexOfLastIndexOf() {
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 0, 1]
    //              [0, 0, 1, ...] << lengthTracking
    //                    [1, ...] << lengthTrackingWithOffset

    assertThrows(() => { IndexOfHelper(fixedLength, 1); });
    assertThrows(() => { IndexOfHelper(fixedLengthWithOffset, 1); });

    assertThrows(() => { LastIndexOfHelper(fixedLength, 1); });
    assertThrows(() => { LastIndexOfHelper(fixedLengthWithOffset, 1); });

    assertEquals(2, IndexOfHelper(lengthTracking, 1));
    assertEquals(-1, IndexOfHelper(lengthTracking, undefined));

    assertEquals(1, LastIndexOfHelper(lengthTracking, 0));
    assertEquals(-1, LastIndexOfHelper(lengthTracking, undefined));

    assertEquals(-1, IndexOfHelper(lengthTrackingWithOffset, 0));
    assertEquals(0, IndexOfHelper(lengthTrackingWithOffset, 1));
    assertEquals(-1, IndexOfHelper(lengthTrackingWithOffset, undefined));

    assertEquals(-1, LastIndexOfHelper(lengthTrackingWithOffset, 0));
    assertEquals(0, LastIndexOfHelper(lengthTrackingWithOffset, 1));
    assertEquals(-1, LastIndexOfHelper(lengthTrackingWithOffset, undefined));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { IndexOfHelper(fixedLength, 0); });
    assertThrows(() => { IndexOfHelper(fixedLengthWithOffset, 0); });
    assertThrows(() => { IndexOfHelper(lengthTrackingWithOffset, 0); });

    assertThrows(() => { LastIndexOfHelper(fixedLength, 0); });
    assertThrows(() => { LastIndexOfHelper(fixedLengthWithOffset, 0); });
    assertThrows(() => { LastIndexOfHelper(lengthTrackingWithOffset, 0); });

    assertEquals(0, IndexOfHelper(lengthTracking, 0));

    assertEquals(0, LastIndexOfHelper(lengthTracking, 0));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { IndexOfHelper(fixedLength, 0); });
    assertThrows(() => { IndexOfHelper(fixedLengthWithOffset, 0); });
    assertThrows(() => { IndexOfHelper(lengthTrackingWithOffset, 0); });

    assertThrows(() => { LastIndexOfHelper(fixedLength, 0); });
    assertThrows(() => { LastIndexOfHelper(fixedLengthWithOffset, 0); });
    assertThrows(() => { LastIndexOfHelper(lengthTrackingWithOffset, 0); });

    assertEquals(-1, IndexOfHelper(lengthTracking, 0));
    assertEquals(-1, IndexOfHelper(lengthTracking, undefined));

    assertEquals(-1, LastIndexOfHelper(lengthTracking, 0));
    assertEquals(-1, LastIndexOfHelper(lengthTracking, undefined));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function IndexOfParameterConversionShrinks() {
  // Shrinking + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertEquals(0, IndexOfHelper(fixedLength, 0));
    // The TA is OOB so indexOf returns -1.
    assertEquals(-1, IndexOfHelper(fixedLength, 0, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertEquals(0, IndexOfHelper(fixedLength, 0));
    // The TA is OOB so indexOf returns -1, also for undefined).
    assertEquals(-1, IndexOfHelper(fixedLength, undefined, evil));
  }

  // Shrinking + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i);
    }

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertEquals(2, IndexOfHelper(lengthTracking, 2));
    // 2 no longer found.
    assertEquals(-1, IndexOfHelper(lengthTracking, 2, evil));
  }
})();

(function LastIndexOfParameterConversionShrinks() {
  // Shrinking + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }};
    assertEquals(3, LastIndexOfHelper(fixedLength, 0));
    // The TA is OOB so lastIndexOf returns -1.
    assertEquals(-1, LastIndexOfHelper(fixedLength, 0, evil));
  }

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }};
    assertEquals(3, LastIndexOfHelper(fixedLength, 0));
    // The TA is OOB so lastIndexOf returns -1, also for undefined).
    assertEquals(-1, LastIndexOfHelper(fixedLength, undefined, evil));
  }

  // Shrinking + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i);
    }

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }};
    assertEquals(2, LastIndexOfHelper(lengthTracking, 2));
    // 2 no longer found.
    assertEquals(-1, LastIndexOfHelper(lengthTracking, 2, evil));
  }
})();

(function IndexOfParameterConversionGrows() {
  // Growing + length-tracking TA.
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
    assertEquals(-1, IndexOfHelper(lengthTracking, 0));
    // The TA grew but we only look at the data until the original length.
    assertEquals(-1, IndexOfHelper(lengthTracking, 0, evil));
  }

  // Growing + length-tracking TA, index conversion.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    WriteToTypedArray(lengthTracking, 0, 1);

    let evil = { valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, 1);
    }

    let evil = { valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
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

    assertEquals('0,2,4,6', fixedLength.join());
    assertEquals('0,2,4,6', fixedLength.toLocaleString());
    assertEquals('4,6', fixedLengthWithOffset.join());
    assertEquals('4,6', fixedLengthWithOffset.toLocaleString());
    assertEquals('0,2,4,6', lengthTracking.join());
    assertEquals('0,2,4,6', lengthTracking.toLocaleString());
    assertEquals('4,6', lengthTrackingWithOffset.join());
    assertEquals('4,6', lengthTrackingWithOffset.toLocaleString());

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.join(); });
    assertThrows(() => { fixedLength.toLocaleString(); });
    assertThrows(() => { fixedLengthWithOffset.join(); });
    assertThrows(() => { fixedLengthWithOffset.toLocaleString(); });

    assertEquals('0,2,4', lengthTracking.join());
    assertEquals('0,2,4', lengthTracking.toLocaleString());
    assertEquals('4', lengthTrackingWithOffset.join());
    assertEquals('4', lengthTrackingWithOffset.toLocaleString());

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { fixedLength.join(); });
    assertThrows(() => { fixedLength.toLocaleString(); });
    assertThrows(() => { fixedLengthWithOffset.join(); });
    assertThrows(() => { fixedLengthWithOffset.toLocaleString(); });
    assertThrows(() => { lengthTrackingWithOffset.join(); });
    assertThrows(() => { lengthTrackingWithOffset.toLocaleString(); });

    assertEquals('0', lengthTracking.join());
    assertEquals('0', lengthTracking.toLocaleString());

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.join(); });
    assertThrows(() => { fixedLength.toLocaleString(); });
    assertThrows(() => { fixedLengthWithOffset.join(); });
    assertThrows(() => { fixedLengthWithOffset.toLocaleString(); });
    assertThrows(() => { lengthTrackingWithOffset.join(); });
    assertThrows(() => { lengthTrackingWithOffset.toLocaleString(); });

    assertEquals('', lengthTracking.join());
    assertEquals('', lengthTracking.toLocaleString());

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

(function JoinParameterConversionShrinks() {
  // Shrinking + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { toString: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return '.';
    }};
    // We iterate 4 elements, since it was the starting length, but the TA is
    // OOB right after parameter conversion, so all elements are converted to
    // the empty string.
    assertEquals('...', fixedLength.join(evil));
  }

  // Shrinking + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let evil = { toString: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return '.';
    }};
    // We iterate 4 elements, since it was the starting length. Elements beyond
    // the new length are converted to the empty string.
    assertEquals('0.0..', lengthTracking.join(evil));
  }
})();

(function JoinParameterConversionGrows() {
  // Growing + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { toString: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return '.';
    }};
    assertEquals('0.0.0.0', fixedLength.join(evil));
  }

  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let evil = { toString: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return '.';
    }};
    // We iterate 4 elements, since it was the starting length.
    assertEquals('0.0.0.0', lengthTracking.join(evil));
  }
})();

(function ToLocaleStringNumberPrototypeToLocaleStringShrinks() {
  const oldNumberPrototypeToLocaleString = Number.prototype.toLocaleString;
  const oldBigIntPrototypeToLocaleString = BigInt.prototype.toLocaleString;

  // Shrinking + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let resizeAfter = 2;
    Number.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    }
    BigInt.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    }

    // We iterate 4 elements, since it was the starting length. The TA goes
    // OOB after 2 elements.
    assertEquals('0,0,,', fixedLength.toLocaleString());
  }

  // Shrinking + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let resizeAfter = 2;
    Number.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    }
    BigInt.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    }

    // We iterate 4 elements, since it was the starting length. Elements beyond
    // the new length are converted to the empty string.
    assertEquals('0,0,,', lengthTracking.toLocaleString());
  }

  Number.prototype.toLocaleString = oldNumberPrototypeToLocaleString;
  BigInt.prototype.toLocaleString = oldBigIntPrototypeToLocaleString;
})();

(function ToLocaleStringNumberPrototypeToLocaleStringGrows() {
  const oldNumberPrototypeToLocaleString = Number.prototype.toLocaleString;
  const oldBigIntPrototypeToLocaleString = BigInt.prototype.toLocaleString;

  // Growing + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);

    let resizeAfter = 2;
    Number.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    }
    BigInt.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    }

    // We iterate 4 elements since it was the starting length. Resizing doesn't
    // affect the TA.
    assertEquals('0,0,0,0', fixedLength.toLocaleString());
  }

  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);

    let resizeAfter = 2;
    Number.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    }
    BigInt.prototype.toLocaleString = function() {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < taWrite.length; ++i) {
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

(function MapShrinkMidIteration() {
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
  function CollectValuesAndResize(n, ix, ta) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == resizeAfter) {
      rab.resize(resizeTo);
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
    array.map(CollectValuesAndResize);
    return values;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, undefined, undefined], Helper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, undefined], Helper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, undefined], Helper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, undefined], Helper(lengthTrackingWithOffset));
  }
})();

(function MapGrowMidIteration() {
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
    return n;
  }

  function Helper(array) {
    values = [];
    array.map(CollectValuesAndResize);
    return values;
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], Helper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], Helper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAfter = 2;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([0, 2, 4, 6], Helper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAfter = 1;
    resizeTo = 5 * ctor.BYTES_PER_ELEMENT;
    assertEquals([4, 6], Helper(lengthTrackingWithOffset));
  }
})();

(function MapSpeciesCreateShrinks() {
  let values;
  let rab;
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
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
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
    assertEquals([undefined, undefined, undefined, undefined],
                 Helper(fixedLength));
    assertEquals(2 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
  }

  for (let ctor of ctors) {
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
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
    assertEquals([0, 1, undefined, undefined], Helper(lengthTracking));
    assertEquals(2 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
  }
})();

(function MapSpeciesCreateGrows() {
  let values;
  let rab;
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
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const fixedLength = new MyArray(rab, 0, 4);
    resizeWhenConstructorCalled = true;
    assertEquals([0, 1, 2, 3], Helper(fixedLength));
    assertEquals(6 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
  }

  for (let ctor of ctors) {
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);

    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    let resizeWhenConstructorCalled = false;
    class MyArray extends ctor {
      constructor(...params) {
        super(...params);
        if (resizeWhenConstructorCalled) {
          rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        }
      }
    };

    const lengthTracking = new MyArray(rab);
    resizeWhenConstructorCalled = true;
    assertEquals([0, 1, 2, 3], Helper(lengthTracking));
    assertEquals(6 * ctor.BYTES_PER_ELEMENT, rab.byteLength);
  }
})();

(function Reverse() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    const wholeArrayView = new ctor(rab);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);
    WriteData();

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    assertThrows(() => { fixedLength.reverse(); });
    assertThrows(() => { fixedLengthWithOffset.reverse(); });

    lengthTracking.reverse();
    assertEquals([4, 2, 0], ToNumbers(wholeArrayView));
    lengthTrackingWithOffset.reverse();
    assertEquals([4, 2, 0], ToNumbers(wholeArrayView));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    WriteData();

    assertThrows(() => { fixedLength.reverse(); });
    assertThrows(() => { fixedLengthWithOffset.reverse(); });
    assertThrows(() => { lengthTrackingWithOffset.reverse(); });

    lengthTracking.reverse();
    assertEquals([0], ToNumbers(wholeArrayView));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { fixedLength.reverse(); });
    assertThrows(() => { fixedLengthWithOffset.reverse(); });
    assertThrows(() => { lengthTrackingWithOffset.reverse(); });

    lengthTracking.reverse();
    assertEquals([], ToNumbers(wholeArrayView));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
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

(function SetWithResizableTarget() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    const taFull = new ctor(rab);

    // Orig. array: [0, 0, 0, 0]
    //              [0, 0, 0, 0] << fixedLength
    //                    [0, 0] << fixedLengthWithOffset
    //              [0, 0, 0, 0, ...] << lengthTracking
    //                    [0, 0, ...] << lengthTrackingWithOffset

    // For making sure we're not calling the source length or element getters
    // if the target is OOB.
    const throwingProxy = new Proxy({}, {
      get(target, prop, receiver) {
        throw new Error('Called getter for ' + prop);
      }});

    SetHelper(fixedLength, [1, 2]);
    assertEquals([1, 2, 0, 0], ToNumbers(taFull));
    SetHelper(fixedLength, [3, 4], 1);
    assertEquals([1, 3, 4, 0], ToNumbers(taFull));
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0], 1)},
                 RangeError);
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
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0, 0])});
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0], 1)});
    assertEquals([8, 10, 12, 14], ToNumbers(taFull));

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [8, 10, 12]
    //              [8, 10, 12, ...] << lengthTracking
    //                     [12, ...] << lengthTrackingWithOffset

    assertThrows(() => { SetHelper(fixedLength, throwingProxy)}, TypeError);
    assertThrows(() => { SetHelper(fixedLengthWithOffset, throwingProxy)},
                 TypeError);
    assertEquals([8, 10, 12], ToNumbers(taFull));

    SetHelper(lengthTracking, [15, 16]);
    assertEquals([15, 16, 12], ToNumbers(taFull));
    SetHelper(lengthTracking, [17, 18], 1);
    assertEquals([15, 17, 18], ToNumbers(taFull));
    assertThrows(() => { SetHelper(lengthTracking, [0, 0, 0, 0])}, RangeError);
    assertThrows(() => { SetHelper(lengthTracking, [0, 0, 0], 1)}, RangeError);
    assertEquals([15, 17, 18], ToNumbers(taFull));

    SetHelper(lengthTrackingWithOffset, [19]);
    assertEquals([15, 17, 19], ToNumbers(taFull));
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0, 0])},
                 RangeError);
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, [0], 1)},
                 RangeError);
    assertEquals([15, 17, 19], ToNumbers(taFull));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { SetHelper(fixedLength, throwingProxy)}, TypeError);
    assertThrows(() => { SetHelper(fixedLengthWithOffset, throwingProxy)},
                 TypeError);
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, throwingProxy)},
                 TypeError);
    assertEquals([15], ToNumbers(taFull));

    SetHelper(lengthTracking, [20]);
    assertEquals([20], ToNumbers(taFull));

    // Shrink to zero.
    rab.resize(0);

    assertThrows(() => { SetHelper(fixedLength, throwingProxy)}, TypeError);
    assertThrows(() => { SetHelper(fixedLengthWithOffset, throwingProxy)},
                 TypeError);
    assertThrows(() => { SetHelper(lengthTrackingWithOffset, throwingProxy)},
                 TypeError);
    assertThrows(() => { SetHelper(lengthTracking, [0])}, RangeError);
    assertEquals([], ToNumbers(taFull));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 0, 0, 0, 0, 0]
    //              [0, 0, 0, 0] << fixedLength
    //                    [0, 0] << fixedLengthWithOffset
    //              [0, 0, 0, 0, 0, 0, ...] << lengthTracking
    //                    [0, 0, 0, 0, ...] << lengthTrackingWithOffset
    SetHelper(fixedLength, [21, 22]);
    assertEquals([21, 22, 0, 0, 0, 0], ToNumbers(taFull));
    SetHelper(fixedLength, [23, 24], 1);
    assertEquals([21, 23, 24, 0, 0, 0], ToNumbers(taFull));
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0, 0])}, RangeError);
    assertThrows(() => { SetHelper(fixedLength, [0, 0, 0, 0], 1)}, RangeError);
    assertEquals([21, 23, 24, 0, 0, 0], ToNumbers(taFull));

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

(function SetSourceLengthGetterShrinksTarget() {
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

  let rab;
  let resizeTo;
  function CreateSourceProxy(length) {
    return new Proxy({}, {
      get(target, prop, receiver) {
        if (prop == 'length') {
          rab.resize(resizeTo);
          return length;
        }
        return true; // Can be converted to both BigInt and Number.
      }
    });
  }

  // Tests where the length getter returns a non-zero value -> these are nop if
  // the TA went OOB.
  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    fixedLength.set(CreateSourceProxy(1));
    assertEquals([0, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    fixedLengthWithOffset.set(CreateSourceProxy(1));
    assertEquals([0, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    lengthTracking.set(CreateSourceProxy(1));
    assertEquals([1, 2, 4], ToNumbers(lengthTracking));
    assertEquals([1, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(1));
    assertEquals([1], ToNumbers(lengthTrackingWithOffset));
    assertEquals([0, 2, 1], ToNumbers(new ctor(rab)));
  }

  // Length-tracking TA goes OOB because of the offset.
  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 1 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(1));
    assertEquals([0], ToNumbers(new ctor(rab)));
  }

  // Tests where the length getter returns a zero -> these don't throw even if
  // the TA went OOB.
  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    fixedLength.set(CreateSourceProxy(0));
    assertEquals([0, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    fixedLengthWithOffset.set(CreateSourceProxy(0));
    assertEquals([0, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    lengthTracking.set(CreateSourceProxy(0));
    assertEquals([0, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(0));
    assertEquals([0, 2, 4], ToNumbers(new ctor(rab)));
  }

  // Length-tracking TA goes OOB because of the offset.
  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 1 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(0));
    assertEquals([0], ToNumbers(new ctor(rab)));
  }
})();

(function SetSourceLengthGetterGrowsTarget() {
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

  let rab;
  let resizeTo;
  function CreateSourceProxy(length) {
    return new Proxy({}, {
      get(target, prop, receiver) {
        if (prop == 'length') {
          rab.resize(resizeTo);
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
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    assertThrows(() => { lengthTracking.set(CreateSourceProxy(6)); },
                 RangeError);
    assertEquals([0, 2, 4, 6, 0, 0], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    assertThrows(() => { lengthTrackingWithOffset.set(CreateSourceProxy(4)); },
                 RangeError);
    assertEquals([0, 2, 4, 6, 0, 0], ToNumbers(new ctor(rab)));
  }
})();

(function SetShrinkTargetMidIteration() {
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

  let rab;
  // Resizing will happen when we're calling Get for the `resizeAt`:th data
  // element, but we haven't yet written it to the target.
  let resizeAt;
  let resizeTo;
  function CreateSourceProxy(length) {
    let requestedIndices = [];
    return new Proxy({}, {
      get(target, prop, receiver) {
        if (prop == 'length') {
          return length;
        }
        requestedIndices.push(prop);
        if (requestedIndices.length == resizeAt) {
          rab.resize(resizeTo);
        }
        return true; // Can be converted to both BigInt and Number.
      }
    });
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAt = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    fixedLength.set(CreateSourceProxy(4));
    assertEquals([1, 2, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAt = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    fixedLengthWithOffset.set(CreateSourceProxy(2));
    assertEquals([0, 2, 1], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAt = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    lengthTracking.set(CreateSourceProxy(2));
    assertEquals([1, 1, 4], ToNumbers(lengthTracking));
    assertEquals([1, 1, 4], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAt = 2;
    resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(2));
    assertEquals([1], ToNumbers(lengthTrackingWithOffset));
    assertEquals([0, 2, 1], ToNumbers(new ctor(rab)));
  }

  // Length-tracking TA goes OOB because of the offset.
  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAt = 1;
    resizeTo = 1 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(2));
    assertEquals([0], ToNumbers(new ctor(rab)));
  }
})();

(function SetGrowTargetMidIteration() {
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

  let rab;
  // Resizing will happen when we're calling Get for the `resizeAt`:th data
  // element, but we haven't yet written it to the target.
  let resizeAt;
  let resizeTo;
  function CreateSourceProxy(length) {
    let requestedIndices = [];
    return new Proxy({}, {
      get(target, prop, receiver) {
        if (prop == 'length') {
          return length;
        }
        requestedIndices.push(prop);
        if (requestedIndices.length == resizeAt) {
          rab.resize(resizeTo);
        }
        return true; // Can be converted to both BigInt and Number.
      }
    });
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    resizeAt = 2;
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    fixedLength.set(CreateSourceProxy(4));
    assertEquals([1, 1, 1, 1], ToNumbers(fixedLength));
    assertEquals([1, 1, 1, 1, 0, 0], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    resizeAt = 1;
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    fixedLengthWithOffset.set(CreateSourceProxy(2));
    assertEquals([1, 1], ToNumbers(fixedLengthWithOffset));
    assertEquals([0, 2, 1, 1, 0, 0], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    resizeAt = 2;
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    lengthTracking.set(CreateSourceProxy(2));
    assertEquals([1, 1, 4, 6, 0, 0], ToNumbers(lengthTracking));
    assertEquals([1, 1, 4, 6, 0, 0], ToNumbers(new ctor(rab)));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    resizeAt = 1;
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    lengthTrackingWithOffset.set(CreateSourceProxy(2));
    assertEquals([1, 1, 0, 0], ToNumbers(lengthTrackingWithOffset));
    assertEquals([0, 2, 1, 1, 0, 0], ToNumbers(new ctor(rab)));
  }
})();

(function SetWithResizableSource() {
  for (let targetIsResizable of [false, true]) {
    for (let targetCtor of ctors) {
      for (let sourceCtor of ctors) {
        const rab = CreateResizableArrayBuffer(
            4 * sourceCtor.BYTES_PER_ELEMENT,
            8 * sourceCtor.BYTES_PER_ELEMENT);
        const fixedLength = new sourceCtor(rab, 0, 4);
        const fixedLengthWithOffset = new sourceCtor(
            rab, 2 * sourceCtor.BYTES_PER_ELEMENT, 2);
        const lengthTracking = new sourceCtor(rab, 0);
        const lengthTrackingWithOffset = new sourceCtor(
            rab, 2 * sourceCtor.BYTES_PER_ELEMENT);

        // Write some data into the array.
        const taFull = new sourceCtor(rab);
        for (let i = 0; i < 4; ++i) {
          WriteToTypedArray(taFull, i, i + 1);
        }

        // Orig. array: [1, 2, 3, 4]
        //              [1, 2, 3, 4] << fixedLength
        //                    [3, 4] << fixedLengthWithOffset
        //              [1, 2, 3, 4, ...] << lengthTracking
        //                    [3, 4, ...] << lengthTrackingWithOffset

        const targetAb = targetIsResizable ?
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

        // Shrink so that fixed length TAs go out of bounds.
        rab.resize(3 * sourceCtor.BYTES_PER_ELEMENT);

        // Orig. array: [1, 2, 3]
        //              [1, 2, 3, ...] << lengthTracking
        //                    [3, ...] << lengthTrackingWithOffset

        assertThrows(() => { SetHelper(target, fixedLength)}, TypeError);
        assertThrows(() => { SetHelper(target, fixedLengthWithOffset)},
                     TypeError);
        assertEquals([3, 3, 4, 3, 4, 0], ToNumbers(target));

        SetHelper(target, lengthTracking);
        assertEquals([1, 2, 3, 3, 4, 0], ToNumbers(target));

        SetHelper(target, lengthTrackingWithOffset);
        assertEquals([3, 2, 3, 3, 4, 0], ToNumbers(target));

        // Shrink so that the TAs with offset go out of bounds.
        rab.resize(1 * sourceCtor.BYTES_PER_ELEMENT);

        assertThrows(() => { SetHelper(target, fixedLength)}, TypeError);
        assertThrows(() => { SetHelper(target, fixedLengthWithOffset)},
                     TypeError);
        assertThrows(() => { SetHelper(target, lengthTrackingWithOffset)},
                     TypeError);

        SetHelper(target, lengthTracking, 3);
        assertEquals([3, 2, 3, 1, 4, 0], ToNumbers(target));

        // Shrink to zero.
        rab.resize(0);

        assertThrows(() => { SetHelper(target, fixedLength)}, TypeError);
        assertThrows(() => { SetHelper(target, fixedLengthWithOffset)},
                     TypeError);
        assertThrows(() => { SetHelper(target, lengthTrackingWithOffset)},
                     TypeError);

        SetHelper(target, lengthTracking, 4);
        assertEquals([3, 2, 3, 1, 4, 0], ToNumbers(target));

        // Grow so that all TAs are back in-bounds.
        rab.resize(6 * sourceCtor.BYTES_PER_ELEMENT);

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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    // We can create subarrays of OOB arrays (which have length 0), as long as
    // the new arrays are not OOB.
    assertEquals([], ToNumbers(fixedLength.subarray(0)));
    assertEquals([], ToNumbers(fixedLengthWithOffset.subarray(0)));

    assertEquals([0, 2, 4], ToNumbers(lengthTracking.subarray(0)));
    assertEquals([4], ToNumbers(lengthTrackingWithOffset.subarray(0)));

    // Also the previously created subarrays are OOB.
    assertEquals(0, fixedLengthSubFull.length);
    assertEquals(0, fixedLengthWithOffsetSubFull.length);

    // Relative offsets
    assertEquals([2, 4], ToNumbers(lengthTracking.subarray(-2)));
    assertEquals([4], ToNumbers(lengthTrackingWithOffset.subarray(-1)));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    assertEquals([], ToNumbers(fixedLength.subarray(0)));
    assertEquals([0], ToNumbers(lengthTracking.subarray(0)));

    // Even the 0-length subarray of fixedLengthWithOffset would be OOB ->
    // this throws.
    assertThrows(() => { fixedLengthWithOffset.subarray(0); }, RangeError);

    // Also the previously created subarrays are OOB.
    assertEquals(0, fixedLengthSubFull.length);
    assertEquals(0, fixedLengthWithOffsetSubFull.length);
    assertEquals(0, lengthTrackingWithOffsetSubFull.length);

    // Shrink to zero.
    rab.resize(0);

    assertEquals([], ToNumbers(fixedLength.subarray(0)));
    assertEquals([], ToNumbers(lengthTracking.subarray(0)));

    assertThrows(() => { fixedLengthWithOffset.subarray(0); }, RangeError);
    assertThrows(() => { lengthTrackingWithOffset.subarray(0); }, RangeError);

    // Also the previously created subarrays are OOB.
    assertEquals(0, fixedLengthSubFull.length);
    assertEquals(0, fixedLengthWithOffsetSubFull.length);
    assertEquals(0, lengthTrackingWithOffsetSubFull.length);

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

    assertEquals([0, 2, 4, 6], ToNumbers(fixedLength.subarray(0)));
    assertEquals([4, 6], ToNumbers(fixedLengthWithOffset.subarray(0)));
    assertEquals([0, 2, 4, 6, 8, 10], ToNumbers(lengthTracking.subarray(0)));
    assertEquals([4, 6, 8, 10],
                 ToNumbers(lengthTrackingWithOffset.subarray(0)));

    // Also the previously created subarrays are no longer OOB.
    assertEquals(4, fixedLengthSubFull.length);
    assertEquals(2, fixedLengthWithOffsetSubFull.length);

    // TODO(v8:11111): Are subarrays of length-tracking TAs also
    // length-tracking? See
    // https://github.com/tc39/proposal-resizablearraybuffer/issues/91
    assertEquals(4, lengthTrackingSubFull.length);
    assertEquals(2, lengthTrackingWithOffsetSubFull.length);
  }
})();

(function SubarrayParameterConversionShrinks() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //              [0, 2, 4, 6, ...] << lengthTracking
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

  // Fixed-length TA + first parameter conversion shrinks. The old length is
  // used in the length computation, and the subarray construction fails.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertThrows(() => { fixedLength.subarray(evil); }, RangeError);
  }

  // Like the previous test, but now we construct a smaller subarray and it
  // succeeds.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertEquals([0], ToNumbers(fixedLength.subarray(evil, 1)));
  }

  // Fixed-length TA + second parameter conversion shrinks. The old length is
  // used in the length computation, and the subarray construction fails.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 3;
    }};
    assertThrows(() => { fixedLength.subarray(0, evil); }, RangeError);
  }

  // Like the previous test, but now we construct a smaller subarray and it
  // succeeds.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 1;
    }};
    assertEquals([0], ToNumbers(fixedLength.subarray(0, evil)));
  }

  // Shrinking + fixed-length TA, subarray construction succeeds even though the
  // TA goes OOB.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    const evil = { valueOf: () => { rab.resize(2 * ctor.BYTES_PER_ELEMENT);
                                    return 0;}};

    assertEquals([0], ToNumbers(fixedLength.subarray(evil, 1)));
  }

  // Length-tracking TA + first parameter conversion shrinks. The old length is
  // used in the length computation, and the subarray construction fails.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertThrows(() => { lengthTracking.subarray(evil); });
  }

  // Like the previous test, but now we construct a smaller subarray and it
  // succeeds.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertEquals([0], ToNumbers(lengthTracking.subarray(evil, 1)));
  }

  // Length-tracking TA + first parameter conversion shrinks. The second
  // parameter is negative -> the relative index is not recomputed, and the
  // subarray construction fails.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertThrows(() => { lengthTracking.subarray(evil, -1); });
  }

  // Length-tracking TA + second parameter conversion shrinks. The second
  // parameter is too large -> the subarray construction fails.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab);

    let evil = { valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 3;
    }};
    assertThrows(() => { lengthTracking.subarray(0, evil); });
  }
})();

(function SubarrayParameterConversionGrows() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //              [0, 2, 4, 6, ...] << lengthTracking
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

  // Growing a fixed length TA back in bounds.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    // Make `fixedLength` OOB.
    rab.resize(2 * ctor.BYTES_PER_ELEMENT);

    const evil = { valueOf: () => { rab.resize(4 * ctor.BYTES_PER_ELEMENT);
                                    return 0;}};

    // The length computation is done before parameter conversion. At that
    // point, the length is 0, since the TA is OOB.
    assertEquals([], ToNumbers(fixedLength.subarray(evil, 0, 1)));
  }

  // Growing + fixed-length TA. Growing won't affect anything.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);

    const evil = { valueOf: () => { rab.resize(6 * ctor.BYTES_PER_ELEMENT);
                                    return 0;}};

    assertEquals([0, 2, 4, 6], ToNumbers(fixedLength.subarray(evil)));
  }

  // Growing + length-tracking TA. The length computation is done with the
  // original length.
  for (let ctor of ctors) {
    const rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);

    const evil = { valueOf: () => { rab.resize(6 * ctor.BYTES_PER_ELEMENT);
                                    return 0;}};

    assertEquals([0, 2, 4, 6], ToNumbers(lengthTracking.subarray(evil)));
  }
})();

(function SortWithDefaultComparison() {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    const taFull = new ctor(rab, 0);
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

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [10, 8, 6]
    //              [10, 8, 6, ...] << lengthTracking
    //                     [6, ...] << lengthTrackingWithOffset

    WriteUnsortedData();
    assertThrows(() => { fixedLength.sort(); }, TypeError);

    WriteUnsortedData();
    assertThrows(() => { fixedLengthWithOffset.sort(); }, TypeError);

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([6, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTrackingWithOffset.sort();
    assertEquals([10, 8, 6], ToNumbers(taFull));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    WriteUnsortedData();
    assertThrows(() => { fixedLength.sort(); }, TypeError);

    WriteUnsortedData();
    assertThrows(() => { fixedLengthWithOffset.sort(); }, TypeError);

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([10], ToNumbers(taFull));

    WriteUnsortedData();
    assertThrows(() => { lengthTrackingWithOffset.sort(); }, TypeError);

    // Shrink to zero.
    rab.resize(0);

    WriteUnsortedData();
    assertThrows(() => { fixedLength.sort(); }, TypeError);

    WriteUnsortedData();
    assertThrows(() => { fixedLengthWithOffset.sort(); }, TypeError);

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([], ToNumbers(taFull));

    WriteUnsortedData();
    assertThrows(() => { lengthTrackingWithOffset.sort(); }, TypeError);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);

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
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    const taFull = new ctor(rab, 0);
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

    WriteUnsortedData(CustomComparison);
    lengthTrackingWithOffset.sort();
    assertEquals([10, 9, 7, 8], ToNumbers(taFull));

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [10, 9, 8]
    //              [10, 9, 8, ...] << lengthTracking
    //                     [8, ...] << lengthTrackingWithOffset

    WriteUnsortedData();
    assertThrows(() => { fixedLength.sort(CustomComparison); }, TypeError);

    WriteUnsortedData();
    assertThrows(() => { fixedLengthWithOffset.sort(CustomComparison); },
                 TypeError);

    WriteUnsortedData();
    lengthTracking.sort(CustomComparison);
    assertEquals([9, 8, 10], ToNumbers(taFull));

    WriteUnsortedData();
    lengthTrackingWithOffset.sort(CustomComparison);
    assertEquals([10, 9, 8], ToNumbers(taFull));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);

    WriteUnsortedData();
    assertThrows(() => { fixedLength.sort(CustomComparison); }, TypeError);

    WriteUnsortedData();
    assertThrows(() => { fixedLengthWithOffset.sort(CustomComparison); },
                 TypeError);

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([10], ToNumbers(taFull));

    WriteUnsortedData();
    assertThrows(() => { lengthTrackingWithOffset.sort(CustomComparison); },
                 TypeError);

    // Shrink to zero.
    rab.resize(0);

    WriteUnsortedData();
    assertThrows(() => { fixedLength.sort(CustomComparison); }, TypeError);

    WriteUnsortedData();
    assertThrows(() => { fixedLengthWithOffset.sort(CustomComparison); },
                 TypeError);

    WriteUnsortedData();
    lengthTracking.sort();
    assertEquals([], ToNumbers(taFull));

    WriteUnsortedData();
    assertThrows(() => { lengthTrackingWithOffset.sort(CustomComparison); },
                 TypeError);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);

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

(function SortCallbackShrinks() {
  function WriteUnsortedData(taFull) {
    for (let i = 0; i < taFull.length; ++i) {
      WriteToTypedArray(taFull, i, 10 - i);
    }
  }

  let rab;
  let resizeTo;
  function CustomComparison(a, b) {
    rab.resize(resizeTo);
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
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 2 * ctor.BYTES_PER_ELEMENT;
    const fixedLength = new ctor(rab, 0, 4);
    const taFull = new ctor(rab, 0);
    WriteUnsortedData(taFull);

    fixedLength.sort(CustomComparison);

    // The data is unchanged.
    assertEquals([10, 9], ToNumbers(taFull));
  }

  // Length-tracking TA.
  for (let ctor of ctors) {
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 2 * ctor.BYTES_PER_ELEMENT;
    const lengthTracking = new ctor(rab, 0);
    const taFull = new ctor(rab, 0);
    WriteUnsortedData(taFull);

    lengthTracking.sort(CustomComparison);

    // The sort result is implementation defined, but it contains 2 elements out
    // of the 4 original ones.
    const newData = ToNumbers(taFull);
    assertEquals(2, newData.length);
    assertTrue([10, 9, 8, 7].includes(newData[0]));
    assertTrue([10, 9, 8, 7].includes(newData[1]));
  }
})();

(function SortCallbackGrows() {
  function WriteUnsortedData(taFull) {
    for (let i = 0; i < taFull.length; ++i) {
      WriteToTypedArray(taFull, i, 10 - i);
    }
  }

  let rab;
  let resizeTo;
  function CustomComparison(a, b) {
    rab.resize(resizeTo);
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
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    const fixedLength = new ctor(rab, 0, 4);
    const taFull = new ctor(rab, 0);
    WriteUnsortedData(taFull);

    fixedLength.sort(CustomComparison);

    // Growing doesn't affect the sorting.
    assertEquals([7, 8, 9, 10, 0, 0], ToNumbers(taFull));
  }

  // Length-tracking TA.
  for (let ctor of ctors) {
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                     8 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    const lengthTracking = new ctor(rab, 0);
    const taFull = new ctor(rab, 0);
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
      const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                             8 * ctor.BYTES_PER_ELEMENT);
      const fixedLength = new ctor(rab, 0, 4);
      const fixedLengthWithOffset = new ctor(
          rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
      const lengthTracking = new ctor(rab, 0);
      const lengthTrackingWithOffset = new ctor(
          rab, 2 * ctor.BYTES_PER_ELEMENT);
      const taFull = new ctor(rab, 0);

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

      // Shrink so that fixed length TAs go out of bounds.
      rab.resize(3 * ctor.BYTES_PER_ELEMENT);

      // Orig. array: [1, 3, 2]
      //              [1, 3, 2, ...] << lengthTracking
      //                    [2, ...] << lengthTrackingWithOffset

      assertThrows(() => { helper(fixedLength, 0, 8); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 0, 8); }, TypeError);
      assertEquals([1, 3, 2], ToNumbers(taFull));

      helper(lengthTracking, 0, 5);
      assertEquals([5, 3, 2], ToNumbers(taFull));
      helper(lengthTrackingWithOffset, 0, 6);
      assertEquals([5, 3, 6], ToNumbers(taFull));

      // Shrink so that the TAs with offset go out of bounds.
      rab.resize(1 * ctor.BYTES_PER_ELEMENT);

      assertThrows(() => { helper(fixedLength, 0, 8); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 0, 8); }, TypeError);
      assertThrows(() => { helper(lengthTrackingWithOffset, 0, 8); },
                   TypeError);
      assertEquals([5], ToNumbers(taFull));

      helper(lengthTracking, 0, 7);
      assertEquals([7], ToNumbers(taFull));

      // Shrink to zero.
      rab.resize(0);

      assertThrows(() => { helper(fixedLength, 0, 8); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 0, 8); }, TypeError);
      assertThrows(() => { helper(lengthTracking, 0, 8); }, TypeError);
      assertThrows(() => { helper(lengthTrackingWithOffset, 0, 8); },
                   TypeError);
      assertEquals([], ToNumbers(taFull));

      // Grow so that all TAs are back in-bounds.
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);

      helper(fixedLength, 0, 9);
      assertEquals([9, 0, 0, 0, 0, 0], ToNumbers(taFull));
      helper(fixedLengthWithOffset, 0, 10);
      assertEquals([9, 0, 10, 0, 0, 0], ToNumbers(taFull));
      helper(lengthTracking, 1, 11);
      assertEquals([9, 11, 10, 0, 0, 0], ToNumbers(taFull));
      helper(lengthTrackingWithOffset, 2, 12);
      assertEquals([9, 11, 10, 0, 12, 0], ToNumbers(taFull));

      // Trying to define properties out of the fixed-length bounds throws.
      assertThrows(() => { helper(fixedLength, 5, 13); }, TypeError);
      assertThrows(() => { helper(fixedLengthWithOffset, 3, 13); }, TypeError);
      assertEquals([9, 11, 10, 0, 12, 0], ToNumbers(taFull));

      helper(lengthTracking, 4, 14);
      assertEquals([9, 11, 10, 0, 14, 0], ToNumbers(taFull));
      helper(lengthTrackingWithOffset, 3, 15);
      assertEquals([9, 11, 10, 0, 14, 15], ToNumbers(taFull));
    }
  }
})();

(function ObjectDefinePropertyParameterConversionShrinks() {
  const helper = ObjectDefinePropertyHelper;
  // Fixed length.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const evil = {toString: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }};
    assertThrows(() => { helper(fixedLength, evil, 8); }, TypeError);
  }
  // Length tracking.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab, 0);
    const evil = {toString: () => {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        return 3;  // Index too large after resize.
    }};
    assertThrows(() => { helper(lengthTracking, evil, 8); }, TypeError);
  }
})();

(function ObjectDefinePropertyParameterConversionGrows() {
  const helper = ObjectDefinePropertyHelper;
  // Fixed length.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    // Make fixedLength go OOB.
    rab.resize(2 * ctor.BYTES_PER_ELEMENT);
    const evil = {toString: () => {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        return 0;
    }};
    helper(fixedLength, evil, 8);
    assertEquals([8, 0, 0, 0], ToNumbers(fixedLength));
  }
  // Length tracking.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab, 0);
    const evil = {toString: () => {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        return 4;  // Index valid after resize.
    }};
    helper(lengthTracking, evil, 8);
    assertEquals([0, 0, 0, 0, 8, 0], ToNumbers(lengthTracking));
  }
})();

(function ObjectFreeze() {
  // Freezing non-OOB non-zero-length TAs throws.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(
        rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(
        rab, 2 * ctor.BYTES_PER_ELEMENT);

    assertThrows(() => { Object.freeze(fixedLength); }, TypeError);
    assertThrows(() => { Object.freeze(fixedLengthWithOffset); }, TypeError);
    assertThrows(() => { Object.freeze(lengthTracking); }, TypeError);
    assertThrows(() => { Object.freeze(lengthTrackingWithOffset); }, TypeError);
  }
  // Freezing zero-length TAs doesn't throw.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 0);
    const fixedLengthWithOffset = new ctor(
        rab, 2 * ctor.BYTES_PER_ELEMENT, 0);
    // Zero-length because the offset is at the end:
    const lengthTrackingWithOffset = new ctor(
        rab, 4 * ctor.BYTES_PER_ELEMENT);

    Object.freeze(fixedLength);
    Object.freeze(fixedLengthWithOffset);
    Object.freeze(lengthTrackingWithOffset);
  }
  // If the buffer has been resized to make length-tracking TAs zero-length,
  // freezing them also doesn't throw.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                           8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab, );
    const lengthTrackingWithOffset = new ctor(
        rab, 2 * ctor.BYTES_PER_ELEMENT);

    rab.resize(2 * ctor.BYTES_PER_ELEMENT);
    Object.freeze(lengthTrackingWithOffset);

    rab.resize(0 * ctor.BYTES_PER_ELEMENT);
    Object.freeze(lengthTracking);
  }
})();
