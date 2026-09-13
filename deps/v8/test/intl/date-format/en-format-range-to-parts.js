// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const date1 = new Date("2019-01-03T03:20");
const date2 = new Date("2019-01-05T19:33");
const date3 = new Date("2019-01-05T22:57");

// value:  "Jan 3 – 5, 2019"
// source:  hhhhShhhEhhhhhh
// type:    mmmldllldllyyyy
// h: Shared, S: startRange, E: endRange
// m: month, l: literal, d: day, y: year
const expected1 = [
  {type: "month", value: "Jan", source: "shared"},
  {type: "literal", value: " ", source: "shared"},
  {type: "day", value: "3", source: "startRange"},
  {type: "literal", value: " – ", source: "shared"},
  {type: "day", value: "5", source: "endRange"},
  {type: "literal", value: ", ", source: "shared"},
  {type: "year", value: "2019", source: "shared"}
];

var dtf = new Intl.DateTimeFormat(["en"], {year: "numeric", month: "short", day: "numeric"});
const ret1 = dtf.formatRangeToParts(date1, date2);
assertEquals(expected1, ret1);

// value:  "Jan 5, 7 – 10 PM"
// source:  hhhhhhhShhhEEhhh
// type:    mmmldlldlllhhlpp
// h: Shared, S: startRange, E: endRange
// m: month, l: literal, d: day, h: hour, p: dayPeriod

const expected2 = [
  {type: "month", value: "Jan", source: "shared"},
  {type: "literal", value: " ", source: "shared"},
  {type: "day", value: "5", source: "shared"},
  {type: "literal", value: ", ", source: "shared"},
  {type: "hour", value: "7", source: "startRange"},
  {type: "literal", value: " – ", source: "shared"},
  {type: "hour", value: "10", source: "endRange"},
  {type: "literal", value: " ", source: "shared"},
  {type: "dayPeriod", value: "PM", source: "shared"}
];
dtf = new Intl.DateTimeFormat(["en"], {month: "short", day: "numeric", hour: "numeric"});
const ret2 = dtf.formatRangeToParts(date2, date3);
assertEquals(expected2, ret2);
