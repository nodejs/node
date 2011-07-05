// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Test Unicode character ranges in regexps.


// Cyrillic.
var cyrillic = {
  FIRST: "\u0410",   // A
  first: "\u0430",   // a
  LAST: "\u042f",    // YA
  last: "\u044f",    // ya
  MIDDLE: "\u0427",  // CHE
  middle: "\u0447",   // che
  // Actually no characters are between the cases in Cyrillic.
  BetweenCases: false};

var SIGMA = "\u03a3";
var sigma = "\u03c3";
var alternative_sigma = "\u03c2";

// Greek.
var greek = {
  FIRST: "\u0391",     // ALPHA
  first: "\u03b1",     // alpha
  LAST: "\u03a9",      // OMEGA
  last: "\u03c9",      // omega
  MIDDLE: SIGMA,       // SIGMA
  middle: sigma,       // sigma
  // Epsilon acute is between ALPHA-OMEGA and alpha-omega, ie it
  // is between OMEGA and alpha.
  BetweenCases: "\u03ad"};


function Range(from, to, flags) {
  return new RegExp("[" + from + "-" + to + "]", flags);
}

// Test Cyrillic and Greek separately.
for (var lang = 0; lang < 2; lang++) {
  var chars = (lang == 0) ? cyrillic : greek;

  for (var i = 0; i < 2; i++) {
    var lc = (i == 0);  // Lower case.
    var first = lc ? chars.first : chars.FIRST;
    var middle = lc ? chars.middle : chars.MIDDLE;
    var last = lc ? chars.last : chars.LAST;
    var first_other_case = lc ? chars.FIRST : chars.first;
    var middle_other_case = lc ? chars.MIDDLE : chars.middle;
    var last_other_case = lc ? chars.LAST : chars.last;

    assertTrue(Range(first, last).test(first), 1);
    assertTrue(Range(first, last).test(middle), 2);
    assertTrue(Range(first, last).test(last), 3);

    assertFalse(Range(first, last).test(first_other_case), 4);
    assertFalse(Range(first, last).test(middle_other_case), 5);
    assertFalse(Range(first, last).test(last_other_case), 6);

    assertTrue(Range(first, last, "i").test(first), 7);
    assertTrue(Range(first, last, "i").test(middle), 8);
    assertTrue(Range(first, last, "i").test(last), 9);

    assertTrue(Range(first, last, "i").test(first_other_case), 10);
    assertTrue(Range(first, last, "i").test(middle_other_case), 11);
    assertTrue(Range(first, last, "i").test(last_other_case), 12);

    if (chars.BetweenCases) {
      assertFalse(Range(first, last).test(chars.BetweenCases), 13);
      assertFalse(Range(first, last, "i").test(chars.BetweenCases), 14);
    }
  }
  if (chars.BetweenCases) {
    assertTrue(Range(chars.FIRST, chars.last).test(chars.BetweenCases), 15);
    assertTrue(Range(chars.FIRST, chars.last, "i").test(chars.BetweenCases), 16);
  }
}

// Test range that covers both greek and cyrillic characters.
for (key in greek) {
  assertTrue(Range(greek.FIRST, cyrillic.last).test(greek[key]), 17 + key);
  if (cyrillic[key]) {
    assertTrue(Range(greek.FIRST, cyrillic.last).test(cyrillic[key]), 18 + key);
  }
}

for (var i = 0; i < 2; i++) {
  var ignore_case = (i == 0);
  var flag = ignore_case ? "i" : "";
  assertTrue(Range(greek.first, cyrillic.LAST, flag).test(greek.first), 19);
  assertTrue(Range(greek.first, cyrillic.LAST, flag).test(greek.middle), 20);
  assertTrue(Range(greek.first, cyrillic.LAST, flag).test(greek.last), 21);

  assertTrue(Range(greek.first, cyrillic.LAST, flag).test(cyrillic.FIRST), 22);
  assertTrue(Range(greek.first, cyrillic.LAST, flag).test(cyrillic.MIDDLE), 23);
  assertTrue(Range(greek.first, cyrillic.LAST, flag).test(cyrillic.LAST), 24);

  // A range that covers the lower case greek letters and the upper case cyrillic
  // letters.
  assertEquals(ignore_case, Range(greek.first, cyrillic.LAST, flag).test(greek.FIRST), 25);
  assertEquals(ignore_case, Range(greek.first, cyrillic.LAST, flag).test(greek.MIDDLE), 26);
  assertEquals(ignore_case, Range(greek.first, cyrillic.LAST, flag).test(greek.LAST), 27);

  assertEquals(ignore_case, Range(greek.first, cyrillic.LAST, flag).test(cyrillic.first), 28);
  assertEquals(ignore_case, Range(greek.first, cyrillic.LAST, flag).test(cyrillic.middle), 29);
  assertEquals(ignore_case, Range(greek.first, cyrillic.LAST, flag).test(cyrillic.last), 30);
}


// Sigma is special because there are two lower case versions of the same upper
// case character.  JS requires that case independece means that you should
// convert everything to upper case, so the two sigma variants are equal to each
// other in a case independt comparison.
for (var i = 0; i < 2; i++) {
  var simple = (i != 0);
  var name = simple ? "" : "[]";
  var regex = simple ? SIGMA : "[" + SIGMA + "]";

  assertFalse(new RegExp(regex).test(sigma), 31 + name);
  assertFalse(new RegExp(regex).test(alternative_sigma), 32 + name);
  assertTrue(new RegExp(regex).test(SIGMA), 33 + name);

  assertTrue(new RegExp(regex, "i").test(sigma), 34 + name);
  // JSC and Tracemonkey fail this one.
  assertTrue(new RegExp(regex, "i").test(alternative_sigma), 35 + name);
  assertTrue(new RegExp(regex, "i").test(SIGMA), 36 + name);

  regex = simple ? sigma : "[" + sigma + "]";

  assertTrue(new RegExp(regex).test(sigma), 41 + name);
  assertFalse(new RegExp(regex).test(alternative_sigma), 42 + name);
  assertFalse(new RegExp(regex).test(SIGMA), 43 + name);

  assertTrue(new RegExp(regex, "i").test(sigma), 44 + name);
  // JSC and Tracemonkey fail this one.
  assertTrue(new RegExp(regex, "i").test(alternative_sigma), 45 + name);
  assertTrue(new RegExp(regex, "i").test(SIGMA), 46 + name);

  regex = simple ? alternative_sigma : "[" + alternative_sigma + "]";

  assertFalse(new RegExp(regex).test(sigma), 51 + name);
  assertTrue(new RegExp(regex).test(alternative_sigma), 52 + name);
  assertFalse(new RegExp(regex).test(SIGMA), 53 + name);

  // JSC and Tracemonkey fail this one.
  assertTrue(new RegExp(regex, "i").test(sigma), 54 + name);
  assertTrue(new RegExp(regex, "i").test(alternative_sigma), 55 + name);
  // JSC and Tracemonkey fail this one.
  assertTrue(new RegExp(regex, "i").test(SIGMA), 56 + name);
}


for (var add_non_ascii_character_to_subject = 0;
     add_non_ascii_character_to_subject < 2;
     add_non_ascii_character_to_subject++) {
  var suffix = add_non_ascii_character_to_subject ? "\ufffe" : "";
  // A range that covers both ASCII and non-ASCII.
  for (var i = 0; i < 2; i++) {
    var full = (i != 0);
    var mixed = full ? "[a-\uffff]" : "[a-" + cyrillic.LAST + "]";
    var f = full ? "f" : "c";
    for (var j = 0; j < 2; j++) {
      var ignore_case = (j == 0);
      var flag = ignore_case ? "i" : "";
      var re = new RegExp(mixed, flag);
      assertEquals(ignore_case || (full && add_non_ascii_character_to_subject),
                   re.test("A" + suffix),
                   58 + flag + f);
      assertTrue(re.test("a" + suffix), 59 + flag + f);
      assertTrue(re.test("~" + suffix), 60 + flag + f);
      assertTrue(re.test(cyrillic.MIDDLE), 61 + flag + f);
      assertEquals(ignore_case || full, re.test(cyrillic.middle), 62 + flag + f);
    }
  }
}
