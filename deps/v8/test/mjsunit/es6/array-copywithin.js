// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function copyWithinArity() {
  assertEquals(Array.prototype.copyWithin.length, 2);
})();


(function copyWithinTargetAndStart() {
  // works with two arguemnts
  assertArrayEquals([4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, 3));
  assertArrayEquals([1, 4, 5, 4, 5], [1, 2, 3, 4, 5].copyWithin(1, 3));
  assertArrayEquals([1, 3, 4, 5, 5], [1, 2, 3, 4, 5].copyWithin(1, 2));
  assertArrayEquals([1, 2, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(2, 2));
})();


(function copyWithinTargetStartAndEnd() {
  // works with three arguments
  assertArrayEquals([1, 2, 3, 4, 5].copyWithin(0, 3, 4), [4, 2, 3, 4, 5]);
  assertArrayEquals([1, 2, 3, 4, 5].copyWithin(1, 3, 4), [1, 4, 3, 4, 5]);
  assertArrayEquals([1, 2, 3, 4, 5].copyWithin(1, 2, 4), [1, 3, 4, 4, 5]);
})();


(function copyWithinNegativeRelativeOffsets() {
  // works with negative arguments
  assertArrayEquals([4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, -2));
  assertArrayEquals([4, 2, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, -2, -1));
  assertArrayEquals([1, 3, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(-4, -3, -2));
  assertArrayEquals([1, 3, 4, 4, 5], [1, 2, 3, 4, 5].copyWithin(-4, -3, -1));
  assertArrayEquals([1, 3, 4, 5, 5], [1, 2, 3, 4, 5].copyWithin(-4, -3));
  // test with arguments equal to -this.length
  assertArrayEquals([1, 2, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(-5, 0));
})();


(function copyWithinArrayLikeValues() {
  // works with array-like values
  var args = (function () { return arguments; }(1, 2, 3));
  Array.prototype.copyWithin.call(args, -2, 0);
  assertArrayEquals([1, 1, 2], Array.prototype.slice.call(args));

  // Object.prototype.toString branding does not change
  assertArrayEquals("[object Arguments]", Object.prototype.toString.call(args));
})();


(function copyWithinNullThis() {
  // throws on null/undefined values
  assertThrows(function() {
    return Array.prototype.copyWithin.call(null, 0, 3);
  }, TypeError);
})();


(function copyWithinUndefinedThis() {
  assertThrows(function() {
    return Array.prototype.copyWithin.call(undefined, 0, 3);
  }, TypeError);
})();


// TODO(caitp): indexed properties of String are read-only and setting them
//              should throw in strict mode. See bug v8:4042
// (function copyWithinStringThis() {
//   // test with this value as string
//   assertThrows(function() {
//     return Array.prototype.copyWithin.call("hello world", 0, 3);
//   }, TypeError);
// })();


(function copyWithinNumberThis() {
  // test with this value as number
  assertEquals(34, Array.prototype.copyWithin.call(34, 0, 3).valueOf());
})();


(function copyWithinSymbolThis() {
  // test with this value as number
  var sym = Symbol("test");
  assertEquals(sym, Array.prototype.copyWithin.call(sym, 0, 3).valueOf());
})();


(function copyyWithinTypedArray() {
  // test with this value as TypedArray
  var buffer = new ArrayBuffer(16);
  var int32View = new Int32Array(buffer);
  for (var i=0; i<int32View.length; i++) {
    int32View[i] = i*2;
  }
  assertArrayEquals(new Int32Array([2, 4, 6, 6]),
                    Array.prototype.copyWithin.call(int32View, 0, 1));
})();


(function copyWithinSloppyArguments() {
  // if arguments object is sloppy, copyWithin must move the arguments around
  function f(a, b, c, d, e) {
    [].copyWithin.call(arguments, 1, 3);
    return [a, b, c, d, e];
  }
  assertArrayEquals([1, 4, 5, 4, 5], f(1, 2, 3, 4, 5));
})();


(function copyWithinStartLessThanTarget() {
  // test with target > start on 2 arguments
  assertArrayEquals([1, 2, 3, 1, 2], [1, 2, 3, 4, 5].copyWithin(3, 0));

  // test with target > start on 3 arguments
  assertArrayEquals([1, 2, 3, 1, 2], [1, 2, 3, 4, 5].copyWithin(3, 0, 4));
})();


(function copyWithinArrayWithHoles() {
  // test on array with holes
  var arr = new Array(6);
  for (var i = 0; i < arr.length; i += 2) {
    arr[i] = i;
  }
  assertArrayEquals([, 4, , , 4, , ], arr.copyWithin(0, 3));
})();


(function copyWithinArrayLikeWithHoles() {
  // test on array-like object with holes
  assertArrayEquals({
    length: 6,
    1: 4,
    4: 4
  }, Array.prototype.copyWithin.call({
    length: 6,
    0: 0,
    2: 2,
    4: 4
  }, 0, 3));
})();


(function copyWithinNonIntegerRelativeOffsets() {
  // test on fractional arguments
  assertArrayEquals([4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0.2, 3.9));
})();


(function copyWithinNegativeZeroTarget() {
  // test with -0
  assertArrayEquals([4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(-0, 3));
})();


(function copyWithinTargetOutsideStart() {
  // test with arguments more than this.length
  assertArrayEquals([1, 2, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, 7));

  // test with arguments less than -this.length
  assertArrayEquals([1, 2, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(-7, 0));
})();


(function copyWithinEmptyArray() {
  // test on empty array
  assertArrayEquals([], [].copyWithin(0, 3));
})();


(function copyWithinTargetCutOff() {
  // test with target range being shorter than end - start
  assertArrayEquals([1, 2, 2, 3, 4], [1, 2, 3, 4, 5].copyWithin(2, 1, 4));
})();


(function copyWithinOverlappingRanges() {
  // test overlapping ranges
  var arr = [1, 2, 3, 4, 5];
  arr.copyWithin(2, 1, 4);
  assertArrayEquals([1, 2, 2, 2, 3], arr.copyWithin(2, 1, 4));
})();


(function copyWithinStrictDelete() {
  // check that [[Delete]] is strict (non-extensible via freeze)
  assertThrows(function() {
    return Object.freeze([1, , 3, , 4, 5]).copyWithin(2, 1, 4);
  }, TypeError);

  // check that [[Delete]] is strict (non-extensible via seal)
  assertThrows(function() {
    return Object.seal([1, , 3, , 4, 5]).copyWithin(2, 1, 4);
  }, TypeError);

  // check that [[Delete]] is strict (non-extensible via preventExtensions)
  assertThrows(function() {
    return Object.preventExtensions([1, , 3, , 4, 5]).copyWithin(2, 1, 4);
  }, TypeError);
})();


(function copyWithinStrictSet() {
  // check that [[Set]] is strict (non-extensible via freeze)
  assertThrows(function() {
    return Object.freeze([1, 2, 3, 4, 5]).copyWithin(0, 3);
  }, TypeError);

  // check that [[Set]] is strict (non-extensible via seal)
  assertThrows(function() {
    return Object.seal([, 2, 3, 4, 5]).copyWithin(0, 3);
  }, TypeError);

  // check that [[Set]] is strict (non-extensible via preventExtensions)
  assertThrows(function() {
    return Object.preventExtensions([ , 2, 3, 4, 5]).copyWithin(0, 3);
  }, TypeError);
})();


(function copyWithinSetterThrows() {
  function Boom() {}
  // test if we throw in between
  var arr = Object.defineProperty([1, 2, 3, 4, 5], 1, {
    set: function () {
      throw new Boom();
    }
  });

  assertThrows(function() {
    return arr.copyWithin(1, 3);
  }, Boom);

  assertArrayEquals([1, , 3, 4, 5], arr);
})();


(function copyWithinDefaultEnd() {
  // undefined as third argument
  assertArrayEquals(
      [4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, 3, undefined));
})();


(function copyWithinGetLengthOnce() {
  // test that this.length is called only once
  var count = 0;
  var arr = Object.defineProperty({ 0: 1, 1: 2, 2: 3, 3: 4, 4: 5 }, "length", {
    get: function () {
      count++;
      return 5;
    }
  });
  Array.prototype.copyWithin.call(arr, 1, 3);
  assertEquals(1, count);

  Array.prototype.copyWithin.call(arr, 1, 3, 4);
  assertEquals(2, count);
})();


(function copyWithinLargeArray() {
  var large = 10000;

  // test on a large array
  var arr = new Array(large);
  assertArrayEquals(arr, arr.copyWithin(45, 9000));

  var expected = new Array(large);
  // test on floating point numbers
  for (var i = 0; i < large; i++) {
    arr[i] = Math.random();
    expected[i] = arr[i];
    if (i >= 9000) {
      expected[(i - 9000) + 45] = arr[i];
    }
  }
  assertArrayEquals(expected, arr.copyWithin(45, 9000));

  // test on array of objects
  for (var i = 0; i < large; i++) {
    arr[i] = { num: Math.random() };
  } + 45
  arr.copyWithin(45, 9000);

  // test copied by reference
  for (var i = 9000; i < large; ++i) {
    assertSame(arr[(i - 9000) + 45], arr[i]);
  }

  // test array length remains same
  assertEquals(large, arr.length);
})();


(function copyWithinSuperLargeLength() {
  // 2^53 - 1 is the maximum value returned from ToLength()
  var large = Math.pow(2, 53) - 1;
  var object = { length: large };

  // Initialize last 10 entries
  for (var i = 1; i <= 10; ++i) {
    object[(large - 11) + i] = { num: i };
  }

  Array.prototype.copyWithin.call(object, 1, large - 10);

  // Test copied values
  for (var i = 1; i <= 10; ++i) {
    var old_ref = object[(large - 11) + i];
    var new_ref = object[i];
    assertSame(old_ref, new_ref);
    assertSame(new_ref.num, i);
  }

  // Assert length has not changed
  assertEquals(large, object.length);
})();


(function copyWithinNullEnd() {
  // test null on third argument is converted to +0
  assertArrayEquals([1, 2, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, 3, null));
})();


(function copyWithinElementsInObjectsPrototype() {
  // tamper the global Object prototype and test this works
  Object.prototype[2] = 1;
  assertArrayEquals([4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, 3));
  delete Object.prototype[2];

  Object.prototype[3] = "FAKE";
  assertArrayEquals(["FAKE", 5, 3, "FAKE", 5], [1, 2, 3, , 5].copyWithin(0, 3));
  delete Object.prototype[3];
})();
