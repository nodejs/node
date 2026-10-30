// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function f2() {
  const v3 = { 4294967295: "nop" };
  // Add a non-enumerable element.
  Object.defineProperty(v3, "42", { configurable: true, value: 0 });
  v3.a = "nop";
  return v3;
}
const v6 = f2();
function f7(a8) {
  a8.f = 0;
  for (const v9 in a8) {
      delete a8.a;
      gc();
  }
}
f7(f2());
