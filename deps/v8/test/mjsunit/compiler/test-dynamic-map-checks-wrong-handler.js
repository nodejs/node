// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks --opt
// Flags: --no-always-opt

function load(obj){
  return obj.x;
}

%PrepareFunctionForOptimization(load);
obj = {};
obj.x = 1;

//get mono feedback
load(obj);

// optimize as mono
%OptimizeFunctionOnNextCall(load);
load(obj);
assertOptimized(load);
load(obj);

// change the object's representation.
obj.x = 2.3;
load(obj);
// deoptimizes on a wrong map but retains the code
assertOptimized(load);

// deoptimizes on the wrong handler.
load(obj);
assertUnoptimized(load);
