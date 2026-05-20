// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function reduceLHS() {
  for (var i = 0; i < 2 ;i++) {
    let [q, r] = [1n, 1n];
    r = r - 1n;
    q += 1n;
    q = r;
  }
}

%PrepareFunctionForOptimization(reduceLHS);
reduceLHS();
%OptimizeFunctionOnNextCall(reduceLHS);
reduceLHS();


function reduceRHS() {
  for (var i = 0; i < 2 ;i++) {
    let [q, r] = [1n, 1n];
    r = 1n - r;
    q += 1n;
    q = r;
  }
}

%PrepareFunctionForOptimization(reduceRHS);
reduceRHS();
%OptimizeFunctionOnNextCall(reduceRHS);
reduceRHS();
