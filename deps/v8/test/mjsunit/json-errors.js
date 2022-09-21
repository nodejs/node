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
TryParse(s, "Unterminated string in JSON at position 7");

var s = `{"a\\\\\u03A9 `;
TryParse(s, "Unterminated string in JSON at position 7");

var s = `{"ab `;
TryParse(s, "Unterminated string in JSON at position 5");

var s = `{"a\u03A9 `;
TryParse(s, "Unterminated string in JSON at position 5");

var s = `{"a\nb":"b"}`;
TryParse(s, "Bad control character in string literal in JSON at position 3");

var s = `{"a\nb":"b\u03A9"}`;
TryParse(s, "Bad control character in string literal in JSON at position 3");
