// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

function CheckEachTypedArray(fn) {
  typedArrayConstructors.forEach(fn);
}

CheckEachTypedArray(function copyWithinArity(constructor) {
  assertEquals(new constructor([]).copyWithin.length, 2);
});


CheckEachTypedArray(function copyWithinTargetAndStart(constructor) {
  // works with two arguments
  assertArrayEquals([4, 5, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3));
  assertArrayEquals([1, 4, 5, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(1, 3));
  assertArrayEquals([1, 3, 4, 5, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(1, 2));
  assertArrayEquals([1, 2, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(2, 2));
});


CheckEachTypedArray(function copyWithinTargetStartAndEnd(constructor) {
  // works with three arguments
  assertArrayEquals(new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3, 4),
                                    [4, 2, 3, 4, 5]);
  assertArrayEquals(new constructor([1, 2, 3, 4, 5]).copyWithin(1, 3, 4),
                                    [1, 4, 3, 4, 5]);
  assertArrayEquals(new constructor([1, 2, 3, 4, 5]).copyWithin(1, 2, 4),
                                    [1, 3, 4, 4, 5]);
});


CheckEachTypedArray(function copyWithinNegativeRelativeOffsets(constructor) {
  // works with negative arguments
  assertArrayEquals([4, 5, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0, -2));
  assertArrayEquals([4, 2, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0, -2, -1));
  assertArrayEquals([1, 3, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(-4, -3, -2));
  assertArrayEquals([1, 3, 4, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(-4, -3, -1));
  assertArrayEquals([1, 3, 4, 5, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(-4, -3));
  // test with arguments equal to -this.length
  assertArrayEquals([1, 2, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(-5, 0));
});


CheckEachTypedArray(function mustBeTypedArray(constructor) {
  // throws on non-TypedArray values
  assertThrows(function() {
    return constructor.prototype.copyWithin.call(null, 0, 3);
  }, TypeError);
  assertThrows(function() {
    return constructor.prototype.copyWithin.call(undefined, 0, 3);
  }, TypeError);
  assertThrows(function() {
    return constructor.prototype.copyWithin.call(34, 0, 3);
  }, TypeError);
  assertThrows(function() {
    return constructor.prototype.copyWithin.call([1, 2, 3, 4, 5], 0, 3);
  }, TypeError);
});


CheckEachTypedArray(function copyWithinStartLessThanTarget(constructor) {
  // test with target > start on 2 arguments
  assertArrayEquals([1, 2, 3, 1, 2],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(3, 0));

  // test with target > start on 3 arguments
  assertArrayEquals([1, 2, 3, 1, 2],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(3, 0, 4));
});

CheckEachTypedArray(function copyWithinNonIntegerRelativeOffsets(constructor) {
  // test on fractional arguments
  assertArrayEquals([4, 5, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0.2, 3.9));
});


CheckEachTypedArray(function copyWithinNegativeZeroTarget(constructor) {
  // test with -0
  assertArrayEquals([4, 5, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(-0, 3));
});


CheckEachTypedArray(function copyWithinTargetOutsideStart(constructor) {
  // test with arguments more than this.length
  assertArrayEquals([1, 2, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0, 7));

  // test with arguments less than -this.length
  assertArrayEquals([1, 2, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(-7, 0));
});


CheckEachTypedArray(function copyWithinEmptyArray(constructor) {
  // test on empty array
  assertArrayEquals([], new constructor([]).copyWithin(0, 3));
});


CheckEachTypedArray(function copyWithinTargetCutOff(constructor) {
  // test with target range being shorter than end - start
  assertArrayEquals([1, 2, 2, 3, 4], [1, 2, 3, 4, 5].copyWithin(2, 1, 4));
});


CheckEachTypedArray(function copyWithinOverlappingRanges(constructor) {
  // test overlapping ranges
  var arr = [1, 2, 3, 4, 5];
  arr.copyWithin(2, 1, 4);
  assertArrayEquals([1, 2, 2, 2, 3], arr.copyWithin(2, 1, 4));
});


CheckEachTypedArray(function copyWithinDefaultEnd(constructor) {
  // undefined as third argument
  assertArrayEquals([4, 5, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3, undefined));
});


CheckEachTypedArray(function copyWithinLargeArray(constructor) {
  var large = 10000;

  // test on a large array
  var arr = new constructor(large);
  assertArrayEquals(arr, arr.copyWithin(45, 9000));

  var expected = new Array(large);
  // test on numbers
  for (var i = 0; i < large; i++) {
    arr[i] = Math.random() * 100;  // May be cast to an int
    expected[i] = arr[i];
    if (i >= 9000) {
      expected[(i - 9000) + 45] = arr[i];
    }
  }
  assertArrayEquals(expected, arr.copyWithin(45, 9000));

  // test array length remains same
  assertEquals(large, arr.length);
});


CheckEachTypedArray(function copyWithinNullEnd(constructor) {
  // test null on third argument is converted to +0
  assertArrayEquals([1, 2, 3, 4, 5],
                    new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3, null));
});
