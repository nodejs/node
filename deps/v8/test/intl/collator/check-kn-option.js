// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let invalid_kn = [
  "invalid",
  "search",
  "standard",
  "abce",
];

let valid_kn = [
  ["en-u-kn", true, "en-u-kn"],
  ["en-u-kn-true", true, "en-u-kn"],
  ["en-u-kn-false",false, "en-u-kn-false"],
];

invalid_kn.forEach(function(kn) {
  let col = new Intl.Collator(["en-u-kn-" + kn]);
  assertEquals("en", col.resolvedOptions().locale);
}
);

valid_kn.forEach(function(l) {
  let col = new Intl.Collator([l[0] + "-fo-obar"]);
  assertEquals(l[1], col.resolvedOptions().numeric);
  assertEquals(l[2], col.resolvedOptions().locale);
}
);
