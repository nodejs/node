// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-normalization

const v0 = [595538.0296383442, -4.0];
const v1 = [
  -1.7976931348623157e+308,
  0.5821211259518391,
  1000000.0,
  0.3229450389090781,
  6.98253182412007e+307
];

function f2(a3) {
  function f4(a5) { return a5; }
  Object.defineProperty(a3, "x", { configurable: true, set: f4 });
  return v1;
}

f2(v1);
const v8 = Array(v1);
v8[0] = v0;
const v9 = f2(v8);
v9[2] = v9; // Causes an elements kind transition.
