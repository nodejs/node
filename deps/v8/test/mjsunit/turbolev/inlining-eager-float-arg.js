// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

class Vec3 {
  constructor(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
  }
  scale(s) {
    return new Vec3(this.x * s, this.y * s, this.z * s);
  }
}

function scaleFirst(s, v) {
  return {
    x: v.x * s,
    y: v.y * s,
    z: v.z * s,
  };
}

function stepMethod(v, k) {
  const a = v.scale(0.5);
  const b = v.scale(k + 0.25);
  const c = scaleFirst(0.5, v);
  return a.x + a.y + a.z + b.x + b.y + b.z + c.x + c.y + c.z;
}

const v = new Vec3(1.5, 2.5, 3.5);
%PrepareFunctionForOptimization(Vec3);
%PrepareFunctionForOptimization(Vec3.prototype.scale);
%PrepareFunctionForOptimization(scaleFirst);
%PrepareFunctionForOptimization(stepMethod);
assertEquals(11.25, stepMethod(v, 0.25));
assertEquals(11.25, stepMethod(v, 0.25));
%OptimizeFunctionOnNextCall(stepMethod);
assertEquals(11.25, stepMethod(v, 0.25));
assertOptimized(stepMethod);
