// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --wasm-code-coverage

// Pausing the debugger while executing Wasm triggers regeneration of the
// Liftoff debug side table. With --wasm-code-coverage, the regenerated code
// must include coverage instrumentation so its PC offsets match the original
// code. Ensure frame inspection does not crash.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let setup_idx = builder.addImport('env', 'setup', kSig_v_v);
builder.addFunction('vuln', kSig_v_v)
    .addBody([
      kExprCallFunction, setup_idx,
      kExprLoop, kWasmVoid,
      kExprEnd,
    ])
    .exportFunc();

let instance = builder.instantiate({
  env: {
    setup: function() {
      send(JSON.stringify({
        id: 1,
        method: 'Debugger.enable',
      }));
      send(JSON.stringify({
        id: 2,
        method: 'Debugger.pause',
      }));
    }
  }
});

instance.exports.vuln();
