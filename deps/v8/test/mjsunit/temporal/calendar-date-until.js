// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.dateuntil
let cal = new Temporal.Calendar("iso8601");

// Test throw
[ "hour", "minute", "second", "millisecond", "microsecond", "nanosecond" ]
.forEach(function(largestUnit) {
  assertThrows(() => cal.dateUntil("2021-07-16", "2021-07-17",
        {largestUnit}), RangeError);
});

assertEquals("PT0S", cal.dateUntil("2021-07-16", "2021-07-16").toJSON());
assertEquals("P1D", cal.dateUntil("2021-07-16", "2021-07-17").toJSON());
assertEquals("P32D", cal.dateUntil("2021-07-16", "2021-08-17").toJSON());
assertEquals("P62D", cal.dateUntil("2021-07-16", "2021-09-16").toJSON());
assertEquals("P365D", cal.dateUntil("2021-07-16", "2022-07-16").toJSON());
assertEquals("P3652D", cal.dateUntil("2021-07-16", "2031-07-16").toJSON());

assertEquals("-P1D", cal.dateUntil("2021-07-17", "2021-07-16").toJSON());
assertEquals("-P32D", cal.dateUntil("2021-08-17", "2021-07-16").toJSON());
assertEquals("-P62D", cal.dateUntil("2021-09-16", "2021-07-16").toJSON());
assertEquals("-P365D", cal.dateUntil("2022-07-16", "2021-07-16").toJSON());
assertEquals("-P3652D", cal.dateUntil("2031-07-16", "2021-07-16").toJSON());

["day", "days"].forEach(function(largestUnit) {
  let opt = {largestUnit};
  assertEquals("PT0S", cal.dateUntil("2021-07-16", "2021-07-16", opt).toJSON());
  assertEquals("P1D", cal.dateUntil("2021-07-16", "2021-07-17", opt).toJSON());
  assertEquals("P32D", cal.dateUntil("2021-07-16", "2021-08-17", opt).toJSON());
  assertEquals("P62D", cal.dateUntil("2021-07-16", "2021-09-16", opt).toJSON());
  assertEquals("P365D",
      cal.dateUntil("2021-07-16", "2022-07-16", opt).toJSON());
  assertEquals("P3652D"
      ,cal.dateUntil("2021-07-16", "2031-07-16", opt).toJSON());

  assertEquals("-P1D",
      cal.dateUntil("2021-07-17", "2021-07-16", opt).toJSON());
  assertEquals("-P32D",
      cal.dateUntil("2021-08-17", "2021-07-16", opt).toJSON());
  assertEquals("-P62D",
      cal.dateUntil("2021-09-16", "2021-07-16", opt).toJSON());
  assertEquals("-P365D",
      cal.dateUntil("2022-07-16", "2021-07-16", opt).toJSON());
  assertEquals("-P3652D",
      cal.dateUntil("2031-07-16", "2021-07-16", opt).toJSON());
});

["week", "weeks"].forEach(function(largestUnit) {
  let opt = {largestUnit};
  assertEquals("PT0S", cal.dateUntil("2021-07-16", "2021-07-16", opt).toJSON());
  assertEquals("P1D", cal.dateUntil("2021-07-16", "2021-07-17", opt).toJSON());
  assertEquals("P1W", cal.dateUntil("2021-07-16", "2021-07-23", opt).toJSON());
  assertEquals("P4W4D",
      cal.dateUntil("2021-07-16", "2021-08-17", opt).toJSON());
  assertEquals("P4W", cal.dateUntil("2021-07-16", "2021-08-13", opt).toJSON());
  assertEquals("P8W6D",
      cal.dateUntil("2021-07-16", "2021-09-16", opt).toJSON());
  assertEquals("P52W1D",
      cal.dateUntil("2021-07-16", "2022-07-16", opt).toJSON());
  assertEquals("P521W5D"
      ,cal.dateUntil("2021-07-16", "2031-07-16", opt).toJSON());

  assertEquals("-P1D",
      cal.dateUntil("2021-07-17", "2021-07-16", opt).toJSON());
  assertEquals("-P4W4D",
      cal.dateUntil("2021-08-17", "2021-07-16", opt).toJSON());
  assertEquals("-P4W",
      cal.dateUntil("2021-08-13", "2021-07-16", opt).toJSON());
  assertEquals("-P8W6D",
      cal.dateUntil("2021-09-16", "2021-07-16", opt).toJSON());
  assertEquals("-P52W1D",
      cal.dateUntil("2022-07-16", "2021-07-16", opt).toJSON());
  assertEquals("-P521W5D",
      cal.dateUntil("2031-07-16", "2021-07-16", opt).toJSON());
});

["month", "months"].forEach(function(largestUnit) {
  let opt = {largestUnit};
  assertEquals("PT0S", cal.dateUntil("2021-07-16", "2021-07-16", opt).toJSON());
  assertEquals("P1D", cal.dateUntil("2021-07-16", "2021-07-17", opt).toJSON());
  assertEquals("P7D", cal.dateUntil("2021-07-16", "2021-07-23", opt).toJSON());
  assertEquals("P1M", cal.dateUntil("2021-07-16", "2021-08-16", opt).toJSON());
  assertEquals("P1M", cal.dateUntil("2020-12-16", "2021-01-16", opt).toJSON());
  assertEquals("P1M", cal.dateUntil("2021-01-05", "2021-02-05", opt).toJSON());
  assertEquals("P2M", cal.dateUntil("2021-01-07", "2021-03-07", opt).toJSON());
  assertEquals("P1M1D",
      cal.dateUntil("2021-07-16", "2021-08-17", opt).toJSON());
  assertEquals("P28D", cal.dateUntil("2021-07-16", "2021-08-13", opt).toJSON());
  assertEquals("P2M", cal.dateUntil("2021-07-16", "2021-09-16", opt).toJSON());
  assertEquals("P12M",
      cal.dateUntil("2021-07-16", "2022-07-16", opt).toJSON());
  assertEquals("P120M"
      ,cal.dateUntil("2021-07-16", "2031-07-16", opt).toJSON());

  assertEquals("-P1D",
      cal.dateUntil("2021-07-17", "2021-07-16", opt).toJSON());
  assertEquals("-P1M1D",
      cal.dateUntil("2021-08-17", "2021-07-16", opt).toJSON());
  assertEquals("-P28D",
      cal.dateUntil("2021-08-13", "2021-07-16", opt).toJSON());
  assertEquals("-P1M",
      cal.dateUntil("2021-08-16", "2021-07-16", opt).toJSON());
  assertEquals("-P1M3D",
      cal.dateUntil("2021-08-16", "2021-07-13", opt).toJSON());
  assertEquals("-P2M",
      cal.dateUntil("2021-09-16", "2021-07-16", opt).toJSON());
  assertEquals("-P2M5D",
      cal.dateUntil("2021-09-21", "2021-07-16", opt).toJSON());
  assertEquals("-P12M",
      cal.dateUntil("2022-07-16", "2021-07-16", opt).toJSON());
  assertEquals("-P12M1D",
      cal.dateUntil("2022-07-17", "2021-07-16", opt).toJSON());
  assertEquals("-P120M",
      cal.dateUntil("2031-07-16", "2021-07-16", opt).toJSON());
});

["year", "years"].forEach(function(largestUnit) {
  let opt = {largestUnit};
  assertEquals("PT0S", cal.dateUntil("2021-07-16", "2021-07-16", opt).toJSON());
  assertEquals("P1D", cal.dateUntil("2021-07-16", "2021-07-17", opt).toJSON());
  assertEquals("P7D", cal.dateUntil("2021-07-16", "2021-07-23", opt).toJSON());
  assertEquals("P1M", cal.dateUntil("2021-07-16", "2021-08-16", opt).toJSON());
  assertEquals("P1M", cal.dateUntil("2020-12-16", "2021-01-16", opt).toJSON());
  assertEquals("P1M", cal.dateUntil("2021-01-05", "2021-02-05", opt).toJSON());
  assertEquals("P2M", cal.dateUntil("2021-01-07", "2021-03-07", opt).toJSON());
  assertEquals("P1M1D", cal.dateUntil("2021-07-16", "2021-08-17", opt).toJSON());
  assertEquals("P28D", cal.dateUntil("2021-07-16", "2021-08-13", opt).toJSON());
  assertEquals("P2M", cal.dateUntil("2021-07-16", "2021-09-16", opt).toJSON());
  assertEquals("P1Y",
      cal.dateUntil("2021-07-16", "2022-07-16", opt).toJSON());
  assertEquals("P1Y3D",
      cal.dateUntil("2021-07-16", "2022-07-19", opt).toJSON());
  assertEquals("P1Y2M3D",
      cal.dateUntil("2021-07-16", "2022-09-19", opt).toJSON());
  assertEquals("P10Y",
      cal.dateUntil("2021-07-16", "2031-07-16", opt).toJSON());
  assertEquals("P10Y5M",
      cal.dateUntil("2021-07-16", "2031-12-16", opt).toJSON());
  assertEquals("P23Y7M",
      cal.dateUntil("1997-12-16", "2021-07-16", opt).toJSON());
  assertEquals("P24Y",
      cal.dateUntil("1997-07-16", "2021-07-16", opt).toJSON());
  assertEquals("P23Y11M29D",
      cal.dateUntil("1997-07-16", "2021-07-15", opt).toJSON());
  assertEquals("P23Y11M30D",
      cal.dateUntil("1997-06-16", "2021-06-15", opt).toJSON());
  assertEquals("P60Y1M",
      cal.dateUntil("1960-02-16", "2020-03-16", opt).toJSON());
  assertEquals("P61Y27D",
      cal.dateUntil("1960-02-16", "2021-03-15", opt).toJSON());
  assertEquals("P60Y28D",
      cal.dateUntil("1960-02-16", "2020-03-15", opt).toJSON());

  assertEquals("P3M16D",
      cal.dateUntil("2021-03-30", "2021-07-16", opt).toJSON());
  assertEquals("P1Y3M16D",
      cal.dateUntil("2020-03-30", "2021-07-16", opt).toJSON());
  assertEquals("P61Y3M16D",
      cal.dateUntil("1960-03-30", "2021-07-16", opt).toJSON());
  assertEquals("P1Y6M16D",
      cal.dateUntil("2019-12-30", "2021-07-16", opt).toJSON());
  assertEquals("P6M16D",
      cal.dateUntil("2020-12-30", "2021-07-16", opt).toJSON());
  assertEquals("P23Y6M16D",
      cal.dateUntil("1997-12-30", "2021-07-16", opt).toJSON());
  assertEquals("P2019Y6M21D",
      cal.dateUntil("0001-12-25", "2021-07-16", opt).toJSON());
  assertEquals("P1Y2M5D",
      cal.dateUntil("2019-12-30", "2021-03-05", opt).toJSON());

  assertEquals("-P1D",
      cal.dateUntil("2021-07-17", "2021-07-16", opt).toJSON());
  assertEquals("-P1M1D",
      cal.dateUntil("2021-08-17", "2021-07-16", opt).toJSON());
  assertEquals("-P28D",
      cal.dateUntil("2021-08-13", "2021-07-16", opt).toJSON());
  assertEquals("-P1M",
      cal.dateUntil("2021-08-16", "2021-07-16", opt).toJSON());
  assertEquals("-P1M3D",
      cal.dateUntil("2021-08-16", "2021-07-13", opt).toJSON());
  assertEquals("-P2M",
      cal.dateUntil("2021-09-16", "2021-07-16", opt).toJSON());
  assertEquals("-P2M5D",
      cal.dateUntil("2021-09-21", "2021-07-16", opt).toJSON());
  assertEquals("-P1Y",
      cal.dateUntil("2022-07-16", "2021-07-16", opt).toJSON());
  assertEquals("-P1Y1D",
      cal.dateUntil("2022-07-17", "2021-07-16", opt).toJSON());
  assertEquals("-P1Y3M1D",
      cal.dateUntil("2022-10-17", "2021-07-16", opt).toJSON());
  assertEquals("-P10Y",
      cal.dateUntil("2031-07-16", "2021-07-16", opt).toJSON());
  assertEquals("-P10Y11M",
      cal.dateUntil("2032-07-16", "2021-08-16", opt).toJSON());

  assertEquals("-P10Y5M",
      cal.dateUntil("2031-12-16", "2021-07-16", opt).toJSON());
  assertEquals("-P13Y7M",
      cal.dateUntil("2011-07-16", "1997-12-16", opt).toJSON());
  assertEquals("-P24Y",
      cal.dateUntil("2021-07-16", "1997-07-16", opt).toJSON());
  assertEquals("-P23Y11M30D",
      cal.dateUntil("2021-07-15", "1997-07-16", opt).toJSON());
  assertEquals("-P23Y11M29D",
      cal.dateUntil("2021-06-15", "1997-06-16", opt).toJSON());
  assertEquals("-P60Y1M",
      cal.dateUntil("2020-03-16", "1960-02-16", opt).toJSON());
  assertEquals("-P61Y28D",
      cal.dateUntil("2021-03-15", "1960-02-16", opt).toJSON());
  assertEquals("-P60Y28D",
      cal.dateUntil("2020-03-15", "1960-02-16", opt).toJSON());

  assertEquals("-P61Y3M17D",
      cal.dateUntil("2021-07-16", "1960-03-30", opt).toJSON());

  assertEquals("-P2019Y6M22D",
      cal.dateUntil("2021-07-16", "0001-12-25", opt).toJSON());
  assertEquals("-P23Y6M17D",
      cal.dateUntil("2021-07-16", "1997-12-30", opt).toJSON());
});
