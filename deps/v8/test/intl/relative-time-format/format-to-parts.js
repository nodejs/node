// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-relative-time-format

// Make sure that RelativeTimeFormat exposes all required properties. Those not specified
// should have undefined value.
// http://tc39.github.io/proposal-intl-relative-time/

let rtf = new Intl.RelativeTimeFormat();

// Test 1.4.4 Intl.RelativeTimeFormat.prototype.formatToParts( value, unit )
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'seconds')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'second')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'minutes')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'minute')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'hours')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'hour')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'days')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'day')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'weeks')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'week')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'months')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'month')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'quarters')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'quarter')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'years')));
assertEquals(true, Array.isArray(rtf.formatToParts(-1, 'year')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'seconds')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'second')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'minutes')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'minute')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'hours')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'hour')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'days')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'day')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'weeks')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'week')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'months')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'month')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'quarters')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'quarter')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'years')));
assertEquals(true, Array.isArray(rtf.formatToParts(-0, 'year')));

assertThrows(() => rtf.formatToParts(-1, 'decades'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'decade'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'centuries'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'century'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'milliseconds'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'millisecond'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'microseconds'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'microsecond'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'nanoseconds'), RangeError);
assertThrows(() => rtf.formatToParts(-1, 'nanosecond'), RangeError);

assertThrows(() => rtf.formatToParts(NaN, 'seconds'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'second'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'minutes'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'minute'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'hours'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'hour'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'days'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'day'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'weeks'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'week'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'months'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'month'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'years'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'year'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'quarters'), RangeError);
assertThrows(() => rtf.formatToParts(NaN, 'quarter'), RangeError);

assertEquals(true, Array.isArray(rtf.formatToParts(100, 'day')));
rtf.formatToParts(100, 'day').forEach(function(part) {
  assertEquals(true, part.type == 'literal' || part.type == 'integer');
  assertEquals('string', typeof part.value);
  if (part.type == 'integer') {
    assertEquals('string', typeof part.unit);
  }
});
