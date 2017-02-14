// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-debug-as debug

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

Debug = debug.Debug

var exception = null;
var break_count = 0;

const expected_frames = [
  // func-name; wasm?; pos; line; col
  ['call_debugger', false],        // --
  ['wasm_2', true, 56, 2, 1],      // --
  ['wasm_1', true, 52, 1, 2],      // --
  ['testFrameInspection', false],  // --
  ['', false]
];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  ++break_count;
  try {
    var break_id = exec_state.break_id;
    var frame_count = exec_state.frameCount();
    assertEquals(expected_frames.length, frame_count, 'frame count');

    for (var i = 0; i < frame_count; ++i) {
      var frame = exec_state.frame(i);
      assertEquals(expected_frames[i][0], frame.func().name(), 'name at ' + i);
      // wasm frames have unresolved function, others resolved ones.
      assertEquals(
          expected_frames[i][1], !frame.func().resolved(), 'resolved at ' + i);
      if (expected_frames[i][1]) {  // wasm frame?
        var script = frame.details().script();
        assertNotNull(script, 'script at ' + i);
        assertEquals(
            expected_frames[i][2], frame.details().sourcePosition(),
            'source pos at ' + i);
        var loc = script.locationFromPosition(frame.details().sourcePosition());
        assertEquals(expected_frames[i][2], loc.position, 'pos at ' + i);
        assertEquals(expected_frames[i][3], loc.line, 'line at ' + i);
        assertEquals(expected_frames[i][4], loc.column, 'column at ' + i);
      }
    }
  } catch (e) {
    exception = e;
  }
};

var builder = new WasmModuleBuilder();

// wasm_1 calls wasm_2 on offset 2.
// wasm_2 calls call_debugger on offset 1.

builder.addImport('func', kSig_v_v);

builder.addFunction('wasm_1', kSig_v_v)
    .addBody([kExprNop, kExprCallFunction, 2])
    .exportAs('main');

builder.addFunction('wasm_2', kSig_v_v).addBody([kExprCallFunction, 0]);

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
