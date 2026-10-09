// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const Debug = debug.Debug;
Debug.enable();

const builder = new WasmModuleBuilder();
const func_a = builder.addFunction('a', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI32Add,  // Breakpoint 1 (offset 5)
      kExprI32Const, 2,
      kExprI32Add,  // Breakpoint 2 (offset 8)
    ])
    .exportFunc();
const func_b = builder.addFunction('b', kSig_v_v)
    .addBody([
      kExprNop,  // Breakpoint 3 (offset 1)
    ])
    .exportFunc();

const instance = builder.instantiate();

// Test 1: Removing a breakpoint in `a` while a younger Wasm frame `b` is on top
// of the stack.
const bp_a1 = Debug.setBreakPoint(instance.exports.a, 0, 5, 'onBreakA1()');
const bp_b = Debug.setBreakPoint(instance.exports.b, 0, 1, 'onBreakB()');

globalThis.onBreakA1 = function() {
  instance.exports.b();
  return false;
};

globalThis.onBreakB = function() {
  Debug.clearBreakPoint(bp_a1);
  Debug.clearBreakPoint(bp_b);
  return false;
};

assertEquals(13, instance.exports.a(10));

// Test 2: Two recursive activations of `a` paused at different breakpoints when
// both breakpoints are removed.
const bp_a_first = Debug.setBreakPoint(
    instance.exports.a, 0, 5, 'onBreakFirst()');
const bp_a_second = Debug.setBreakPoint(
    instance.exports.a, 0, 8, 'onBreakSecond()');

let depth = 0;
globalThis.onBreakFirst = function() {
  if (depth === 0) {
    depth++;
    Debug.clearBreakPoint(bp_a_first);
    assertEquals(23, instance.exports.a(20));
  }
  return false;
};

globalThis.onBreakSecond = function() {
  Debug.clearBreakPoint(bp_a_second);
  return false;
};

assertEquals(13, instance.exports.a(10));
