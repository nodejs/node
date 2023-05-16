// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --harmony-string-is-well-formed

(function TestIsWellFormed() {
  const short2ByteWellFormed = '\u1234';
  const short2ByteIllFormed = '\uD83D';

  assertTrue(short2ByteWellFormed.isWellFormed());
  assertFalse(short2ByteIllFormed.isWellFormed());

  try {
    // Turn the strings into uncached external strings to hit the slow runtime
    // path.
    externalizeString(short2ByteWellFormed, true);
    externalizeString(short2ByteIllFormed, true);
  } catch (e) {}

  assertTrue(short2ByteWellFormed.isWellFormed());
  assertFalse(short2ByteIllFormed.isWellFormed());
})();

(function TestToWellFormed() {
  const short2ByteWellFormed = '\u1234';
  const short2ByteIllFormed = '\uD83D';

  assertTrue(short2ByteWellFormed.isWellFormed());
  assertFalse(short2ByteIllFormed.isWellFormed());

  try {
    // Turn the strings into uncached external strings to hit the slow runtime
    // path.
    externalizeString(short2ByteWellFormed, true);
    externalizeString(short2ByteIllFormed, true);
  } catch (e) {}

  assertEquals('\u1234', short2ByteWellFormed.toWellFormed());
  // U+FFFD (REPLACEMENT CHARACTER)
  assertEquals('\uFFFD', short2ByteIllFormed.toWellFormed());
})();
