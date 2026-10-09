// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let subtags = [
  "heploc", "alalc97", "baku1926", "luna1918", "colb1945", "petr17", "aaland",
  "saaho", "posix", "oxendict", "pinyin", "fonipa", "1990", "1901", "1606pcm",
  "1959", "1994", "1996", "16102", "16103", "aluku", "ao1990", "aragon",
  "arkadj", "asyrc", "balanka", "barla"
];

if (typeof Intl !== 'undefined') {
  assertThrows(() => {
    new Intl.Locale("en", {
      variants: subtags.join("-"),
      calendar: "gregory"
    });
  }, RangeError);

  assertThrows(() => {
    new Intl.Locale("en-u-ca-gregory", {
      variants: subtags.join("-")
    });
  }, RangeError);
}
