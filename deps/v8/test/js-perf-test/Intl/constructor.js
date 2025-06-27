// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function NewIntlCollator() {
  let obj = new Intl.Collator();
}
createSuite('NewIntlCollator', 100, NewIntlCollator, ()=>{});

function NewIntlDateTimeFormat() {
  let obj = new Intl.DateTimeFormat();
}
createSuite('NewIntlDateTimeFormat', 100, NewIntlDateTimeFormat, ()=>{});

function NewIntlNumberFormat() {
  let obj = new Intl.NumberFormat();
}
createSuite('NewIntlNumberFormat', 100, NewIntlNumberFormat, ()=>{});

function NewIntlPluralRules() {
  let obj = new Intl.PluralRules();
}
createSuite('NewIntlPluralRules', 100, NewIntlPluralRules, ()=>{});

function NewIntlListFormat() {
  let obj = new Intl.ListFormat();
}
createSuite('NewIntlListFormat', 100, NewIntlListFormat, ()=>{});

function NewIntlRelativeTimeFormat() {
  let obj = new Intl.RelativeTimeFormat();
}
createSuite('NewIntlRelativeTimeFormat', 100, NewIntlRelativeTimeFormat, ()=>{});

function NewIntlLocaleWithOptions() {
  let obj = new Intl.Locale("en-Latn-US-u-nu-thai",
      { language: "zh", region: "TW", script: "Hant", calendar: "roc", collation: "zhuyin",
        hourCycle: "h11", caseFirst: "upper", numberingSystem: "hanidec"});
}
createSuite('NewIntlLocaleWithOptions', 100, NewIntlLocaleWithOptions, ()=>{});

function NewIntlLocale() {
  let obj = new Intl.Locale("zh");
}
createSuite('NewIntlLocale', 100, NewIntlLocale, ()=>{});
