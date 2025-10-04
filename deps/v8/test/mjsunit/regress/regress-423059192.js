// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

function observableHoleOnRhs(array, otherValue) {
  let value = array[0];
  return [otherValue - value, value];
}
let testArray = new Array();
for (let i = 0; i < 1e4; ++i) {
  let rhsResult2 = observableHoleOnRhs(testArray, undefined);
  if (rhsResult2[0] == rhsResult2[0] || rhsResult2[1] !== undefined) {
    throw "Error on observableHoleOnRhs at i = " + i;
  }
}
