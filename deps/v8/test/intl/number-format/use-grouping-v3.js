// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let validUseGrouping = [
    "min2",
    "auto",
    "always",
    false,
];

let fallbackUseGrouping = [
    "true",
    "false",
];

let invalidUseGrouping = [
    "min-2",
];


validUseGrouping.forEach(function(useGrouping) {
  let nf = new Intl.NumberFormat(undefined, {useGrouping});
  assertEquals(useGrouping, nf.resolvedOptions().useGrouping);
});

fallbackUseGrouping.forEach(function(useGrouping) {
  let nf = new Intl.NumberFormat(undefined, {useGrouping});
  assertEquals("auto", nf.resolvedOptions().useGrouping);
});

invalidUseGrouping.forEach(function(useGrouping) {
  assertThrows(
      () => new Intl.NumberFormat(undefined, {useGrouping}),
      RangeError);
});

// useGrouping: undefined get "auto"
assertEquals("auto",
    (new Intl.NumberFormat()).resolvedOptions().useGrouping);
assertEquals("auto",
    (new Intl.NumberFormat(undefined, {useGrouping: undefined}))
        .resolvedOptions().useGrouping);

// useGrouping: true get "always"
assertEquals("always",
    (new Intl.NumberFormat(undefined, {useGrouping: true}))
        .resolvedOptions().useGrouping);

// useGrouping: false get false
// useGrouping: "" get false
assertEquals(false,
    (new Intl.NumberFormat(undefined, {useGrouping: false}))
        .resolvedOptions().useGrouping);
assertEquals(false,
    (new Intl.NumberFormat(undefined, {useGrouping: ""}))
        .resolvedOptions().useGrouping);

// Some locales with default minimumGroupingDigits
let mgd1 = ["en"];
// Some locales with default minimumGroupingDigits{"2"}
let mgd2 = ["es", "pl", "lv"];
let all = mgd1.concat(mgd2);

// Check "always"
all.forEach(function(locale) {
  let off = new Intl.NumberFormat(locale, {useGrouping: false});
  let msg = "locale: " + locale + " useGrouping: false";
  // In useGrouping: false, no grouping.
  assertEquals(3, off.format(123).length, msg);
  assertEquals(4, off.format(1234).length, msg);
  assertEquals(5, off.format(12345).length, msg);
  assertEquals(6, off.format(123456).length, msg);
  assertEquals(7, off.format(1234567).length, msg);
});

// Check false
all.forEach(function(locale) {
  let always = new Intl.NumberFormat(locale, {useGrouping: "always"});
  let msg = "locale: " + locale + " useGrouping: 'always'";
  assertEquals(3, always.format(123).length);
  // In useGrouping: "always", has grouping when more than 3 digits..
  assertEquals(4 + 1, always.format(1234).length, msg);
  assertEquals(5 + 1, always.format(12345).length, msg);
  assertEquals(6 + 1, always.format(123456).length, msg);
  assertEquals(7 + 2, always.format(1234567).length, msg);
});

// Check "min2"
all.forEach(function(locale) {
  let always = new Intl.NumberFormat(locale, {useGrouping: "min2"});
  let msg = "locale: " + locale + " useGrouping: 'min2'";
  assertEquals(3, always.format(123).length);
  // In useGrouping: "min2", no grouping for 4 digits but has grouping
  // when more than 4 digits..
  assertEquals(4, always.format(1234).length, msg);
  assertEquals(5 + 1, always.format(12345).length, msg);
  assertEquals(6 + 1, always.format(123456).length, msg);
  assertEquals(7 + 2, always.format(1234567).length, msg);
});

// Check "auto"
mgd1.forEach(function(locale) {
  let auto = new Intl.NumberFormat(locale, {useGrouping: "auto"});
  let msg = "locale: " + locale + " useGrouping: 'auto'";
  assertEquals(3, auto.format(123).length, msg);
  assertEquals(4 + 1, auto.format(1234).length, msg);
  assertEquals(5 + 1, auto.format(12345).length, msg);
  assertEquals(6 + 1, auto.format(123456).length, msg);
  assertEquals(7 + 2, auto.format(1234567).length, msg);
});
mgd2.forEach(function(locale) {
  let auto = new Intl.NumberFormat(locale, {useGrouping: "auto"});
  let msg = "locale: " + locale + " useGrouping: 'auto'";
  assertEquals(3, auto.format(123).length, msg);
  // In useGrouping: "auto", since these locales has
  // minimumGroupingDigits{"2"}, no grouping for 4 digits but has grouping
  // when more than 4 digits..
  assertEquals(4, auto.format(1234).length, msg);
  assertEquals(5 + 1, auto.format(12345).length, msg);
  assertEquals(6 + 1, auto.format(123456).length, msg);
  assertEquals(7 + 2, auto.format(1234567).length, msg);
});
