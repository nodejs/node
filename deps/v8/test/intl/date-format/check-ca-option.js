// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


let invalid_ca = [
  "invalid",
  "abce",
];

// https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/calendar.xml
let valid_ca= [
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

invalid_ca.forEach(function(ca) {
  let df = new Intl.DateTimeFormat(["en-u-ca-" + ca + "-fo-obar"]);
  assertEquals("en", df.resolvedOptions().locale);
}
);

valid_ca.forEach(function(ca) {
  locales.forEach(function(base) {
    let l = base + "-u-ca-" + ca;
    let df = new Intl.DateTimeFormat([l + "-fo-obar"]);
    assertEquals(l, df.resolvedOptions().locale);
  });
}
);
