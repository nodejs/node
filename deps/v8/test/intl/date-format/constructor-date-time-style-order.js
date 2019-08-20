// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-datetime-style

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;
let weekday = new Array();
let year = new Array();
let month = new Array();
let day = new Array();
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
    return "full";
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
  }
});

assertEquals(1, weekday.length);
assertEquals(1, weekday[0]);
assertEquals(1, year.length);
assertEquals(2, year[0]);
assertEquals(1, month.length);
assertEquals(3, month[0]);
assertEquals(1, day.length);
assertEquals(4, day[0]);
assertEquals(1, hour.length);
assertEquals(5, hour[0]);
assertEquals(1, minute.length);
assertEquals(6, minute[0]);
assertEquals(1, second.length);
assertEquals(7, second[0]);
assertEquals(1, localeMatcher.length);
assertEquals(8, localeMatcher[0]);
assertEquals(1, hour12.length);
assertEquals(9, hour12[0]);
assertEquals(1, hourCycle.length);
assertEquals(10, hourCycle[0]);
assertEquals(1, timeZone.length);
assertEquals(11, timeZone[0]);
assertEquals(1, dateStyle.length);
assertEquals(12, dateStyle[0]);
assertEquals(1, timeStyle.length);
assertEquals(13, timeStyle[0]);
assertEquals(0, era.length);
assertEquals(0, timeZoneName.length);
assertEquals(0, formatMatcher.length);
