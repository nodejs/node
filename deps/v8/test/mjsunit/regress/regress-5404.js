// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function foo(a, b) {
  return a + "0123456789012";
}

%PrepareFunctionForOptimization(foo);
foo("a");
foo("a");
%OptimizeFunctionOnNextCall(foo);
foo("a");

var a = "a".repeat(%StringMaxLength());
assertThrows(function() { foo(a); }, RangeError);

%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
assertThrows(function() { foo(a); }, RangeError);
assertOptimized(foo);
