// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(primitive) {
  return primitive.__proto__;
}
assertEquals(Symbol.prototype, f(Symbol()));
assertEquals(Symbol.prototype, f(Symbol()));
%OptimizeFunctionOnNextCall(f);
assertEquals(Symbol.prototype, f(Symbol()));
