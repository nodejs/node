// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo(x) {
  var inner = function() {
    return x;
  }
  return inner();
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo(1));
assertEquals(2, foo(2));
%OptimizeMaglevOnNextCall(foo);
assertEquals(1, foo(1));
assertEquals(2, foo(2));
assertTrue(isMaglevved(foo));
