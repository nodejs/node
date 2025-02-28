// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test
// https://github.com/tc39/proposal-intl-datetime-style/pull/43
// https://github.com/tc39/proposal-intl-datetime-style/pull/47
// https://github.com/tc39/proposal-intl-datetime-style/pull/50
let opt = {
  weekday: 'narrow',
  era: 'narrow',
  year: '2-digit',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
  fractionalSecondDigits: 2,
};

let keys = Object.keys(opt);
let testDateStyle = { ...opt };
let testTimeStyle = { ...opt };
testDateStyle.dateStyle = 'long';
testTimeStyle.timeStyle = 'long';

for (key of keys) {
  assertThrows(
      () => new Intl.DateTimeFormat('en', testDateStyle),
      TypeError, "Invalid option : option");
  assertThrows(
      () => new Intl.DateTimeFormat('en', testTimeStyle),
      TypeError, "Invalid option : option");
  testDateStyle[key] = undefined;
  testTimeStyle[key] = undefined;
}

assertThrows(
    () => (new Date()).toLocaleDateString("en", {timeStyle: "long"}),
     TypeError, "Invalid option : timeStyle");

assertThrows(
    () => (new Date()).toLocaleTimeString("en", {dateStyle: "long"}),
     TypeError, "Invalid option : dateStyle");

let logs = [];
try {
  var dtf = new Intl.DateTimeFormat("en", {
    get timeStyle() {
      logs.push("get timeStyle");
      return "full";
    },
    get timeZoneName() {
      logs.push("get timeZoneName");
      return "short";
    },
  });
  logs.push(dtf.resolvedOptions().timeStyle);
  logs.push(dtf.resolvedOptions().timeZoneName);
} catch(e) {
  logs.push(e.name);
}

assertEquals(
    "get timeZoneName,get timeStyle,TypeError",
    logs.join(','));
