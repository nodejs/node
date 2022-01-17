// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks --opt
// Flags: --no-always-opt

function load(obj){
  return obj.x;
}

var o = {x:20, y:30};
var o1 = {x:20, y:30, z:40};

%PrepareFunctionForOptimization(load);
load(o);
load(o1);

%OptimizeFunctionOnNextCall(load);
load(o);
load(o1);
assertOptimized(load);

// deprecate maps in IC
o.x = 21.32;
o1.x = 21.32;

// transition poly -> mono
var o2 = {y:20, x:20};
// This bails out to interpreter and updates the IC state
load(o2);
// Optimized code sees monomorphic and should deopt.
load(o2);
// should deptimize since we wouldn't generate checks for monomorphic when
// starting off with polymorphic
assertUnoptimized(load);
