// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --concurrent-recompilation

const init_ta = new Int8Array(10);
init_ta[0] = 1;

const global_obj = Object.create(init_ta);

function foo() {
  return Object.getPrototypeOf(global_obj)[0];
}
%PrepareFunctionForOptimization(foo);
assertEquals(1, foo());

%BlockAt('MaglevGraphBuilding', 5000);
%OptimizeMaglevOnNextCall(foo, 'concurrent');
assertEquals(1, foo());

%WaitUntilBlocked('MaglevGraphBuilding', 5000);

const ta = new Int8Array(1024);
Object.setPrototypeOf(global_obj, ta);
foo();

// Start building the Maglev graph - now global_obj.__proto__ == ta, so that's
// what the graph builder sees.
%Resume('MaglevGraphBuilding');
%WaitForBackgroundOptimization();

assertEquals(0, foo());
assertMaglevved(foo);
