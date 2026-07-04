// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function createExpandingString(length) {
  return '\u0130'.repeat(length);
}

function convertCaseOptimized(str) {
  try {
    return str.toLowerCase();
  } catch(e) {
    return 42;
  }
}

%PrepareFunctionForOptimization(convertCaseOptimized);
assertEquals("i\u0307i\u0307i\u0307", convertCaseOptimized(createExpandingString(3)));
%OptimizeFunctionOnNextCall(convertCaseOptimized);
const throwingLength = Math.floor(%StringMaxLength() / 2) + 1;
assertEquals(42, convertCaseOptimized(createExpandingString(throwingLength)));
