// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var upperStart = "A".charCodeAt();
var upperEnd = "Z".charCodeAt();
var lowerStart = "a".charCodeAt();
var lowerEnd = "z".charCodeAt(0);

function foo(a) {
  if (upperStart <= a && a <= upperEnd) {
    return a + lowerEnd - lowerStart;
  }
  return a;
}

%PrepareFunctionForOptimization(foo);
foo(75);
%OptimizeFunctionOnNextCall(foo);
foo(65);
