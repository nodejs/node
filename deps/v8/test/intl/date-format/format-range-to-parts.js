// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-date-format-range

let descriptor = Object.getOwnPropertyDescriptor(
      Intl.DateTimeFormat.prototype, "formatRangeToParts");
assertTrue(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

const date1 = new Date("2019-1-3");
const date2 = new Date("2019-3-4");
const dtf = new Intl.DateTimeFormat();
assertThrows(() => dtf.formatRangeToParts(), RangeError);
assertThrows(() => dtf.formatRangeToParts(date1), RangeError);
assertThrows(() => dtf.formatRangeToParts(undefined, date2), RangeError);
assertThrows(() => dtf.formatRangeToParts(date1, undefined), RangeError);
assertThrows(() => dtf.formatRangeToParts("2019-1-3", date2), RangeError);
assertThrows(() => dtf.formatRangeToParts(date1, "2019-5-4"), RangeError);
assertThrows(() => dtf.formatRangeToParts(date2, date1), RangeError);

assertDoesNotThrow(() =>dtf.formatRangeToParts(date1, date2));
