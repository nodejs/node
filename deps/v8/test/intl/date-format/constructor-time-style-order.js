// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;
let weekday = new Array();
let year = new Array();
let month = new Array();
let day = new Array();
let dayPeriod = new Array();
let hour = new Array();
let minute = new Array();
let second = new Array();
let localeMatcher = new Array();
let hour12 = new Array();
let hourCycle = new Array();
let dateStyle = new Array();
let timeStyle = new Array();
let timeZone = new Array();
let era = new Array();
let timeZoneName = new Array();
let formatMatcher = new Array();
let fractionalSecondDigits = new Array();

new Intl.DateTimeFormat(['en-US'], {
  get weekday() {
    weekday.push(++getCount);
  },
  get year() {
    year.push(++getCount);
  },
  get month() {
    month.push(++getCount);
  },
  get day() {
    day.push(++getCount);
  },
  get dayPeriod() {
    dayPeriod.push(++getCount);
  },
  get hour() {
    hour.push(++getCount);
  },
  get minute() {
    minute.push(++getCount);
  },
  get second() {
    second.push(++getCount);
  },
  get localeMatcher() {
    localeMatcher.push(++getCount);
  },
  get hour12() {
    hour12.push(++getCount);
  },
  get hourCycle() {
    hourCycle.push(++getCount);
  },
  get timeZone() {
    timeZone.push(++getCount);
  },
  get dateStyle() {
    dateStyle.push(++getCount);
  },
  get timeStyle() {
    timeStyle.push(++getCount);
    return "full";
  },
  get era() {
    era.push(++getCount);
  },
  get timeZoneName() {
    timeZoneName.push(++getCount);
  },
  get formatMatcher() {
    formatMatcher.push(++getCount);
  },
  get fractionalSecondDigits() {
    fractionalSecondDigits.push(++getCount);
  }
});

assertEquals(1, weekday.length);
assertEquals(1, hour.length);
assertEquals(1, minute.length);
assertEquals(1, second.length);
assertEquals(1, year.length);
assertEquals(1, month.length);
assertEquals(1, day.length);
assertEquals(1, era.length);
assertEquals(1, timeZoneName.length);
assertEquals(1, dateStyle.length);
assertEquals(1, timeStyle.length);
assertEquals(1, localeMatcher.length);
assertEquals(1, hour12.length);
assertEquals(1, hourCycle.length);
assertEquals(1, timeZone.length);
assertEquals(1, formatMatcher.length);

// InitializeDateTimeFormat
assertEquals(1, localeMatcher[0]);
assertEquals(2, hour12[0]);
assertEquals(3, hourCycle[0]);
assertEquals(4, timeZone[0]);

assertEquals(5, weekday[0]);
assertEquals(6, era[0]);
assertEquals(7, year[0]);
assertEquals(8, month[0]);
assertEquals(9, day[0]);
assertEquals(10, dayPeriod[0]);
assertEquals(11, hour[0]);
assertEquals(12, minute[0]);
assertEquals(13, second[0]);
assertEquals(14, fractionalSecondDigits[0]);
assertEquals(15, timeZoneName[0]);

assertEquals(16, formatMatcher[0]);
assertEquals(17, dateStyle[0]);
assertEquals(18, timeStyle[0]);

assertEquals(18, getCount);
