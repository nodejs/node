// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let intlClasses = [
    Intl.RelativeTimeFormat,
    Intl.NumberFormat,
    Intl.DateTimeFormat
];

// Check "nu" on Intl.RelativeTimeFormat, Intl.NumberFormat and
// Intl.DateTimeFormat
intlClasses.forEach(function(cls) {
  let o1 = (new cls("en")).resolvedOptions();
  assertEquals('latn', o1.numberingSystem);

  let o2 = (new cls("en-u-nu-arab")).resolvedOptions();
  assertEquals("arab", o2.numberingSystem);
  assertEquals("en-u-nu-arab", o2.locale);

  let o3 = (new cls(
      "en-u-nu-arab", {numberingSystem: 'thai'})).resolvedOptions();
  assertEquals("thai", o3.numberingSystem);
  assertEquals("en", o3.locale);

  let o4 = (new cls(
      "en", {numberingSystem: 'bali'})).resolvedOptions();
  assertEquals("bali", o4.numberingSystem);
  assertEquals("en", o4.locale);

  let o5 = (new cls("ar-SA")).resolvedOptions();
  assertEquals('arab', o5.numberingSystem);


  let o6 = (new cls(
      "ar-SA-u-nu-arab", {numberingSystem: 'thai'})).resolvedOptions();
  assertEquals("thai", o6.numberingSystem);
  assertEquals("ar-SA", o6.locale);

  let o7 = (new cls(
      "ar-SA-u-nu-arab", {numberingSystem: 'arab'})).resolvedOptions();
  assertEquals("arab", o7.numberingSystem);
  assertEquals("ar-SA-u-nu-arab", o7.locale);
});

// Check "ca" on Intl.DateTimeFormat
cls = Intl.DateTimeFormat;
let o1 = (new cls("en")).resolvedOptions();
assertEquals('gregory', o1.calendar);

let o2 = (new cls("en-u-ca-roc")).resolvedOptions();
assertEquals("roc", o2.calendar);
assertEquals("en-u-ca-roc", o2.locale);

let o3 = (new cls(
    "en-u-ca-roc", {calendar: 'chinese'})).resolvedOptions();
assertEquals("chinese", o3.calendar);
assertEquals("en", o3.locale);

let o4 = (new cls(
    "en", {calendar: 'japanese'})).resolvedOptions();
assertEquals("japanese", o4.calendar);
assertEquals("en", o4.locale);

let o5 = (new cls("fa")).resolvedOptions();
assertEquals('persian', o5.calendar);
