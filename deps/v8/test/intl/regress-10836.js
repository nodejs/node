// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify the order of resolvedOptions()
let df = new Intl.DateTimeFormat("en", {
  weekday: "narrow",
  era: "narrow",
  year: "2-digit",
  month: "2-digit",
  day: "2-digit",
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
  fractionalSecondDigits: 2,
  timeZoneName: "short"});
let resolvedOptionsKeys = Object.keys(df.resolvedOptions()).join(":");

assertEquals(
    "locale:calendar:numberingSystem:timeZone:hourCycle:hour12:weekday:era:" +
    "year:month:day:hour:minute:second:fractionalSecondDigits:timeZoneName",
    resolvedOptionsKeys);

// Verify the order of reading the options.

let read = [];
let options = {
  get second() {
    read.push("second");
    return undefined;
  },
  get fractionalSecondDigits() {
    read.push("fractionalSecondDigits");
    return undefined;
  },
  get timeZoneName() {
    read.push("timeZoneName");
    return undefined;
  }
};

df = new Intl.DateTimeFormat("en", options);
assertEquals(
    "second:fractionalSecondDigits:second:fractionalSecondDigits:timeZoneName",
    read.join(":"));
