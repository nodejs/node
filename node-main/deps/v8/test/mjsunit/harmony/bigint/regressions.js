// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = 5n;
var b = a / -1n;
assertEquals(5n, a);
assertEquals(-5n, b);
assertEquals(5n, 5n / 1n);
assertEquals(5n, -5n / -1n);
assertEquals(-5n, -5n / 1n);

assertEquals(0n, 5n % 1n);
assertEquals(0n, -5n % 1n);
assertEquals(0n, 5n % -1n);
assertEquals(0n, -5n % -1n);

assertTrue(0n === 0n);

// crbug.com/818277: Must throw without DCHECK failures.
// In order to run acceptably fast in Debug mode, this test assumes that
// we allow at least 1 billion bits in a BigInt.
var close_to_limit = 2n ** 1000000000n;
assertThrows(() => close_to_limit ** 100n, RangeError);

// Check boundary conditions of the power-of-two fast path.
// The following "max" constants are just under BigInt::kMaxLengthBits
// and replicate the computation of that constant.
var kMaxInt = 2n ** 31n - 1n;
var max64 = kMaxInt - 64n - 2n;
var max32 = kMaxInt - 32n - 2n;
// Platform independence trick: at least one of the two operations will throw!
assertThrows(() => { var a = 2n ** max32; var b = 2n ** max64; }, RangeError);

(function() {
  function Constructor() { }
  Constructor.prototype = 5n;
  assertThrows(() => ({}) instanceof Constructor, TypeError);
})();
