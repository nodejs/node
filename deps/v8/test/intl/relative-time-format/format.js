// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure that RelativeTimeFormat exposes all required properties. Those not specified
// should have undefined value.
// http://tc39.github.io/proposal-intl-relative-time/

let rtf = new Intl.RelativeTimeFormat();

// Test 1.4.3 Intl.RelativeTimeFormat.prototype.format( value, unit )
assertEquals('string', typeof rtf.format(-1, 'seconds'));
assertEquals('string', typeof rtf.format(-1, 'second'));
assertEquals('string', typeof rtf.format(-1, 'minutes'));
assertEquals('string', typeof rtf.format(-1, 'minute'));
assertEquals('string', typeof rtf.format(-1, 'hours'));
assertEquals('string', typeof rtf.format(-1, 'hour'));
assertEquals('string', typeof rtf.format(-1, 'days'));
assertEquals('string', typeof rtf.format(-1, 'day'));
assertEquals('string', typeof rtf.format(-1, 'weeks'));
assertEquals('string', typeof rtf.format(-1, 'week'));
assertEquals('string', typeof rtf.format(-1, 'months'));
assertEquals('string', typeof rtf.format(-1, 'month'));
assertEquals('string', typeof rtf.format(-1, 'years'));
assertEquals('string', typeof rtf.format(-1, 'year'));
assertEquals('string', typeof rtf.format(-1, 'quarter'));
assertEquals('string', typeof rtf.format(-1, 'quarters'));

assertEquals('string', typeof rtf.format(-0, 'seconds'));
assertEquals('string', typeof rtf.format(-0, 'second'));
assertEquals('string', typeof rtf.format(-0, 'minutes'));
assertEquals('string', typeof rtf.format(-0, 'minute'));
assertEquals('string', typeof rtf.format(-0, 'hours'));
assertEquals('string', typeof rtf.format(-0, 'hour'));
assertEquals('string', typeof rtf.format(-0, 'days'));
assertEquals('string', typeof rtf.format(-0, 'day'));
assertEquals('string', typeof rtf.format(-0, 'weeks'));
assertEquals('string', typeof rtf.format(-0, 'week'));
assertEquals('string', typeof rtf.format(-0, 'months'));
assertEquals('string', typeof rtf.format(-0, 'month'));
assertEquals('string', typeof rtf.format(-0, 'years'));
assertEquals('string', typeof rtf.format(-0, 'year'));
assertEquals('string', typeof rtf.format(-0, 'quarter'));
assertEquals('string', typeof rtf.format(-0, 'quarters'));

assertThrows(() => rtf.format(NaN, 'seconds'), RangeError);
assertThrows(() => rtf.format(NaN, 'second'), RangeError);
assertThrows(() => rtf.format(NaN, 'minutes'), RangeError);
assertThrows(() => rtf.format(NaN, 'minute'), RangeError);
assertThrows(() => rtf.format(NaN, 'hours'), RangeError);
assertThrows(() => rtf.format(NaN, 'hour'), RangeError);
assertThrows(() => rtf.format(NaN, 'days'), RangeError);
assertThrows(() => rtf.format(NaN, 'day'), RangeError);
assertThrows(() => rtf.format(NaN, 'weeks'), RangeError);
assertThrows(() => rtf.format(NaN, 'week'), RangeError);
assertThrows(() => rtf.format(NaN, 'months'), RangeError);
assertThrows(() => rtf.format(NaN, 'month'), RangeError);
assertThrows(() => rtf.format(NaN, 'years'), RangeError);
assertThrows(() => rtf.format(NaN, 'year'), RangeError);
assertThrows(() => rtf.format(NaN, 'quarters'), RangeError);
assertThrows(() => rtf.format(NaN, 'quarter'), RangeError);

assertThrows(() => rtf.format(-1, 'decades'), RangeError);
assertThrows(() => rtf.format(-1, 'decade'), RangeError);
assertThrows(() => rtf.format(-1, 'centuries'), RangeError);
assertThrows(() => rtf.format(-1, 'century'), RangeError);
assertThrows(() => rtf.format(-1, 'milliseconds'), RangeError);
assertThrows(() => rtf.format(-1, 'millisecond'), RangeError);
assertThrows(() => rtf.format(-1, 'microseconds'), RangeError);
assertThrows(() => rtf.format(-1, 'microsecond'), RangeError);
assertThrows(() => rtf.format(-1, 'nanoseconds'), RangeError);
assertThrows(() => rtf.format(-1, 'nanosecond'), RangeError);

assertEquals('string', typeof rtf.format(5, 'day'));
assertEquals('string', typeof rtf.format('5', 'day'));
assertEquals('string', typeof rtf.format('-5', 'day'));
assertEquals('string', typeof rtf.format('534', 'day'));
assertEquals('string', typeof rtf.format('-534', 'day'));

//assertThrows(() => rtf.format('xyz', 'day'), RangeError);
