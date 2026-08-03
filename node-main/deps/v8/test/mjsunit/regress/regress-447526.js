// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {
  throw "done";
}

function foo() {
  var i;
  while (i) {
    while (i) {
}
    i++;
  }
  while (true) {
    bar();
  }
}
%PrepareFunctionForOptimization(foo);


%OptimizeFunctionOnNextCall(foo);
assertThrows(foo);
