// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const magic0 = 2396;
const magic1 = 1972;

// Fill xs with float arrays.
const xs = [];
for (let j = 0; j < magic0; ++j) {
  xs[j] = [j + 0.1];
}

// Sort, but trim the array at some point.
let cmp_calls = 0;
xs.sort((lhs, rhs) => {
  lhs = lhs || [0];
  rhs = rhs || [0];
  if (cmp_calls++ == magic1) xs.length = 1;
  return lhs[0] - rhs[0];
});

// The final shape of the array is unspecified since the comparison function is
// inconsistent.
