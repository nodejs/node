// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertPresentOfDigits(
    options, expectSignificant, expectFractional, user_message) {
  if (expectSignificant) {
    assertNotUndefined(options.minimumSignificantDigits, user_message);
    assertNotUndefined(options.maximumSignificantDigits, user_message);
  } else {
    assertEquals(undefined, options.minimumSignificantDigits, user_message);
    assertEquals(undefined, options.maximumSignificantDigits, user_message);
  }
  if (expectFractional) {
    assertNotUndefined(options.minimumFractionDigits, user_message);
    assertNotUndefined(options.maximumFractionDigits, user_message);
  } else {
    assertEquals(undefined, options.minimumFractionDigits, user_message);
    assertEquals(undefined, options.maximumFractionDigits, user_message);
  }
}

// Should contain ONLY significant digits (both v2 and v3)
let options = new Intl.NumberFormat("und",
    { maximumSignificantDigits: 3 }).resolvedOptions();
assertPresentOfDigits(options, true, false,
    "maximumSignificantDigits: 3");

// Should contain ONLY fraction digits (both v2 and v3)
options = new Intl.NumberFormat("und",
    { maximumFractionDigits: 3 }).resolvedOptions();
assertPresentOfDigits(options, false, true,
    "maximumFractionDigits: 3");

// in v2, this should NOT have EITHER significant nor fraction digits
// in v3, should contain BOTH significant and fraction digits, plus
// roundingPriority
options = new Intl.NumberFormat("und",
    { notation: "compact" }).resolvedOptions();
assertPresentOfDigits(options, true, true, "notation: 'compact'");

// in v2, should contain ONLY significant digits
// in v3, should contain BOTH significant and fraction digits, plus
// roundingPriority
options = new Intl.NumberFormat("und",
    { maximumSignificantDigits: 3, maximumFractionDigits: 3,
      roundingPriority: "morePrecision" }).resolvedOptions();
assertPresentOfDigits(options, true, true, "roundingPriority: 'morePrecision'");

// Should contain ONLY fraction digits (both v2 and v3)
options = new Intl.NumberFormat('en',
    { style: 'currency', currency: 'USD' }).resolvedOptions();
assertPresentOfDigits(options, false, true,
    "style: 'currency', currency: 'USD'");

// Should contain ONLY fraction digits (both v2 and v3)
options = new Intl.NumberFormat('en',
    { style: 'currency', currency: 'JPY' }).resolvedOptions();
assertPresentOfDigits(options, false, true,
    "style: 'currency', currency: 'JPY'");
