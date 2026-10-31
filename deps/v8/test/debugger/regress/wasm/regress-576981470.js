// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-code-coverage

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const Debug = debug.Debug;

let exception = null;
let names = [];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    for (let i = 0; i < exec_state.frameCount(); ++i) {
      names.push(exec_state.frame(i).func().name());
    }
  } catch (e) {
    exception = e;
  }
}

const builder = new WasmModuleBuilder();
builder.addImport('mod', 'func', kSig_v_v);
builder.addFunction('w\u00e9sm', kSig_v_v)
    .addBody([kExprCallFunction, 0])
    .exportAs('main');

function call_debugger() {
  debugger;
}

const instance = builder.instantiate({mod: {func: call_debugger}});

Debug.setListener(listener);
instance.exports.main();
Debug.setListener(null);

assertNull(exception);
assertTrue(names.includes('$w\u00e9sm'), names.join(','));

const coverage = %DebugCollectWasmCoverage();
assertTrue(coverage.length > 0);
