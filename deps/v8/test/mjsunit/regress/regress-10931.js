// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kLargerThanFixedArrayMaxLength = 200_000_000;
var x = new Int8Array(kLargerThanFixedArrayMaxLength);
try {
  var y = x.sort((a, b) => b - a);
} catch (e) {
  // Throwing is okay, crashing is not.
  assertInstanceof(e, TypeError);
  assertMatches(
      /not supported for huge TypedArrays/, e.message, 'Error message');
}
