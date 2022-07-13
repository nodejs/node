// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Somehow only ar-SA fails on Android in regress-8413*.js.
// Split it into this test just for ar-SA.
// This is likely to be caused by an Android-specific ICU data trimming.
let locales = [ "ar-SA" ];

// "Table 5: Components of date and time formats" as in
// https://ecma-international.org/ecma-402/#sec-datetimeformat-abstracts
let table5 = [
  ["weekday", ["narrow", "short", "long"]],
  ["era", ["narrow", "short", "long"]],
  ["year", ["2-digit", "numeric"]],
  ["month", ["2-digit", "numeric", "narrow", "short", "long"]],
  ["day", ["2-digit", "numeric"]],
  ["hour", ["2-digit", "numeric"]],
  ["minute", ["2-digit", "numeric"]],
  ["second", ["2-digit", "numeric"]],
  ["timeZoneName", ["short", "long"]]
];

// Test each locale
for (let loc of locales) {
  // Test each property in Table 5
  for (let row of table5) {
    let prop = row[0];
    let values = row[1];
    // Test each value of the property
    for (let value of values) {
      let opt = {};
      opt[prop] = value;
      let dft = new Intl.DateTimeFormat([loc], opt);
      let result = dft.resolvedOptions();
      assertTrue(values.indexOf(result[prop]) >= 0,
          "Expect new Intl.DateTimeFormat([" + loc + "], {" + prop + ": '" +
          value + "'}).resolvedOptions()['" + prop + "'] to return one of [" +
          values + "] but got '" + result[prop] + "'");
    }
  }
}
