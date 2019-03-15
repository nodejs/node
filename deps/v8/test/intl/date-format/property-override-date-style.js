// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-datetime-style

// Checks for security holes introduced by Object.property overrides.
// For example:
// Object.defineProperty(Array.prototype, 'locale', {
//   set: function(value) {
//     throw new Error('blah');
//   },
//   configurable: true,
//   enumerable: false
// });
//
// would throw in case of (JS) x.locale = 'us' or (C++) x->Set('locale', 'us').
//
// First get supported properties.
// Some of the properties are optional, so we request them.
var properties = [];
var options = Intl.DateTimeFormat(
  'en-US', {dateStyle: 'full'}).resolvedOptions();
for (var prop in options) {
  if (options.hasOwnProperty(prop)) {
    properties.push(prop);
  }
}

// In the order of Table 6 of
// ecma402 #sec-intl.datetimeformat.prototype.resolvedoptions
var expectedProperties = [
  'locale',
  'calendar',
  'numberingSystem',
  'timeZone',
  'hourCycle',
  'hour12',
  'weekday',
  'year',
  'month',
  'day',
  'dateStyle',
];

assertEquals(expectedProperties.length, properties.length);

properties.forEach(function(prop) {
  assertFalse(expectedProperties.indexOf(prop) === -1);
});

taintProperties(properties);

var locale = Intl.DateTimeFormat().resolvedOptions().locale;
