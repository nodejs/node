// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// https://bugs.chromium.org/p/v8/issues/detail?id=10816
// When V8 sees a deprecated map, update the IC with its target.

function A() { this.x = 1 }
function B() { this.x = 1 }
function load(o) { return o.x }
%PrepareFunctionForOptimization(load);

// Initialize the load IC with the map of A before we deprecate it.
load(new A());

const oldB = new B();
(new A()).x = 1.5; // deprecates map already in FV
(new B()).x = 1.5; // deprecates map for oldB

// Should update the load IC with the target of the deprecated map.
load(oldB);

%OptimizeFunctionOnNextCall(load);
load(oldB);
assertOptimized(load);
