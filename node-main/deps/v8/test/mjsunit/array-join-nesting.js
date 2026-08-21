// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DEPTH = 128;

function makeNestedArray(depth, value) {
  return depth > 0 ? [value, makeNestedArray(depth - 1, value)] : [value];
}

const array = makeNestedArray(DEPTH, 'a');
const expected = 'a' + ',a'.repeat(DEPTH);
assertSame(expected, array.join());

// Verify cycle detection is still working.
assertSame(expected, array.join());
