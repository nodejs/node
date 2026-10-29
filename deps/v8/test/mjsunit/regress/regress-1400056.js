// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turboshaft --allow-natives-syntax --no-lazy-feedback-allocation


function foo() {
  var arr = new Uint8Array();
  try {
    x.next();
  } catch (e) {}
  var x = arr.entries();
  x.next();
}

function bar() {
  foo();
}

%PrepareFunctionForOptimization(bar);
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
