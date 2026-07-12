// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let validRoundingIncrements = [
  1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000];

let invalidRoundingIncrements = [
  -1, -5, 0, 3, 1001, 1100, 5500, 10000, 20000, 25000, 100000, 200000, 500005,
  10000000
];

validRoundingIncrements.forEach(function(roundingIncrement) {
  assertDoesNotThrow(() => {
    new Intl.NumberFormat(undefined, {roundingIncrement,
      minimumFractionDigits:3})});
  for (mfd = 0; mfd < 20; mfd++) {
    let fn = new Intl.NumberFormat(undefined, {
      roundingIncrement,
      minimumFractionDigits: mfd, maximumFractionDigits: mfd});
    let res = fn.resolvedOptions();
    assertEquals(roundingIncrement, res.roundingIncrement);
    assertEquals(mfd, res.minimumFractionDigits);
    assertEquals(mfd, res.maximumFractionDigits);
  }
  // Test
  // "If roundingIncrement is not 1 and numberFormat.[[MaximumFractionDigits]]
  // is not equal to numberFormat.[[MinimumFractionDigits]], throw a RangeError
  // exception.
  if (roundingIncrement == 1) return;
  for (mnfd = 0; mnfd < 20; mnfd++) {
    for (mxfd = mnfd+1; mxfd < 20; mxfd++) {
      assertThrows(() => {
        let nf = new Intl.NumberFormat(undefined, {
          roundingIncrement, minimumFractionDigits:mnfd,
          maximumFractionDigits: mxfd})},
        RangeError);
    }
  }
});

invalidRoundingIncrements.forEach(function(roundingIncrement) {
  assertThrows(() => {
    let nf = new Intl.NumberFormat(undefined,
        {roundingIncrement, minimumFractionDigits:3})},
    RangeError);
});
