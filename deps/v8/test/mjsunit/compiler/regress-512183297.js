// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic

function f(o) {
  return o.a;
}

function Map1() { this.a = 1.1; }
function Map2() { this.a = 2.1; }
function Map3() { this.a = 3.1; }
function Map4() { this.a = 4.1; }
function Map5() { this.a = 5.1; }

let objects = [new Map1(), new Map2(), new Map3(), new Map4(), new Map5()];

for (let i = 0; i < 1000; i++) {
  for (let o of objects) f(o);
}

%PrepareFunctionForOptimization(f);
for (let o of objects) f(o);
%OptimizeFunctionOnNextCall(f);
f(objects[0]);

objects[0].a = "string";

// Smi that results in an out-of-bounds access for the map load
objects[0].a = 0x3fffffff;

assertEquals(0x3fffffff, f(objects[0]));
