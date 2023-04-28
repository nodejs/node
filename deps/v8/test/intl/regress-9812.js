// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const locales = [
  "cs",
  "cs-CZ",
  "en-001",
  "en-150",
  "en-TV",
  "es-419",
  "es-AR",
  "fil",
  "fr-CA",
  "id",
  "in",
  "lt",
  "nl",
  "pl",
  "pt-PT",
  "sr-ME",
  "sv",
  "uk",
  "vi",
];

const calendars = [
    // Calendars we know have issues
    "islamic",
    "islamic-civil",
    "islamic-tbla",
    "islamic-umalqura",
    "ethiopic-amete-alem",
    "islamicc",
    "ethioaa",
    "islamic-rgsa",

    // Other calendars
    "gregory",
    "japanese",
    "buddhist",
    "roc",
    "persian",
    "islamic",
    "hebrew",
    "chinese",
    "indian",
    "coptic",
    "ethiopic",
    "iso8601",
    "dangi",
    "chinese",
];

let d1 = new Date(2019, 3, 4);
let d2 = new Date(2019, 5, 6);

calendars.forEach(function(calendar) {
  locales.forEach(function(baseLocale) {
    let locale = `${baseLocale}-u-ca-${calendar}`;
    assertDoesNotThrow(
        () => (new Intl.DateTimeFormat(locale)).formatRange(d1, d2),
        `Using Intl.DateFormat formatRange with ${locale} should not throw`);
  })
})
