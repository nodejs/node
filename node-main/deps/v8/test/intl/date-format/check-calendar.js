// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let invalidCalendar = [
  "invalid",
  "abce",
  "abc-defghi",
];

let illFormedCalendar = [
  "",
  "i",
  "ij",
  "abcdefghi",
  "abc-ab",
];

// https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/calendar.xml
let validCalendar= [
  "buddhist",
  "chinese",
  "coptic",
  "dangi",
  "ethioaa",
  "ethiopic",
  "gregory",
  "hebrew",
  "indian",
  "islamic",
  "islamic-umalqura",
  "islamic-tbla",
  "islamic-civil",
  "islamic-rgsa",
  "iso8601",
  "japanese",
  "persian",
  "roc",
];

let locales = [
  "en",
  "ar",
];

invalidCalendar.forEach(function(calendar) {
  locales.forEach(function(base) {
     var df;
     assertDoesNotThrow(() => df = new Intl.DateTimeFormat([base], {calendar}));
     assertEquals(
         (new Intl.DateTimeFormat([base])).resolvedOptions().calendar,
         df.resolvedOptions().calendar);
  });
});

illFormedCalendar.forEach(function(calendar) {
  assertThrows(
      () => new Intl.DateTimeFormat(["en"], {calendar}),
      RangeError);
}
);

let value = new Date();
validCalendar.forEach(function(calendar) {
  locales.forEach(function(base) {
    let l = base + "-u-ca-" + calendar;
    let dtf = new Intl.DateTimeFormat([base], {calendar});
    assertEquals(base, dtf.resolvedOptions().locale);

    // Test the formatting result is the same as passing in via u-ca-
    // in the locale.
    let dtf2 = new Intl.DateTimeFormat([l]);
    assertEquals(dtf2.format(value), dtf.format(value));
  });
}
);
