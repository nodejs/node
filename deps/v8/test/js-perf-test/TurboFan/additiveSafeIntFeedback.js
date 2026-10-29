// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Pair {
  constructor(v0, v1) { this.v0 = v0; this.v1 = v1; }
}

const N = 100000;
const list = new Array(N);
for (let i = 0; i < N; i++) list[i] = new Pair(i, i);

function SumField() {
  let sum = 0;
  for (let i = 0; i < N; i++) {
    sum = sum + list[i].v0;
  }
  return sum;
}

createSuite('AdditiveSafeIntFeedback', 1000, SumField);
