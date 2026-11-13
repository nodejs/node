// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

// Trigger a veneer split on a switch case label with two consecutive tbz
// branches spanning >32KB from the jump table. Sweep padding sizes to hit
// the 32KB window across build configurations and flags.
function test(n1, n2, n3) {
  const pad1 = "c1 = (c1 % d) | 0;\n".repeat(n1);
  const pad2 = "c2 = (c2 % d) | 0;\n".repeat(n2);
  const pad3 = "c1 = (c1 % d) | 0;\n".repeat(n3);

  const f = new Function("s", "a", "b", "c1", "c2", "d", `
    switch (s) {
      case 0:
        ${pad1}
        if (a & 1) {
          ${pad2}
          if (b & 1) {
            ${pad3}
            return 1 + c1 + c2;
          }
        }
      case 1: return 2;
      case 2: return 3;
      case 3: return 4;
      case 4: return 5;
      case 5: return 6;
    }
    return 7;
  `);

  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 5; i++) {
    assertEquals(4, f(0, 1, 1, 7, 8, 3));
  }
  %OptimizeMaglevOnNextCall(f);

  assertEquals(4, f(0, 1, 1, 7, 8, 3));
  assertEquals(2, f(1, 1, 1, 7, 8, 3));
  assertEquals(2, f(0, 0, 1, 7, 8, 3));
  assertEquals(2, f(0, 1, 0, 7, 8, 3));
  assertEquals(6, f(5, 1, 1, 7, 8, 3));
  assertEquals(7, f(9, 1, 1, 7, 8, 3));
}

for (const n of [110, 140, 175, 220, 275, 345, 430]) {
  test(n, n, n);
}
test(500, 500, 400);
