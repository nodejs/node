// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function Thingy() {}
Thingy.prototype = {
  foo: function() { return 42; }
};

const x = new Thingy();

function f(o) {
  return o.foo();
}

%PrepareFunctionForOptimization(f);
assertEquals(42, f(x));
%OptimizeMaglevOnNextCall(f);
assertEquals(42, f(x));

Thingy.prototype.foo = function() { return 56; }
assertEquals(56, f(x));
