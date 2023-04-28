// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.yearmonthfromfields
let cal = new Temporal.Calendar("iso8601")

// Check throw for first arg
assertThrows(() => cal.yearMonthFromFields(),
    TypeError);
[undefined, true, false, 123, 456n, Symbol(), "string"].forEach(
    function(fields) {
      assertThrows(() => cal.yearMonthFromFields(fields), TypeError);
      assertThrows(() => cal.yearMonthFromFields(fields, undefined), TypeError);
      assertThrows(() => cal.yearMonthFromFields(fields,
            {overflow: "constrain"}), TypeError);
      assertThrows(() => cal.yearMonthFromFields(fields,
            {overflow: "reject"}), TypeError);
    });

assertThrows(() => cal.yearMonthFromFields({month: 1}), TypeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021}), TypeError);

assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "m1"}),
    RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "M1"}),
    RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "m01"}),
    RangeError);

assertThrows(() => cal.yearMonthFromFields({year: 2021, month: 12,
  monthCode: "M11"}), RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "M00"}),
    RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "M19"}),
    RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "M99"}),
    RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, monthCode: "M13"}),
    RangeError);

assertThrows(() => cal.yearMonthFromFields({year: 2021, month: -1}),
    RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, month: -Infinity}),
    RangeError);

assertThrows(() => cal.yearMonthFromFields({year: 2021, month: 0, day: 5},
      {overflow: "reject"}), RangeError);
assertThrows(() => cal.yearMonthFromFields({year: 2021, month: 13, day: 5},
      {overflow: "reject"}), RangeError);

assertThrows(() => cal.yearMonthFromFields(
    {year: 2021, monthCode: "M00"}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.yearMonthFromFields(
    {year: 2021, monthCode: "M13"}, {overflow: "reject"}), RangeError);

assertThrows(() => cal.yearMonthFromFields(
    {year: 2021, month: 0}), RangeError);

// Check throw for the second arg
assertThrows(() => cal.yearMonthFromFields(
    {year: 2021, month: 7}, {overflow: "invalid"}), RangeError);

assertEquals("2021-07",
    cal.yearMonthFromFields({year: 2021, month: 7}).toJSON());
assertEquals("2021-12",
    cal.yearMonthFromFields({year: 2021, month: 12}).toJSON());
assertEquals("2021-07",
    cal.yearMonthFromFields({year: 2021, monthCode: "M07"}).toJSON());
assertEquals("2021-12",
    cal.yearMonthFromFields({year: 2021, monthCode: "M12"}).toJSON());

assertEquals("2021-01",
    cal.yearMonthFromFields({year: 2021, month: 1}).toJSON());
assertEquals("2021-02",
    cal.yearMonthFromFields({year: 2021, month: 2}).toJSON());
assertEquals("2021-03",
    cal.yearMonthFromFields({year: 2021, month: 3}).toJSON());
assertEquals("2021-04",
    cal.yearMonthFromFields({year: 2021, month: 4}).toJSON());
assertEquals("2021-05",
    cal.yearMonthFromFields({year: 2021, month: 5}).toJSON());
assertEquals("2021-06",
    cal.yearMonthFromFields({year: 2021, month: 6}).toJSON());
assertEquals("2021-07",
    cal.yearMonthFromFields({year: 2021, month: 7}).toJSON());
assertEquals("2021-08",
    cal.yearMonthFromFields({year: 2021, month: 8}).toJSON());
assertEquals("2021-09",
    cal.yearMonthFromFields({year: 2021, month: 9}).toJSON());
assertEquals("2021-10",
    cal.yearMonthFromFields({year: 2021, month: 10}).toJSON());
assertEquals("2021-11",
    cal.yearMonthFromFields({year: 2021, month: 11}).toJSON());
assertEquals("2021-12",
    cal.yearMonthFromFields({year: 2021, month: 12}).toJSON());
assertEquals("2021-12",
    cal.yearMonthFromFields({year: 2021, month: 13}).toJSON());
assertEquals("2021-12",
    cal.yearMonthFromFields({year: 2021, month: 999999}).toJSON());
assertEquals("2021-01",
    cal.yearMonthFromFields({year: 2021, monthCode: "M01"}).toJSON());
assertEquals("2021-02",
    cal.yearMonthFromFields({year: 2021, monthCode: "M02"}).toJSON());
assertEquals("2021-03",
    cal.yearMonthFromFields({year: 2021, monthCode: "M03"}).toJSON());
assertEquals("2021-04",
    cal.yearMonthFromFields({year: 2021, monthCode: "M04"}).toJSON());
assertEquals("2021-05",
    cal.yearMonthFromFields({year: 2021, monthCode: "M05"}).toJSON());
assertEquals("2021-06",
    cal.yearMonthFromFields({year: 2021, monthCode: "M06"}).toJSON());
assertEquals("2021-07",
    cal.yearMonthFromFields({year: 2021, monthCode: "M07"}).toJSON());
assertEquals("2021-08",
    cal.yearMonthFromFields({year: 2021, monthCode: "M08"}).toJSON());
assertEquals("2021-09",
    cal.yearMonthFromFields({year: 2021, monthCode: "M09"}).toJSON());
assertEquals("2021-10",
    cal.yearMonthFromFields({year: 2021, monthCode: "M10"}).toJSON());
assertEquals("2021-11",
    cal.yearMonthFromFields({year: 2021, monthCode: "M11"}).toJSON());
assertEquals("2021-12",
    cal.yearMonthFromFields({year: 2021, monthCode: "M12"}).toJSON());

assertThrows(() => cal.yearMonthFromFields(
    {year: 2021, month: 13}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.yearMonthFromFields(
    {year: 2021, month: 9995}, {overflow: "reject"}), RangeError);
