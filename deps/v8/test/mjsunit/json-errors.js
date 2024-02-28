// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TryParse(s, message) {
  try {
    JSON.parse(s);
    assertUnreachable();
  } catch(e) {
    assertEquals(message, e.message);
  }
}

var s = `{"a\\\\b `;
TryParse(s, "Unterminated string in JSON at position 7 (line 1 column 8)");

var s = `{"a\\\\\u03A9 `;
TryParse(s, "Unterminated string in JSON at position 7 (line 1 column 8)");

var s = `{"ab `;
TryParse(s, "Unterminated string in JSON at position 5 (line 1 column 6)");

var s = `{"a\u03A9 `;
TryParse(s, "Unterminated string in JSON at position 5 (line 1 column 6)");

var s = `{"a\nb":"b"}`;
TryParse(s, "Bad control character in string literal in JSON " +
            "at position 3 (line 1 column 4)");

var s = `{"a\nb":"b\u03A9"}`;
TryParse(s, "Bad control character in string literal in JSON " +
            "at position 3 (line 1 column 4)");

// === Test line counting ===
// \r and \n are line terminators.
var s = `{\n7:1}`;
TryParse(s, "Expected property name or '}' in JSON " +
            "at position 2 (line 2 column 1)");

var s = `{\r7:1}`;
TryParse(s, "Expected property name or '}' in JSON " +
            "at position 2 (line 2 column 1)");

// The sequence \r\n counts as a single line terminator.
var s = `{\r\n7:1}`;
TryParse(s, "Expected property name or '}' in JSON " +
            "at position 3 (line 2 column 1)");

// The sequences \r\r, \n\n and \n\r count as 2 line terminators.
var s = `{\r\r7:1}`;
TryParse(s, "Expected property name or '}' in JSON " +
            "at position 3 (line 3 column 1)");
var s = `{\n\n7:1}`;
TryParse(s, "Expected property name or '}' in JSON " +
            "at position 3 (line 3 column 1)");
var s = `{\n\r7:1}`;
TryParse(s, "Expected property name or '}' in JSON " +
            "at position 3 (line 3 column 1)");

// \u2028 and \u2029 (line terminators according to ECMA-262 11.3) aren't
// allowed in JSON (and therefore don't need to be counted).
var s = `\u2028{}`;
TryParse(s, "Unexpected token '\u2028', \"\u2028{}\" is not valid JSON");
var s = `\u2029{}`;
TryParse(s, "Unexpected token '\u2029', \"\u2029{}\" is not valid JSON");

var s = `\n  { \n\n"12345": 5,,}`;
TryParse(s, "Expected double-quoted property name in JSON " +
            "at position 18 (line 4 column 12)");
