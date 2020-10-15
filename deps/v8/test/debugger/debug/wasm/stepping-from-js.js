// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test stepping from JS through Wasm.
// A similar test exists as an inspector test already, but inspector tests are
// not run concurrently in multiple isolates (see `run-tests.py --isolates`).

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const imp_fun = builder.addImport('imp', 'ort', kSig_i_v);
const sub_fun = builder.addFunction('sub', kSig_i_ii).addBody([
  kExprLocalGet, 0,  // local.get i0
  kExprLocalGet, 1,  // local.get i1
  kExprI32Sub        // i32.sub i0 i1
]);
builder.addFunction('main', kSig_i_v)
    .addBody([
      kExprCallFunction, imp_fun,       // call import
      kExprI32Const, 3,                 // i32.const 3
      kExprCallFunction, sub_fun.index  // call 'sub'
    ])
    .exportFunc();
// Compute the line number of the 'imported' function (to avoid hard-coding it).
const import_line_nr = parseInt((new Error()).stack.match(/:([0-9]+):/)[1]) + 1;
function imported() {
  debugger;
  return 7;
}
const instance = builder.instantiate({imp: {ort: imported}});

Debug = debug.Debug;

const expected_breaks = [
  `imported:${import_line_nr + 1}:2`,   // debugger;
  `imported:${import_line_nr + 2}:2`,   // return 7;
  `imported:${import_line_nr + 2}:11`,  // return 7;
  'sub:1:58', 'sub:1:60', 'sub:1:62', 'sub:1:63', 'main:1:72'
];
let error;
function onBreak(event, exec_state, data) {
  try {
    if (event != Debug.DebugEvent.Break) return;
    if (error) return;
    if (data.sourceLineText().indexOf('Main returned.') >= 0) {
      assertEquals(0, expected_breaks.length, 'hit all expected breaks');
      return;
    }
    const pos =
        [data.functionName(), data.sourceLine(), data.sourceColumn()].join(':');
    const loc = [pos, data.sourceLineText()].join(':');
    print(`Break at ${loc}`);
    assertTrue(expected_breaks.length > 0, 'expecting more breaks');
    const expected_pos = expected_breaks.shift();
    assertEquals(expected_pos, pos);
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    if (!error) error = e;
  }
}

Debug.setListener(onBreak);
print('Running main.');
const result = instance.exports.main();
print('Main returned.');
if (error) throw error;
assertEquals(4, result);
