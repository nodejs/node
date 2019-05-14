// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  const r = {e: NaN, g: undefined, c: undefined};
  const u = {__proto__: {}, e: new  Set(), g: 0, c: undefined};
  return r;
}
foo();
%OptimizeFunctionOnNextCall(foo);
const o = foo();
Object.defineProperty(o, 'c', {value: 42});
