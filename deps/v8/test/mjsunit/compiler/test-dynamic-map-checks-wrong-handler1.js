// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks --opt
// Flags: --no-always-opt

function load(obj){
  return obj.x;
}

var o = {x: 10, y:20};
var o1 = {x:10, y:20, z:30};

%PrepareFunctionForOptimization(load);
// polymorphic with same handler
load(o);
load(o1);

%OptimizeFunctionOnNextCall(load);
load(o);
load(o1);
assertOptimized(load);

var o2 = {y: 10, x:20};
// deopts but stays optimized
load(o2);
assertOptimized(load);

// deopts and discard code on wrong handler
load(o2);
assertUnoptimized(load);
