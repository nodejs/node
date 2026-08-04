// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let validRoundingMode = [
    "ceil",
    "floor",
    "expand",
    "halfCeil",
    "halfExpand",
    "halfFloor",
    "halfTrunc",
    "halfEven",
    "trunc",
];

let invalidRoundingMode = [
    "ceiling",
    "down",
    "Down",
    "flooring",
    "halfDown",
    "halfUp",
    "halfup",
    "halfeven",
    "halfdown",
    "half-up",
    "half-even",
    "half-down",
    "up",
    "Up",
];

validRoundingMode.forEach(function(roundingMode) {
  let nf = new Intl.NumberFormat(undefined, {roundingMode});
  assertEquals(roundingMode, nf.resolvedOptions().roundingMode);
});

invalidRoundingMode.forEach(function(roundingMode) {
  assertThrows(() => {
    let nf = new Intl.NumberFormat(undefined, {roundingMode}); });
});

// Check default is "halfExpand"
assertEquals("halfExpand", (new Intl.NumberFormat().resolvedOptions().roundingMode));
assertEquals("halfExpand", (new Intl.NumberFormat(
    undefined, {roundingMode: undefined}).resolvedOptions().roundingMode));

// Check roundingMode is read once after reading signDisplay

let read = [];
let options = {
  get signDisplay() { read.push('signDisplay'); return undefined; },
  get roundingMode() { read.push('roundingMode'); return undefined; },
};

assertDoesNotThrow(() => new Intl.NumberFormat(undefined, options));
assertEquals("roundingMode,signDisplay", read.join(","));
