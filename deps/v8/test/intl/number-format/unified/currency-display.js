// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-numberformat-unified

// Test defaults
let nf = new Intl.NumberFormat();
assertEquals(undefined, nf.resolvedOptions().currencyDisplay);

nf = new Intl.NumberFormat("en");
assertEquals(undefined, nf.resolvedOptions().currencyDisplay);

nf = new Intl.NumberFormat("en", {style: "decimal"});
assertEquals(undefined, nf.resolvedOptions().currencyDisplay);

nf = new Intl.NumberFormat("en", {style: "percent"});
assertEquals(undefined, nf.resolvedOptions().currencyDisplay);

nf = new Intl.NumberFormat("en", {style: "unit", unit: "meter"});
assertEquals(undefined, nf.resolvedOptions().currencyDisplay);

nf = new Intl.NumberFormat("en", {style: "currency", currency: "TWD"});
assertEquals("symbol", nf.resolvedOptions().currencyDisplay);

const testData = [
    ["name", "123.00 New Taiwan dollars"],
    ["code", "TWD 123.00"],
    ["symbol", "NT$123.00"],
    ["narrow-symbol", "$123.00"],  // new
];

for (const [currencyDisplay, expectation] of testData) {
  nf = new Intl.NumberFormat("en",
      {style: 'currency', currency: "TWD", currencyDisplay});
  assertEquals('currency', nf.resolvedOptions().style);
  assertEquals(currencyDisplay, nf.resolvedOptions().currencyDisplay);
  assertEquals(expectation, nf.format(123));
}
