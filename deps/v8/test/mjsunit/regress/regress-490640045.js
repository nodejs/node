// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

const ab = new ArrayBuffer(8);
const f64 = new Float64Array(ab);
const u32 = new Uint32Array(ab);
u32[0] = 0xFFF7FFFF;
u32[1] = 0xFFF7FFFF;
const holeNaN = f64[0];

class C {}
C.prototype.x = holeNaN;
for (let i = 0; i < 2; i++) {
  C.prototype.y = holeNaN;
}

function readX(obj) { return obj.x; }
function readY(obj) { return obj.y; }
const instance = new C();

readX(instance);
readY(instance);

C.prototype.x = 42;
C.prototype.y = 153;

let x = readX(instance);
let y = readY(instance);
assertEquals(x, 42);
assertEquals(y, 153);
