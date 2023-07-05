// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestIdentityEscapes() {
  // \u not followed by 4 hex digits is treated as an identity escape.
  var r0 = /\u/;
  assertTrue(r0.test("u"));

  r0 = RegExp("\\u");
  assertTrue(r0.test("u"));

  var r1 = /\usecond/;
  assertTrue(r1.test("usecond"));

  r1 = RegExp("\\usecond");
  assertTrue(r1.test("usecond"));

  var r2 = /first\u/;
  assertTrue(r2.test("firstu"));
  // This used to return true (which was a bug).
  assertFalse(r2.test("first\\u"));

  r2 = RegExp("first\\u");
  assertTrue(r2.test("firstu"));
  // This used to return true (which was a bug).
  assertFalse(r2.test("first\\u"));

  var r3 = /first\usecond/;
  assertTrue(r3.test("firstusecond"));
  assertFalse(r3.test("first\\usecond"));

  r3 = RegExp("first\\usecond");
  assertTrue(r3.test("firstusecond"));
  assertFalse(r3.test("first\\usecond"));

  var r4 = /first\u123second/;
  assertTrue(r4.test("firstu123second"));
  assertFalse(r4.test("first\\u123second"));

  r4 = RegExp("first\\u123second");
  assertTrue(r4.test("firstu123second"));
  assertFalse(r4.test("first\\u123second"));

  // \X where X is not a legal escape character is treated as identity escape
  // too.
  var r5 = /\a/;
  assertTrue(r5.test("a"));

  r5 = RegExp("\\a");
  assertTrue(r5.test("a"));

  var r6 = /\asecond/;
  assertTrue(r6.test("asecond"));

  r6 = RegExp("\\asecond");
  assertTrue(r6.test("asecond"));

  var r7 = /first\a/;
  assertTrue(r7.test("firsta"));
  assertFalse(r7.test("first\\a"));

  r7 = RegExp("first\\a");
  assertTrue(r7.test("firsta"));
  assertFalse(r7.test("first\\a"));

  var r8 = /first\asecond/;
  assertTrue(r8.test("firstasecond"));
  assertFalse(r8.test("first\\asecond"));

  r8 = RegExp("first\\asecond");
  assertTrue(r8.test("firstasecond"));
  assertFalse(r8.test("first\\asecond"));
})();
