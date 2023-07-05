// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string
let str1 = "external string turned into two byte";
let str2 = str1.substring(1);
try {
  // Turn the string to a two-byte external string, so that the sliced
  // string looks like one-byte, but its parent is actually two-byte.
  externalizeString(str1, true);
} catch (e) { }
assertEquals(
  ["x", "t", "e", "r", "n", "a", "l", " ",
  "s", "t", "r", "i", "n", "g", " ",
  "t", "u", "r", "n", "e", "d", " ",
  "i", "n", "t", "o", " ",
  "t", "w", "o", " ",
  "b", "y", "t", "e"], str2.split(""));
