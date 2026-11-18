// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev

const funcs = [];
for (let i = 0; i < 20; i++) {
  const fn = new Function('a', 'b', `
    /* test_id_${i} */
    let v1 = a.x; // polymorphic
    b.x = 42; // polymorphic
    let v2 = a.x; // can we eliminate this load?
    return v1 + v2;
  `);

  const a_objs = [
    {x: 1, a1: 1, ['dummy_a_' + i]: 1},
    {x: 2, a2: 2, ['dummy_a_' + i]: 2},
    {x: 3, a3: 3, ['dummy_a_' + i]: 3},
    {x: 4, a4: 4, ['dummy_a_' + i]: 4},
  ];
  // Make this field mutable
  for (let a of a_objs) a.x = 10;

  const b_objs = [
    {x: 1, b1: 1, ['dummy_b_' + i]: 1},
    {x: 2, b2: 2, ['dummy_b_' + i]: 2},
    {x: 3, b3: 3, ['dummy_b_' + i]: 3},
    {x: 4, b4: 4, ['dummy_b_' + i]: 4},
  ];
  // Make this field mutable
  for (let b of b_objs) b.x = 10;

  funcs.push({fn, a_objs, b_objs});
}

for (let {fn, a_objs, b_objs} of funcs) {
  %PrepareFunctionForOptimization(fn);
  for (let j = 0; j < 4; j++) {
    fn(a_objs[j], b_objs[j]);
  }
  %OptimizeFunctionOnNextCall(fn);
  const result = fn(a_objs[0], b_objs[0]);
  // Unfortunately we cannot observe whether the load was eliminated, we can
  // only assert the result. Run with --turboshaft-trace-load-elimination to
  // observe the load being eliminated.
  assertEquals(20, result);
}
