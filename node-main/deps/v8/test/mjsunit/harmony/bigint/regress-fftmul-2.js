// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function fastAssertEquals(expected, actual) {
  if (expected === actual) return;
  // Formatting large BigInts to base-10 string is slow and verbose, so format
  // them to hex strings and report only a prefix.
  throw new MjsUnitAssertionError(
      'Failure:\nexpected:\n0x' +
      expected.toString(16).substring(0, 1000) +
      '...n\nfound:\n0x' +
      actual.toString(16).substring(0, 1000) + '...n');
}

function regress_1228267(power) {
  let a = 2n ** power;
  let a_squared = a * a;
  let expected = 2n ** (2n * power);
  fastAssertEquals(expected, a_squared);
}
regress_1228267(1273000n);  // This triggered the bug on 32-bit platforms.
regress_1228267(2564000n);  // This triggered the bug on 64-bit platforms.
