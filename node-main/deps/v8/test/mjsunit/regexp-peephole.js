// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test peephole optimization patterns not covered by other tests.

function test(re, expectMatch, expectNoMatch = []) {
  for (const match of expectMatch) {
    assertTrue(re.test(match), `${re}.test(${match})`);
  }
  for (const noMatch of expectNoMatch) {
    assertFalse(re.test(noMatch), `${re}.test(${noMatch})`);
  }
}

// Test SkipUntilMatchInAlternativeWithFewChars
test(
    /[ab]bbbc|[de]eeef/, ['abbbcccc', 'bbbbcccc', 'deeeffff', 'eeeeffff'],
    ['aeeef', 'beeef', 'dbbbc', 'ebbbc']);
