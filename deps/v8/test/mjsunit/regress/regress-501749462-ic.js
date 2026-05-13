// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const ab = new ArrayBuffer(8);
const f64 = new Float64Array(ab);
const u32 = new Uint32Array(ab);
u32[0] = 0xFFF7FFFF;
u32[1] = 0xFFF7FFFF;
const holeNaN = f64[0];

(function() {
  function set_a(o, v) { o.a = v; }
  const o1 = {}; set_a(o1, 1.1);
  const o2 = {}; set_a(o2, holeNaN);
  function load_o2() { return o2.a; }

  // Exercise ICs without triggering full optimization
  for (let i = 0; i < 20; i++) {
    load_o2();
  }
})();
