// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


let invalid_hc = [
  "invalid",
  "abce",
  "h10",
  "h13",
  "h22",
  "h25",
];

// https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/calendar.xml
let valid_hc= [
  "h11",
  "h12",
  "h23",
  "h24",
];

let locales = [
  "en",
  "ar",
];

invalid_hc.forEach(function(hc) {
  let df = new Intl.DateTimeFormat(
      ["en-u-hc-" + hc + "-fo-obar"], {hour: "2-digit"});
  assertEquals("en", df.resolvedOptions().locale);
}
);

valid_hc.forEach(function(hc) {
  locales.forEach(function(base) {
    let l = base + "-u-hc-" + hc;
    let df = new Intl.DateTimeFormat(
        [l + "-fo-obar"], {hour: "2-digit"});
    assertEquals(l, df.resolvedOptions().locale);
  });
}
);
