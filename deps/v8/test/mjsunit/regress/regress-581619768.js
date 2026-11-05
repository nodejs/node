// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-lazy

const promises = [];
function F1() {
  const v5 = this.constructor;
  let v6;
  try {
    v6 = new v5();
  } catch (e) {
  }
  async function* f7() {
    %PrepareFunctionForOptimization(f7);
    for (let v10 = 0; v10 < 5; v10++) {
      %OptimizeOsr();
    }
  }
  const p = f7().next();
  promises[promises.length] = p;
}
new F1();
for (const p of promises) {
  p.then(() => {}, () => {});
}
