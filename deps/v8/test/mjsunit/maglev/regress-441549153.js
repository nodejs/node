// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function F1() { }

function bar(str) {
  str.toLocaleUpperCase();
  F1 + str;
  return bar;
}

%PrepareFunctionForOptimization(bar);
bar("abc");
try { bar(bar); } catch(e) {}

function foo() {
  const arr = [1347035734,-2];
  arr.slice(0, 0).forEach(bar);

  const proxy = new Proxy(arr, {});
  return proxy.pop();
}

%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeMaglevOnNextCall(foo);
foo();
