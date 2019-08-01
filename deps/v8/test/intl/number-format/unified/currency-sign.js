// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-numberformat-unified

// Test default.
let nf = new Intl.NumberFormat();
assertEquals(undefined, nf.resolvedOptions().currencySign);

nf = new Intl.NumberFormat("en");
assertEquals(undefined, nf.resolvedOptions().currencySign);

nf = new Intl.NumberFormat("en", {style: 'decimal'});
assertEquals(undefined, nf.resolvedOptions().currencySign);

nf = new Intl.NumberFormat("en", {style: 'percent'});
assertEquals(undefined, nf.resolvedOptions().currencySign);

nf = new Intl.NumberFormat("en", {style: 'unit', unit: "meter"});
assertEquals(undefined, nf.resolvedOptions().currencySign);


nf = new Intl.NumberFormat("en", {style: 'currency', currency: "TWD"});
assertEquals("standard", nf.resolvedOptions().currencySign);

const testData = [
    ["standard", "-NT$123.40", "-NT$0.00", "NT$0.00", "NT$123.40"],
    ["accounting", "(NT$123.40)", "(NT$0.00)", "NT$0.00", "NT$123.40"],
];

for (const [currencySign, neg, negZero, zero, pos] of testData) {
  nf = new Intl.NumberFormat("en", {style: 'currency', currency: "TWD",
    currencySign});
  assertEquals('currency', nf.resolvedOptions().style);
  assertEquals(currencySign, nf.resolvedOptions().currencySign);
  assertEquals(neg, nf.format(-123.4));
  assertEquals(negZero, nf.format(-0));
  assertEquals(zero, nf.format(0));
  assertEquals(pos, nf.format(123.4));
}
