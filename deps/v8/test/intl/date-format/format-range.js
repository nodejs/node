// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let descriptor = Object.getOwnPropertyDescriptor(
      Intl.DateTimeFormat.prototype, "formatRange");
assertTrue(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

const date1 = new Date("2019-1-3");
const date2 = new Date("2019-1-5");
const date3 = new Date("2019-3-4");
const date4 = new Date("2020-3-4");
var dtf = new Intl.DateTimeFormat(["en"]);
assertThrows(() => dtf.formatRange(), RangeError);
assertThrows(() => dtf.formatRange(date1), RangeError);
assertThrows(() => dtf.formatRange(undefined, date2), RangeError);
assertThrows(() => dtf.formatRange(date1, undefined), RangeError);
assertThrows(() => dtf.formatRange("2019-1-3", date2), RangeError);
assertThrows(() => dtf.formatRange(date1, "2019-5-4"), RangeError);
assertThrows(() => dtf.formatRange(date2, date1), RangeError);

assertDoesNotThrow(() =>dtf.formatRange(date1, date2));

assertEquals("1/3/2019 – 1/5/2019", dtf.formatRange(date1, date2));
assertEquals("1/3/2019 – 3/4/2019", dtf.formatRange(date1, date3));
assertEquals("1/3/2019 – 3/4/2020", dtf.formatRange(date1, date4));
assertEquals("1/5/2019 – 3/4/2019", dtf.formatRange(date2, date3));
assertEquals("1/5/2019 – 3/4/2020", dtf.formatRange(date2, date4));
assertEquals("3/4/2019 – 3/4/2020", dtf.formatRange(date3, date4));

dtf = new Intl.DateTimeFormat(["en"], {year: "numeric", month: "short", day: "numeric"});
assertEquals("Jan 3 – 5, 2019", dtf.formatRange(date1, date2));
assertEquals("Jan 3 – Mar 4, 2019", dtf.formatRange(date1, date3));
assertEquals("Jan 3, 2019 – Mar 4, 2020", dtf.formatRange(date1, date4));
assertEquals("Jan 5 – Mar 4, 2019", dtf.formatRange(date2, date3));
assertEquals("Jan 5, 2019 – Mar 4, 2020", dtf.formatRange(date2, date4));
assertEquals("Mar 4, 2019 – Mar 4, 2020", dtf.formatRange(date3, date4));

// Test the sequence of ToNumber and TimeClip
var secondDateAccessed = false;
assertThrows(
    () =>
    dtf.formatRange(
        new Date(864000000*10000000 + 1), // a date will cause TimeClip return NaN
        { get [Symbol.toPrimitive]() { secondDateAccessed = true; return {}} }),
    TypeError);
assertTrue(secondDateAccessed);
