// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Largely ported from
// https://github.com/tc39/Array.prototype.includes/tree/master/test/built-ins/TypedArray/prototype/includes
// using https://www.npmjs.org/package/test262-to-mjsunit with further edits


function testTypedArrays(callback) {
  [
    Uint8Array,
    Int8Array,
    Uint16Array,
    Int16Array,
    Uint32Array,
    Int32Array,
    Uint8ClampedArray,
    Float32Array,
    Float64Array
  ]
  .forEach(callback);
}

testTypedArrays.floatOnly = function (callback) {
  [Float32Array, Float64Array].forEach(callback);
};


// %TypedArray%.prototype.includes throws a TypeError when used on non-typed
// arrays
(function() {
  var taIncludes = Uint8Array.prototype.includes;

  assertThrows(function() {
    taIncludes.call({
      length: 2,
      0: 1,
      1: 2
    }, 2);
  }, TypeError);

  assertThrows(function() {
    taIncludes.call([1, 2, 3], 2);
  }, TypeError);

  assertThrows(function() {
    taIncludes.call(null, 2);
  }, TypeError);

  assertThrows(function() {
    taIncludes.call(undefined, 2);
  }, TypeError);
})();


// %TypedArray%.prototype.includes should terminate if ToNumber ends up being
// called on a symbol fromIndex
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([1, 2, 3]);

    assertThrows(function() {
      ta.includes(2, Symbol());
    }, TypeError);
  });
})();


// %TypedArray%.prototype.includes should terminate if an exception occurs
// converting the fromIndex to a number
(function() {
  function Test262Error() {}

  var fromIndex = {
    valueOf: function() {
      throw new Test262Error();
    }
  };

  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([1, 2, 3]);

    assertThrows(function() {
      ta.includes(2, fromIndex);
    }, Test262Error);
  });
})();


// %TypedArray%.prototype.includes should search the whole array, as the
// optional second argument fromIndex defaults to 0
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([1, 2, 3]);
    assertTrue(ta.includes(1));
    assertTrue(ta.includes(2));
    assertTrue(ta.includes(3));
  });
})();


// %TypedArray%.prototype.includes returns false if fromIndex is greater or
// equal to the length of the array
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([1, 2]);
    assertFalse(ta.includes(2, 3));
    assertFalse(ta.includes(2, 2));
  });
})();


// %TypedArray%.prototype.includes searches the whole array if the computed
// index from the given negative fromIndex argument is less than 0
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([1, 3]);
    assertTrue(ta.includes(1, -4));
    assertTrue(ta.includes(1, -4));
  });
})();


// %TypedArray%.prototype.includes should use a negative value as the offset
// from the end of the array to compute fromIndex
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([12, 13]);
    assertTrue(ta.includes(13, -1));
    assertFalse(ta.includes(12, -1));
    assertTrue(ta.includes(12, -2));
  });
})();


// %TypedArray%.prototype.includes converts its fromIndex parameter to an
// integer
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([1, 2, 3]);
    assertFalse(ta.includes(1, 3.3));
    assertTrue(ta.includes(1, -Infinity));
    assertTrue(ta.includes(3, 2.9));
    assertTrue(ta.includes(3, NaN));

    var numberLike = {
      valueOf: function() {
        return 2;
      }
    };

    assertFalse(ta.includes(1, numberLike));
    assertFalse(ta.includes(1, "2"));
    assertTrue(ta.includes(3, numberLike));
    assertTrue(ta.includes(3, "2"));
  });
})();


// %TypedArray%.prototype.includes should have length 1
(function() {
  assertEquals(1, Uint8Array.prototype.includes.length);
})();


// %TypedArray%.prototype.includes should have name property with value
// 'includes'
(function() {
  assertEquals("includes", Uint8Array.prototype.includes.name);
})();


// %TypedArray%.prototype.includes should always return false on zero-length
// typed arrays
(function() {
  testTypedArrays(function(TypedArrayConstructor) {
    var ta = new TypedArrayConstructor([]);
    assertFalse(ta.includes(2));
    assertFalse(ta.includes());
    assertFalse(ta.includes(undefined));
    assertFalse(ta.includes(NaN));
  });
})();


// %TypedArray%.prototype.includes should use the SameValueZero algorithm to
// compare
(function() {
  testTypedArrays.floatOnly(function(FloatArrayConstructor) {
    assertTrue(new FloatArrayConstructor([1, 2, NaN]).includes(NaN));
    assertTrue(new FloatArrayConstructor([1, 2, -0]).includes(+0));
    assertTrue(new FloatArrayConstructor([1, 2, -0]).includes(-0));
    assertTrue(new FloatArrayConstructor([1, 2, +0]).includes(-0));
    assertTrue(new FloatArrayConstructor([1, 2, +0]).includes(+0));
    assertFalse(new FloatArrayConstructor([1, 2, -Infinity]).includes(+Infinity));
    assertTrue(new FloatArrayConstructor([1, 2, -Infinity]).includes(-Infinity));
    assertFalse(new FloatArrayConstructor([1, 2, +Infinity]).includes(-Infinity));
    assertTrue(new FloatArrayConstructor([1, 2, +Infinity]).includes(+Infinity));
  });
})();
