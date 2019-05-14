// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[
  // Grandfathered tags without a preferred value in the IANA language
  // tag registry. Nonetheless, ICU cooks up a value when canonicalizing.
  // v8 works around that ICU issue.
  // See https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry .
  ["cel-gaulish", "cel-gaulish"],
  ["i-default", "i-default"],
  ["i-mingo", "i-mingo"],
  ["i-enochian", "i-enochian"],
  ["zh-min", "zh-min"],

  // Matching should be case-insensitive.
  ["I-default", "i-default"],
  ["i-DEFAULT", "i-default"],
  ["I-DEFAULT", "i-default"],
  ["i-DEfauLT", "i-default"],
  ["zh-Min", "zh-min"],
  ["Zh-min", "zh-min"],
].forEach(([inputLocale, expectedLocale]) => {
  const canonicalLocales = Intl.getCanonicalLocales(inputLocale);
  assertEquals(canonicalLocales.length, 1);
  assertEquals(expectedLocale, canonicalLocales[0]);
})
