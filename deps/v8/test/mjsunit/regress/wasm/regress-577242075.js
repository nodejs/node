// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap --max-old-space-size=64

const weakSets = [];
const fillers = [];
for (let b = 0; b < 40; b++) {
  for (let f = 0; f < 120; f++) {
    fillers.push({});
  }
  for (let w = 0; w < 4; w++) {
    weakSets.push(new WeakSet());
  }
}
gc();

for (let round = 0; round < 20; round++) {
  const youngKey = {};
  for (let i = 0; i < weakSets.length; i++) {
    weakSets[i].add(youngKey);
  }
  let mem1 = new WebAssembly.Memory({ initial: 0 });
  let mem2 = new WebAssembly.Memory({ initial: 0 });
  try {
    mem1.grow(512);
    mem2.grow(16384);
  } catch (e) {}
  mem1 = null;
  mem2 = null;
  gc();
}
