// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-debug-as debug

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

Debug = debug.Debug

var exception = null;
var break_count = 0;

const expected_num_frames = 5;
const expected_wasm_frames = [false, true, true, false, false];
const expected_wasm_positions = [0, 1, 2, 0, 0];
const expected_function_names = ["call_debugger", "wasm_2", "wasm_1", "testFrameInspection", ""];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  ++break_count;
  try {
    var break_id = exec_state.break_id;
    var frame_count = exec_state.frameCount();
    assertEquals(expected_num_frames, frame_count);

    for (var i = 0; i < frame_count; ++i) {
      var frame = exec_state.frame(i);
      // wasm frames have unresolved function, others resolved ones.
      assertEquals(expected_wasm_frames[i], !frame.func().resolved());
      assertEquals(expected_function_names[i], frame.func().name());
      if (expected_wasm_frames[i]) {
        var script = frame.details().script();
        assertNotNull(script);
        assertEquals(expected_wasm_positions[i], frame.details().sourcePosition());
        var loc = script.locationFromPosition(frame.details().sourcePosition());
        assertEquals(expected_wasm_positions[i], loc.column);
        assertEquals(expected_wasm_positions[i], loc.position);
      }
    }
  } catch (e) {
    exception = e;
  }
};

var builder = new WasmModuleBuilder();

// wasm_1 calls wasm_2 on offset 2.
// wasm_2 calls call_debugger on offset 1.

builder.addImport("func", kSig_v_v);

builder.addFunction("wasm_1", kSig_v_v)
  .addBody([kExprNop, kExprCallFunction, 2])
  .exportAs("main");

builder.addFunction("wasm_2", kSig_v_v)
  .addBody([kExprCallFunction, 0]);

function call_debugger() {
  debugger;
}

var module = builder.instantiate({func: call_debugger});

(function testFrameInspection() {
  Debug.setListener(listener);
  module.exports.main();
  Debug.setListener(null);

  assertEquals(1, break_count);
  if (exception) throw exception;
})();
