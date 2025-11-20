// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ES6 extends the \uxxxx escape and also allows \u{xxxxx}.

// Unicode escapes in variable names.

(function TestVariableNames1() {
  var foobar = 1;
  assertEquals(foob\u0061r, 1);
  assertEquals(foob\u{0061}r, 1);
  assertEquals(foob\u{61}r, 1);
  assertEquals(foob\u{0000000061}r, 1);
})();

(function TestVariableNames2() {
  var foobar = 1;
  assertEquals(\u0066oobar, 1);
  assertEquals(\u{0066}oobar, 1);
  assertEquals(\u{66}oobar, 1);
  assertEquals(\u{0000000066}oobar, 1);
})();

// Unicode escapes in strings.

(function TestStrings() {
  var s1 = "foob\u0061r";
  assertEquals(s1, "foobar");
  var s2 = "foob\u{0061}r";
  assertEquals(s2, "foobar");
  var s3 = "foob\u{61}r";
  assertEquals(s3, "foobar");
  var s4 = "foob\u{0000000061}r";
  assertEquals(s4, "foobar");
})();


(function TestSurrogates() {
  // U+10E6D corresponds to the surrogate pair [U+D803, U+DE6D].
  var s1 = "foo\u{10e6d}";
  var s2 = "foo\u{d803}\u{de6d}";
  assertEquals(s1, s2);
})();
