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
         () => df = new Intl.DateTimeFormat([base], {numberingSystem}));
     assertEquals(
         (new Intl.DateTimeFormat([base])).resolvedOptions().numberingSystem,
         df.resolvedOptions().numberingSystem);
  });
});

illFormedNumberingSystem.forEach(function(numberingSystem) {
  assertThrows(
      () => new Intl.DateTimeFormat(["en"], {numberingSystem}),
      RangeError);
});

let value = new Date();
validNumberingSystem.forEach(function(numberingSystem) {
  locales.forEach(function(base) {
    let l = base + "-u-nu-" + numberingSystem;
    let dtf = new Intl.DateTimeFormat([base], {numberingSystem});
    assertEquals(base, dtf.resolvedOptions().locale);
    assertEquals(numberingSystem, dtf.resolvedOptions().numberingSystem);

    // Test the formatting result is the same as passing in via u-nu-
    // in the locale.
    let dtf2 = new Intl.DateTimeFormat([l]);
    assertEquals(dtf2.format(value), dtf.format(value));
  });
}
);
