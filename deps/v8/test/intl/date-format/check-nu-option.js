// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


let invalid_nu = [
  "invalid",
  "abce",
  "finance",
  "native",
  "traditio",
];

// https://tc39.github.io/ecma402/#table-numbering-system-digits
let valid_nu= [
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


invalid_nu.forEach(function(nu) {
  let df = new Intl.DateTimeFormat(["en-u-nu-" + nu + "-fo-obar"]);
  assertEquals("en", df.resolvedOptions().locale);
}
);

valid_nu.forEach(function(nu) {
  locales.forEach(function(base) {
    let l = base + "-u-nu-" + nu;
    let df = new Intl.DateTimeFormat([l + "-fo-obar"]);
    assertEquals(l, df.resolvedOptions().locale);
  });
}
);
