// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let descriptor = Object.getOwnPropertyDescriptor(
      Intl.DateTimeFormat.prototype, "formatRangeToParts");
assertTrue(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

const date1 = new Date("2019-1-3");
const date2 = new Date("2019-1-5");
const date3 = new Date("2019-3-4");
const date4 = new Date("2020-3-4");
let dtf = new Intl.DateTimeFormat();
assertThrows(() => dtf.formatRangeToParts(), RangeError);
assertThrows(() => dtf.formatRangeToParts(date1), RangeError);
assertThrows(() => dtf.formatRangeToParts(undefined, date2), RangeError);
assertThrows(() => dtf.formatRangeToParts(date1, undefined), RangeError);
assertThrows(() => dtf.formatRangeToParts("2019-1-3", date2), RangeError);
assertThrows(() => dtf.formatRangeToParts(date1, "2019-5-4"), RangeError);
assertThrows(() => dtf.formatRangeToParts(date2, date1), RangeError);

assertDoesNotThrow(() =>dtf.formatRangeToParts(date1, date2));

function partsToString(parts) {
  return parts.map(x => x.value).join("");
}

const validSources = ["startRange", "endRange", "shared"];
const validTypes = ["literal", "year", "month", "day", "hour", "minute", "second",
                    "weekday", "dayPeriod", "timeZoneName", "era"];

function assertParts(parts) {
  const str = partsToString(parts);
  parts.forEach(function(part) {
    // Check the range of part.source
    assertTrue(validSources.includes(part.source),
        "Invalid source '" + part.source + "' in '" + str + "' for '" + part.value + "'");
    // Check the range of part.type
    assertTrue(validTypes.includes(part.type),
        "Invalid type '" + part.type + "' in '" + str + "' for '" + part.value + "'");
    // Check the part.value is a string
    assertEquals("string", typeof part.value, "Invalid value for '" + str + "'");
  });
}

function verifyFormatRangeToParts(a, b, dtf) {
  var parts = dtf.formatRangeToParts(a, b);
  // Check each parts fulfill basic property of the parts.
  assertParts(parts);
  // ensure the 'value' in the parts is the same as the output of
  // the formatRange.
  assertEquals(dtf.formatRange(a, b), partsToString(parts));
}

verifyFormatRangeToParts(date1, date2, dtf);
verifyFormatRangeToParts(date1, date3, dtf);
verifyFormatRangeToParts(date1, date4, dtf);
verifyFormatRangeToParts(date2, date3, dtf);
verifyFormatRangeToParts(date2, date4, dtf);
verifyFormatRangeToParts(date3, date4, dtf);

dtf = new Intl.DateTimeFormat(["en"], {year: "numeric", month: "short", day: "numeric"});

verifyFormatRangeToParts(date1, date2, dtf);
verifyFormatRangeToParts(date1, date3, dtf);
verifyFormatRangeToParts(date1, date4, dtf);
verifyFormatRangeToParts(date2, date3, dtf);
verifyFormatRangeToParts(date2, date4, dtf);
verifyFormatRangeToParts(date3, date4, dtf);

// Test the sequence of ToNumber and TimeClip
var secondDateAccessed = false;
assertThrows(
    () =>
    dtf.formatRangeToParts(
        new Date(864000000*10000000 + 1), // a date will cause TimeClip return NaN
        { get [Symbol.toPrimitive]() { secondDateAccessed = true; return {}} }),
    TypeError);
assertTrue(secondDateAccessed);
