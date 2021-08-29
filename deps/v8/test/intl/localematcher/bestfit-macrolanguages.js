// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony_intl_best_fit_matcher
//
// Test the supportedLocales and resolvedOptions handle macrolanguages
// documented in https://unicode.org/reports/tr35/#Field_Definitions

const intlObjs = [
    Intl.Collator,
    Intl.DateTimeFormat,
    Intl.DisplayNames,
    Intl.ListFormat,
    Intl.NumberFormat,
    Intl.PluralRules,
    Intl.RelativeTimeFormat,
    Intl.Segmenter,
];

// Macrolanguages in
// https://unicode.org/reports/tr35/#Field_Definitionsk
const macroLanguageMap = {
  'cmn': 'zh',
  'arb': 'ar',
  'zsm': 'ms',
  'swh': 'sw',
  'uzn': 'uz',
//  'knn': 'kok',  // chrome does not ship data for kok locale
  'kmr': 'ku',
};

let bestFitOpt = {localeMatcher: "best fit"};
let defaultLocale = (new Intl.NumberFormat()).resolvedOptions().locale;
intlObjs.forEach(function(obj) {
  for (const [macro, lang] of Object.entries(macroLanguageMap)) {
    const justMacro = [macro];
    // Test the macro language will be persist in the supportedLocalesOf
    assertEquals([lang], obj.supportedLocalesOf(justMacro));
    assertEquals([lang], obj.supportedLocalesOf(justMacro, bestFitOpt));

    // Test the macro language would be resolved to a locale other than the
    // default locale.
    if (obj == Intl.DisplayNames) {
      assertTrue(defaultLocale != (new obj(macro, {type: "language"}))
        .resolvedOptions().locale);
      assertTrue(defaultLocale !=
          (new obj(macro, {type: "language", localeMatcher: "best fit"}))
          .resolvedOptions().locale);
    } else {
      assertTrue(defaultLocale != (new obj(macro)).resolvedOptions().locale);
      assertTrue(defaultLocale != (new obj(macro, bestFitOpt))
          .resolvedOptions().locale);
    }
  }
});
