// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-debug-as debug

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

Debug = debug.Debug

// Initialized in setup().
var exception;
var break_count;
var num_wasm_scripts;
var module;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      ++break_count;
      // Request frame details. This should trigger creation of the Script
      // objects for all frames on the stack.
      var num_frames = exec_state.frameCount();
      for (var i = 0; i < num_frames; ++i) {
        var frame = exec_state.frame(i);
        var details = frame.details();
        var script = details.script();
        if (script.type == Debug.ScriptType.Wasm) {
          var pos = frame.sourcePosition();
          var name = script.nameOrSourceURL();
          var disassembly = Debug.disassembleWasmFunction(script.id);
          var offset_table = Debug.getWasmFunctionOffsetTable(script.id);
          assertEquals(0, offset_table.length % 3);
          var lineNr = null;
          var columnNr = null;
          for (var p = 0; p < offset_table.length; p += 3) {
            if (offset_table[p] != pos) continue;
            lineNr = offset_table[p+1];
            columnNr = offset_table[p+2];
          }
          assertNotNull(lineNr, "position should occur in offset table");
          assertNotNull(columnNr, "position should occur in offset table");
          var line = disassembly.split("\n")[lineNr];
          assertTrue(!!line, "line number must occur in disassembly");
          assertTrue(line.length > columnNr, "column number must be valid");
          var expected_string;
          if (name.endsWith("/1")) {
            // Function 0 calls the imported function.
            expected_string = "kExprCallFunction,";
          } else if (name.endsWith("/2")) {
            // Function 1 calls function 0.
            expected_string = "kExprCallFunction,";
          } else {
            assertTrue(false, "Unexpected wasm script: " + name);
          }
          assertTrue(line.substr(columnNr).startsWith(expected_string),
              "offset " + columnNr + " should start with '" + expected_string
              + "': " + line);
        }
      }
    } else if (event == Debug.DebugEvent.AfterCompile) {
      var script = event_data.script();
      if (script.scriptType() == Debug.ScriptType.Wasm) {
        ++num_wasm_scripts;
      }
    }
  } catch (e) {
    print("exception: " + e);
    exception = e;
  }
};

var builder = new WasmModuleBuilder();

builder.addImport("func", kSig_v_v);

builder.addFunction("call_import", kSig_v_v)
  .addBody([kExprCallFunction, 0])
  .exportFunc();

// Add a bit of unneccessary code to increase the byte offset.
builder.addFunction("call_call_import", kSig_v_v)
  .addLocals({i32_count: 2})
  .addBody([
      kExprI32Const, 27, kExprSetLocal, 0,
      kExprI32Const, (-7 & 0x7f), kExprSetLocal, 1,
        kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add, kExprI64UConvertI32,
        kExprI64Const, 0,
      kExprI64Ne, kExprIf, kAstStmt,
        kExprCallFunction, 1,
      kExprEnd
  ])
  .exportFunc();

function call_debugger() {
  debugger;
}

function setup() {
  module = builder.instantiate({func: call_debugger});
  exception = null;
  break_count = 0;
  num_wasm_scripts = 0;
}

(function testRegisteredWasmScripts1() {
  setup();
  Debug.setListener(listener);
  // Call the "call_import" function -> 1 script.
  module.exports.call_import();
  module.exports.call_import();
  module.exports.call_call_import();
  Debug.setListener(null);

  assertEquals(3, break_count);
  if (exception) throw exception;
})();

(function testRegisteredWasmScripts2() {
  setup();
  Debug.setListener(listener);
  module.exports.call_call_import();
  Debug.setListener(null);

  assertEquals(1, break_count);
  if (exception) throw exception;
})();
