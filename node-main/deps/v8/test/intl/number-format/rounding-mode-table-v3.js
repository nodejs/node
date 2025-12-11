// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check the rounding behavior.
// Based on https://tc39.es/proposal-intl-numberformat-v3/out/numberformat/diff.html#table-intl-rounding-modes
let inputs = [-1.5, 0.4, 0.5, 0.6, 1.5];
let expectations = {
  "ceil":       ["-1", "1", "1", "1", "2"],
  "floor":      ["-2", "0", "0", "0", "1"],
  "expand":     ["-2", "1", "1", "1", "2"],
  "trunc":      ["-1", "0", "0", "0", "1"],
  "halfCeil":   ["-1", "0", "1", "1", "2"],
  "halfFloor":  ["-2", "0", "0", "1", "1"],
  "halfExpand": ["-2", "0", "1", "1", "2"],
  "halfTrunc":  ["-1", "0", "0", "1", "1"],
  "halfEven":   ["-2", "0", "0", "1", "2"],
};
Object.keys(expectations).forEach(function(roundingMode) {
  let exp = expectations[roundingMode];
  let idx = 0;
  let nf = new Intl.NumberFormat("en", {roundingMode, maximumFractionDigits: 0});
  assertEquals(roundingMode, nf.resolvedOptions().roundingMode);
  inputs.forEach(function(input) {
    let msg = "input: " + input + " with roundingMode: " + roundingMode;
    assertEquals(exp[idx++], nf.format(input), msg);
  })
});
