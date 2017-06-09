// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests that RegExp dotall features are not enabled when
// --harmony-regexp-dotall is not passed.

// Flags: --no-harmony-regexp-dotall

// Construction does not throw.
{
  assertThrows("/./s", SyntaxError);
  assertThrows(() => RegExp(".", "s"), SyntaxError);
  assertThrows(() => new RegExp(".", "s"), SyntaxError);
  assertThrows(() => new RegExp(".", "wtf"), SyntaxError);
}

// The flags accessors.
{
  let re = /./gimyu;
  assertEquals("gimuy", re.flags);
  assertTrue(re.global);
  assertTrue(re.ignoreCase);
  assertTrue(re.multiline);
  assertTrue(re.sticky);
  assertTrue(re.unicode);

  assertEquals(re.dotAll, undefined);
  assertFalse("dotAll" in re);

  let callCount = 0;
  re.__defineGetter__("dotAll", () => { callCount++; return undefined; });
  assertEquals("gimuy", re.flags);
  assertEquals(callCount, 0);
}

// Default '.' behavior.
{
  let re = /^.$/;
  assertTrue(re.test("a"));
  assertTrue(re.test("3"));
  assertTrue(re.test("π"));
  assertTrue(re.test("\u2027"));
  assertTrue(re.test("\u0085"));
  assertTrue(re.test("\v"));
  assertTrue(re.test("\f"));
  assertTrue(re.test("\u180E"));
  assertFalse(re.test("\u{10300}"));  // Supplementary plane.
  assertFalse(re.test("\n"));
  assertFalse(re.test("\r"));
  assertFalse(re.test("\u2028"));
  assertFalse(re.test("\u2029"));
}

// Default '.' behavior (unicode).
{
  let re = /^.$/u;
  assertTrue(re.test("a"));
  assertTrue(re.test("3"));
  assertTrue(re.test("π"));
  assertTrue(re.test("\u2027"));
  assertTrue(re.test("\u0085"));
  assertTrue(re.test("\v"));
  assertTrue(re.test("\f"));
  assertTrue(re.test("\u180E"));
  assertTrue(re.test("\u{10300}"));  // Supplementary plane.
  assertFalse(re.test("\n"));
  assertFalse(re.test("\r"));
  assertFalse(re.test("\u2028"));
  assertFalse(re.test("\u2029"));
}
