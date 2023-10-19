// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

function Bar(x) {
  this.bar = x
}

function foo(x) {
  return new Bar(...x, 1);
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo([1]).bar);
assertEquals(2, foo([2]).bar);
%OptimizeMaglevOnNextCall(foo);
assertEquals(1, foo([1]).bar);
assertEquals(2, foo([2]).bar);
assertTrue(isMaglevved(foo));
