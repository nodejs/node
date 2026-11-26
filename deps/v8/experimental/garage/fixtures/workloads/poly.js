// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Monomorphic -> polymorphic -> megamorphic property access. Produces varied
// type feedback in the Maglev graph (CheckMaps vs. generic load) and IC state
// transitions in the bytecode/feedback dumps.
function load(o) {
  return o.v;
}

const shapes = [
  {v: 1},
  {a: 1, v: 2},
  {b: 1, c: 2, v: 3},
  {d: 1, e: 2, f: 3, v: 4},
  {g: 1, h: 2, i: 3, j: 4, v: 5},
  {k: 1, l: 2, m: 3, n: 4, o: 5, v: 6},
];

let acc = 0;
// Monomorphic warmup.
for (let i = 0; i < 20000; i++) acc += load(shapes[0]);
// Polymorphic.
for (let i = 0; i < 20000; i++) acc += load(shapes[i % 3]);
// Megamorphic.
for (let i = 0; i < 60000; i++) acc += load(shapes[i % shapes.length]);
print(acc);
