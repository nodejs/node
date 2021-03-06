// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(s) {
  s = s + '0123456789012';
  return s.indexOf('0');
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo('0'));
assertEquals(0, foo('0'));
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo('0'));
