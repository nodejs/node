// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class C extends Object { constructor() { super(); } }
function opt_me() {
  return Reflect.construct(C, [], WeakMap);
}

%PrepareFunctionForOptimization(C.prototype.constructor);
opt_me();
%OptimizeMaglevOnNextCall(C.prototype.constructor);
let obj = opt_me();

assertThrows(() => obj.set({}, 123), TypeError);
