// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-enable-experimental-regexp-engine
// Flags: --no-enable-experimental-regexp-engine-on-excessive-backtracks

const kNoBacktrackLimit = 0;  // To match JSRegExp::kNoBacktrackLimit.
const re0 = %NewRegExpWithBacktrackLimit("(\\d+)+x", "", kNoBacktrackLimit);
const re1 = %NewRegExpWithBacktrackLimit("(\\d+)+x", "", 50);

// Backtracks remain below the limit on this subject string.
{
  let s = "3333ax3333x";
  assertArrayEquals(["3333x", "3333"], re0.exec(s));
  assertEquals(["3333x", "3333"], re1.exec(s));
}

// A longer subject exceeds the limit.
{
  let s = "333333333ax3333x";
  assertArrayEquals(["3333x", "3333"], re0.exec(s));
  assertEquals(null, re1.exec(s));
}

// ATOM regexp construction with a limit; in this case the limit should just be
// ignored, ATOMs never backtrack.
{
  const re = %NewRegExpWithBacktrackLimit("ax", "", 50);
  let s = "3333ax3333x";
  assertArrayEquals(["ax"], re.exec(s));
}
