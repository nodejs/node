// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-verify-load-elimination

function MakeBase(val) { this.x = val; }
const sentinel = new MakeBase(-1);
function cbTransition(o) { o.y = 42; return 1; }
%NeverOptimizeFunction(cbTransition);

function victim(x, y) {
  x.x;
  cbTransition(x);
  y.x;
  x == sentinel; // Will load x's map without a map check.
  y.z = 99;
  return x.y;
}

%PrepareFunctionForOptimization(victim);
let xObj = new MakeBase(0);
let yObj = new MakeBase(100);
cbTransition(yObj);
victim(xObj, yObj);

%OptimizeFunctionOnNextCall(victim);
let target = new MakeBase(1337);
victim(target, target);
