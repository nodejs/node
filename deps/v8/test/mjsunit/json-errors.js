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
TryParse(s, "Unexpected end of JSON input");

var s = `{"a\\\\\u03A9 `;
TryParse(s, "Unexpected end of JSON input");

var s = `{"ab `;
TryParse(s, "Unexpected end of JSON input");

var s = `{"a\u03A9 `;
TryParse(s, "Unexpected end of JSON input");

var s = `{"a\nb":"b"}`;
TryParse(s, "Unexpected token \n in JSON at position 3");

var s = `{"a\nb":"b\u03A9"}`;
TryParse(s, "Unexpected token \n in JSON at position 3");
