// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --regexp-mode-modifiers

aa(/(a)(?i)\1/);
aa(/([az])(?i)\1/);

function aa(re) {
  assertTrue(re.test("aa"));
  assertTrue(re.test("aA"));
  assertFalse(re.test("Aa"));
  assertFalse(re.test("AA"));
}

aai(/(a)(?-i)\1/i);
aai(/([az])(?-i)\1/i);

function aai(re) {
  assertTrue(re.test("aa"));
  assertFalse(re.test("aA"));
  assertFalse(re.test("Aa"));
  assertTrue(re.test("AA"));
}

abcd(/a(b(?i)c)d/);
abcd(/[aw]([bx](?i)[cy])[dz]/);

function abcd(re) {
  assertTrue(re.test("abcd"));
  assertFalse(re.test("abcD"));
  assertTrue(re.test("abCd"));
  assertFalse(re.test("abCD"));
  assertFalse(re.test("aBcd"));
  assertFalse(re.test("aBcD"));
  assertFalse(re.test("aBCd"));
  assertFalse(re.test("aBCD"));
  assertFalse(re.test("Abcd"));
  assertFalse(re.test("AbcD"));
  assertFalse(re.test("AbCd"));
  assertFalse(re.test("AbCD"));
  assertFalse(re.test("ABcd"));
  assertFalse(re.test("ABcD"));
  assertFalse(re.test("ABCd"));
  assertFalse(re.test("ABCD"));
}

abcdei(/a(b(?-i)c)d/i);
abcdei(/[aw]([bx](?-i)[cy])[dz]/i);

function abcdei(re) {
  assertTrue(re.test("abcd"));
  assertTrue(re.test("abcD"));
  assertFalse(re.test("abCd"));
  assertFalse(re.test("abCD"));
  assertTrue(re.test("aBcd"));
  assertTrue(re.test("aBcD"));
  assertFalse(re.test("aBCd"));
  assertFalse(re.test("aBCD"));
  assertTrue(re.test("Abcd"));
  assertTrue(re.test("AbcD"));
  assertFalse(re.test("AbCd"));
  assertFalse(re.test("AbCD"));
  assertTrue(re.test("ABcd"));
  assertTrue(re.test("ABcD"));
  assertFalse(re.test("ABCd"));
  assertFalse(re.test("ABCD"));
}

abc(/a(?i:b)c/);
abc(/[ax](?i:[by])[cz]/);

function abc(re) {
  assertTrue(re.test("abc"));
  assertFalse(re.test("abC"));
  assertTrue(re.test("aBc"));
  assertFalse(re.test("aBC"));
  assertFalse(re.test("Abc"));
  assertFalse(re.test("AbC"));
  assertFalse(re.test("ABc"));
  assertFalse(re.test("ABC"));
}

abci(/a(?-i:b)c/i);
abci(/[ax](?-i:[by])[cz]/i);

function abci(re) {
  assertTrue(re.test("abc"));
  assertTrue(re.test("abC"));
  assertFalse(re.test("aBc"));
  assertFalse(re.test("aBC"));
  assertTrue(re.test("Abc"));
  assertTrue(re.test("AbC"));
  assertFalse(re.test("ABc"));
  assertFalse(re.test("ABC"));
}

assertThrows(() => new RegExp("foo(?i:"));
assertThrows(() => new RegExp("foo(?--i)"));
assertThrows(() => new RegExp("foo(?i-i)"));

assertThrows(() => new RegExp("foo(?m:"));
assertThrows(() => new RegExp("foo(?--m)"));
assertThrows(() => new RegExp("foo(?m-m)"));

var re = /^\s(?m)^.$\s(?-m)$/;
assertTrue(re.test("\n.\n"));
assertFalse(re.test(" .\n"));
assertFalse(re.test("\n. "));
assertFalse(re.test(" . "));
assertFalse(re.test("_\n.\n"));
assertFalse(re.test("\n.\n_"));
assertFalse(re.test("_\n.\n_"));

assertEquals(["abcd", "d"], /a.*?(.)(?i)\b/.exec('abcd\u017F cd'));
assertEquals(["abcd", "d"], /a.*?(.)(?i)\b/.exec('abcd\u212A cd'));

assertEquals(["a\u017F ", " "], /a.*?(?i)\B(.)/.exec('a\u017F '));
assertEquals(["a\u212A ", " "], /a.*?(?i)\B(.)/.exec('a\u212A '));

// Nested flags.
var res = [
  /^a(?i:b(?-i:c(?i:d)e)f)g$/,
  /^a(?i:b(?-i)c(?i)d(?-i)e(?i)f)g$/,
  /^(?-i:a(?i:b(?-i:c(?i:d)e)f)g)$/i,
  /^(?-i:a(?i:b(?-i)c(?i)d(?-i)e(?i)f)g)$/i,
];

for (var idx = 0; idx < res.length; idx++) {
  var re = res[idx];
  for (var i = 0; i < 128; i++) {
    var s = (i & 1) ? "A" : "a";
    s += (i & 2) ? "B" : "b";
    s += (i & 4) ? "C" : "c";
    s += (i & 8) ? "D" : "d";
    s += (i & 16) ? "E" : "e";
    s += (i & 32) ? "F" : "f";
    s += (i & 64) ? "G" : "g";
    if ((i & (1 | 4 | 16 | 64)) != 0) {
      assertFalse(re.test(s), s);
    } else {
      assertTrue(re.test(s), s);
    }
  }
}
