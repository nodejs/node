// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let invalidNumberingSystem = [
  "invalid",
  "abce",
  "finance",
  "native",
  "traditio",
  "abc-defghi",
];

let illFormedNumberingSystem = [
  "",
  "i",
  "ij",
  "abcdefghi",
  "abc-ab",
];

// https://tc39.github.io/ecma402/#table-numbering-system-digits
let validNumberingSystem= [
  "arab",
  "arabext",
  "bali",
  "beng",
  "deva",
  "fullwide",
  "gujr",
  "guru",
  "hanidec",
  "khmr",
  "knda",
  "laoo",
  "latn",
  "limb",
  "mlym",
  "mong",
  "mymr",
  "orya",
  "tamldec",
  "telu",
  "thai",
  "tibt",
];

let locales = [
  "en",
  "ar",
];


invalidNumberingSystem.forEach(function(numberingSystem) {
  locales.forEach(function(base) {
    var df;
    assertDoesNotThrow(
        () => df = new Intl.RelativeTimeFormat([base], {numberingSystem}));
    assertEquals(
        (new Intl.RelativeTimeFormat([base])).resolvedOptions().numberingSystem,
        df.resolvedOptions().numberingSystem);
  });
});

illFormedNumberingSystem.forEach(function(numberingSystem) {
  assertThrows(
      () => new Intl.RelativeTimeFormat(["en"], {numberingSystem}),
      RangeError);
});

let value = 1234567.89;
validNumberingSystem.forEach(function(numberingSystem) {
  locales.forEach(function(base) {
    let l = base + "-u-nu-" + numberingSystem;
    let nf = new Intl.RelativeTimeFormat([base], {numberingSystem});
    assertEquals(base, nf.resolvedOptions().locale);
    assertEquals(numberingSystem, nf.resolvedOptions().numberingSystem);

    // Test the formatting result is the same as passing in via u-nu-
    // in the locale.
    let nf2 = new Intl.RelativeTimeFormat([l]);
    assertEquals(nf2.format(value, "day"), nf.format(value, "day"));
  });
}
);
