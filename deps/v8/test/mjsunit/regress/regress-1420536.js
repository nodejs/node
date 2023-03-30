// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() { this.x = 1; }
for (var i = 0; i < 10; i++) new f();

function foo() {
  var obj = new f();
  obj.y = -1073741825;
  return obj;
}

function bar(t) {
  var arr = [];
  for (var p in t){
    arr.push([ t[p]]);
  }
  return arr;
}

function test() {
  return bar(foo());
}

%PrepareFunctionForOptimization(f);
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(test);
assertEquals([[1], [-1073741825]], test(foo, bar));
%OptimizeFunctionOnNextCall(test);
assertEquals([[1], [-1073741825]], test(foo, bar));
