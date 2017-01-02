// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu_case_mapping

// Some edge cases that unibrow got wrong

assertEquals("𐐘", "𐑀".toUpperCase());
assertEquals("𐑀", "𐐘".toLowerCase());
assertEquals("σ", "Σ".toLowerCase());

// Some different paths in the ICU case conversion fastpath

assertEquals("σς", "\u03A3\u03A3".toLowerCase());
// Expand sharp s in latin1 fastpath
assertEquals("ASSB", "A\u00DFB".toUpperCase());
assertEquals("AB", "Ab".toUpperCase());
// Find first upper case in fastpath
assertEquals("ab", "aB".toLowerCase());
assertEquals("AÜ", "aü".toUpperCase());
assertEquals("AÜ", "AÜ".toUpperCase());
assertEquals("aü", "aü".toLowerCase());
assertEquals("aü", "AÜ".toLowerCase());
assertEquals("aü", "AÜ".toLowerCase());

// Starts with fastpath, but switches to full Unicode path
// U+00FF is uppercased to U+0178.
assertEquals("AŸ", "aÿ".toUpperCase());
// U+00B5 (µ) is uppercased to U+039C (Μ)
assertEquals("AΜ", "aµ".toUpperCase());

// Buffer size increase
assertEquals("CSSBẶ", "cßbặ".toUpperCase());
assertEquals("FIFLFFIFFL", "\uFB01\uFB02\uFB03\uFB04".toUpperCase());
// OneByte input with buffer size increase: non-fast path
assertEquals("ABCSS", "abCß".toLocaleUpperCase("tr"));

// More comprehensive tests for "tr", "az" and "lt" are in
// test262/intl402/Strings/*

// Buffer size decrease with a single locale or locale list.
// In Turkic (tr, az), U+0307 preceeded by Capital Letter I is dropped.
assertEquals("abci", "aBcI\u0307".toLocaleLowerCase("tr"));
assertEquals("abci", "aBcI\u0307".toLocaleLowerCase("az"));
assertEquals("abci", "aBcI\u0307".toLocaleLowerCase(["tr", "en"]));

// Cons string
assertEquals("abcijkl", ("aBcI" + "\u0307jkl").toLocaleLowerCase("tr"));
assertEquals("abcijkl",
             ("aB" + "cI" + "\u0307j" + "kl").toLocaleLowerCase("tr"));
assertEquals("abci\u0307jkl", ("aBcI" + "\u0307jkl").toLocaleLowerCase("en"));
assertEquals("abci\u0307jkl",
             ("aB" + "cI" + "\u0307j" + "kl").toLocaleLowerCase("en"));
assertEquals("abci\u0307jkl", ("aBcI" + "\u0307jkl").toLowerCase());
assertEquals("abci\u0307jkl",
             ("aB" + "cI" + "\u0307j" + "kl").toLowerCase());

// "tr" and "az" should behave identically.
assertEquals("aBcI\u0307".toLocaleLowerCase("tr"),
             "aBcI\u0307".toLocaleLowerCase("az"));
// What matters is the first locale in the locale list.
assertEquals("aBcI\u0307".toLocaleLowerCase(["tr", "en", "fr"]),
             "aBcI\u0307".toLocaleLowerCase("tr"));
assertEquals("aBcI\u0307".toLocaleLowerCase(["en", "tr", "az"]),
             "aBcI\u0307".toLocaleLowerCase("en"));
assertEquals("aBcI\u0307".toLocaleLowerCase(["en", "tr", "az"]),
             "aBcI\u0307".toLowerCase());

// An empty locale list is the same as the default locale. Try these tests
// under Turkish and Greek locale.
assertEquals("aBcI\u0307".toLocaleLowerCase([]),
             "aBcI\u0307".toLocaleLowerCase());
assertEquals("aBcI\u0307".toLocaleLowerCase([]),
             "aBcI\u0307".toLocaleLowerCase(Intl.GetDefaultLocale));
assertEquals("άόύώ".toLocaleUpperCase([]), "άόύώ".toLocaleUpperCase());
assertEquals("άόύώ".toLocaleUpperCase([]),
             "άόύώ".toLocaleUpperCase(Intl.GetDefaultLocale));


// English/root locale keeps U+0307 (combining dot above).
assertEquals("abci\u0307", "aBcI\u0307".toLocaleLowerCase("en"));
assertEquals("abci\u0307", "aBcI\u0307".toLocaleLowerCase(["en", "tr"]));
assertEquals("abci\u0307", "aBcI\u0307".toLowerCase());

// Greek uppercasing: not covered by intl402/String/*, yet. Tonos (U+0301) and
// other diacritic marks are dropped. This rule is based on the current CLDR's
// el-Upper transformation, but Greek uppercasing rules are more sophisticated
// than this. See http://bugs.icu-project.org/trac/ticket/10582 and
// http://unicode.org/cldr/trac/ticket/7905 .
assertEquals("Α", "α\u0301".toLocaleUpperCase("el"));
assertEquals("Α", "α\u0301".toLocaleUpperCase("el-GR"));
assertEquals("Α", "α\u0301".toLocaleUpperCase("el-Grek"));
assertEquals("Α", "α\u0301".toLocaleUpperCase("el-Grek-GR"));
assertEquals("Α", "ά".toLocaleUpperCase("el"));
assertEquals("ΑΟΥΩ", "άόύώ".toLocaleUpperCase("el"));
assertEquals("ΑΟΥΩ", "α\u0301ο\u0301υ\u0301ω\u0301".toLocaleUpperCase("el"));
assertEquals("ΑΟΥΩ", "άόύώ".toLocaleUpperCase("el"));
assertEquals("ΟΕ", "Ό\u1f15".toLocaleUpperCase("el"));
assertEquals("ΟΕ", "Ο\u0301ε\u0314\u0301".toLocaleUpperCase("el"));

// Input and output are identical.
assertEquals("αβγδε", "αβγδε".toLocaleLowerCase("el"));
assertEquals("ΑΒΓΔΕ", "ΑΒΓΔΕ".toLocaleUpperCase("el"));
assertEquals("ΑΒΓΔΕАБ𝐀𝐁", "ΑΒΓΔΕАБ𝐀𝐁".toLocaleUpperCase("el"));
assertEquals("ABCDEÂÓḴ123", "ABCDEÂÓḴ123".toLocaleUpperCase("el"));
// ASCII-only or Latin-1 only: 1-byte
assertEquals("ABCDE123", "ABCDE123".toLocaleUpperCase("el"));
assertEquals("ABCDEÂÓ123", "ABCDEÂÓ123".toLocaleUpperCase("el"));

// To make sure that the input string is not overwritten in place.
var strings = ["abCdef", "αβγδε", "άόύώ", "аб"];
for (var s  of strings) {
  var backupAsArray = s.split("");
  var uppered = s.toLocaleUpperCase("el");
  assertEquals(s, backupAsArray.join(""));
}

// In other locales, U+0301 is preserved.
assertEquals("Α\u0301Ο\u0301Υ\u0301Ω\u0301",
             "α\u0301ο\u0301υ\u0301ω\u0301".toLocaleUpperCase("en"));
assertEquals("Α\u0301Ο\u0301Υ\u0301Ω\u0301",
             "α\u0301ο\u0301υ\u0301ω\u0301".toUpperCase());

// Plane 1; Deseret and Warang Citi Script.
assertEquals("\u{10400}\u{118A0}", "\u{10428}\u{118C0}".toUpperCase());
assertEquals("\u{10428}\u{118C0}", "\u{10400}\u{118A0}".toLowerCase());
// Mathematical Bold {Capital, Small} Letter A do not change.
assertEquals("\u{1D400}\u{1D41A}", "\u{1D400}\u{1D41A}".toUpperCase());
assertEquals("\u{1D400}\u{1D41A}", "\u{1D400}\u{1D41A}".toLowerCase());
// Plane 1; New characters in Unicode 8.0
assertEquals("\u{10C80}", "\u{10CC0}".toUpperCase());
assertEquals("\u{10CC0}", "\u{10C80}".toLowerCase());
assertEquals("\u{10C80}", "\u{10CC0}".toLocaleUpperCase());
assertEquals("\u{10CC0}", "\u{10C80}".toLocaleLowerCase());
assertEquals("\u{10C80}", "\u{10CC0}".toLocaleUpperCase(["tr"]));
assertEquals("\u{10C80}", "\u{10CC0}".toLocaleUpperCase(["tr"]));
assertEquals("\u{10CC0}", "\u{10C80}".toLocaleLowerCase());
