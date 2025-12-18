// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure that invalid locales throw RangeError

var invalid_locales = ["arcdefghl-Latn", "fil-Latn-kxx", "fr-Latn-CAK",
                       "en-Latin-US", "en-a-foo-9charlong", "en-a-b",
                      ];

for (let locale  of invalid_locales) {
  assertThrows("var nf = new Intl.NumberFormat('" + locale + "')", RangeError);
}

var not_so_long_locales = [
  "bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu-cab-cbs-avnllubs-avnihq-zcu-cab-cbs-ubs-avnihu-cabs-flus-xxd",
  "bs-u-nu-bzcu-cab-cabs-avnlubs-avnihu-zcu",
];

for (let locale  of not_so_long_locales) {
  assertEquals((new Intl.NumberFormat(locale)).resolvedOptions().numberingSystem,
      "latn");
}
