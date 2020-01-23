// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test notation: "engineering" with formatToParts.

const nf = Intl.NumberFormat("en", {notation: "engineering"});

let parts = nf.formatToParts(123.456);
// [{type: "integer", value: "123"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "456"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "0"}]
assertEquals("integer", parts[0].type);
assertEquals("123", parts[0].value);
assertEquals("decimal", parts[1].type);
assertEquals(".", parts[1].value);
assertEquals("fraction", parts[2].type);
assertEquals("456", parts[2].value);
assertEquals("exponentSeparator", parts[3].type);
assertEquals("E", parts[3].value);
assertEquals("exponentInteger", parts[4].type);
assertEquals("0", parts[4].value);
assertEquals(5, parts.length);

parts = nf.formatToParts(-123.456);
// [{type: "minusSign", value: "-"}, {type: "integer", value: "123"},
//  {type: "decimal", value: "."}, {type: "fraction", value: "456"},
//  {type: "exponentSeparator", value: "E"}, {type: "exponentInteger", value: "0"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("123", parts[1].value);
assertEquals("decimal", parts[2].type);
assertEquals(".", parts[2].value);
assertEquals("fraction", parts[3].type);
assertEquals("456", parts[3].value);
assertEquals("exponentSeparator", parts[4].type);
assertEquals("E", parts[4].value);
assertEquals("exponentInteger", parts[5].type);
assertEquals("0", parts[5].value);
assertEquals(6, parts.length);

parts = nf.formatToParts(12345678901234567890);
// [{type: "integer", value: "12"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "346"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentInteger", value: "18"}]
assertEquals("integer", parts[0].type);
assertEquals("12", parts[0].value);
assertEquals("decimal", parts[1].type);
assertEquals(".", parts[1].value);
assertEquals("fraction", parts[2].type);
assertEquals("346", parts[2].value);
assertEquals("exponentSeparator", parts[3].type);
assertEquals("E", parts[3].value);
assertEquals("exponentInteger", parts[4].type);
assertEquals("18", parts[4].value);
assertEquals(5, parts.length);

parts = nf.formatToParts(-12345678901234567890);
// [{type: "minusSign", value: "-"}, {type: "integer", value: "12"},
//  {type: "decimal", value: "."}, {type: "fraction", value: "346"},
//  {type: "exponentSeparator", value: "E"}, {type: "exponentInteger", value: "18"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("12", parts[1].value);
assertEquals("decimal", parts[2].type);
assertEquals(".", parts[2].value);
assertEquals("fraction", parts[3].type);
assertEquals("346", parts[3].value);
assertEquals("exponentSeparator", parts[4].type);
assertEquals("E", parts[4].value);
assertEquals("exponentInteger", parts[5].type);
assertEquals("18", parts[5].value);
assertEquals(6, parts.length);

parts = nf.formatToParts(0.000000000000123456);
// [{type: "integer", value: "123"}, {type: "decimal", value: "."},
//  {type: "fraction", value: "456"}, {type: "exponentSeparator", value: "E"},
//  {type: "exponentMinusSign", value: "-"}, {type: "exponentInteger", value: "15"}]
assertEquals("integer", parts[0].type);
assertEquals("123", parts[0].value);
assertEquals("decimal", parts[1].type);
assertEquals(".", parts[1].value);
assertEquals("fraction", parts[2].type);
assertEquals("456", parts[2].value);
assertEquals("exponentSeparator", parts[3].type);
assertEquals("E", parts[3].value);
assertEquals("exponentMinusSign", parts[4].type);
assertEquals("-", parts[4].value);
assertEquals("exponentInteger", parts[5].type);
assertEquals("15", parts[5].value);
assertEquals(6, parts.length);

parts = nf.formatToParts(-0.000000000000123456);
// [{type: "minusSign", value: "-"}, {type: "integer", value: "123"},
//  {type: "decimal", value: "."}, {type: "fraction", value: "456"},
//  {type: "exponentSeparator", value: "E"},
//  {type: "exponentMinusSign", value: "-"}, {type: "exponentInteger", value: "15"}]
assertEquals("minusSign", parts[0].type);
assertEquals("-", parts[0].value);
assertEquals("integer", parts[1].type);
assertEquals("123", parts[1].value);
assertEquals("decimal", parts[2].type);
assertEquals(".", parts[2].value);
assertEquals("fraction", parts[3].type);
assertEquals("456", parts[3].value);
assertEquals("exponentSeparator", parts[4].type);
assertEquals("E", parts[4].value);
assertEquals("exponentMinusSign", parts[5].type);
assertEquals("-", parts[5].value);
assertEquals("exponentInteger", parts[6].type);
assertEquals("15", parts[6].value);
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
