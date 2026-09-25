// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --ignore-unhandled-promises

const ta = new Uint32Array();
async function foo(a, b) {
  Object.defineProperty(a, 98, { value: 0 });
  ({'buffer':b, 'length':b,} = ta);
}
%PrepareFunctionForOptimization(foo);
foo();
foo(Int8Array);
%OptimizeMaglevOnNextCall(foo);
foo(Int8Array);
%DeoptimizeFunction(foo);
