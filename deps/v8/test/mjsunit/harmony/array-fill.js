// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-arrays

assertEquals(1, Array.prototype.fill.length);

assertArrayEquals([].fill(8), []);
assertArrayEquals([0, 0, 0, 0, 0].fill(), [undefined, undefined, undefined, undefined, undefined]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8), [8, 8, 8, 8, 8]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, 1), [0, 8, 8, 8, 8]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, 10), [0, 0, 0, 0, 0]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, -5), [8, 8, 8, 8, 8]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, 1, 4), [0, 8, 8, 8, 0]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, 1, -1), [0, 8, 8, 8, 0]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, 1, 42), [0, 8, 8, 8, 8]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, -3, 42), [0, 0, 8, 8, 8]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, -3, 4), [0, 0, 8, 8, 0]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, -2, -1), [0, 0, 0, 8, 0]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, -1, -3), [0, 0, 0, 0, 0]);
assertArrayEquals([0, 0, 0, 0, 0].fill(8, undefined, 4), [8, 8, 8, 8, 0]);
assertArrayEquals([ ,  ,  ,  , 0].fill(8, 1, 3), [, 8, 8, , 0]);

// If the range is empty, the array is not actually modified and
// should not throw, even when applied to a frozen object.
assertArrayEquals(Object.freeze([1, 2, 3]).fill(0, 0, 0), [1, 2, 3]);

// Test exceptions
assertThrows('Object.freeze([0]).fill()', TypeError);
assertThrows('Array.prototype.fill.call(null)', TypeError);
assertThrows('Array.prototype.fill.call(undefined)', TypeError);
