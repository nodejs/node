// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-regexp-property

function test(source, message) {
  try {
    eval(source);
  } catch (e) {
    assertEquals(message, e.message);
    return;
  }
  assertUnreachable();
}

test("/\\pL/u", "Invalid regular expression: /\\pL/: Invalid escape");
test("/[\\p{L}]/u", "Invalid regular expression: /[\\p{L}]/: Invalid escape");
