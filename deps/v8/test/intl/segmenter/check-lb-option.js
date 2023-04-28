// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let invalid_lb = [
  "invalid",
  "abce",
  "breakall",
  "keepall",
  "none",
  "standard",
  "strict",
  "normal",
  "loose",
];

let locales = [
  "en",
  "ja",
  "zh",
];

invalid_lb.forEach(function(lb) {
  let df = new Intl.Segmenter(["en-u-lb-" + lb + "-fo-obar"]);
  assertEquals("en", df.resolvedOptions().locale);
}
);
