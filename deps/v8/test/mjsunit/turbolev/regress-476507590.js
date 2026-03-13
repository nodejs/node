// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future

let empty;
empty = function () {
};
%PrepareFunctionForOptimization(empty);
function __wrapTC(f) {
  return f();
}
%PrepareFunctionForOptimization(__wrapTC);
function __f_33() {
  const __v_47 = __wrapTC(() => false);
  let __v_48;
  try {
    __v_48 = __v_47(); // Will throw
    empty();
  } catch (e) {}
  const __v_50 = __wrapTC(() => Array());
  return Array(-__v_50.unshift(false), __v_48);
}
%PrepareFunctionForOptimization(__f_33);

function __f_13() {
  return __f_33();
}
%PrepareFunctionForOptimization(__f_13);
__f_13();
%OptimizeFunctionOnNextCall(__f_13);
__f_13();
