// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-numberformat-unified
//
// Test the handling of "percent" w/ "unit"

let nf1 = new Intl.NumberFormat("en-US", {
  style: "percent",
  unitDisplay: "long"  // Read, but ignored.
});

let resolved1 = nf1.resolvedOptions();
assertEquals("percent", resolved1.style);
assertEquals(undefined, resolved1.unit);
assertEquals(undefined, resolved1.unitDisplay);

let parts1 = nf1.formatToParts(100);
assertEquals(4, parts1.length);
assertEquals("integer", parts1[0].type);
assertEquals("10", parts1[0].value);
assertEquals("group", parts1[1].type);
assertEquals(",", parts1[1].value);
assertEquals("integer", parts1[2].type);
assertEquals("000", parts1[2].value);
assertEquals("percentSign", parts1[3].type);
assertEquals("%", parts1[3].value);

let nf2 = new Intl.NumberFormat("en-US", {
  style: "unit",
  unit: "percent",
  unitDisplay: "long"  // This is OK
});

let resolved2 = nf2.resolvedOptions();
assertEquals("unit", resolved2.style);
assertEquals("percent", resolved2.unit);
assertEquals("long", resolved2.unitDisplay);

let parts2 = nf2.formatToParts(100);
assertEquals(3, parts2.length);
assertEquals("integer", parts2[0].type);
assertEquals("100", parts2[0].value);
assertEquals("literal", parts2[1].type);
assertEquals(" ", parts2[1].value);
assertEquals("unit", parts2[2].type);
assertEquals("percent", parts2[2].value);

let nf3 = new Intl.NumberFormat("en-US", {
  style: "unit",
  unit: "percent"
});

let resolved3 = nf3.resolvedOptions();
assertEquals("unit", resolved3.style);
assertEquals("percent", resolved3.unit);
assertEquals("short", resolved3.unitDisplay);

let parts3 = nf3.formatToParts(100);
assertEquals(2, parts3.length);
assertEquals("integer", parts3[0].type);
assertEquals("100", parts3[0].value);
assertEquals("unit", parts3[1].type);
assertEquals("%", parts3[1].value);
