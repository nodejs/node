// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
builder.addFunction('sub', kSig_i_ii)
// input is 2 args of type int and output is int
.addBody([
  kExprLocalGet, 0, // local.get i0
  kExprLocalGet, 1, // local.get i1
  kExprI32Sub]) // i32.sub i0 i1
.exportFunc();
const instance = builder.instantiate();
const wasm_f = instance.exports.sub;

Debug = debug.Debug;

var exception = null;
var js_break_line = 0;
var break_count = 0;
var wasm_break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    print(event_data.functionName());
    if (event_data.sourceLineText() == 'Debug.setListener(null);') {
      return;
    }
    if (event_data.functionName() == 'f') {
      break_count++;
      assertTrue(
        event_data.sourceLineText().indexOf(`Line ${js_break_line}.`) > 0);
      js_break_line++;
    } else {
      assertTrue(event_data.functionName() == '$sub');
      wasm_break_count++;
    }
    exec_state.prepareStep(Debug.StepAction.StepInto);
  } catch (e) {
    exception = e;
    print(e);
  }
};

function f() {
  var result = wasm_f(3, 2); // Line 0.
  result++; // Line 1.
  return result; // Line 2.
}
assertEquals(2, f());

Debug.setListener(listener);
// Set a breakpoint on line 0.
Debug.setBreakPoint(f, 1);
// Set a breakpoint on line 2.
Debug.setBreakPoint(f, 3);
f();
Debug.setListener(null);

var break_count2 = 0;
// In the second execution, only break at javascript frame.
function listener2(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    if (event_data.sourceLineText() == 'Debug.setListener(null);') {
      return;
    }
    print(event_data.functionName());
    assertTrue(event_data.sourceLineText().indexOf(`Line `) > 0);
    assertEquals(event_data.functionName(), 'f');
    break_count2++;
    exec_state.prepareStep(Debug.StepAction.StepOut);
  } catch (e) {
    exception = e;
    print(e);
  }
};

Debug.setListener(listener2);
f();
Debug.setListener(null);

assertEquals(3, break_count);
assertEquals(3, js_break_line);
assertEquals(4, wasm_break_count);
assertEquals(2, break_count2);
assertNull(exception);
