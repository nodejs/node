// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-numberformat-unified

// Test notation: "scientific" with formatToParts.

const nf = Intl.NumberFormat("en", {notation: "scientific"});

let parts = nf.formatToParts(123.456);
// [{type: "integer", value: "1"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "235"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "2"}]
assertEquals("integer", parts[0].type);
assertEquals("1", parts[0].value);
assertEquals("decimal", parts[1].type);
assertEquals(".", parts[1].value);
assertEquals("fraction", parts[2].type);
assertEquals("235", parts[2].value);
assertEquals("exponentSeparator", parts[3].type);
assertEquals("E", parts[3].value);
assertEquals("exponentInteger", parts[4].type);
assertEquals("2", parts[4].value);
assertEquals(5, parts.length);

parts = nf.formatToParts(-123.456);
// [{type: "minusSign", value: "-"},i
//  {type: "integer", value: "1"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "235"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "2"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("1", parts[1].value);
assertEquals("decimal", parts[2].type);
assertEquals(".", parts[2].value);
assertEquals("fraction", parts[3].type);
assertEquals("235", parts[3].value);
assertEquals("exponentSeparator", parts[4].type);
assertEquals("E", parts[4].value);
assertEquals("exponentInteger", parts[5].type);
assertEquals("2", parts[5].value);
assertEquals(6, parts.length);

parts = nf.formatToParts(12345678901234567890);
// [{type: "integer", value: "1"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "235"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "19"}]
assertEquals("integer", parts[0].type);
assertEquals("1", parts[0].value);
assertEquals("decimal", parts[1].type);
assertEquals(".", parts[1].value);
assertEquals("fraction", parts[2].type);
assertEquals("235", parts[2].value);
assertEquals("exponentSeparator", parts[3].type);
assertEquals("E", parts[3].value);
assertEquals("exponentInteger", parts[4].type);
assertEquals("19", parts[4].value);
assertEquals(5, parts.length);

parts = nf.formatToParts(-12345678901234567890);
// [{type: "minusSign", value: "-"},
// {type: "integer", value: "1"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "235"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "19"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("1", parts[1].value);
assertEquals("decimal", parts[2].type);
assertEquals(".", parts[2].value);
assertEquals("fraction", parts[3].type);
assertEquals("235", parts[3].value);
assertEquals("exponentSeparator", parts[4].type);
assertEquals("E", parts[4].value);
assertEquals("exponentInteger", parts[5].type);
assertEquals("19", parts[5].value);
assertEquals(6, parts.length);

parts = nf.formatToParts(0.000000000000123456);
// [{type: "integer", value: "1"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "235"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentMinusSign", value: "-"}, {type: "exponentInteger", value: "13"}]
assertEquals("integer", parts[0].type);
assertEquals("1", parts[0].value);
assertEquals("decimal", parts[1].type);
assertEquals(".", parts[1].value);
assertEquals("fraction", parts[2].type);
assertEquals("235", parts[2].value);
assertEquals("exponentSeparator", parts[3].type);
assertEquals("E", parts[3].value);
assertEquals("exponentMinusSign", parts[4].type);
assertEquals("-", parts[4].value);
assertEquals("exponentInteger", parts[5].type);
assertEquals("13", parts[5].value);
assertEquals(6, parts.length);

parts = nf.formatToParts(-0.000000000000123456);
// [{type: "minusSign", value: "-"}, {type: "integer", value: "1"},
//  {type: "decimal", value: "."}, {type: "fraction", value: "235"},
//  {type: "exponentSeparator", value: "E"},
//  {type: "exponentMinusSign", value: "-"}, {type: "exponentInteger", value: "13"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("1", parts[1].value);
assertEquals("decimal", parts[2].type);
assertEquals(".", parts[2].value);
assertEquals("fraction", parts[3].type);
assertEquals("235", parts[3].value);
assertEquals("exponentSeparator", parts[4].type);
assertEquals("E", parts[4].value);
assertEquals("exponentMinusSign", parts[5].type);
assertEquals("-", parts[5].value);
assertEquals("exponentInteger", parts[6].type);
assertEquals("13", parts[6].value);
assertEquals(7, parts.length);

parts = nf.formatToParts(0);
// [{type: "integer", value: "0"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "0"}]
assertEquals("integer", parts[0].type);
assertEquals("0", parts[0].value);
assertEquals("exponentSeparator", parts[1].type);
assertEquals("E", parts[1].value);
assertEquals("exponentInteger", parts[2].type);
assertEquals("0", parts[2].value);
assertEquals(3, parts.length);

parts = nf.formatToParts(-0);
// [{type: "minusSign", value: "-"}, {type: "integer", value: "0"},
//  {type: "exponentSeparator", value: "E"}, {type: "exponentInteger", value: "0"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("0", parts[1].value);
assertEquals("exponentSeparator", parts[2].type);
assertEquals("E", parts[2].value);
assertEquals("exponentInteger", parts[3].type);
assertEquals("0", parts[3].value);
assertEquals(4, parts.length);

parts = nf.formatToParts(Infinity);
// [{type: "infinity", value: "∞"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "0"}]
assertEquals("infinity", parts[0].type);
assertEquals("∞", parts[0].value);
assertEquals("exponentSeparator", parts[1].type);
assertEquals("E", parts[1].value);
assertEquals("exponentInteger", parts[2].type);
assertEquals("0", parts[2].value);
assertEquals(3, parts.length);

parts = nf.formatToParts(-Infinity);
// [{type: "minusSign", value: "-"}, {type: "infinity", value: "∞"},
//  {type: "exponentSeparator", value: "E"}, {type: "exponentInteger", value: "0"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("infinity", parts[1].type);
assertEquals("∞", parts[1].value);
assertEquals("exponentSeparator", parts[2].type);
assertEquals("E", parts[2].value);
assertEquals("exponentInteger", parts[3].type);
assertEquals("0", parts[3].value);
assertEquals(4, parts.length);

parts = nf.formatToParts(NaN);
// [{type: "nan", value: "NaN"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "0"}]
assertEquals("nan", parts[0].type);
assertEquals("NaN", parts[0].value);
assertEquals("exponentSeparator", parts[1].type);
assertEquals("E", parts[1].value);
assertEquals("exponentInteger", parts[2].type);
assertEquals("0", parts[2].value);
assertEquals(3, parts.length);
