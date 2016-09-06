// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property

function t(re, s) { assertTrue(re.test(s)); }
function f(re, s) { assertFalse(re.test(s)); }

t(/\p{Bidi_Control}+/u, "\u200E");
f(/\p{Bidi_C}+/u, "On a dark desert highway, cool wind in my hair");
t(/\p{AHex}+/u, "DEADBEEF");
t(/\p{Alphabetic}+/u, "abcdefg");
t(/\P{Alphabetic}+/u, "1234");
t(/\p{White_Space}+/u, "\u00A0");
t(/\p{Uppercase}+/u, "V");
f(/\p{Lower}+/u, "U");
t(/\p{Ideo}+/u, "字");
f(/\p{Ideo}+/u, "x");

assertThrows("/\\p{Hiragana}/u");
assertThrows("/\\p{Bidi_Class}/u");
assertThrows("/\\p{Bidi_C=False}/u");
assertThrows("/\\P{Bidi_Control=Y}/u");
assertThrows("/\\p{AHex=Yes}/u");
