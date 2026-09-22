// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-future --no-lazy-feedback-allocation

for (let v0 = 0; v0 < 5; v0++) {
  const v3 = [1.5, 2.5];
  async function* f4(a5, a6, ...a7) {
    for (let v8 = 0; v8 < 5; v8++) {
      v8++;
      for (let v11 = 0; v11 < 5; v11++) {
        %OptimizeOsr();
      }
      function f13(a14, a15) {
        return a15;
      }
      f13(v0, Symbol);
    }
    for (let v19 = 0; v19 < 5; v19++) {
      const v22 = Array(undefined, v19);
      const v24 = {
        [Symbol]() {},
      };
      for await (await using v25 of v3) {}
      v22[99] = Array;
    }
    return Symbol;
  }
  f4(Symbol, v3, Symbol, v0, v0, v0).next(v3).catch(() => {});
}
