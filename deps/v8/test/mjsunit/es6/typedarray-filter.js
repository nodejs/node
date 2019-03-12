// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array];

function TestTypedArrayFilter(constructor) {
  assertEquals(1, constructor.prototype.filter.length);

  // Throw type error if source array is detached while executing a callback
  let ta1 = new constructor(10);
  assertThrows(() =>
    ta1.filter(() => %ArrayBufferDetach(ta1.buffer))
  , TypeError);

  // A new typed array should be created after finishing callbacks
  var speciesCreated = 0;

  class MyTypedArray extends constructor {
    static get [Symbol.species]() {
      return function() {
        speciesCreated++;
        return new constructor(10);
      };
    }
  }

  new MyTypedArray(10).filter(() => {
    assertEquals(0, speciesCreated);
    return true;
  });
  assertEquals(1, speciesCreated);

  // A new typed array should be initialized to 0s
  class LongTypedArray extends constructor {
    static get [Symbol.species]() {
      return function(len) {
        return new constructor(len * 2);
      }
    }
  }
  let ta2 = new LongTypedArray(3).fill(1);
  let ta3 = ta2.filter((val, index, array) => val > 0);
  assertArrayEquals(ta3, [1, 1, 1, 0, 0, 0]);
  assertEquals(ta3.constructor, constructor);

  // Throw if a new typed array is too small
  class ShortTypedArray extends constructor {
    static get [Symbol.species]() {
      return function(len) {
        return new constructor(len/2);
      }
    }
  }
  assertThrows(() => new ShortTypedArray(10).filter(() => true));

  // Throw if callback is not callable
  assertThrows(() => new constructor(10).filter(123));
  assertThrows(() => new constructor(10).filter({}));

  // Empty result
  assertEquals(new constructor(10).filter(() => false), new constructor(0));

  // If a new typed array shares a buffer with a source array
  let ab = new ArrayBuffer(100);
  class SharedBufferTypedArray extends constructor {
    static get [Symbol.species]() {
      return function(len) {
        return new constructor(ab, 0, 5);
      }
    }
  }
  let ta4 = new SharedBufferTypedArray(ab, 0, 5).fill(1);
  let ta5 = ta4.filter(() => {
    ta4[0] = 123;
    ta4[2] = 123;
    return true;
  });
  assertEquals(ta4.buffer, ta5.buffer);
  assertArrayEquals(ta4, [1, 1, 123, 1, 1]);
  assertArrayEquals(ta5, [1, 1, 123, 1, 1]);

  // If a new typed array has a different type with source array
  for (let j = 0; j < typedArrayConstructors.length; j++) {
    let otherConstructor = typedArrayConstructors[j];
    class OtherTypedArray extends constructor {
      static get [Symbol.species]() {
        return function(len) {
          return new otherConstructor(len);
        }
      }
    }
    let ta6 = new OtherTypedArray(10).fill(123);
    assertEquals(ta6.filter(() => true), new otherConstructor(10).fill(123));
  }
}

for (i = 0; i < typedArrayConstructors.length; i++) {
  TestTypedArrayFilter(typedArrayConstructors[i]);
}
