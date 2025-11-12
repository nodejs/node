// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony_intl_best_fit_matcher
//
// Test the supportedLocales and resolvedOptions handle "zh-TW", "zh-Hant-TW",
// etc.

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

let bestFitOpt = {localeMatcher: "best fit"};
let defaultLocale = (new Intl.NumberFormat()).resolvedOptions().locale;
intlObjs.forEach(function(obj) {
  [{}, bestFitOpt].forEach(function(opt) {
    // Test consistent between "zh-TW" and "zh-Hant-TW". Either both are
    // supported or neither are supported.
    assertEquals((obj.supportedLocalesOf(["zh-TW"], opt)).length,
                 (obj.supportedLocalesOf(["zh-Hant-TW"], opt)).length);
    assertEquals((obj.supportedLocalesOf(["zh-MO"], opt)).length,
                 (obj.supportedLocalesOf(["zh-Hant-MO"], opt)).length);
    assertEquals((obj.supportedLocalesOf(["zh-HK"], opt)).length,
                 (obj.supportedLocalesOf(["zh-Hant-HK"], opt)).length);
    assertEquals((obj.supportedLocalesOf(["zh-CN"], opt)).length,
                 (obj.supportedLocalesOf(["zh-Hans-CN"], opt)).length);
    assertEquals((obj.supportedLocalesOf(["zh-SG"], opt)).length,
                 (obj.supportedLocalesOf(["zh-Hans-SG"], opt)).length);
  });

});
