// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

const gsab = new SharedArrayBuffer(4, {maxByteLength: 8});
const gsab_ta = new Uint16Array(gsab);
const sab = new SharedArrayBuffer(4);
function foo(ta) {
    ta[1] = 0;
}
%PrepareFunctionForOptimization(foo);
function test() {
  const normal_ta = new Uint32Array(sab);
  foo(normal_ta);
  foo(gsab_ta);
}
%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
