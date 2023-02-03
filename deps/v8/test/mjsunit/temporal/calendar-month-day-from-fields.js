// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.monthdayfromfields
let cal = new Temporal.Calendar("iso8601")

// Check throw for first arg
assertThrows(() => cal.monthDayFromFields(), TypeError);
[undefined, true, false, 123, 456n, Symbol(), "string"].forEach(
    function(fields) {
      assertThrows(() => cal.monthDayFromFields(fields), TypeError);
      assertThrows(() => cal.monthDayFromFields(fields, undefined), TypeError);
      assertThrows(() => cal.monthDayFromFields(fields,
            {overflow: "constrain"}), TypeError);
      assertThrows(() => cal.monthDayFromFields(fields, {overflow: "reject"}),
          TypeError);
    });

assertThrows(() => cal.monthDayFromFields({month: 1, day: 17}), TypeError);
assertThrows(() => cal.monthDayFromFields({year: 2021, day: 17}), TypeError);
assertThrows(() => cal.monthDayFromFields({year: 2021, month: 12}), TypeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "m1", day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M1", day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "m01", day: 17}), RangeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 12, monthCode: "M11", day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M00", day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M19", day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M99", day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M13", day: 17}), RangeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: -1, day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: -Infinity, day: 17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 7, day: -17}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 7, day: -Infinity}), RangeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 12, day: 0}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 12, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 1, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 2, day: 29}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 6, day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 9, day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 0, day: 5}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 13, day: 5}, {overflow: "reject"}), RangeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M12", day: 0}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M12", day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M01", day: 32}, {overflow: "reject"}), RangeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M06", day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M09", day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M00", day: 5}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, monthCode: "M13", day: 5}, {overflow: "reject"}), RangeError);

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 12, day: 0}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 0, day: 3}), RangeError);

// Check throw for the second arg
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 7, day: 13}, {overflow: "invalid"}), RangeError);

assertEquals("07-15", cal.monthDayFromFields(
    {year: 2021, month: 7, day: 15}).toJSON());
assertEquals("07-03", cal.monthDayFromFields(
    {year: 2021, month: 7, day: 3}).toJSON());
assertEquals("12-31", cal.monthDayFromFields(
    {year: 2021, month: 12, day: 31}).toJSON());
assertEquals("07-15", cal.monthDayFromFields(
    {year: 2021, monthCode: "M07", day: 15}).toJSON());
assertEquals("07-03", cal.monthDayFromFields(
    {year: 2021, monthCode: "M07", day: 3}).toJSON());
assertEquals("12-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M12", day: 31}).toJSON());
assertEquals("02-29", cal.monthDayFromFields(
    {year: 2021, monthCode: "M02", day: 29}).toJSON());

assertEquals("01-31", cal.monthDayFromFields(
    {year: 2021, month: 1, day: 133}).toJSON());
assertEquals("02-28", cal.monthDayFromFields(
    {year: 2021, month: 2, day: 133}).toJSON());
assertEquals("03-31", cal.monthDayFromFields(
    {year: 2021, month: 3, day: 9033}).toJSON());
assertEquals("04-30", cal.monthDayFromFields(
    {year: 2021, month: 4, day: 50}).toJSON());
assertEquals("05-31", cal.monthDayFromFields(
    {year: 2021, month: 5, day: 77}).toJSON());
assertEquals("06-30", cal.monthDayFromFields(
    {year: 2021, month: 6, day: 33}).toJSON());
assertEquals("07-31", cal.monthDayFromFields(
    {year: 2021, month: 7, day: 33}).toJSON());
assertEquals("08-31", cal.monthDayFromFields(
    {year: 2021, month: 8, day: 300}).toJSON());
assertEquals("09-30", cal.monthDayFromFields(
    {year: 2021, month: 9, day: 400}).toJSON());
assertEquals("10-31", cal.monthDayFromFields(
    {year: 2021, month: 10, day: 400}).toJSON());
assertEquals("11-30", cal.monthDayFromFields(
    {year: 2021, month: 11, day: 400}).toJSON());
assertEquals("12-31", cal.monthDayFromFields(
    {year: 2021, month: 12, day: 500}).toJSON());
assertEquals("12-31", cal.monthDayFromFields(
    {year: 2021, month: 13, day: 500}).toJSON());
assertEquals("12-31", cal.monthDayFromFields(
    {year: 2021, month: 999999, day: 500}).toJSON());
assertEquals("01-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M01", day: 133}).toJSON());
assertEquals("02-29", cal.monthDayFromFields(
    {year: 2021, monthCode: "M02", day: 133}).toJSON());
assertEquals("03-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M03", day: 9033}).toJSON());
assertEquals("04-30", cal.monthDayFromFields(
    {year: 2021, monthCode: "M04", day: 50}).toJSON());
assertEquals("05-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M05", day: 77}).toJSON());
assertEquals("06-30", cal.monthDayFromFields(
    {year: 2021, monthCode: "M06", day: 33}).toJSON());
assertEquals("07-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M07", day: 33}).toJSON());
assertEquals("08-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M08", day: 300}).toJSON());
assertEquals("09-30", cal.monthDayFromFields(
    {year: 2021, monthCode: "M09", day: 400}).toJSON());
assertEquals("10-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M10", day: 400}).toJSON());
assertEquals("11-30", cal.monthDayFromFields(
    {year: 2021, monthCode: "M11", day: 400}).toJSON());
assertEquals("12-31", cal.monthDayFromFields(
    {year: 2021, monthCode: "M12", day: 500}).toJSON());

assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 1, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 2, day: 29}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 3, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 4, day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 5, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 6, day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 7, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 8, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 9, day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 10, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 11, day: 31}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 12, day: 32}, {overflow: "reject"}), RangeError);
assertThrows(() => cal.monthDayFromFields(
    {year: 2021, month: 13, day: 5}, {overflow: "reject"}), RangeError);
