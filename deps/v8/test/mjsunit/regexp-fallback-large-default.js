// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --no-enable-experimental-regexp-engine
// Flags: --enable-experimental-regexp-engine-on-excessive-backtracks
// Flags: --regexp-backtracks-before-fallback=1000000000

// This test is similar to regexp-fallback.js but with
// large--regexp-backtracks-before-fallback value.
//
// If the backtrack limit from --regexp-backtracks-before-fallback is larger
// than an explicit limit, then we should take the explicit limit.
let regexp = %NewRegExpWithBacktrackLimit(".+".repeat(100) + "x", "", 5000);
let subject = "a".repeat(100) + "x" + "a".repeat(99);
let result = ["a".repeat(100) + "x"];

assertArrayEquals(result, regexp.exec(subject));
assertArrayEquals(result, regexp.exec(subject));
