// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-inlining

function inner(o) {
  "use strict"
  return 10 + o.x + 100;
}

function foo(o) {
  return 1000 + inner(o) + 10000;
}

%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(foo);
assertEquals(11111, foo({x:1}));
assertEquals(11111, foo({x:1}));

%OptimizeMaglevOnNextCall(foo);
// The inlined inner function will deopt -- this deopt should succeed.
assertEquals(11111, foo({y:2,x:1}));
