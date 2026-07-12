// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function load(a, i) {
  return a[i];
};
%PrepareFunctionForOptimization(load);
load([]);
load(0);
load("x", 0);
%OptimizeFunctionOnNextCall(load);
load([], 0);
