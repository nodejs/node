// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-dotall

function toSlowMode(re) {
  re.exec = (str) => RegExp.prototype.exec.call(re, str);
  return re;
}

// Construction does not throw.
{
  let re = /./s;
  re = RegExp(".", "s");
  re = new RegExp(".", "s");
  assertThrows(() => new RegExp(".", "wtf"), SyntaxError);
}

// The flags accessors.
{
  let re = /./s;
  assertEquals("s", re.flags);
  assertFalse(re.global);
  assertFalse(re.ignoreCase);
  assertFalse(re.multiline);
  assertFalse(re.sticky);
  assertFalse(re.unicode);
  assertTrue(re.dotAll);

  re = toSlowMode(/./s);
  assertEquals("s", re.flags);
  assertFalse(re.global);
  assertFalse(re.ignoreCase);
  assertFalse(re.multiline);
  assertFalse(re.sticky);
  assertFalse(re.unicode);
  assertTrue(re.dotAll);

  re = /./gimyus;
  assertEquals("gimsuy", re.flags);
  assertTrue(re.global);
  assertTrue(re.ignoreCase);
  assertTrue(re.multiline);
  assertTrue(re.sticky);
  assertTrue(re.unicode);
  assertTrue(re.dotAll);

  re = /./gimyu;
  assertEquals("gimuy", re.flags);
  assertTrue(re.global);
  assertTrue(re.ignoreCase);
  assertTrue(re.multiline);
  assertTrue(re.sticky);
  assertTrue(re.unicode);
  assertFalse(re.dotAll);
}

// Different construction variants with all flags.
{
  assertEquals("gimsuy", new RegExp("", "yusmig").flags);
  assertEquals("gimsuy", new RegExp().compile("", "yusmig").flags);
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

// DotAll '.' behavior.
{
  let re = /^.$/s;
  assertTrue(re.test("a"));
  assertTrue(re.test("3"));
  assertTrue(re.test("π"));
  assertTrue(re.test("\u2027"));
  assertTrue(re.test("\u0085"));
  assertTrue(re.test("\v"));
  assertTrue(re.test("\f"));
  assertTrue(re.test("\u180E"));
  assertFalse(re.test("\u{10300}"));  // Supplementary plane.
  assertTrue(re.test("\n"));
  assertTrue(re.test("\r"));
  assertTrue(re.test("\u2028"));
  assertTrue(re.test("\u2029"));
}

// DotAll '.' behavior (unicode).
{
  let re = /^.$/su;
  assertTrue(re.test("a"));
  assertTrue(re.test("3"));
  assertTrue(re.test("π"));
  assertTrue(re.test("\u2027"));
  assertTrue(re.test("\u0085"));
  assertTrue(re.test("\v"));
  assertTrue(re.test("\f"));
  assertTrue(re.test("\u180E"));
  assertTrue(re.test("\u{10300}"));  // Supplementary plane.
  assertTrue(re.test("\n"));
  assertTrue(re.test("\r"));
  assertTrue(re.test("\u2028"));
  assertTrue(re.test("\u2029"));
}
