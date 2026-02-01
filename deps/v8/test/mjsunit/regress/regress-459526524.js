// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __getProperties(obj) {
  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
 properties.push(name);
  }
  return properties;
}
{
  __getRandomProperty = function (obj, seed) {
    let properties = __getProperties(obj);
    return properties[seed % properties.length];
  };
}
  var __v_11 = new ArrayBuffer(8);
  var __v_12 = new Int32Array(__v_11);
  __v_12[0] = %GetHoleNaNUpper();
  __v_12[1] = %GetHoleNaNLower();
  var __v_13 = new Float64Array(__v_11);
  var __v_14 = new Float64Array(1);
  __v_14[0] = __v_13[0];
function __f_3(__v_17) {
    this.x = __v_17[0];
}
  var __v_15 = new __f_3(__v_14);
function __f_7() {
    __v_15[__getRandomProperty(__v_15, 373995)]();
}
try {
  __f_7();
} catch (e) { }
  %PrepareFunctionForOptimization(__f_7);
  %OptimizeFunctionOnNextCall(__f_7);
try {
  __f_7();
} catch (e) { }
