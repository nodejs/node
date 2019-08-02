// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-numberformat-unified

// Test default.
let nf = new Intl.NumberFormat();
assertEquals(undefined, nf.resolvedOptions().unitDisplay);

nf = new Intl.NumberFormat("en");
assertEquals(undefined, nf.resolvedOptions().unitDisplay);

nf = new Intl.NumberFormat("en", {style: 'decimal'});
assertEquals(undefined, nf.resolvedOptions().unitDisplay);

nf = new Intl.NumberFormat("en", {style: 'currency', currency: 'TWD'});
assertEquals(undefined, nf.resolvedOptions().unitDisplay);

nf = new Intl.NumberFormat("en", {style: 'unit', unit: "meter"});
assertEquals("short", nf.resolvedOptions().unitDisplay);

nf = new Intl.NumberFormat("en", {style: 'percent'});
assertEquals("short", nf.resolvedOptions().unitDisplay);

const testData = [
    ["short"],
    ["narrow"],
    ["long"],
];

for (const [unitDisplay] of testData) {
  nf = new Intl.NumberFormat("en", {style: 'unit', unit: "meter", unitDisplay});
  assertEquals('unit', nf.resolvedOptions().style);
  assertEquals(unitDisplay, nf.resolvedOptions().unitDisplay);
}
