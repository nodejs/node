// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function make_callee() {
  let vars = "";
  let code = `(function() { ${vars} return function callee() {
  }; })()`;
  return eval(code);
}
const global_callee = make_callee();
function mycall(f) {
  return f();
}
function get_f(flag) {
  let s = 0;
  for(let i=0; i<100; i++) s++;
  return flag ? global_callee : global_callee;
}
function foo() {
  let f = get_f();
  return mycall(f);
}
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);
mycall(() =>{length: 1});
foo();
%OptimizeFunctionOnNextCall(foo);
"Result: " + foo();
