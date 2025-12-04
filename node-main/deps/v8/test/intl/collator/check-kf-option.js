// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let invalid_kf = [
  "invalid",
  "abce",
  "none",
  "true",
];

let valid_kf= [
  "false",
  "upper",
  "lower",
];

let locales = [
  "en",
  "fr",
];

invalid_kf.forEach(function(kf) {
  let col = new Intl.Collator(["en-u-kf-" + kf + "-fo-obar"]);
  assertEquals("en", col.resolvedOptions().locale);
}
);

valid_kf.forEach(function(kf) {
  locales.forEach(function(base) {
    let l = base + "-u-kf-" + kf;
    let col = new Intl.Collator([l + "-fo-obar"]);
    assertEquals(l, col.resolvedOptions().locale);
  });
}
);
