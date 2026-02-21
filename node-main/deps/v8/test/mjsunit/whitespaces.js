// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var whitespaces = [
  // WhiteSpace defined in ECMA-262 5.1, 7.2
  0x0009,  // Tab                  TAB
  0x000B,  // Vertical Tab         VT
  0x000C,  // Form Feed            FF
  0x0020,  // Space                SP
  0x00A0,  // No-break space       NBSP
  0xFEFF,  // Byte Order Mark      BOM

  // LineTerminator defined in ECMA-262 5.1, 7.3
  0x000A,  // Line Feed            LF
  0x000D,  // Carriage Return      CR
  0x2028,  // Line Separator       LS
  0x2029,  // Paragraph Separator  PS

  // Unicode 6.3.0 whitespaces (category 'Zs')
  0x1680,  // Ogham Space Mark
  0x2000,  // EN QUAD
  0x2001,  // EM QUAD
  0x2002,  // EN SPACE
  0x2003,  // EM SPACE
  0x2004,  // THREE-PER-EM SPACE
  0x2005,  // FOUR-PER-EM SPACE
  0x2006,  // SIX-PER-EM SPACE
  0x2007,  // FIGURE SPACE
  0x2008,  // PUNCTUATION SPACE
  0x2009,  // THIN SPACE
  0x200A,  // HAIR SPACE
  0x2028,  // LINE SEPARATOR
  0x2029,  // PARAGRAPH SEPARATOR
  0x202F,  // NARROW NO-BREAK SPACE
  0x205F,  // MEDIUM MATHEMATICAL SPACE
  0x3000,  // IDEOGRAPHIC SPACE
];

// Add single twobyte char to force twobyte representation.
// Interestingly, snowman is not "white" space :)
var twobyte = "\u2603";
var onebyte = "\u007E";
var twobytespace = "\u2000";
var onebytespace = "\u0020";

function is_whitespace(c) {
  return whitespaces.indexOf(c.charCodeAt(0)) > -1;
}

function test_regexp(str) {
  var pos_match = str.match(/\s/);
  var neg_match = str.match(/\S/);
  var test_char = str[0];
  var postfix = str[1];
  if (is_whitespace(test_char)) {
    assertEquals(test_char, pos_match[0]);
    assertEquals(postfix, neg_match[0]);
  } else {
    assertEquals(test_char, neg_match[0]);
    assertNull(pos_match);
  }
}

function test_trim(c, infix) {
  var str = c + c + c + infix + c;
  if (is_whitespace(c)) {
    assertEquals(infix, str.trim());
  } else {
    assertEquals(str, str.trim());
  }
}

function test_parseInt(c, postfix) {
  // Skip if prefix is a digit.
  if (c >= "0" && c <= "9") return;
  var str = c + c + "123" + postfix;
  if (is_whitespace(c)) {
    assertEquals(123, parseInt(str));
  } else {
    assertEquals(NaN, parseInt(str));
  }
}

function test_eval(c, content) {
  if (!is_whitespace(c)) return;
  var str = c + c + "'" + content + "'" + c + c;
  assertEquals(content, eval(str));
}

function test_stringtonumber(c, postfix) {
  // Skip if prefix is a digit.
  if (c >= "0" && c <= "9") return;
  var result = 1 + Number(c + "123" + c + postfix);
  if (is_whitespace(c)) {
    assertEquals(124, result);
  } else {
    assertEquals(NaN, result);
  }
}

// Test is split into parts to increase parallelism.
const number_of_tests = 10;
const max_codepoint = 0x10000;

function firstCodePointOfRange(i) {
  return Math.floor(i * (max_codepoint / number_of_tests));
}

function testCodePointRange(i) {
  assertTrue(i >= 0 && i < number_of_tests);

  const from = firstCodePointOfRange(i);
  const to = (i == number_of_tests - 1)
      ? max_codepoint : firstCodePointOfRange(i + 1);

  for (let i = from; i < to; i++) {
    c = String.fromCharCode(i);
    test_regexp(c + onebyte);
    test_regexp(c + twobyte);
    test_trim(c, onebyte + "trim");
    test_trim(c, twobyte + "trim");
    test_parseInt(c, onebyte);
    test_parseInt(c, twobyte);
    test_eval(c, onebyte);
    test_eval(c, twobyte);
    test_stringtonumber(c, onebytespace);
    test_stringtonumber(c, twobytespace);
  }
}
