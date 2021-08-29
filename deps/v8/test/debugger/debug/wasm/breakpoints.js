// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test setting and removing breakpoints in Wasm.
// Similar tests exist as inspector tests already, but inspector tests are not
// run concurrently in multiple isolates (see `run-tests.py --isolates`).

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const body_a = [
  kExprLocalGet, 0,  // local.get i0
  kExprI32Const, 1,  // i32.const 1
  kExprI32Add        // i32.add i0 1
];
const fun_a = builder.addFunction('a', kSig_i_i).addBody(body_a).exportFunc();
const body_b = [
  kExprLocalGet, 0,                // local.get i0
  kExprCallFunction, fun_a.index,  // call a
  kExprLocalGet, 1,                // local.get i1
  kExprI32Sub,                     // i32.sub a(i0) i1
  kExprCallFunction, fun_a.index,  // call a
];
const fun_b = builder.addFunction('b', kSig_i_ii).addBody(body_b).exportFunc();

const instance = builder.instantiate();

Debug = debug.Debug;

const a_localget_offset = body_a.indexOf(kExprLocalGet);
const a_const_offset = body_a.indexOf(kExprI32Const);
const a_add_offset = body_a.indexOf(kExprI32Add);
const b_sub_offset = body_b.indexOf(kExprI32Sub);

const expected_breaks = [
  `$a:1:${fun_a.body_offset + a_add_offset}`,       // break in a at i32.add
  `$b:1:${fun_b.body_offset + b_sub_offset}`,       // break in b at i32.sub
  `$a:1:${fun_a.body_offset + a_localget_offset}`,  // break in a at local.get i0
  `$a:1:${fun_a.body_offset + a_const_offset}`      // break in a at i32.const 1
];
let error;
function onBreak(event, exec_state, data) {
  try {
    if (event != Debug.DebugEvent.Break) return;
    if (error) return;
    const pos =
        [data.functionName(), data.sourceLine(), data.sourceColumn()].join(':');
    const loc = [pos, data.sourceLineText()].join(':');
    print(`Break at ${loc}`);
    assertTrue(expected_breaks.length > 0, 'expecting more breaks');
    const expected_pos = expected_breaks.shift();
    assertEquals(expected_pos, pos);
    // When we stop in b, we add another breakpoint in a at offset 0 and remove
    // the existing breakpoint.
    if (data.functionName() === '$b') {
      Debug.setBreakPoint(instance.exports.a, 0, a_localget_offset);
      Debug.clearBreakPoint(breakpoint_a);
    }
    // When we stop at a at local.get, we set another breakpoint *in the same
    // function*, one instruction later (at i32.const).
    if (data.functionName() === '$a' &&
        data.sourceColumn() == fun_a.body_offset) {
      Debug.setBreakPoint(instance.exports.a, 0, a_const_offset);
    }
  } catch (e) {
    if (!error) error = e;
  }
}

Debug.setListener(onBreak);
const breakpoint_a = Debug.setBreakPoint(instance.exports.a, 0, a_add_offset);
const breakpoint_b = Debug.setBreakPoint(instance.exports.b, 0, b_sub_offset);
print('Running b(11).');
const result = instance.exports.b(11);
print('Returned from wasm.');
if (error) throw error;
assertEquals(0, expected_breaks.length, 'all breaks were hit');
