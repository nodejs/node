// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test defaults.

let nf = new Intl.NumberFormat();
assertEquals("standard", nf.resolvedOptions().notation);

nf = new Intl.NumberFormat("en");
assertEquals("standard", nf.resolvedOptions().notation);

nf = new Intl.NumberFormat("en", {style: "percent"});
assertEquals("standard", nf.resolvedOptions().notation);

const testData = [
    ["standard", undefined, "987,654,321"],
    ["scientific", undefined, "9.877E8"],
    ["engineering", undefined, "987.654E6"],
    ["compact", undefined, "988M"],
    ["compact", "short", "988M"],
    ["compact", "long", "988 million"],
];

for (const [notation, compactDisplay, expect1] of testData) {
  nf = new Intl.NumberFormat("en", {notation, compactDisplay});
  assertEquals(notation, nf.resolvedOptions().notation);
  if (notation != "compact") {
    assertEquals(undefined, nf.resolvedOptions().compactDisplay);
  } else if (compactDisplay == "long") {
    assertEquals("long", nf.resolvedOptions().compactDisplay);
  } else {
    assertEquals("short", nf.resolvedOptions().compactDisplay);
  }
  assertEquals(expect1, nf.format(987654321));
}

// Test Germany which has different decimal marks.
let notation = "compact";
nf = new Intl.NumberFormat("de", {notation, compactDisplay: "short"});
assertEquals("988 Mio.", nf.format(987654321));
assertEquals("99 Mio.", nf.format(98765432));
assertEquals("98.765", nf.format(98765));
assertEquals("9876", nf.format(9876));
nf = new Intl.NumberFormat("de", {notation, compactDisplay: "long"});
assertEquals("988 Millionen", nf.format(987654321));
assertEquals("99 Millionen", nf.format(98765432));
assertEquals("99 Tausend", nf.format(98765));
assertEquals("9,9 Tausend", nf.format(9876));

// Test Chinese, Japanese and Korean, which group by 4 digits.
nf = new Intl.NumberFormat("zh-TW", {notation, compactDisplay: "short"});
assertEquals("9.9億", nf.format(987654321));
assertEquals("9877萬", nf.format(98765432));
assertEquals("9.9萬", nf.format(98765));
assertEquals("9876", nf.format(9876));
nf = new Intl.NumberFormat("zh-TW", {notation, compactDisplay: "long"});
assertEquals("9.9億", nf.format(987654321));
assertEquals("9877萬", nf.format(98765432));
assertEquals("9.9萬", nf.format(98765));
assertEquals("9876", nf.format(9876));

// Test Japanese with compact.
nf = new Intl.NumberFormat("ja", {notation, compactDisplay: "short"});
assertEquals("9.9億", nf.format(987654321));
assertEquals("9877万", nf.format(98765432));
assertEquals("9.9万", nf.format(98765));
assertEquals("9876", nf.format(9876));
nf = new Intl.NumberFormat("ja", {notation, compactDisplay: "long"});
assertEquals("9.9億", nf.format(987654321));
assertEquals("9877万", nf.format(98765432));
assertEquals("9.9万", nf.format(98765));
assertEquals("9876", nf.format(9876));

// Test Korean with compact.
nf = new Intl.NumberFormat("ko", {notation, compactDisplay: "short"});
assertEquals("9.9억", nf.format(987654321));
assertEquals("9877만", nf.format(98765432));
assertEquals("9.9만", nf.format(98765));
assertEquals("9.9천", nf.format(9876));
assertEquals("987", nf.format(987));
nf = new Intl.NumberFormat("ko", {notation, compactDisplay: "long"});
assertEquals("9.9억", nf.format(987654321));
assertEquals("9877만", nf.format(98765432));
assertEquals("9.9만", nf.format(98765));
assertEquals("9.9천", nf.format(9876));
assertEquals("987", nf.format(987));
