// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let invalid_co = [
  "invalid",
  "search",
  "standard",
  "abce",
];

let valid_locales = [
  "zh-u-co-zhuyin",
  "zh-u-co-stroke",
  "ar-u-co-compat",
  "en-u-co-emoji",
  "en-u-co-eor",
  "zh-Hant-u-co-pinyin",
  "ko-u-co-searchjl",
  "ja-u-co-unihan",
];

invalid_co.forEach(function(co) {
  let col = new Intl.Collator(["en-u-co-" + co]);
  assertEquals("en", col.resolvedOptions().locale);
}
);

valid_locales.forEach(function(l) {
  let col = new Intl.Collator([l + "-fo-obar"]);
  assertEquals(l, col.resolvedOptions().locale);
}
);
