// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function makeDictionaryObject() {
  const obj = {};
  for (let i = 0; i < 200; i++) {
    obj["prop_" + i] = i;
  }
  obj.target = { valid: true };
  return obj;
}

function victim(obj, float_array) {
  const f = float_array[0];
  const res = obj.target.valid;
  return res ? f : 0.0;
}

const dict_obj = makeDictionaryObject();

const ab = new ArrayBuffer(8);
const u32 = new Uint32Array(ab);
const f64 = new Float64Array(ab);
u32[0] = 0x41414141;
u32[1] = 0x00004141;

%PrepareFunctionForOptimization(victim);

for (let i = 0; i < 100; i++) {
  victim(dict_obj, f64);
}

%OptimizeMaglevOnNextCall(victim);
victim(dict_obj, f64);

Object.defineProperty(dict_obj, "target", {
  configurable: true,
  get() {
    gc();
    return { valid: false };
  }
});

victim(dict_obj, f64);
