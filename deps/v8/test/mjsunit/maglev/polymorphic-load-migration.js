// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-always-turbofan

function A() { this.x = 1 }
function B() { this.x = 1 }

const a = new A();
a.y = 1;
(new A()).x = 0.1;  // deprecate a

const b = new B();
b.y = 1;
(new B()).x = 0.1;  // deprecate b

function load(o) { return o.y }

// Make o.y polymorphic and compile it with maglev. a and b will be deprecated
// causing migrations in a way that o.y is still at the same offset. This allows
// maglev to optimize it behind a CheckMapsWithMigration() node.

//   |   a    |   |    |     a     |
//   |--------|   |    |-----------|
//   | x: Smi |   |    | x: Double |
//                |
//       |       -|->      |
//       V       -|->      V
//                |
//   |   a    |   |    |     a     |
//   |--------|   |    |-----------|
//   | x: Smi |   |    | x: Double |
//   | y: Smi |   |    | y: Smi    |   <-- y did not change.

%PrepareFunctionForOptimization(load);
load(a);
load(b);

%OptimizeMaglevOnNextCall(load);
load(a);
load(b);
assertOptimized(load);

// Create a fresh deprecated map, to test the migration path in
// CheckMapsWithMigration().

function C() { this.x = 1 }

const c = new C();
c.y = 1;
(new C()).x = 0.1;  // deprecate c

load(c);
assertUnoptimized(load);
