// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let validRoundingIncrements = [
  1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000];
let maximumFractionDigits = 3;
let minimumFractionDigits = maximumFractionDigits;

validRoundingIncrements.forEach(function(roundingIncrement) {
  let nf = new Intl.NumberFormat(undefined,
      {roundingIncrement, minimumFractionDigits, maximumFractionDigits});
  assertEquals(roundingIncrement, nf.resolvedOptions().roundingIncrement);
});
