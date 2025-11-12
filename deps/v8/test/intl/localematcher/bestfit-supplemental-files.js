// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony_intl_best_fit_matcher
//
// Test the supportedLocales and resolvedOptions based on
// https://github.com/unicode-org/cldr/blob/master/common/supplemental/languageInfo.xml

const intlObjs = [
    Intl.Collator,
    Intl.DateTimeFormat,
    Intl.DisplayNames,
    Intl.ListFormat,
    Intl.NumberFormat,
    Intl.RelativeTimeFormat,
    Intl.PluralRules,
    Intl.Segmenter,
];


function verifyResolvedLocale(obj, opt, l1, l2) {
  let options = {}
  options = Object.assign(options, opt);
  if (obj == Intl.DisplayNames) {
    options['type'] = "language";
  }
  let resolvedLocale = (new obj(l1, options)).resolvedOptions().locale;
  assertTrue((l1 == resolvedLocale) ||
             (l2 == resolvedLocale.substring(0, l2.length)));
}

let bestFitOpt = {localeMatcher: "best fit"};
let defaultLocale = (new Intl.NumberFormat()).resolvedOptions().locale;
intlObjs.forEach(function(obj) {
  [{}, bestFitOpt].forEach(function(opt) {
    print(obj);
    verifyResolvedLocale(obj, opt, "co", "fr");
    verifyResolvedLocale(obj, opt, "crs", "fr");
    verifyResolvedLocale(obj, opt, "lb", "de");
    verifyResolvedLocale(obj, opt, "gsw", "de");
    verifyResolvedLocale(obj, opt, "btj", "ms");
    verifyResolvedLocale(obj, opt, "bjn", "ms");
  });

});
