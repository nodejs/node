// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony_intl_best_fit_matcher
//
// Test the return list is subset of the request list.
// https://tc39.es/ecma402/#sec-bestfitsupportedlocales
//  9.2.9 BestFitSupportedLocales ( availableLocales, requestedLocales )
// The BestFitSupportedLocales abstract operation returns the SUBSET of the
// provided BCP 47 language priority list requestedLocales for which
// availableLocales has a matching locale when using the Best Fit Matcher
// algorithm. Locales appear in the same order in the returned list as in
// requestedLocales. The steps taken are implementation dependent.


function assertSubarray(a, b) {
  assertTrue(a.every(val => b.includes(val)), ['returns:', a, 'requested:', b]);
}

function verifySupportedLocalesAreSubarray(f, ll) {
  assertSubarray(f(ll), ll);
}

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
intlObjs.forEach(function(obj) {
  verifySupportedLocalesAreSubarray(obj.supportedLocalesOf, ['en', 'ceb']);
  verifySupportedLocalesAreSubarray(obj.supportedLocalesOf, ['en', 'ceb', 'fil']);
});
