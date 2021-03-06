// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test to check "algorithmic" numbering systems stated in UTS35 but not
// mandated by ECMA402 won't crash.
// The entries which type is "algorithmic" in
// https://github.com/unicode-org/cldr/blob/master/common/supplemental/numberingSystems.xml
// These are numbering systems which is not supported in ECMA402 but we should
// not crash.
let algorithmicNumberingSystems = [
    "armn",
    "armnlow",
    "cyrl",
    "ethi",
    "geor",
    "grek",
    "greklow",
    "hanidays",
    "hans",
    "hansfin",
    "hant",
    "hantfin",
    "hebr",
    "jpan",
    "jpanfin",
    "jpanyear",
    "roman",
    "romanlow",
    "taml",
];

for (numberingSystem of algorithmicNumberingSystems) {
  let baseLocale = "en";
  let locale = baseLocale + "-u-nu-" + numberingSystem;

  // Ensure the creation won't crash
  let rtf = new Intl.RelativeTimeFormat(locale);
  let rtf2 = new Intl.RelativeTimeFormat(baseLocale, {numberingSystem});

  let dtf = new Intl.DateTimeFormat(locale);
  let dtf2 = new Intl.DateTimeFormat(baseLocale, {numberingSystem});

  let nf = new Intl.NumberFormat(locale);
  let nf2 = new Intl.NumberFormat(baseLocale, {numberingSystem});
}
