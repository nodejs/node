// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(x) { delete x.bla; x.bla = 23 }

function foo() {
  let obj = {bla: 0};
  Object.defineProperty(obj, 'bla', {writable: false});
  bar(obj);
  return obj.bla;
}

%PrepareFunctionForOptimization(foo);
assertEquals(23, foo());
assertEquals(23, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(23, foo());
