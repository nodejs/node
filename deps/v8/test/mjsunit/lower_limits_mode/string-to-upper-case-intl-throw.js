// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function createExpandingString(length) {
  return '\u00df'.repeat(length);
}

function convertCaseOptimized(str) {
  try {
    return str.toUpperCase();
  } catch(e) {
    return 42;
  }
}

%PrepareFunctionForOptimization(convertCaseOptimized);
assertEquals("SSSSSS", convertCaseOptimized(createExpandingString(3)));
%OptimizeFunctionOnNextCall(convertCaseOptimized);
const throwingLength = Math.floor(%StringMaxLength() / 2) + 1;
assertEquals(42, convertCaseOptimized(createExpandingString(throwingLength)));
