// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

Debug = debug.Debug

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.AfterCompile) {
    // The actual source doesn't matter, just don't crash.
    var source = event_data.script().source();
    // Source will be empty for the script representing the entire module,
    // disassembly for the script representing just the function.
    assertTrue(source == "func $main\nend\n" || source == "");
  }
};

// Add the debug event listener.
Debug.setListener(listener);

var builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_v_v).addBody([]).exportFunc();
var promise = WebAssembly.compile(builder.toBuffer());

// Clear the debug listener only after the event fired.
promise.then(() => Debug.setListener(null), assertUnreachable);
