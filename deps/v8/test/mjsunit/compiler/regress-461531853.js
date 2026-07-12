// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function createExpandingString(length) {
    return '\u00df'.repeat(length);
}

function convertCaseOptimized(str) {
    return str.toUpperCase();
}

%PrepareFunctionForOptimization(convertCaseOptimized);
convertCaseOptimized(createExpandingString(10));
%OptimizeFunctionOnNextCall(convertCaseOptimized);
assertThrows(() => convertCaseOptimized(createExpandingString(0x0FFFFFFF)));
