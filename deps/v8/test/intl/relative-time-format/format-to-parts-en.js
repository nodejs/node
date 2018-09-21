// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-relative-time-format

// The following test are not part of the comformance. Just some output in
// English to verify the format does return something reasonable for English.
// It may be changed when we update the CLDR data.
// NOTE: These are UNSPECIFIED behavior in
// http://tc39.github.io/proposal-intl-relative-time/

// From Sample code in https://github.com/tc39/proposal-intl-relative-time#intlrelativetimeformatprototypeformattopartsvalue-unit
//   // Format relative time using the day unit.
//   rtf.formatToParts(-1, "day");
//   // > [{ type: "literal", value: "yesterday"}]
let longAuto = new Intl.RelativeTimeFormat(
    "en", {style: "long", localeMatcher: 'lookup', numeric: 'auto'});
var parts = longAuto.formatToParts(-1, "day");
assertEquals(1, parts.length);
assertEquals(2, Object.getOwnPropertyNames(parts[0]).length);
assertEquals('literal', parts[0].type);
assertEquals('yesterday', parts[0].value);

// From Sample code in https://github.com/tc39/proposal-intl-relative-time#intlrelativetimeformatprototypeformattopartsvalue-unit
//   rtf.formatToParts(100, "day");
//   // > [{ type: "literal", value: "in " }, { type: "integer", value: "100", unit: "day" }, { type: "literal", value: " days" }]
let longAlways = new Intl.RelativeTimeFormat(
    "en", {style: "long", localeMatcher: 'lookup', numeric: 'always'});

parts = longAlways.formatToParts(100, "day");
assertEquals(3, parts.length);

assertEquals(2, Object.getOwnPropertyNames(parts[0]).length);
assertEquals('literal', parts[0].type);
assertEquals('in ', parts[0].value);

assertEquals(3, Object.getOwnPropertyNames(parts[1]).length);
assertEquals('integer', parts[1].type);
assertEquals('100', parts[1].value);
assertEquals('day', parts[1].unit);

assertEquals(2, Object.getOwnPropertyNames(parts[2]).length);
assertEquals('literal', parts[2].type);
assertEquals(' days', parts[2].value);

assertThrows(() => longAlways.format(NaN, 'second'), RangeError);
assertThrows(() => longAuto.format(NaN, 'second'), RangeError);

parts = longAlways.formatToParts(-10, "day");
assertEquals(2, parts.length);
assertEquals(3, Object.getOwnPropertyNames(parts[0]).length);
assertEquals('integer', parts[0].type);
assertEquals('10', parts[0].value);
assertEquals('day', parts[0].unit);
assertEquals(2, Object.getOwnPropertyNames(parts[1]).length);
assertEquals('literal', parts[1].type);
assertEquals(' days ago', parts[1].value);

parts = longAlways.formatToParts(-0, "day");
assertEquals(2, parts.length);
assertEquals(3, Object.getOwnPropertyNames(parts[0]).length);
assertEquals('integer', parts[0].type);
assertEquals('0', parts[0].value);
assertEquals('day', parts[0].unit);
assertEquals(2, Object.getOwnPropertyNames(parts[1]).length);
assertEquals('literal', parts[1].type);
assertEquals(' days ago', parts[1].value);
