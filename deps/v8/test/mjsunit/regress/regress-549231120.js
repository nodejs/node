// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensure that invalid or duplicate variants (including underscore delimiters)
// throw RangeError without crashing ICU.
assertThrows(
    () => new Intl.Locale("en", {
      variants: Array(31).fill("posix").join("_"),
      calendar: "gregory"
    }),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "posix_posix"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "posix_1990"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "posix-posix"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "posix-POSIX"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "-posix"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "posix-"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "posix--oxendict"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "abc"}),
    RangeError);

assertThrows(
    () => new Intl.Locale("en", {variants: "abcdefghi"}),
    RangeError);

// Valid variants should work.
assertEquals("en-u-va-posix", new Intl.Locale("en", {variants: "posix"}).toString());
assertEquals(
    "en-oxendict-spanglis",
    new Intl.Locale("en", {variants: "spanglis-oxendict"}).toString());
