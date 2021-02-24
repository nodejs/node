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
load(obj);

// optimize as mono
%OptimizeFunctionOnNextCall(load);
load(obj);
assertOptimized(load);
load(obj);

// transition to poly - should retain optimized code
obj.y = 2;
load(obj);
assertOptimized(load);
load(obj);

// transition to more polymorphic
obj.z = 3;
load(obj);
obj.q =4;
load(obj);

// transition to megamorphic
assertOptimized(load);
obj.r = 5;
load(obj);
obj.s = 6;
load(obj);
assertUnoptimized(load);
load(obj);
