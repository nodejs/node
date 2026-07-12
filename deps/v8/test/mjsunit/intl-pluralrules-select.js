// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Intl) {
  var pr;
  var suffixes;
  function format(n) {
    return "" + n + suffixes[pr.select(n)];
  }

  // These English examples illustrate the purpose of the PluralRules class.
  pr = new Intl.PluralRules("en-US");
  suffixes = {
    one:   " day",
    other: " days",
  };
  assertEquals("0 days",   format(0));
  assertEquals("0.5 days", format(0.5));
  assertEquals("1 day",    format(1));
  assertEquals("1.5 days", format(1.5));
  assertEquals("2 days",   format(2));

  pr = new Intl.PluralRules("en-US", {type: "ordinal"});
  suffixes = {
    one:   "st",
    two:   "nd",
    few:   "rd",
    other: "th",
  };
  assertEquals("0th",   format(0));
  assertEquals("1st",   format(1));
  assertEquals("2nd",   format(2));
  assertEquals("3rd",   format(3));
  assertEquals("4th",   format(4));
  assertEquals("11th",  format(11));
  assertEquals("21st",  format(21));
  assertEquals("103rd", format(103));

  // Arabic can cause every possible return value from select()
  pr = new Intl.PluralRules("ar");
  suffixes = null;
  assertEquals("zero",  pr.select(0));
  assertEquals("one",   pr.select(1));
  assertEquals("two",   pr.select(2));
  assertEquals("few",   pr.select(3));
  assertEquals("many",  pr.select(11));
  assertEquals("other", pr.select(100));
  assertEquals("other", pr.select(1.5));
}
