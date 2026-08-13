// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --homomorphic-ic --allow-natives-syntax --maglev

function test() {
  let constructors = [];
  for (let i = 0; i < 20; i++) {
    constructors.push(new Function("this.x = 1.1;"));
  }

  let objects = constructors.map(C => new C());

  function get_x(o) {
    return o.x;
  }
  %PrepareFunctionForOptimization(get_x);

  // Warm up IC
  for (let obj of objects) {
    get_x(obj);
  }
  for (let obj of objects) {
    get_x(obj);
  }

  %OptimizeMaglevOnNextCall(get_x);
  get_x(objects[0]);

  // Generalize field x from Double to Tagged in-place for objects[0]'s map
  objects[0].x = {}; // This should generalize the field representation

  // Call optimized function again. It should deopt or handle it correctly.
  let res = get_x(objects[0]);

  // Check if it returned the correct value
  assertEquals(res, {});
}

test();
