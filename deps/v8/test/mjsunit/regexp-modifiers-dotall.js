// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --regexp-mode-modifiers

// S flag switches dotall mode on and off.  Combine with i flag changes to test
// the parser.
test(/.(?s).(?i-s).a(?-i)a/);
test(/.(?s:.)(?i:.a)a/);
test(/.(?s).(?i-s).a(?-i)a/u);
test(/.(?s:.)(?i:.a)a/u);

// m flag makes no difference
test(/.(?sm).(?i-s).a(?-i)a/);
test(/.(?s:.)(?i:.a)a/);
test(/.(?sm).(?im-s).a(?m-i)a/u);
test(/.(?s:.)(?i:.a)a/u);

function test(re) {
  assertTrue(re.test("...aa"));
  assertTrue(re.test(".\n.aa"));
  assertTrue(re.test(".\n.Aa"));
  assertFalse(re.test("\n\n.Aa"));
  assertFalse(re.test(".\n\nAa"));
  assertFalse(re.test(".\n.AA"));
}
