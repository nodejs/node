// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
function incAndToStr(n) {
    return (n+1) + "";
}

%PrepareFunctionForOptimization(incAndToStr);
assertEquals("13", incAndToStr(12));

// Emits Int32ToString.
%OptimizeMaglevOnNextCall(incAndToStr);
assertEquals("1073741824", incAndToStr(1073741823)); // kSmiMaxValue
assertTrue(isOptimized(incAndToStr));

assertEquals("5.2", incAndToStr(4.2));
assertFalse(isOptimized(incAndToStr));

// Emits Float64ToString.
%OptimizeMaglevOnNextCall(incAndToStr);
assertEquals("1073741824", incAndToStr(1073741823));
assertEquals("5.2", incAndToStr(4.2));
assertTrue(isOptimized(incAndToStr));
